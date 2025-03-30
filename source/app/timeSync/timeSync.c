#include "timeSync.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "cJSON.h"
#include "cmsis_os.h"
#include "dns_resolver.h"
#include "error.h"
#include "httpClient.h"
#include "httpSessionMgr.h"
#include "logger.h"
#include "lwip/sockets.h"

/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/
#define NTP_PORT        123
#define NTP_PACKET_SIZE 48
#define UNIX_OFFSET     2208988800UL  // Seconds from 1900 to 1970
#define ONE_HOUR_MS     ( 3600000 )   // 1 hour in milliseconds

#define TIMEZONE_SERVER "http://ip-api.com/json"
#define TIMEZONE_PORT   ( 80 )

#define TIMEZONE_MAX_HTTP_RETRIES 3
#define TIMEZONE_REQUEST_TIMEOUT  30000

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/
typedef enum
{
    TIME_SYNC_INIT = 0,
    TIME_SYNC_GET_TIMEZONE,
    TIME_SYNC_WAIT_FOR_TIMEZONE_RESPONSE,
    TIME_SYNC_GET_NTP_TIME,
    TIME_SYNC_TIME_SYNC,
    TIME_SYNC_IDLE
} tTimeSync_state;

typedef enum
{
    PRIMARY_TIME_SERVER,
    SECONDARY_TIME_SERVER
} tTimeSync_serverType;

typedef struct
{
    uint8_t start_month;  // Month when DST starts (1 = January, 12 = December)
    uint8_t start_week;   // Week of the month when DST starts (1 = first week, 5 = last week)
    uint8_t start_wday;   // Day of the week when DST starts (0 = Sunday, 6 = Saturday)
    uint8_t end_month;    // Month when DST ends
    uint8_t end_week;     // Week of the month when DST ends
    uint8_t end_wday;     // Day of the week when DST ends
    int32_t offset;       // Additional offset in seconds for DST (e.g., +3600)
} tDstInfo;

typedef struct
{
    const char *name;    // Timezone name
    int32_t utc_offset;  // Offset from UTC in seconds (e.g., +3600 for UTC+1)
    tDstInfo dst_info;   // DST information
} tTimeZoneInfo;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void timeSyncTask( void *pvParameters );
static bool parseTimeZoneInfo( const char *data, size_t dataSize );
static void timeZoneHttpResponseCallback( const char *data, size_t dataSize );
static void timeZoneHttpErrorCallback( uint32_t errorCode );
static bool getNtpTime( tTimeSync_serverType serverType, uint32_t *timestamp );
static void syncRtcWithTime( uint32_t ntpTimestamp );
static int32_t getTimezoneOffset( const char *timezoneName, const struct tm *timeinfo );
static bool isDstActive( const tTimeZoneInfo *timezone, const struct tm *timeinfo );

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
extern RTC_HandleTypeDef hrtc;

static const char *m_timeSync_timeServer[] = {
    [PRIMARY_TIME_SERVER] = "0.pl.pool.ntp.org",
    [SECONDARY_TIME_SERVER] = "1.pl.pool.ntp.org"
};

static const tTimeZoneInfo m_timeSync_timezones[] = {
    { "UTC", 0, { 0, 0, 0, 0, 0, 0, 0 } },                           // UTC (no DST)
    { "Europe/Lisbon", 0, { 3, 5, 0, 10, 5, 0, 3600 } },             // Portugal (UTC+0, DST: UTC+1)
    { "Europe/Warsaw", 3600, { 3, 5, 0, 10, 5, 0, 3600 } },          // Poland (UTC+1, DST: UTC+2)
    { "America/New_York", -18000, { 3, 2, 0, 11, 1, 0, 3600 } },     // New York (UTC-5, DST: UTC-4)
    { "America/Los_Angeles", -28800, { 3, 2, 0, 11, 1, 0, 3600 } },  // Los Angeles (UTC-8, DST: UTC-7)
    { "Asia/Tokyo", 32400, { 0, 0, 0, 0, 0, 0, 0 } },                // Tokyo (UTC+9, no DST)
    { "Europe/London", 0, { 3, 5, 0, 10, 5, 0, 3600 } },             // London (UTC+0, DST: UTC+1)
    { "Australia/Sydney", 39600, { 10, 1, 0, 4, 1, 0, 3600 } },      // Sydney (UTC+10, DST: UTC+11)
    { "Asia/Kolkata", 19800, { 0, 0, 0, 0, 0, 0, 0 } },              // India (UTC+5:30, no DST)
    { "Africa/Johannesburg", 7200, { 0, 0, 0, 0, 0, 0, 0 } },        // Johannesburg (UTC+2, no DST)
    { "America/Sao_Paulo", -10800, { 2, 3, 0, 11, 1, 0, 3600 } },    // Sao Paulo (UTC-3, DST: UTC-2)
    { "Asia/Shanghai", 28800, { 0, 0, 0, 0, 0, 0, 0 } }              // Shanghai (UTC+8, no DST)
};

// TODO: Handle DST time

static bool m_timeSync_initalized = false;
static tTimeSync_state m_timeSync_state = TIME_SYNC_INIT;
static tTimeSync_serverType m_timeSync_serverType = PRIMARY_TIME_SERVER;
static tHttpClient_client *m_timeSync_timeZoneHttpClient = NULL;
static uint8_t m_timeSync_httpRetryCount = 0;

static tTimeSync_localizationInfo m_timeSync_localizationInfo;

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
// Initialize the time sync task
void timeSync_init( void )
{
    if( !m_timeSync_initalized )
    {
        m_timeSync_initalized = true;
        const osThreadAttr_t attributes = {
            .name = "TimeSyncTask",
            .stack_size = 2048,
            .priority = (osPriority_t)osPriorityNormal,
        };
        osThreadNew( timeSyncTask, NULL, &attributes );
    }
}

const tTimeSync_localizationInfo *timeSync_getLocalizationInfo( void )
{
    return &m_timeSync_localizationInfo;
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static void timeSyncTask( void *pvParameters )
{
    uint32_t timestamp = 0;

    while( 1 )
    {
        switch( m_timeSync_state )
        {
            case TIME_SYNC_INIT:
            {
                memset( &m_timeSync_localizationInfo, 0, sizeof( tTimeSync_localizationInfo ) );

                m_timeSync_timeZoneHttpClient = httpClient_createNewHttpClient( timeZoneHttpResponseCallback, timeZoneHttpErrorCallback );
                httpClient_configureRequest( m_timeSync_timeZoneHttpClient, TIMEZONE_SERVER, TIMEZONE_PORT, GET );

                m_timeSync_state = TIME_SYNC_GET_TIMEZONE;
            }
            break;
            case TIME_SYNC_GET_TIMEZONE:
            {
                if( m_timeSync_httpRetryCount < TIMEZONE_MAX_HTTP_RETRIES )
                {
                    httpSessionMgr_startNewSession( m_timeSync_timeZoneHttpClient );
                }
                else
                {
                    // After 3 failed attempts, fall back to UTC timezone
                    snprintf( m_timeSync_localizationInfo.timezone, sizeof( m_timeSync_localizationInfo.timezone ), "UTC" );
                    m_timeSync_state = TIME_SYNC_GET_NTP_TIME;
                }

                vTaskDelay( 1000 );
            }
            break;
            case TIME_SYNC_WAIT_FOR_TIMEZONE_RESPONSE:
            {
                // Wait for HTTP response or timeout
                vTaskDelay( TIMEZONE_REQUEST_TIMEOUT );
                if( ( m_timeSync_httpRetryCount < TIMEZONE_MAX_HTTP_RETRIES ) && ( TIME_SYNC_WAIT_FOR_TIMEZONE_RESPONSE == m_timeSync_state ) )
                {  // No response or error
                    m_timeSync_httpRetryCount++;
                    m_timeSync_state = TIME_SYNC_GET_TIMEZONE;  // Retry the request
                }
                break;
            }
            break;
            case TIME_SYNC_GET_NTP_TIME:
            {
                LOG_INFO( "Requesting time from NTP server from %s", m_timeSync_timeServer[m_timeSync_serverType] );
                if( getNtpTime( m_timeSync_serverType, &timestamp ) )
                {
                    m_timeSync_state = TIME_SYNC_TIME_SYNC;
                }
                else
                {
                    LOG_WARNING( "No response from time server. Swapping server" );
                    m_timeSync_serverType = ( PRIMARY_TIME_SERVER == m_timeSync_serverType ) ? SECONDARY_TIME_SERVER : PRIMARY_TIME_SERVER;
                }
            }
            break;
            case TIME_SYNC_TIME_SYNC:
            {
                LOG_INFO( "Synchronizing RTC..." );
                syncRtcWithTime( timestamp );
                m_timeSync_state = TIME_SYNC_IDLE;
            }
            break;
            case TIME_SYNC_IDLE:
            {
                vTaskDelay( ONE_HOUR_MS );
                m_timeSync_state = TIME_SYNC_GET_NTP_TIME;
            }
            break;
            default:
            {
                LOG_ERROR( "Unknown state" );
            }
            break;
        }
    }
}

static void timeZoneHttpResponseCallback( const char *data, size_t dataSize )
{
    if( data == NULL || dataSize == 0 )
    {
        LOG_ERROR( "Error: No data received from HTTP request" );
        return;
    }

    // Parse the received JSON data
    if( parseTimeZoneInfo( data, dataSize ) )
    {
        // Free httpClient
        httpClient_deleteClient( &m_timeSync_timeZoneHttpClient );
        // Successfully parsed the timezone info, proceed to next state
        m_timeSync_state = TIME_SYNC_GET_NTP_TIME;
    }
    else
    {
        // Retry if the parsing fails
        m_timeSync_httpRetryCount++;
        if( m_timeSync_httpRetryCount < TIMEZONE_MAX_HTTP_RETRIES )
        {
            m_timeSync_state = TIME_SYNC_GET_TIMEZONE;  // Retry
        }
        else
        {
            // Fallback to UTC if retries exceed the limit
            snprintf( m_timeSync_localizationInfo.timezone, sizeof( m_timeSync_localizationInfo.timezone ), "UTC" );
            m_timeSync_state = TIME_SYNC_GET_NTP_TIME;  // Proceed to NTP time retrieval
        }
    }
}

static void timeZoneHttpErrorCallback( uint32_t errorCode )
{
    LOG_ERROR( "Http Error recevied with error code 0x%X", errorCode );
}

static bool parseTimeZoneInfo( const char *data, size_t dataSize )
{
    // Initialize the cJSON object
    cJSON *json = cJSON_ParseWithLength( data, dataSize );
    if( json == NULL )
    {
        LOG_ERROR( "Error parsing JSON response" );
        return false;  // Return false if parsing fails
    }

    // Extract the "timezone", "country", and "city" fields from the JSON response
    cJSON *timezone = cJSON_GetObjectItemCaseSensitive( json, "timezone" );
    cJSON *country = cJSON_GetObjectItemCaseSensitive( json, "country" );
    cJSON *city = cJSON_GetObjectItemCaseSensitive( json, "city" );
    cJSON *latitude = cJSON_GetObjectItemCaseSensitive( json, "lat" );
    cJSON *longitude = cJSON_GetObjectItemCaseSensitive( json, "lon" );

    // Check if the necessary fields exist in the JSON
    if( cJSON_IsString( timezone ) && timezone->valuestring != NULL )
    {
        strncpy( m_timeSync_localizationInfo.timezone, timezone->valuestring, sizeof( m_timeSync_localizationInfo.timezone ) - 1 );
    }
    else
    {
        LOG_ERROR( "Timezone information is missing or invalid\n" );
        cJSON_Delete( json );
        return false;  // Return false if "timezone" is missing or invalid
    }

    if( cJSON_IsString( country ) && country->valuestring != NULL )
    {
        strncpy( m_timeSync_localizationInfo.country, country->valuestring, sizeof( m_timeSync_localizationInfo.country ) - 1 );
    }
    else
    {
        LOG_ERROR( "Country information is missing or invalid\n" );
        cJSON_Delete( json );
        return false;  // Return false if "country" is missing or invalid
    }

    if( cJSON_IsString( city ) && city->valuestring != NULL )
    {
        strncpy( m_timeSync_localizationInfo.city, city->valuestring, sizeof( m_timeSync_localizationInfo.city ) - 1 );
    }
    else
    {
        LOG_ERROR( "City information is missing or invalid\n" );
        cJSON_Delete( json );
        return false;  // Return false if "city" is missing or invalid
    }

    if( cJSON_IsNumber( latitude ) && latitude->valuedouble != 0 )
    {
        snprintf( m_timeSync_localizationInfo.latitude, sizeof( m_timeSync_localizationInfo.latitude ) - 1, "%0.2f", latitude->valuedouble );
    }
    else
    {
        LOG_ERROR( "Latitude information is missing or invalid\n" );
        cJSON_Delete( json );
        return false;  // Return false if "latitude" is missing or invalid
    }

    if( cJSON_IsNumber( longitude ) && longitude->valuedouble != 0 )
    {
        snprintf( m_timeSync_localizationInfo.longitude, sizeof( m_timeSync_localizationInfo.longitude ) - 1, "%0.2f", longitude->valuedouble );
    }
    else
    {
        LOG_ERROR( "Longitude information is missing or invalid\n" );
        cJSON_Delete( json );
        return false;  // Return false if "longitude" is missing or invalid
    }

    // Successfully parsed the timezone information
    LOG_INFO( "Timezone: %s\n", m_timeSync_localizationInfo.timezone );
    LOG_INFO( "Country: %s\n", m_timeSync_localizationInfo.country );
    LOG_INFO( "City: %s\n", m_timeSync_localizationInfo.city );
    LOG_INFO( "Latitude: %s\n", m_timeSync_localizationInfo.latitude );
    LOG_INFO( "Longitude: %s\n", m_timeSync_localizationInfo.longitude );

    // Clean up cJSON object
    cJSON_Delete( json );

    return true;  // Return true if parsing was successful
}

static bool getNtpTime( tTimeSync_serverType serverType, uint32_t *timestamp )
{
    if( NULL != timestamp )
    {
        struct sockaddr_in server_addr;
        int sockfd;
        uint8_t ntp_packet[NTP_PACKET_SIZE];
        ssize_t bytes_sent;
        ssize_t bytes_received;
        struct timeval timeout = { 1, 0 };  // 1 second timeout
        fd_set readfds;
        ip_addr_t ntp_ip;

        // Resolve the NTP server hostname to an IP address
        if( !dnsResolver_resolveHostname( m_timeSync_timeServer[serverType], &ntp_ip ) )
        {
            LOG_ERROR( "Error: Unable to resolve NTP server hostname\n" );
            return false;
        }

        // Prepare NTP request packet
        memset( ntp_packet, 0, NTP_PACKET_SIZE );
        ntp_packet[0] = 0x1B;  // LI = 0 (no leap warning), VN = 3 (version 3), Mode = 3 (client)

        // Create socket
        sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if( sockfd < 0 )
        {
            LOG_ERROR( "Socket creation failed" );
            return false;
        }

        // Set timeout for receiving data
        setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) );

        // Set server address and port
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons( NTP_PORT );
        server_addr.sin_addr.s_addr = ntp_ip.addr;  // Use the resolved IP address

        // Send NTP request
        bytes_sent = sendto( sockfd, ntp_packet, NTP_PACKET_SIZE, 0, (struct sockaddr *)&server_addr, sizeof( server_addr ) );
        if( bytes_sent < 0 )
        {
            LOG_ERROR( "Send failed" );
            close( sockfd );
            return false;
        }

        // Wait for the response
        FD_ZERO( &readfds );
        FD_SET( sockfd, &readfds );

        bytes_received = select( sockfd + 1, &readfds, NULL, NULL, &timeout );
        if( bytes_received <= 0 )
        {
            LOG_ERROR( "NTP server did not respond within the timeout\n" );
            close( sockfd );
            return false;
        }

        // Receive the response
        bytes_received = recvfrom( sockfd, ntp_packet, NTP_PACKET_SIZE, 0, NULL, NULL );
        if( bytes_received < 0 )
        {
            LOG_ERROR( "Receive failed" );
            close( sockfd );
            return false;
        }

        // Parse response. Extract Transmit Timestamp field: 40-47 byte
        // The first 32 bits are the number of seconds since January 1, 1900 (NTP epoch)
        // The next 32 bits are fractions of a second.
        *timestamp = ( ntp_packet[40] << 24 ) | ( ntp_packet[41] << 16 ) | ( ntp_packet[42] << 8 ) | ntp_packet[43];
        *timestamp -= UNIX_OFFSET;  // Convert NTP time to UNIX time

        // Close the socket
        close( sockfd );

        return true;
    }
    else
    {
        return false;
    }
}

static int32_t getTimezoneOffset( const char *timezoneName, const struct tm *timeinfo )
{
    for( size_t i = 0; i < sizeof( m_timeSync_timezones ) / sizeof( m_timeSync_timezones[0] ); i++ )
    {
        if( strcmp( m_timeSync_timezones[i].name, timezoneName ) == 0 )
        {
            int32_t offset = m_timeSync_timezones[i].utc_offset;
            if( isDstActive( &m_timeSync_timezones[i], timeinfo ) )
            {
                offset += m_timeSync_timezones[i].dst_info.offset;
            }
            return offset;
        }
    }
    return 0;
}

static void syncRtcWithTime( uint32_t ntpTimestamp )
{
    struct tm timeinfo;
    uint32_t localTime = ntpTimestamp + getTimezoneOffset( m_timeSync_localizationInfo.timezone, &timeinfo );

    gmtime_r( (time_t *)&localTime, &timeinfo );

    RTC_TimeTypeDef rtc_time;
    RTC_DateTypeDef rtc_date;

    // Set RTC time
    rtc_time.Hours = timeinfo.tm_hour;
    rtc_time.Minutes = timeinfo.tm_min;
    rtc_time.Seconds = timeinfo.tm_sec;
    rtc_time.TimeFormat = RTC_HOURFORMAT12_AM;
    rtc_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtc_time.StoreOperation = RTC_STOREOPERATION_RESET;

    if( HAL_RTC_SetTime( &hrtc, &rtc_time, RTC_FORMAT_BIN ) != HAL_OK )
    {
        LOG_ERROR( "Failed to set RTC time\n" );
        return;
    }

    // Set RTC date
    rtc_date.WeekDay = timeinfo.tm_wday + 1;  // RTC expects value from 1 to 7 (Monday=1)
    rtc_date.Month = timeinfo.tm_mon + 1;     // RTC expects value from 11 to 13 (Januaray=1)
    rtc_date.Date = timeinfo.tm_mday;
    rtc_date.Year = timeinfo.tm_year - 100;  // RTC expects valye from 0 to 99

    if( HAL_RTC_SetDate( &hrtc, &rtc_date, RTC_FORMAT_BIN ) != HAL_OK )
    {
        LOG_ERROR( "Failed to set RTC date\n" );
        return;
    }

    LOG_INFO( "RTC synchronized to local time: %02d-%02d-%04d %02d:%02d:%02d\n",
              rtc_date.Date, rtc_date.Month, rtc_date.Year + 2000,
              rtc_time.Hours, rtc_time.Minutes, rtc_time.Seconds );
}

static bool isDstActive( const tTimeZoneInfo *timezone, const struct tm *timeinfo )
{
    const tDstInfo *dst = &timezone->dst_info;

    if( dst->offset == 0 )
    {
        return false;
    }

    struct tm start = { .tm_year = timeinfo->tm_year, .tm_mon = dst->start_month - 1, .tm_mday = 1, .tm_hour = 2 };
    mktime( &start );
    while( start.tm_wday != dst->start_wday )
    {
        start.tm_mday++;
        mktime( &start );
    }
    if( dst->start_week < 5 )
    {
        start.tm_mday += ( dst->start_week - 1 ) * 7;
        mktime( &start );
    }

    struct tm end = { .tm_year = timeinfo->tm_year, .tm_mon = dst->end_month - 1, .tm_mday = 1, .tm_hour = 2 };
    mktime( &end );
    while( end.tm_wday != dst->end_wday )
    {
        end.tm_mday++;
        mktime( &end );
    }
    if( dst->end_week < 5 )
    {
        end.tm_mday += ( dst->end_week - 1 ) * 7;
        mktime( &end );
    }

    time_t current = mktime( (struct tm *)timeinfo );
    return ( current >= mktime( &start ) && current < mktime( &end ) );
}
