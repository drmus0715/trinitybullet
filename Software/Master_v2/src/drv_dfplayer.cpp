/*****************************************************************************/
/**
* @file dev_can.c
* @comments CAN送受信モジュール
*
* MODIFICATION HISTORY:
*
* Ver   Who           Date        Changes
* ----- ------------- ----------- --------------------------------------------
* 0.01  drmus0715     2019/08/20  First release
*
******************************************************************************/

/*****************************************************************************/
/* Include Files
******************************************************************************/
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

/* Driver includes */
#include "DFRobotDFPlayerMini.h"

/* User Includes */
#include "def_system.h"
#include "drv_dfplayer.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* FreeRTOS TaskConfigure */
#define tskstacDRV_DFPTASK 4096
#define tskprioDRV_DFPTASK 9

/* Queue Configure */
#define QUEUE_DFPLAYER_SIZE 2
#define QUEUE_DFPLAYER_WAIT portMAX_DELAY

/* ESPLOGGER Configure */
#define LOG_TAG "DFPlayer"

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
// Semaphore

// Task Handler
TaskHandle_t xDfplayerTask;

// Queue
QueueHandle_t xDfplayerQueue = NULL;

// DFPlayer Def
// HardwareSerial Serial2(2);
DFRobotDFPlayerMini DFPlayer;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
// Task
static void prvDfplayerTask(void *pvParameters);

/*****************************************************************************/
/* Public Function
******************************************************************************/

/*****************************************************************************/
/**
* Dfplayerモジュール初期化
*
* @param    ##
*
* @return   pdPASS / pdFAIL
* 		
* @note		##
*
******************************************************************************/
esp_err_t lInitDfplayer()
{
    BOOL_t xStatus;

    // DFPlayer Initialize
    Serial2.begin(9600);
    delay(10);
    ESP_ERROR_CHECK(DFPlayer.begin(Serial2));
    ESP_LOGI(LOG_TAG, "DFPlayer Init");
    DFPlayer.volume(DFPLAYER_DEFAULT_VOLUME);

    // Create Queue
    xDfplayerQueue = xQueueCreate(QUEUE_DFPLAYER_SIZE, sizeof(dfpCtrlMsg_t));
    configASSERT(xDfplayerQueue);

    // Create Task
    xStatus = xTaskCreatePinnedToCore(prvDfplayerTask, "CAN_rx", tskstacDRV_DFPTASK, NULL,
                                      tskprioDRV_DFPTASK, &xDfplayerTask, tskNO_AFFINITY);
    configASSERT(xStatus);

    return (xStatus == pdPASS ? ESP_OK : ESP_FAIL);
}

/*****************************************************************************/
/**
* Dfplayer Queueへ送信
*
* @param    canCommMsg_t：canで送信するメッセージ
*
* @return   pdPASS / pdFAIL
* 		
* @note		##
*
******************************************************************************/
BOOL_t xSendDfplayerQueue(dfpCtrlMsg_t dfpCtrlMsg)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xDfplayerQueue, &dfpCtrlMsg, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/* Private Function
******************************************************************************/

/*****************************************************************************/
/**
 * DFPlayerへコマンドを送信するタスク。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
static void prvDfplayerTask(void *pvParameters)
{
    dfpCtrlMsg_t dfpCtrlMsg = {};

    // Set Volume
    DFPlayer.volume(DFPLAYER_DEFAULT_VOLUME);

    for (;;)
    {
        // Queue Wait
        xQueueReceive(xDfplayerQueue, &dfpCtrlMsg, QUEUE_DFPLAYER_WAIT);
        ESP_LOGI(LOG_TAG, "dfpCtrlMsg = {.uiVolume = %d, .eSound = %d",
                 dfpCtrlMsg.uiVolume, dfpCtrlMsg.eSound);

        // // Set Volume
        // DFPlayer.volume(dfpCtrlMsg.uiVolume);

        // Play Sound
        DFPlayer.play(dfpCtrlMsg.eSound);

        // ねんのため
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}