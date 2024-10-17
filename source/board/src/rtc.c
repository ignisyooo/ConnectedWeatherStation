#include "rtc.h"

RTC_HandleTypeDef hrtc;

void RTC_Init( void )
{
 

    RTC_TimeTypeDef sTime = { 0 };
    RTC_DateTypeDef sDate = { 0 };


    /** Initialize RTC Only
     */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if( HAL_RTC_Init( &hrtc ) != HAL_OK )
    {
        Error_Handler();
    }

    /** Initialize RTC and set the Time and Date
     */
    sTime.Hours = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    if( HAL_RTC_SetTime( &hrtc, &sTime, RTC_FORMAT_BCD ) != HAL_OK )
    {
        Error_Handler();
    }
    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month = RTC_MONTH_JANUARY;
    sDate.Date = 0x1;
    sDate.Year = 0x0;

    if( HAL_RTC_SetDate( &hrtc, &sDate, RTC_FORMAT_BCD ) != HAL_OK )
    {
        Error_Handler();
    }
}

void HAL_RTC_MspInit( RTC_HandleTypeDef* rtcHandle )
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };
    if( rtcHandle->Instance == RTC )
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
        if( HAL_RCCEx_PeriphCLKConfig( &PeriphClkInitStruct ) != HAL_OK )
        {
            Error_Handler();
        }
        __HAL_RCC_RTC_ENABLE();
    }
}

void HAL_RTC_MspDeInit( RTC_HandleTypeDef* rtcHandle )
{
    if( rtcHandle->Instance == RTC )
    {
        __HAL_RCC_RTC_DISABLE();
    }
}