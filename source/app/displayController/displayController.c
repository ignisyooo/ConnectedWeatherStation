#include "displayController.h"

#include <stdbool.h>
#include <stdio.h>

#include "cmsis_os.h"
#include "logger.h"
#include "lvglWrapper.h"
#include "timeSync.h"
/***********************************************************************************
 * PRIVATE MACROS DEFINTIONS
 ***********************************************************************************/

/***********************************************************************************
 * PRIVATE TYPES DEFINTIONS
 ***********************************************************************************/

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static bool m_displayController_initalized;
extern RTC_HandleTypeDef hrtc;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
void displayControllerTask( void *args );
void updateClockScreen( void );
/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void displayController_init( void )
{
    if( !m_displayController_initalized )
    {
        m_displayController_initalized = true;

        const osThreadAttr_t attributes = {
            .name = "displayControllerTask",
            .stack_size = 2048,
            .priority = (osPriority_t)osPriorityNormal,
        };
        osThreadNew( displayControllerTask, NULL, &attributes );
    }
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
void displayControllerTask( void *args )
{
    lvglWrapper_init();
    lvglWrapper_displayInit();

    lvglWrapper_createClockScreen();
    while( 1 )
    {
        updateClockScreen();
        osDelay( 1000 );
    }
}

void updateClockScreen( void )
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    const tTimeSync_localizationInfo *info = timeSync_getLocalizationInfo();
    char time_buffer[20];
    char date_buffer[30];
    char location_buffer[101];

    HAL_RTC_GetTime( &hrtc, &sTime, RTC_FORMAT_BIN );
    HAL_RTC_GetDate( &hrtc, &sDate, RTC_FORMAT_BIN );

    LOG_DEBUG( "Current time: %02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds );

    snprintf( time_buffer, sizeof( time_buffer ), "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds );
    snprintf( date_buffer, sizeof( date_buffer ), "%02d-%02d-%04d", sDate.Date, sDate.Month, sDate.Year + 2000 );
    snprintf( location_buffer, sizeof( location_buffer ), "%s, %s", info->city, info->country );

    lvglWrapper_updateClockLabel( time_buffer );
    lvglWrapper_updateDateLabel( date_buffer );
    lvglWrapper_updateLocationLabel( location_buffer );
}