/*
 * sl_wfx_iot_wifi.c
 *
 *  Created on: Jan 21, 2020
 *      Author: mbruno
 */

#ifndef SL_WFX_IOT_WIFI_C_
#define SL_WFX_IOT_WIFI_C_

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"

//#include "FreeRTOS_IP.h"
//#include "FreeRTOS_Sockets.h"
//#include "FreeRTOS_DHCP.h"

/* Library headers */
#include "soc.h"

/* BSP/bitstream headers */
#include "bitstream_devices.h"
#include "spi_master_driver.h"
#include "gpio_driver.h"
#include "sl_wfx.h"
#include "sl_wfx_host.h"
#include "brd8023a_pds.h"

#include "sl_wfx_iot_wifi.h"


static sl_wfx_context_t wfx_ctx;

WIFIReturnCode_t WIFI_On( void )
{
    sl_status_t ret;

    sl_wfx_host_set_hif(bitstream_spi_devices[BITSTREAM_SPI_DEVICE_A],
                        bitstream_gpio_devices[BITSTREAM_GPIO_DEVICE_A],
                        gpio_1I, 0,  /* header pin 9 */
                        gpio_1P, 0,  /* header pin 10 */
                        gpio_1J, 0); /* header pin 12 */

    sl_wfx_host_set_pds(pds_table_brd8023a, SL_WFX_ARRAY_COUNT(pds_table_brd8023a));

    ret = sl_wfx_init(&wfx_ctx);

    if (ret == SL_STATUS_OK) {
        return eWiFiSuccess;
    } else {
        return eWiFiFailure;
    }
}

WIFIReturnCode_t WIFI_Off( void )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_ConnectAP( const WIFINetworkParams_t * const pxNetworkParams )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_Disconnect( void )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_Reset( void )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_SetMode( WIFIDeviceMode_t xDeviceMode )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetMode( WIFIDeviceMode_t * pxDeviceMode )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_NetworkAdd( const WIFINetworkProfile_t * const pxNetworkProfile,
                                  uint16_t * pusIndex )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_NetworkGet( WIFINetworkProfile_t * pxNetworkProfile,
                                  uint16_t usIndex )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_NetworkDelete( uint16_t usIndex )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_Ping( uint8_t * pucIPAddr,
                            uint16_t usCount,
                            uint32_t ulIntervalMS )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetIP( uint8_t * pucIPAddr )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetMAC( uint8_t * pucMac )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetHostIP( char * pcHost,
                                 uint8_t * pucIPAddr )
{
    return eWiFiNotSupported;
}

static WIFIScanResult_t *scan_results;
static uint8_t scan_result_max_count;
static int scan_count;

void sl_wfx_scan_result_callback(sl_wfx_scan_result_ind_body_t* scan_result)
{
//  printf(
//    "# %2d %2d  %03d %02X:%02X:%02X:%02X:%02X:%02X  %s",
//    scan_count + 1,
//    scan_result->channel,
//    ((int16_t)(scan_result->rcpi - 220) / 2),
//    scan_result->mac[0], scan_result->mac[1],
//    scan_result->mac[2], scan_result->mac[3],
//    scan_result->mac[4], scan_result->mac[5],
//    scan_result->ssid_def.ssid);
  /*Report one AP information*/
//  printf("\r\n");

    if (scan_count < scan_result_max_count) {
        size_t ssid_len = scan_result->ssid_def.ssid_length;
        if (ssid_len > wificonfigMAX_SSID_LEN) {
            ssid_len = wificonfigMAX_SSID_LEN;
        }
        memcpy(scan_results[scan_count].cSSID, scan_result->ssid_def.ssid, ssid_len);
        memcpy(scan_results[scan_count].ucBSSID, scan_result->mac, wificonfigMAX_BSSID_LEN);
        scan_results[scan_count].cChannel = scan_result->channel;
        scan_results[scan_count].ucHidden = 0;
        scan_results[scan_count].cRSSI = scan_result->rcpi; //TODO: conversion?

        if (*((uint8_t *) &scan_result->security_mode) == 0) {
            scan_results[scan_count].xSecurity = eWiFiSecurityOpen;
        } else {

            scan_results[scan_count].xSecurity = eWiFiSecurityNotSupported;

            if (scan_result->security_mode.wep) {
                scan_results[scan_count].xSecurity = eWiFiSecurityWEP;
            }

            if (scan_result->security_mode.wpa) {
                scan_results[scan_count].xSecurity = eWiFiSecurityWPA;
            }

            if (scan_result->security_mode.wpa2) {
                if (scan_result->security_mode.psk) {
                    scan_results[scan_count].xSecurity = eWiFiSecurityWPA2;
                } else if (scan_result->security_mode.eap) {
                    scan_results[scan_count].xSecurity = eWiFiSecurityWPA2_ent;
                }
            }
        }

        scan_count++;
    }
}

void sl_wfx_scan_complete_callback(uint32_t status)
{
  xEventGroupSetBits(sl_wfx_event_group, SL_WFX_SCAN_COMPLETE);
}

WIFIReturnCode_t WIFI_Scan( WIFIScanResult_t * pxBuffer,
                            uint8_t ucNumNetworks )
{
    EventBits_t bits;
    const uint8_t channel_list[] = {1,2,3,4,5,6,7,8,9,10,11,12,13};

    scan_results = pxBuffer;
    scan_result_max_count = ucNumNetworks;
    scan_count = 0;

    memset(pxBuffer, 0, sizeof(WIFIScanResult_t) * ucNumNetworks);

    sl_wfx_send_scan_command(WFM_SCAN_MODE_ACTIVE,
                             channel_list,
                             SL_WFX_ARRAY_COUNT(channel_list),
                             NULL,
                             0,
                             NULL,
                             0,
                             NULL);

    bits = xEventGroupWaitBits(sl_wfx_event_group, SL_WFX_SCAN_COMPLETE, pdTRUE, pdTRUE, pdMS_TO_TICKS(1000));

    if (bits & SL_WFX_SCAN_COMPLETE) {
        rtos_printf("scan complete\n");
        return eWiFiSuccess;
    } else {
        rtos_printf("scan did not complete\n");
        return eWiFiTimeout;
    }
}

WIFIReturnCode_t WIFI_StartAP( void )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_StopAP( void )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_ConfigureAP( const WIFINetworkParams_t * const pxNetworkParams )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_SetPMMode( WIFIPMMode_t xPMModeType,
                                 const void * pvOptionValue )
{
    return eWiFiNotSupported;
}

WIFIReturnCode_t WIFI_GetPMMode( WIFIPMMode_t * pxPMModeType,
                                 void * pvOptionValue )
{
    return eWiFiNotSupported;
}

BaseType_t WIFI_IsConnected( void )
{
    return eWiFiNotSupported;
}

#endif /* SL_WFX_IOT_WIFI_C_ */
