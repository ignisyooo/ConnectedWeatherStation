#include "lvglWrapper.h"

#include "cmsis_os.h"
#include "ili9341.h"
#include "spi.h"

#if defined( LV_LVGL_H_INCLUDE_SIMPLE )
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/***********************************************************************************
 * PRIVATE MACROS DEFINTIONS
 ***********************************************************************************/
#define DISP_HOR_RES 320
#define DISP_VER_RES 240
#define BUFF_SIZE    ( DISP_HOR_RES * 80 )
/***********************************************************************************
 * PRIVATE TYPES DEFINTIONS
 ***********************************************************************************/

/************************************************************************************
 * PRIVATE VARIABLES DECLERATION
 ***********************************************************************************/
static lv_disp_draw_buf_t m_lvglWrapper_diplayBuffer;
static lv_disp_drv_t m_lvglWrapper_dispDrv;
static lv_color_t m_lvglWrapper_buf_1[BUFF_SIZE];
static lv_color_t m_lvglWrapper_buf_2[BUFF_SIZE];

static lv_obj_t * label_time;
static lv_obj_t * label_date;
static lv_obj_t * label_location;

/************************************************************************************
 * PRIVATE FUNTCTION DECLERATION
 ***********************************************************************************/
static void lvglTimerTask( void *argument );

static void disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p );

/************************************************************************************
 * PUBLIC FUNTCTION DEFINTIONS
 ***********************************************************************************/
void lvglWrapper_init( void )
{
    const osThreadAttr_t lvglTimerTaskAttr = {
        .name = "lvgl_timer",
        .stack_size = 2048,
        .priority = (osPriority_t)osPriorityRealtime,
    };

    osThreadNew( lvglTimerTask, NULL, &lvglTimerTaskAttr );
}

void lvglWrapper_displayInit( void )
{
    /* Initialise LVGL UI library */
    lv_init();
    /* Initalize ili9341 controller */
    ILI9341_Init();

    lv_disp_draw_buf_init( &m_lvglWrapper_diplayBuffer, m_lvglWrapper_buf_1, m_lvglWrapper_buf_2, BUFF_SIZE ); /*Initialize the display buffer*/

    lv_disp_drv_init( &m_lvglWrapper_dispDrv );

    m_lvglWrapper_dispDrv.draw_buf = &m_lvglWrapper_diplayBuffer;

    /*Used to copy the buffer's content to the display*/
    m_lvglWrapper_dispDrv.flush_cb = disp_flush;

    /*Set the resolution of the display*/
    m_lvglWrapper_dispDrv.hor_res = DISP_HOR_RES;
    m_lvglWrapper_dispDrv.ver_res = DISP_VER_RES;

    lv_disp_drv_register( &m_lvglWrapper_dispDrv );
}

void lvglWrapper_createClockScreen( void )
{
    lv_obj_t *screen = lv_scr_act();

    // Ustawiamy tło ekranu
    lv_obj_set_style_bg_color( screen, lv_color_hex( 0x000000 ), LV_PART_MAIN );  // Czarny background

    // Tworzymy labelkę dla godziny
    label_time = lv_label_create( screen );
    lv_obj_align( label_time, LV_ALIGN_CENTER, 0, -30 );                                // Pozycjonujemy na środku
    lv_obj_set_style_text_color( label_time, lv_color_hex( 0xFFFFFF ), LV_PART_MAIN );  // Biały kolor tekstu
    lv_obj_set_style_text_font( label_time, &lv_font_montserrat_48, LV_PART_MAIN );     // Duża czcionka

    // Tworzymy labelkę dla daty
    label_date = lv_label_create( screen );
    lv_obj_align( label_date, LV_ALIGN_CENTER, 0, 30 );
    lv_obj_set_style_text_color( label_date, lv_color_hex( 0xFFFFFF ), LV_PART_MAIN );
    lv_obj_set_style_text_font( label_date, &lv_font_montserrat_24, LV_PART_MAIN );

    // Tworzymy labelkę dla lokalizacji
    label_location = lv_label_create( screen );
    lv_obj_align( label_location, LV_ALIGN_BOTTOM_MID, 0, -10 );
    lv_obj_set_style_text_color( label_location, lv_color_hex( 0xFFFFFF ), LV_PART_MAIN );
    lv_obj_set_style_text_font( label_location, &lv_font_montserrat_16, LV_PART_MAIN );
}

void lvglWrapper_updateClockLabel( const char *timeBuffer )
{
    lv_label_set_text( label_time, timeBuffer );
}

void lvglWrapper_updateDateLabel( const char *dateBuffer )
{
    lv_label_set_text( label_date, dateBuffer );
}

void lvglWrapper_updateLocationLabel( const char *locationBuffer )
{
    lv_label_set_text( label_location, locationBuffer );
}

/************************************************************************************
 * PRIVATE FUNTCTION DEFINITIONS
 ***********************************************************************************/
static void disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    ILI9341_SetWindow( area->x1, area->y1, area->x2, area->y2 );

    int height = area->y2 - area->y1 + 1;
    int width = area->x2 - area->x1 + 1;

    ILI9341_DrawBitmapDMA( width, height, (uint8_t *)color_p );
}

void HAL_SPI_TxCpltCallback( SPI_HandleTypeDef *hspi )
{
    lv_disp_flush_ready( &m_lvglWrapper_dispDrv );
}

void lvglTimerTask( void *argument )
{
    while( 1 )
    {
        lv_timer_handler();
        osDelay( 5 );
    }
}

void vApplicationTickHook( void )
{
    lv_tick_inc( 1 );
}