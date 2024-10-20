#include "timeSync.h"

#include <stdbool.h>

#include "cmsis_os.h"
#include "httpClient.h"
#include "httpSessionMgr.h"
#include "logger.h"
#include "rtc.h"

/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/
#define SYNC_TIMER_TIMEOUT ( 3600 * 1000 )

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/
typedef struct
{
    const char *timeServerUrl;
    uint16_t timeServerPort;
    RTC_HandleTypeDef *rtc;
    tHttpClient_client *httpClient;
    osTimerId syncTimer;
    bool isInitalized;
} tTimeSync_config;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void timeSyncHttpResponseCallback( void );
static void timeSyncHttpErrorCallback( uint32_t errorCode );
static void timeSyncTimerCallback( void *args );

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static tTimeSync_config m_timeSync_config;

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void timeSync_init( void )
{
    if( !m_timeSync_config.isInitalized )
    {
        m_timeSync_config.timeServerUrl = "http://worldclockapi.com/api/json/cet/now";
        m_timeSync_config.timeServerPort = 80;

        m_timeSync_config.rtc = &hrtc;

        osTimerAttr_t timerAttr = {
            .name = "TimeSyncTimer"
        };

        m_timeSync_config.syncTimer = osTimerNew( timeSyncTimerCallback, osTimerPeriodic, NULL, &timerAttr );

        m_timeSync_config.httpClient = httpClient_createNewHttpClient( timeSyncHttpResponseCallback, timeSyncHttpErrorCallback );

        if( NULL != m_timeSync_config.httpClient )
        {
            httpClient_configureRequest( m_timeSync_config.httpClient, m_timeSync_config.timeServerUrl, m_timeSync_config.timeServerPort, GET );
        }

        osTimerStart( m_timeSync_config.syncTimer, SYNC_TIMER_TIMEOUT );

        // We want to sync time at start up
        httpSessionMgr_startNewSession( m_timeSync_config.httpClient );

        LOG_INFO( "TimeSync initialized" );

        m_timeSync_config.isInitalized = true;
    }
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static void timeSyncHttpResponseCallback( void )
{
    LOG_INFO( "timeSyncHttpResponseCallback received!" );
}

static void timeSyncHttpErrorCallback( uint32_t erroCode )
{
    LOG_INFO( "timeSyncHttpErrorCallback received!. Error code: %u", erroCode );
}

static void timeSyncTimerCallback( void *args )
{
    httpSessionMgr_startNewSession( m_timeSync_config.httpClient );
}