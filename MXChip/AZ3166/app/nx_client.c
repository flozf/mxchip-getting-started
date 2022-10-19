/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include "nx_client.h"

#include <stdio.h>

#include "screen.h"
#include "sensor.h"
#include "stm32f4xx_hal.h"

#include "nx_api.h"
#include "nx_azure_iot_hub_client.h"
#include "nx_azure_iot_json_reader.h"
#include "nx_azure_iot_provisioning_client.h"

#include "azure_iot_nx_client.h"

#include "azure_config.h"
#include "azure_device_x509_cert_config.h"
#include "azure_pnp_info.h"
#include "wwd_networking.h"

#include "tools/string.h"
#include "ssd1306.h"
#include "mcp9808.h"

#define IOT_MODEL_ID "dtmi:azurertos:devkit:gsgmxchip;2"

// Device telemetry names
#define TELEMETRY_HUMIDITY          "humidity"
#define TELEMETRY_TEMPERATURE       "temperature"
#define TELEMETRY_TEMPERATURE2      "temperature2"
#define TELEMETRY_PRESSURE          "pressure"
#define TELEMETRY_MAGNETOMETERX     "magnetometerX"
#define TELEMETRY_MAGNETOMETERY     "magnetometerY"
#define TELEMETRY_MAGNETOMETERZ     "magnetometerZ"
#define TELEMETRY_ACCELEROMETERX    "accelerometerX"
#define TELEMETRY_ACCELEROMETERY    "accelerometerY"
#define TELEMETRY_ACCELEROMETERZ    "accelerometerZ"
#define TELEMETRY_GYROSCOPEX        "gyroscopeX"
#define TELEMETRY_GYROSCOPEY        "gyroscopeY"
#define TELEMETRY_GYROSCOPEZ        "gyroscopeZ"
#define TELEMETRY_INTERVAL_PROPERTY "telemetryInterval"

// Properties
#define LED_STATE_PROPERTY          "ledState"

// Commands
#define SET_LED_STATE_COMMAND       "setLedState"
#define SET_DISPLAY_TEXT_COMMAND    "setDisplayText"

typedef enum TELEMETRY_STATE_ENUM
{
    TELEMETRY_STATE_DEFAULT,
    TELEMETRY_STATE_MAGNETOMETER,
    TELEMETRY_STATE_ACCELEROMETER,
    TELEMETRY_STATE_GYROSCOPE,
    TELEMETRY_STATE_END
} TELEMETRY_STATE;

static AZURE_IOT_NX_CONTEXT azure_iot_nx_client;

static int32_t telemetry_interval = 10;

static UINT append_device_info_properties(NX_AZURE_IOT_JSON_WRITER* json_writer)
{
    if (nx_azure_iot_json_writer_append_property_with_string_value(json_writer,
            (UCHAR*)DEVICE_INFO_MANUFACTURER_PROPERTY_NAME,
            sizeof(DEVICE_INFO_MANUFACTURER_PROPERTY_NAME) - 1,
            (UCHAR*)DEVICE_INFO_MANUFACTURER_PROPERTY_VALUE,
            sizeof(DEVICE_INFO_MANUFACTURER_PROPERTY_VALUE) - 1) ||
        nx_azure_iot_json_writer_append_property_with_string_value(json_writer,
            (UCHAR*)DEVICE_INFO_MODEL_PROPERTY_NAME,
            sizeof(DEVICE_INFO_MODEL_PROPERTY_NAME) - 1,
            (UCHAR*)DEVICE_INFO_MODEL_PROPERTY_VALUE,
            sizeof(DEVICE_INFO_MODEL_PROPERTY_VALUE) - 1) ||
        nx_azure_iot_json_writer_append_property_with_string_value(json_writer,
            (UCHAR*)DEVICE_INFO_SW_VERSION_PROPERTY_NAME,
            sizeof(DEVICE_INFO_SW_VERSION_PROPERTY_NAME) - 1,
            (UCHAR*)DEVICE_INFO_SW_VERSION_PROPERTY_VALUE,
            sizeof(DEVICE_INFO_SW_VERSION_PROPERTY_VALUE) - 1) ||
        nx_azure_iot_json_writer_append_property_with_string_value(json_writer,
            (UCHAR*)DEVICE_INFO_OS_NAME_PROPERTY_NAME,
            sizeof(DEVICE_INFO_OS_NAME_PROPERTY_NAME) - 1,
            (UCHAR*)DEVICE_INFO_OS_NAME_PROPERTY_VALUE,
            sizeof(DEVICE_INFO_OS_NAME_PROPERTY_VALUE) - 1) ||
        nx_azure_iot_json_writer_append_property_with_string_value(json_writer,
            (UCHAR*)DEVICE_INFO_PROCESSOR_ARCHITECTURE_PROPERTY_NAME,
            sizeof(DEVICE_INFO_PROCESSOR_ARCHITECTURE_PROPERTY_NAME) - 1,
            (UCHAR*)DEVICE_INFO_PROCESSOR_ARCHITECTURE_PROPERTY_VALUE,
            sizeof(DEVICE_INFO_PROCESSOR_ARCHITECTURE_PROPERTY_VALUE) - 1) ||
        nx_azure_iot_json_writer_append_property_with_string_value(json_writer,
            (UCHAR*)DEVICE_INFO_PROCESSOR_MANUFACTURER_PROPERTY_NAME,
            sizeof(DEVICE_INFO_PROCESSOR_MANUFACTURER_PROPERTY_NAME) - 1,
            (UCHAR*)DEVICE_INFO_PROCESSOR_MANUFACTURER_PROPERTY_VALUE,
            sizeof(DEVICE_INFO_PROCESSOR_MANUFACTURER_PROPERTY_VALUE) - 1) ||
        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)DEVICE_INFO_TOTAL_STORAGE_PROPERTY_NAME,
            sizeof(DEVICE_INFO_TOTAL_STORAGE_PROPERTY_NAME) - 1,
            DEVICE_INFO_TOTAL_STORAGE_PROPERTY_VALUE,
            2) ||
        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)DEVICE_INFO_TOTAL_MEMORY_PROPERTY_NAME,
            sizeof(DEVICE_INFO_TOTAL_MEMORY_PROPERTY_NAME) - 1,
            DEVICE_INFO_TOTAL_MEMORY_PROPERTY_VALUE,
            2))
    {
        return NX_NOT_SUCCESSFUL;
    }

    return NX_AZURE_IOT_SUCCESS;
}

static UINT append_device_telemetry(NX_AZURE_IOT_JSON_WRITER* json_writer)
{
    lps22hb_t lps22hb_data    = lps22hb_data_read();
    hts221_data_t hts221_data = hts221_data_read();
    double mcp9808_temp       = mcp9808_read();

    // TODO: the screen update should be put elsewhere
    ssd1306_Fill(Black);

    char buf[100];
    ssd1306_SetCursor(0, 7);
    ssd1306_WriteString("onboard", Font_7x10, White);
    ssd1306_SetCursor(0, 29);
    ssd1306_WriteString("mcp9808", Font_7x10, White);

    fk_dtoa( lps22hb_data.temperature_degC, -2, buf, sizeof buf );
    strcat( buf, " C" );
    ssd1306_SetCursor(52, 0);
    ssd1306_WriteString(buf, Font_11x18, White);
    ssd1306_DrawCircle( 112, 4, 2, White );
    ssd1306_DrawCircle( 112, 4, 3, White );

    ssd1306_SetCursor(52, 22);
    if( mcp9808_temp > -9000.0 ) {
        fk_dtoa( mcp9808_temp, -2, buf, sizeof buf );
        strcat( buf, " C" );
        ssd1306_WriteString(buf, Font_11x18, White);
        ssd1306_DrawCircle( 112, 26, 2, White );
        ssd1306_DrawCircle( 112, 26, 3, White );
    } else {
        ssd1306_WriteString("---", Font_11x18, White);
    }

    ssd1306_UpdateScreen();

    if (nx_azure_iot_json_writer_append_property_with_double_value(
            json_writer, (UCHAR*)TELEMETRY_HUMIDITY, sizeof(TELEMETRY_HUMIDITY) - 1, hts221_data.humidity_perc, 2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_TEMPERATURE,
            sizeof(TELEMETRY_TEMPERATURE) - 1,
            lps22hb_data.temperature_degC,
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_TEMPERATURE2,
            sizeof(TELEMETRY_TEMPERATURE2) - 1,
            mcp9808_temp,
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(
            json_writer, (UCHAR*)TELEMETRY_PRESSURE, sizeof(TELEMETRY_PRESSURE) - 1, lps22hb_data.pressure_hPa, 2))
    {
        return NX_NOT_SUCCESSFUL;
    }

    return NX_AZURE_IOT_SUCCESS;
}

static UINT append_device_telemetry_magnetometer(NX_AZURE_IOT_JSON_WRITER* json_writer)
{
    lis2mdl_data_t lis2mdl_data = lis2mdl_data_read();

    if (nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_MAGNETOMETERX,
            sizeof(TELEMETRY_MAGNETOMETERX) - 1,
            lis2mdl_data.magnetic_mG[0],
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_MAGNETOMETERY,
            sizeof(TELEMETRY_MAGNETOMETERY) - 1,
            lis2mdl_data.magnetic_mG[1],
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_MAGNETOMETERZ,
            sizeof(TELEMETRY_MAGNETOMETERZ) - 1,
            lis2mdl_data.magnetic_mG[2],
            2))
    {
        return NX_NOT_SUCCESSFUL;
    }

    return NX_AZURE_IOT_SUCCESS;
}

static UINT append_device_telemetry_accelerometer(NX_AZURE_IOT_JSON_WRITER* json_writer)
{
    lsm6dsl_data_t lsm6dsl_data = lsm6dsl_data_read();

    if (nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_ACCELEROMETERX,
            sizeof(TELEMETRY_ACCELEROMETERX) - 1,
            lsm6dsl_data.acceleration_mg[0],
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_ACCELEROMETERY,
            sizeof(TELEMETRY_ACCELEROMETERY) - 1,
            lsm6dsl_data.acceleration_mg[1],
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_ACCELEROMETERZ,
            sizeof(TELEMETRY_ACCELEROMETERZ) - 1,
            lsm6dsl_data.acceleration_mg[2],
            2))
    {
        return NX_NOT_SUCCESSFUL;
    }

    return NX_AZURE_IOT_SUCCESS;
}

static UINT append_device_telemetry_gyroscope(NX_AZURE_IOT_JSON_WRITER* json_writer)
{
    lsm6dsl_data_t lsm6dsl_data = lsm6dsl_data_read();

    if (nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_GYROSCOPEX,
            sizeof(TELEMETRY_GYROSCOPEX) - 1,
            lsm6dsl_data.angular_rate_mdps[0],
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_GYROSCOPEY,
            sizeof(TELEMETRY_GYROSCOPEY) - 1,
            lsm6dsl_data.angular_rate_mdps[1],
            2) ||

        nx_azure_iot_json_writer_append_property_with_double_value(json_writer,
            (UCHAR*)TELEMETRY_GYROSCOPEZ,
            sizeof(TELEMETRY_GYROSCOPEZ) - 1,
            lsm6dsl_data.angular_rate_mdps[2],
            2))
    {
        return NX_NOT_SUCCESSFUL;
    }

    return NX_AZURE_IOT_SUCCESS;
}

static void set_led_state(bool level)
{
    if (level)
    {
        printf("\tLED is turned ON\r\n");
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    }
    else
    {
        printf("\tLED is turned OFF\r\n");
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    }
}

static void command_received_cb(AZURE_IOT_NX_CONTEXT* nx_context_ptr,
    const UCHAR* component,
    USHORT component_length,
    const UCHAR* method,
    USHORT method_length,
    UCHAR* payload,
    USHORT payload_length,
    VOID* context_ptr,
    USHORT context_length)
{
    UINT status;

    if (strncmp((CHAR*)method, SET_LED_STATE_COMMAND, method_length) == 0)
    {
        bool arg = (strncmp((CHAR*)payload, "true", payload_length) == 0);
        set_led_state(arg);

        if ((status = nx_azure_iot_hub_client_command_message_response(
                 &nx_context_ptr->iothub_client, 200, context_ptr, context_length, NULL, 0, NX_WAIT_FOREVER)))
        {
            printf("Direct method response failed! (0x%08x)\r\n", status);
            return;
        }

        azure_iot_nx_client_publish_bool_property(&azure_iot_nx_client, NULL, LED_STATE_PROPERTY, arg);
    }
    else if (strncmp((CHAR*)method, SET_DISPLAY_TEXT_COMMAND, method_length) == 0)
    {
        // drop the first and last character to remove the quotes
        screen_printn((CHAR*)payload + 1, payload_length - 2, L0);
        if ((status = nx_azure_iot_hub_client_command_message_response(
                 &nx_context_ptr->iothub_client, 200, context_ptr, context_length, NULL, 0, NX_WAIT_FOREVER)))
        {
            printf("Direct method response failed! (0x%08x)\r\n", status);
            return;
        }
    }
    else
    {
        printf("Direct method is not for this device\r\n");

        if ((status = nx_azure_iot_hub_client_command_message_response(
                 &nx_context_ptr->iothub_client, 501, context_ptr, context_length, NULL, 0, NX_WAIT_FOREVER)))
        {
            printf("Direct method response failed! (0x%08x)\r\n", status);
            return;
        }
    }
}

static void writable_property_received_cb(AZURE_IOT_NX_CONTEXT* nx_context,
    const UCHAR* component_name,
    UINT component_name_len,
    UCHAR* property_name,
    UINT property_name_len,
    NX_AZURE_IOT_JSON_READER* json_reader_ptr,
    UINT version)
{
    UINT status;

    if (strncmp((CHAR*)property_name, TELEMETRY_INTERVAL_PROPERTY, property_name_len) == 0)
    {
        status = nx_azure_iot_json_reader_token_int32_get(json_reader_ptr, &telemetry_interval);
        if (status == NX_AZURE_IOT_SUCCESS)
        {
            printf("Updating %s to %ld\r\n", TELEMETRY_INTERVAL_PROPERTY, telemetry_interval);

            // Confirm reception back to hub
            azure_nx_client_respond_int_writable_property(
                nx_context, NULL, TELEMETRY_INTERVAL_PROPERTY, telemetry_interval, 200, version);

            azure_nx_client_periodic_interval_set(nx_context, telemetry_interval);
        }
    }
}

static void property_received_cb(AZURE_IOT_NX_CONTEXT* nx_context,
    const UCHAR* component_name,
    UINT component_name_len,
    UCHAR* property_name,
    UINT property_name_len,
    NX_AZURE_IOT_JSON_READER* json_reader_ptr,
    UINT version)
{
    UINT status;

    if (strncmp((CHAR*)property_name, TELEMETRY_INTERVAL_PROPERTY, property_name_len) == 0)
    {
        status = nx_azure_iot_json_reader_token_int32_get(json_reader_ptr, &telemetry_interval);
        if (status == NX_AZURE_IOT_SUCCESS)
        {
            printf("Updating %s to %ld\r\n", TELEMETRY_INTERVAL_PROPERTY, telemetry_interval);
            azure_nx_client_periodic_interval_set(nx_context, telemetry_interval);
        }
    }
}

static void properties_complete_cb(AZURE_IOT_NX_CONTEXT* nx_context)
{
    // Device twin processing is done, send out property updates
    azure_iot_nx_client_publish_properties(nx_context, DEVICE_INFO_COMPONENT_NAME, append_device_info_properties);
    azure_iot_nx_client_publish_bool_property(nx_context, NULL, LED_STATE_PROPERTY, false);
    azure_iot_nx_client_publish_int_writable_property(
        nx_context, NULL, TELEMETRY_INTERVAL_PROPERTY, telemetry_interval);

    printf("\r\nStarting Main loop\r\n");
    screen_print("Azure IoT", L0);
}

static void telemetry_cb(AZURE_IOT_NX_CONTEXT* nx_context)
{
    static TELEMETRY_STATE telemetry_state = TELEMETRY_STATE_DEFAULT;

    switch (telemetry_state)
    {
        case TELEMETRY_STATE_DEFAULT:
            azure_iot_nx_client_publish_telemetry(&azure_iot_nx_client, NULL, append_device_telemetry);
            break;

        case TELEMETRY_STATE_MAGNETOMETER:
            azure_iot_nx_client_publish_telemetry(&azure_iot_nx_client, NULL, append_device_telemetry_magnetometer);
            break;

        case TELEMETRY_STATE_ACCELEROMETER:
            azure_iot_nx_client_publish_telemetry(&azure_iot_nx_client, NULL, append_device_telemetry_accelerometer);
            break;

        case TELEMETRY_STATE_GYROSCOPE:
            azure_iot_nx_client_publish_telemetry(&azure_iot_nx_client, NULL, append_device_telemetry_gyroscope);
            break;

        default:
            break;
    }

    telemetry_state = (telemetry_state + 1) % TELEMETRY_STATE_END;
}

UINT azure_iot_nx_client_entry(
    NX_IP* ip_ptr, NX_PACKET_POOL* pool_ptr, NX_DNS* dns_ptr, UINT (*unix_time_callback)(ULONG* unix_time))
{
    UINT status;

    if ((status = azure_iot_nx_client_create(&azure_iot_nx_client,
             ip_ptr,
             pool_ptr,
             dns_ptr,
             unix_time_callback,
             IOT_MODEL_ID,
             sizeof(IOT_MODEL_ID) - 1)))
    {
        printf("ERROR: azure_iot_nx_client_create failed (0x%08x)\r\n", status);
        return status;
    }

    // Register the callbacks
    azure_iot_nx_client_register_command_callback(&azure_iot_nx_client, command_received_cb);
    azure_iot_nx_client_register_writable_property_callback(&azure_iot_nx_client, writable_property_received_cb);
    azure_iot_nx_client_register_property_callback(&azure_iot_nx_client, property_received_cb);
    azure_iot_nx_client_register_properties_complete_callback(&azure_iot_nx_client, properties_complete_cb);
    azure_iot_nx_client_register_timer_callback(&azure_iot_nx_client, telemetry_cb, telemetry_interval);

    // Setup authentication
#ifdef ENABLE_X509
    if ((status = azure_iot_nx_client_cert_set(&azure_iot_nx_client,
             (UCHAR*)iot_x509_device_cert,
             iot_x509_device_cert_len,
             (UCHAR*)iot_x509_private_key,
             iot_x509_private_key_len)))
    {
        printf("ERROR: azure_iot_nx_client_cert_set (0x%08x)\r\n", status);
        return status;
    }
#else
    if ((status = azure_iot_nx_client_sas_set(&azure_iot_nx_client, IOT_DEVICE_SAS_KEY)))
    {
        printf("ERROR: azure_iot_nx_client_sas_set (0x%08x)\r\n", status);
        return status;
    }
#endif

    // Enter the main loop
#ifdef ENABLE_DPS
    azure_iot_nx_client_dps_run(&azure_iot_nx_client, IOT_DPS_ID_SCOPE, IOT_DPS_REGISTRATION_ID, wwd_network_connect);
#else
    azure_iot_nx_client_hub_run(&azure_iot_nx_client, IOT_HUB_HOSTNAME, IOT_HUB_DEVICE_ID, wwd_network_connect);
#endif

    return NX_SUCCESS;
}
