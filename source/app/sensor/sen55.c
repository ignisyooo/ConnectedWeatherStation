#include "sen55.h"

#include <math.h>
#include <string.h>

#include "cmsis_os.h"
#include "i2c.h"
#include "logger.h"

/************************************************************************************
 * PRIVATE MACROS
 ***********************************************************************************/

#define SEN55_I2C_ADDRESS ( 0x69 << 1 )

#define SEN55_START_MEASUREMENT    ( 0x0021 )
#define SEN55_STOP_MEASUREMENT     ( 0x0104 )
#define SEN55_READ_DATA_READY_FLAG ( 0x0202 )
#define SEN55_READ_MEASURED_VALUES ( 0x03C4 )
#define SEN55_READ_PRODUCT_NAME    ( 0xD014 )
#define SEN55_READ_DEVICE_STATUS   ( 0xD206 )
#define SEN55_CLEAR_DEVICE_STATUS  ( 0xD210 )
#define SEN55_RESET                ( 0xD304 )

#define SEN55_DEFAULT_COMMAND_EXUCTION_TIME        ( 20 )
#define SEN55_START_MEASUREMENT_CMD_EXUECTION_TIME ( 50 )

#define SEN55_PRODUCT_NAME_LEN ( 6u )

#define SEN55_I2C_TX_COMMAND_SIZE           ( 2 )
#define SEN55_I2C_RX_BUFFER_DEVICE_STATUS   ( 6 )
#define SEN55_I2C_RX_BUFFER_PRODUCT_NAME    ( 9 )
#define SEN55_I2C_RX_BUFFER_MEASURED_VALUES ( 24 )

#define SEN55_DEVICE_STATUS_MASK ( 0x2800F0 )

/************************************************************************************
 * PRIVATE TYPES DECLARATION
 ***********************************************************************************/
typedef enum
{
    SEN55_STATE_INIT,
    SEN55_STATE_READ_PRODUCT_NAME,
    SEN55_STATE_CHECK_DEVICE_STATUS,
    SEN55_STATE_START_MEASUREMENT,
    SEN55_STATE_READ_DATA,
    SEN55_STATE_PROCESS_DATA,
    SEN55_STATE_IDLE,
    SEN55_STATE_ERROR
} tSen55Controler_state;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static bool sendCommand( uint16_t command, uint32_t delayMs );
static bool receiveData( uint8_t *buffer, size_t length );
static bool checkDeviceStatus( void );
static uint8_t calcCrc( const uint8_t *data, uint8_t length );
static bool readProductName( uint8_t *productName, size_t productNameMaxLen );
static bool startMeasurement( void );
static bool readMeasurement( void );
static bool validateCRC( uint8_t *data, size_t length );
static void parseSensorData( const uint8_t *buffer, tSen55_data *data );
static float getAQISegment( float concentration, const float breakpoints[][2], const int aqi[], int size );
static void sen55Task( void *args );
static void dumpSensorData( const tSen55_data *sensorData );

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
extern I2C_HandleTypeDef hi2c1;

static tSen55Controler_state m_sen55_currentState = SEN55_STATE_INIT;
static uint8_t i2cTxBuffer[SEN55_I2C_TX_COMMAND_SIZE];
static uint8_t i2cRxBuffer[24];
static bool i2cRxTransferComplete = false;
static bool i2cTxTransferComplete = false;
static bool i2cErrorOccurred = false;
static osMutexId_t m_sen55_dataMutex;
static tSen55_data m_sen55_sensorData;
static bool m_sen55_initalized = false;

static uint8_t m_sen55_productName[SEN55_PRODUCT_NAME_LEN];

// Breakpoints for PM2.5 and PM10 (in µg/m³) and corresponding AQI ranges
const float pm25Breakpoints[][2] = {
    { 0.0, 12.0 }, { 12.1, 35.4 }, { 35.5, 55.4 }, { 55.5, 150.4 }, { 150.5, 250.4 }, { 250.5, 350.4 }, { 350.5, 500.4 }
};
const float pm10Breakpoints[][2] = {
    { 0.0, 54.0 }, { 55.0, 154.0 }, { 155.0, 254.0 }, { 255.0, 354.0 }, { 355.0, 424.0 }, { 425.0, 504.0 }, { 505.0, 604.0 }
};
const int aqi[] = { 0, 50, 100, 150, 200, 300, 400, 500 };

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void sen55_init( void )
{
    if( !m_sen55_initalized )
    {
        const osThreadAttr_t attributes = {
            .name = "Sen55Task",
            .stack_size = 2048,
            .priority = (osPriority_t)osPriorityNormal,
        };
        osThreadNew( sen55Task, NULL, &attributes );
    }
}

float sen55_calculateAQI( uint16_t pm25, uint16_t pm10 )
{
    // Calculate AQI for PM2.5 and PM10
    float aqiPM25 = getAQISegment( (float)pm25 / 10.0, pm25Breakpoints, aqi, 7 );
    float aqiPM10 = getAQISegment( (float)pm10 / 10.0, pm10Breakpoints, aqi, 7 );

    // Return the maximum AQI value
    return fmaxf( aqiPM25, aqiPM10 );
}

bool sen55_getSensorData( tSen55_data *data )
{
    bool result = false;

    if( osMutexAcquire( m_sen55_dataMutex, osWaitForever ) == osOK )
    {
        memcpy( data, &m_sen55_sensorData, sizeof( tSen55_data ) );
        osMutexRelease( m_sen55_dataMutex );
        result = true;
    }

    return result;
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/

void sen55Task( void *args )
{
    m_sen55_dataMutex = osMutexNew( NULL );

    for( ;; )
    {
        switch( m_sen55_currentState )
        {
            case SEN55_STATE_INIT:
            {
                i2cTxTransferComplete = false;
                i2cRxTransferComplete = false;
                i2cErrorOccurred = false;
                m_sen55_currentState = SEN55_STATE_READ_PRODUCT_NAME;
            }
            break;

            case SEN55_STATE_READ_PRODUCT_NAME:
            {
                if( readProductName( m_sen55_productName, SEN55_PRODUCT_NAME_LEN ) )
                {
                    LOG_INFO( "ProductName: %s", m_sen55_productName );
                    m_sen55_currentState = SEN55_STATE_CHECK_DEVICE_STATUS;
                }
                else
                {
                    LOG_ERROR( "Error reading product name" );
                    m_sen55_currentState = SEN55_STATE_ERROR;
                }
            }
            break;

            case SEN55_STATE_CHECK_DEVICE_STATUS:
            {
                if( checkDeviceStatus() )
                {
                    m_sen55_currentState = SEN55_STATE_START_MEASUREMENT;
                }
                else
                {
                    m_sen55_currentState = SEN55_STATE_ERROR;
                }
            }
            break;
            case SEN55_STATE_START_MEASUREMENT:
            {
                if( startMeasurement() )
                {
                    LOG_INFO( "Starting measurement" );
                    m_sen55_currentState = SEN55_STATE_READ_DATA;
                }
                else
                {
                    m_sen55_currentState = SEN55_STATE_ERROR;
                }
            }
            break;

            case SEN55_STATE_READ_DATA:
            {
                if( readMeasurement() )
                {
                    m_sen55_currentState = SEN55_STATE_PROCESS_DATA;
                }
                else
                {
                    m_sen55_currentState = SEN55_STATE_ERROR;
                }
            }
            break;

            case SEN55_STATE_PROCESS_DATA:
            {
                tSen55_data tempData;
                parseSensorData( i2cRxBuffer, &tempData );

                // Update global sensor data with mutex protection
                if( osMutexAcquire( m_sen55_dataMutex, osWaitForever ) == osOK )
                {
                    memcpy( &m_sen55_sensorData, &tempData, sizeof( tSen55_data ) );
                    osMutexRelease( m_sen55_dataMutex );
                }

                dumpSensorData( &m_sen55_sensorData );

                m_sen55_currentState = SEN55_STATE_IDLE;
                break;
            }

            case SEN55_STATE_ERROR:
                // Handle error (e.g., reset sensor, retry, log)
                osDelay( 5000 );  // Wait and retry
                // m_sen55_currentState = SEN55_STATE_INIT;
                break;

            case SEN55_STATE_IDLE:
                osDelay( 1000 );
                m_sen55_currentState = SEN55_STATE_READ_DATA;
                break;

            default:
                m_sen55_currentState = SEN55_STATE_ERROR;
                break;
        }
    }
}

static bool sendCommand( uint16_t command, uint32_t delayMs )
{
    i2cTxBuffer[0] = ( command >> 8 ) & 0xFF;
    i2cTxBuffer[1] = command & 0xFF;
    if( HAL_I2C_Master_Transmit_IT( &hi2c1, SEN55_I2C_ADDRESS, i2cTxBuffer, 2 ) == HAL_OK )
    {
        while( !i2cTxTransferComplete && !i2cErrorOccurred )
        {
            osDelay( 1 );
        }
        if( i2cErrorOccurred )
        {
            i2cErrorOccurred = false;
            return false;
        }
        i2cTxTransferComplete = false;
        return true;
    }
    return false;
}

static bool receiveData( uint8_t *buffer, size_t length )
{
    if( HAL_I2C_Master_Receive_IT( &hi2c1, SEN55_I2C_ADDRESS, buffer, length ) == HAL_OK )
    {
        while( !i2cRxTransferComplete && !i2cErrorOccurred )
        {
            osDelay( 1 );
        }
        if( i2cErrorOccurred )
        {
            i2cErrorOccurred = false;
            return false;
        }
        i2cRxTransferComplete = false;
        return true;
    }

    return false;
}

static bool readProductName( uint8_t *productName, size_t productNameMaxLen )
{
    if( sendCommand( SEN55_READ_PRODUCT_NAME, SEN55_DEFAULT_COMMAND_EXUCTION_TIME ) &&
        receiveData( i2cRxBuffer, 9 ) )
    {
        if( validateCRC( i2cRxBuffer, 9 ) )
        {
            for( uint8_t i = 0; i < ( productNameMaxLen / 2 ); ++i )
            {
                memcpy( &productName[i * 2], &i2cRxBuffer[i * 3], 2 );
            }
            return true;
        }
    }
    return false;
}

static bool checkDeviceStatus( void )
{
    bool result = false;

    if( sendCommand( SEN55_READ_DEVICE_STATUS, SEN55_DEFAULT_COMMAND_EXUCTION_TIME ) &&
        receiveData( i2cRxBuffer, SEN55_I2C_RX_BUFFER_DEVICE_STATUS ) )
    {
        if( validateCRC( i2cRxBuffer, SEN55_I2C_RX_BUFFER_DEVICE_STATUS ) )
        {
            uint32_t deviceStatus = i2cRxBuffer[4] << 24 | i2cRxBuffer[3] << 16 | i2cRxBuffer[1] << 8 | i2cRxBuffer[0];
            if( ( deviceStatus & SEN55_DEVICE_STATUS_MASK ) == 0 )
            {
                LOG_INFO( "Device status OK" );
                result = true;
            }
            else
            {
                LOG_ERROR( "Device status erorr: 0x%08X", ( deviceStatus & SEN55_DEVICE_STATUS_MASK ) );
            }
        }
        else
        {
            LOG_ERROR( "CRC error in device status response" );
        }
    }

    return result;
}

static bool startMeasurement( void )
{
    bool result = false;

    if( sendCommand( SEN55_START_MEASUREMENT, SEN55_START_MEASUREMENT_CMD_EXUECTION_TIME ) )
    {
        result = true;
    }

    return result;
}

static bool readMeasurement( void )
{
    bool result = false;

    if( sendCommand( SEN55_READ_MEASURED_VALUES, SEN55_DEFAULT_COMMAND_EXUCTION_TIME ) &&
        receiveData( i2cRxBuffer, SEN55_I2C_RX_BUFFER_MEASURED_VALUES ) )
    {
        if( validateCRC( i2cRxBuffer, SEN55_I2C_RX_BUFFER_MEASURED_VALUES ) )
        {
            result = true;
        }
    }

    return result;
}

static bool validateCRC( uint8_t *data, size_t length )
{
    for( size_t i = 0; i < length; i += 3 )
    {
        if( calcCrc( &data[i], 2 ) != data[i + 2] )
        {
            return false;
        }
    }
    return true;
}

static void parseSensorData( const uint8_t *buffer, tSen55_data *data )
{
    // PM1.0 (µg/m³) - scale factor = 10
    data->pm1_0 = ( ( buffer[0] << 8 ) | buffer[1] ) / 10.0f;

    // PM2.5 (µg/m³) - scale factor = 10
    data->pm2_5 = ( ( buffer[3] << 8 ) | buffer[4] ) / 10.0f;

    // PM4.0 (µg/m³) - scale factor = 10
    data->pm4_0 = ( ( buffer[6] << 8 ) | buffer[7] ) / 10.0f;

    // PM10 (µg/m³) - scale factor = 10
    data->pm10 = ( ( buffer[9] << 8 ) | buffer[10] ) / 10.0f;

    // Humidity (%) - scale factor = 100
    data->humidity = ( ( buffer[12] << 8 ) | buffer[13] ) / 100.0f;

    // Temperature (°C) - scale factor = 200
    data->temperature = ( ( buffer[15] << 8 ) | buffer[16] ) / 200.0f;

    // VOC Index - scale factor = 10
    data->vocIndex = ( ( buffer[18] << 8 ) | buffer[19] ) / 10.0f;

    // NOx Index - scale factor = 10
    data->noxIndex = ( ( buffer[21] << 8 ) | buffer[22] ) / 10.0f;
}

static uint8_t calcCrc( const uint8_t *data, uint8_t length )
{
    uint8_t crc = 0xFF;
    for( uint8_t i = 0; i < length; i++ )
    {
        crc ^= data[i];
        for( uint8_t bit = 0; bit < 8; bit++ )
        {
            if( crc & 0x80 )
            {
                crc = ( crc << 1 ) ^ 0x31;
            }
            else
            {
                crc = ( crc << 1 );
            }
        }
    }
    return crc;
}

static float getAQISegment( float concentration, const float breakpoints[][2], const int aqi[], int size )
{
    for( int i = 0; i < size; i++ )
    {
        if( concentration >= breakpoints[i][0] && concentration <= breakpoints[i][1] )
        {
            return aqi[i] + ( ( concentration - breakpoints[i][0] ) / ( breakpoints[i][1] - breakpoints[i][0] ) ) * ( aqi[i + 1] - aqi[i] );
        }
    }
    return 0;
}

static void dumpSensorData( const tSen55_data *sensorData )
{
    LOG_DEBUG( "Sensor Data Dump:\n" );
    LOG_DEBUG( "PM1.0: %.2f ug/m³\n", sensorData->pm1_0 );     // Mass Concentration PM1.0 [µg/m³]
    LOG_DEBUG( "PM2.5: %.2f ug/m³\n", sensorData->pm2_5 );     // Mass Concentration PM2.5 [µg/m³]
    LOG_DEBUG( "PM4.0: %.2f ug/m³\n", sensorData->pm4_0 );     // Mass Concentration PM4.0 [µg/m³]
    LOG_DEBUG( "PM10: %.2f ug/m³\n", sensorData->pm10 );       // Mass Concentration PM10 [µg/m³]
    LOG_DEBUG( "Humidity: %.2f %%RH\n", sensorData->humidity ); // Compensated Ambient Humidity [%RH]
    LOG_DEBUG( "Temperature: %.2f *C\n", sensorData->temperature ); // Compensated Ambient Temperature [°C]
    LOG_DEBUG( "VOC Index: %.2f\n", sensorData->vocIndex );    // VOC Index
    LOG_DEBUG( "NOx Index: %.2f\n", sensorData->noxIndex );    // NOx Index
}

// HAL I2C Callbacks
void HAL_I2C_MasterTxCpltCallback( I2C_HandleTypeDef *hi2c )
{
    if( hi2c == &hi2c1 )
    {
        i2cTxTransferComplete = true;
    }
}

void HAL_I2C_MasterRxCpltCallback( I2C_HandleTypeDef *hi2c )
{
    if( hi2c == &hi2c1 )
    {
        i2cRxTransferComplete = true;
    }
}

void HAL_I2C_ErrorCallback( I2C_HandleTypeDef *hi2c )
{
    if( hi2c == &hi2c1 )
    {
        i2cErrorOccurred = true;
    }
}
