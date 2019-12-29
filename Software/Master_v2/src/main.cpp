/*****************************************************************************/
/**
 * @file main.c
 * @comments Master_main基板メインモジュール
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Who           Date        Changes
 * ----- ------------- ----------- --------------------------------------------
 * 0.00  drmus0715     2019/08/20  First release
 *
 ******************************************************************************/

/*****************************************************************************/
/* Include Files
******************************************************************************/
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif
/* Arduino Framework */
#include <Arduino.h>

/* FreeRTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Standard Lib Includes */
#include <stdio.h>
#include <stdlib.h>

/* ESP-IDF Includes */
#include "esp_err.h"
#include "esp_log.h"

/* Driver Includes */
#include "drv_can.h"
#include "drv_dfplayer.h"
#include "drv_gamemng.h"
#include "drv_hpdltb.h"
#include "ctrl_main.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/

/*****************************************************************************/
/* Private Function
******************************************************************************/
/*****************************************************************************/
/**
 * app_main関数
 * ESP-IDFの初期起動タスク。
 *
 * @param    ##
 *
 * @return   ##
 *
 * @note     ##
 *
 ******************************************************************************/

extern "C" void app_main()
{
    // Arduino Framework Install
    initArduino();
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);

    // Init Function
    // ex. ESP_ERROR_CHECK(iInitFunction( ... ));
    // Error系はesp_err_tを使用してね
    ESP_ERROR_CHECK(lInitCanFunction());
    ESP_ERROR_CHECK(lInitDfplayer());
    ESP_ERROR_CHECK(lInitGameMng());
    ESP_ERROR_CHECK(lInitHpdltb());
    ESP_ERROR_CHECK(lInitCtrlMainFunction());
}
