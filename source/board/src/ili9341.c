#include "ili9341.h"

#include "cmsis_os.h"  // For RTOS features (e.g., mutex, osDelay)
#include "gpio.h"      // Hardware setting

// Mutex for thread safety
static osMutexId_t spi_mutex;

typedef enum
{
    ROTATE_0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270
} LCD_Horizontal_t;

extern SPI_HandleTypeDef hspi1;

// GPIO Control Helpers
static inline void GPIO_WritePin( GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state )
{
    HAL_GPIO_WritePin( port, pin, state );
}

static inline void RESET_L( void ) { GPIO_WritePin( RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET ); }
static inline void RESET_H( void ) { GPIO_WritePin( RESET_GPIO_Port, RESET_Pin, GPIO_PIN_SET ); }
static inline void CS_L( void ) { GPIO_WritePin( SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET ); }
static inline void DC_L( void ) { GPIO_WritePin( DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET ); }
static inline void DC_H( void ) { GPIO_WritePin( DC_GPIO_Port, DC_Pin, GPIO_PIN_SET ); }
static inline void LED_H( void ) { /* Example: GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); */ }

// SPI Transmission (Thread-Safe)
static void sendSPI( uint8_t *data, uint16_t size )
{
    osMutexAcquire( spi_mutex, osWaitForever );
    HAL_SPI_Transmit( &hspi1, data, size, HAL_MAX_DELAY );
    osMutexRelease( spi_mutex );
}

// Non-Blocking Delay
static void Delay( uint16_t ms )
{
    osDelay( ms );
}

// LCD Command/Parameter Writing
static void LCD_WriteCommand( uint8_t cmd )
{
    DC_L();
    sendSPI( &cmd, 1 );
}

static void LCD_WriteData( uint8_t data )
{
    DC_H();
    sendSPI( &data, 1 );
}

static void LCD_WriteDataBuffer( uint8_t *data, uint16_t size )
{
    DC_H();
    sendSPI( data, size );
}

// Initialization Functions
void ILI9341_Reset( void )
{
    RESET_L();
    Delay( 100 );
    RESET_H();
    Delay( 100 );
    CS_L();
    LED_H();
}

void ILI9341_SoftReset( void )
{
    LCD_WriteCommand( 0x01 );  // Software reset
    Delay( 5 );
}

static void LCD_SetDirection( LCD_Horizontal_t direction )
{
    LCD_WriteCommand( 0x36 );
    switch( direction )
    {
        case ROTATE_0:
            LCD_WriteData( 0x48 );
            break;
        case ROTATE_90:
            LCD_WriteData( 0x28 );
            break;
        case ROTATE_180:
            LCD_WriteData( 0x88 );
            break;
        case ROTATE_270:
            LCD_WriteData( 0xE8 );
            break;
    }
}

// Initialization Sequence
void ILI9341_Init( void )
{
    spi_mutex = osMutexNew( NULL );

    ILI9341_Reset();
    ILI9341_SoftReset();

    struct
    {
        uint8_t cmd;
        uint8_t data[16];
        uint8_t data_size;
    } init_cmds[] = {
        { 0xCB, { 0x39, 0x2C, 0x00, 0x34, 0x02 }, 5 },  // Reset Display: Display initialization settings
        { 0xCF, { 0x00, 0xC1, 0x30 }, 3 },              // Voltage settings: Voltage regulation
        { 0xE8, { 0x85, 0x00, 0x78 }, 3 },              // Gamma controller
        { 0xEA, { 0x00, 0x00 }, 2 },                    // Gamma settings
        { 0xED, { 0x64, 0x03, 0x12, 0x81 }, 4 },        // Driver initialization
        { 0xF7, { 0x20 }, 1 },                          // Set delay
        { 0xC0, { 0x10 }, 1 },                          // Contrast regulator configuration
        { 0xC1, { 0x10 }, 1 },                          // Contrast regulator configuration (continued)
        { 0xC5, { 0x3E, 0x28 }, 2 },                    // Brightness settings
        { 0xC7, { 0x86 }, 1 },                          // Gain setting
        { 0x3A, { 0x55 }, 1 },                          // Pixel Format Set: 16-bit
        { 0xB1, { 0x00, 0x18 }, 2 },                    // Refresh rate adjustment
        { 0xB6, { 0x08, 0x82, 0x27 }, 3 },              // Display settings selection
        { 0xF2, { 0x00 }, 1 },                          // Enable gamma options
        { 0x26, { 0x01 }, 1 },                          // Enable RGB colors
    };

    for( uint8_t i = 0; i < sizeof( init_cmds ) / sizeof( init_cmds[0] ); i++ )
    {
        LCD_WriteCommand( init_cmds[i].cmd );
        LCD_WriteDataBuffer( init_cmds[i].data, init_cmds[i].data_size );
    }

    // Gamma correction commands
    LCD_WriteCommand( 0xE0 );
    uint8_t positive_gamma[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
    LCD_WriteDataBuffer( positive_gamma, sizeof( positive_gamma ) );

    LCD_WriteCommand( 0xE1 );
    uint8_t negative_gamma[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
    LCD_WriteDataBuffer( negative_gamma, sizeof( negative_gamma ) );

    LCD_WriteCommand( 0x11 );  // Exit Sleep
    Delay( 120 );
    LCD_WriteCommand( 0x29 );  // Display ON

    LCD_SetDirection( ROTATE_90 );
}

// Window and Pixel Operations
void ILI9341_SetWindow( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2 )
{
    uint8_t data[4];

    LCD_WriteCommand( 0x2A );  // Column Address Set
    data[0] = x1 >> 8;
    data[1] = x1 & 0xFF;
    data[2] = x2 >> 8;
    data[3] = x2 & 0xFF;
    LCD_WriteDataBuffer( data, 4 );

    LCD_WriteCommand( 0x2B );  // Page Address Set
    data[0] = y1 >> 8;
    data[1] = y1 & 0xFF;
    data[2] = y2 >> 8;
    data[3] = y2 & 0xFF;
    LCD_WriteDataBuffer( data, 4 );

    LCD_WriteCommand( 0x2C );  // Memory Write
}

void ILI9341_WritePixel( uint16_t x, uint16_t y, uint16_t color )
{
    ILI9341_SetWindow( x, y, x, y );
    uint8_t data[] = { color >> 8, color & 0xFF };
    LCD_WriteDataBuffer( data, 2 );
}

static void ConvHL( uint8_t *s, int32_t l )
{
    uint8_t v;
    while( l > 0 )
    {
        v = *( s + 1 );
        *( s + 1 ) = *s;
        *s = v;
        s += 2;
        l -= 2;
    }
}

// Bitmap Operations
void ILI9341_DrawBitmap( uint16_t w, uint16_t h, uint8_t *bitmap )
{
    LCD_WriteCommand( 0x2C );  // Memory Write
    ConvHL( bitmap, (int32_t)w * h * 2 );
    LCD_WriteDataBuffer( bitmap, w * h * 2 );
}

void ILI9341_DrawBitmapDMA( uint16_t w, uint16_t h, uint8_t *bitmap )
{
    LCD_WriteCommand( 0x2C );  // Memory Write
    DC_H();
    ConvHL( bitmap, (int32_t)w * h * 2 );
    HAL_SPI_Transmit_DMA( &hspi1, bitmap, w * h * 2 );
}
