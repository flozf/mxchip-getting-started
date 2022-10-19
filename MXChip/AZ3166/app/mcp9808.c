#include <stdio.h>

#include "stm32f4xx_hal.h"
//#include "stm32f4xx_hal_gpio.h"

#include "tools/string.h"
#include "mcp9808.h"

extern I2C_HandleTypeDef I2cHandle;
#define MCP9808_I2C_PORT    I2cHandle
#define MCP9808_I2C_ADDR    (0x18 << 1)

static int g_initialized = 0;

static HAL_StatusTypeDef read_register( uint8_t reg, uint16_t *dest )
{
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&MCP9808_I2C_PORT, MCP9808_I2C_ADDR, reg, 1, (void*) dest, 2, HAL_MAX_DELAY);

    if( ret != HAL_OK ) {
        printf( "MCP9809: error reading register %d\r\n", reg );
        *dest = 0;
    } else {
        // data is big endian
        *dest = __builtin_bswap16( *dest );
    }

    return ret;
}


static void print_config( uint16_t config )
{
    if( config & 1 )
        printf( "Alert output mode: Interrupt\r\n" );
    else
        printf( "Alert output mode: Comparator\r\n" );

    if( config & 2 )
        printf( "Alert output polarity: Active High\r\n" );
    else
        printf( "Alert output polarity: Active Low\r\n" );

    if( config & 4 )
        printf( "Alert output select: TA > TCrit only\r\n" );
    else
        printf( "Alert output select: Alert Output for TUpper, TLower, TCrit\r\n" );

    if( config & 8 )
        printf( "Alert output control: disabled\r\n" );
    else
        printf( "Alert output control: enabled\r\n" );

    if( config & 16 )
        printf( "Alert output status: Alert output is asserted as a comparator/Interrupt or critical temperature output\r\n" );
    else
        printf( "Alert output status: Alert output is not asserted by the device\r\n" );

    if( config & 32 )
        printf( "Interrupt clear: Clear interrupt output; when read, this bit returns to '0'\r\n");
    else
        printf( "Interrupt clear: No effect\r\n" );

    if( config & 64 )
        printf( "Win. Lock: Unlocked; TUPPER and TLOWER registers can be written\r\n" );
    else
        printf( "Win. Lock: Locked; TUPPER and TLOWER registers can not be written\r\n" );

    if( config & 128 )
        printf( "Crit Lock: Locked. TCRIT register can not be written\r\n" );
    else
        printf( "Crit Lock: Unlocked. TCRIT register can be written\r\n" );

    if( config & 256 )
        printf( "Shutdown Mode: Shutdown (Low-Power mode)\r\n" );
    else
        printf( "Shutdown Mode: Continuous conversion\r\n" );

    switch( ( config >> 9 ) & 3 ) {
        case 0:     printf( "THyst: 0°C\r\n" ); break;
        case 1:     printf( "THyst: +1.5°C\r\n" ); break;
        case 2:     printf( "THyst: +3.0°C\r\n" ); break;
        case 3:     printf( "THyst: +6.0°C\r\n" ); break;
    }
}


double mcp9808_read( void )
{
    uint16_t raw;

    if( !g_initialized || read_register( 5, &raw ) != HAL_OK )
        return -9999.0;         // NaN is not supported on a lot of platforms

    double result = ( raw & 0xfff ) / 16.0;
    if( raw & 0x1000 )
        result -= 256.0;

    return result;
}

HAL_StatusTypeDef mcp9808_init( void )
{
    printf( "Initializing MCP9808...\r\n" );

    //HAL_I2C_Mem_Write(&MCP9808_I2C_PORT, MCP9808_I2C_ADDR, 0x00, 1, &byte, 1, HAL_MAX_DELAY);
    uint16_t manufacturer_id = 0, device_id = 0, config = 0;
    if( read_register( 0, &config ) != HAL_OK )
        return HAL_ERROR;
    if( read_register( 6, &manufacturer_id ) != HAL_OK )
        return HAL_ERROR;
    if( read_register( 7, &device_id ) != HAL_OK )
        return HAL_ERROR;

    printf( "manufacturer id: 0x%04x, device id: 0x%04x\r\n", manufacturer_id, device_id );
    print_config( config );

    g_initialized = 1;

    double temp = mcp9808_read();
    // following won't work:
    //printf( "current temp: %.2f\r\n", temp );

    char buf[100];
    printf( "current temp: %s °C\r\n", fk_dtoa( temp, 2, buf, sizeof buf ));

    return HAL_OK;
}


