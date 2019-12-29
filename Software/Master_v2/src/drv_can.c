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

/* FreeRTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Standard Lib Includes */
#include <stdio.h>
#include <stdlib.h>

/* ESP-IDF Includes */
#include "driver/can.h"
#include "esp_err.h"
#include "esp_log.h"

#include "drv_can.h"
#include "ctrl_main.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* FreeRTOS TaskConfigure */
#define tskstacMAIN_CANTASK 4096
#define tskprioMAIN_CANRXTASK 9
#define tskprioMAIN_CANTXTASK 8

/* Queue Configure */
#define QUEUE_CAN_TX_SIZE 32
#define QUEUE_CAN_TX_WAIT portMAX_DELAY

/* CAN Driver Configure */
#define TX_GPIO_NUM 25
#define RX_GPIO_NUM 26
#define MSG_ID 0x000 // 11 bit standard format ID
static const can_timing_config_t t_config = CAN_TIMING_CONFIG_500KBITS();
// Filter all other IDs except MSG_ID
static const can_filter_config_t f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();
// Set to NO_ACK mode due to self testing with single module
static const can_general_config_t g_config =
    CAN_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, CAN_MODE_NO_ACK);

/* ESPLOGGER Configure */
#define EXAMPLE_TAG "CAN Driver"

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
// Semaphore

// Task Handler
TaskHandle_t xCanRxTask;
TaskHandle_t xCanTxTask;

// Queue
QueueHandle_t xCanTxQueue = NULL;

// CAN
uint32_t ulCanId;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
// Task
static void prvCanRxTask(void *pvParameters);
static void prvCanTxTask(void *pvParameters);

// CAN Function
void vCanSendWrapper(canCommMsg_t canMsg);
void vCanMsgConv(can_message_t *canMsg, canCommMsg_t *canCommMsg);

/*****************************************************************************/
/* Public Function
******************************************************************************/

/*****************************************************************************/
/**
 * CANモジュール初期化
 *
 * @param    ##
 *
 * @return   pdPASS / pdFAIL
 *
 * @note		##
 *
 ******************************************************************************/
esp_err_t lInitCanFunction()
{
    BOOL_t xStatus;

    // Install CAN driver
    ESP_ERROR_CHECK(can_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(EXAMPLE_TAG, "Driver installed");

    // Create Queue
    xCanTxQueue = xQueueCreate(QUEUE_CAN_TX_SIZE, sizeof(canCommMsg_t));
    configASSERT(xCanTxQueue);

    // Create Task
    xStatus = xTaskCreatePinnedToCore(
        prvCanRxTask, "CAN_rx", tskstacMAIN_CANTASK, NULL,
        tskprioMAIN_CANRXTASK, NULL, tskNO_AFFINITY);
    configASSERT(xStatus);

    xStatus = xTaskCreatePinnedToCore(
        prvCanTxTask, "CAN_tx", tskstacMAIN_CANTASK, NULL,
        tskprioMAIN_CANTXTASK, NULL, tskNO_AFFINITY);
    configASSERT(xStatus);

    // CAN Driver Started
    ESP_ERROR_CHECK(can_start());
    ESP_LOGI(EXAMPLE_TAG, "Driver started");

    return (xStatus == pdPASS ? ESP_OK : ESP_FAIL);
}

/*****************************************************************************/
/**
 * CAN Tx Queueへ送信
 *
 * @param    canCommMsg_t：canで送信するメッセージ
 *
 * @return   pdPASS / pdFAIL
 *
 * @note		##
 *
 ******************************************************************************/
BOOL_t xSendCanTxQueue(canCommMsg_t canTxMsg)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xCanTxQueue, &canTxMsg, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/* Private Function
******************************************************************************/

/*****************************************************************************/
/**
 * CANを受信するタスク。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
static void prvCanRxTask(void *pvParameters)
{
    canCommMsg_t rxCanMsg;
    can_message_t rx_message;

    ESP_LOGI(EXAMPLE_TAG, "RX_TASK started");
    for (;;)
    {
        // Wait CAN Message
        ESP_ERROR_CHECK(can_receive(&rx_message, portMAX_DELAY));
        ESP_LOGI(EXAMPLE_TAG, "Msg received - ID = %d", rx_message.identifier);

        // メッセージ形式を変換
        vCanMsgConv(&rx_message, &rxCanMsg);
        ESP_LOGI(EXAMPLE_TAG,
                 "rxCanMsg = {.PanelID = %d, .BtnFlag = %d, .ColorInfo = R%d "
                 "G%d B%d, "
                 ".LightTime = %d}",
                 rxCanMsg.bPanelId, rxCanMsg.bBtnFlag, rxCanMsg.bColorInfoR,
                 rxCanMsg.bColorInfoG, rxCanMsg.bColorInfoB,
                 rxCanMsg.bLightTime);

        // ctrl_mainへ送信
        xSendCtrlCanRxQueue(rxCanMsg);
    }
}

/*****************************************************************************/
/**
 * CAN Messageを送信するタスク。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
static void prvCanTxTask(void *pvParameters)
{
    canCommMsg_t txCanMsg;

    ESP_LOGI(EXAMPLE_TAG, "TX_TASK started");
    for (;;)
    {
        // Wait Queue notification
        xQueueReceive(xCanTxQueue, &txCanMsg, QUEUE_CAN_TX_WAIT);

        // Panelへ送信
        vCanSendWrapper(txCanMsg);
    }
}

/*****************************************************************************/
/**
 * CAN メッセージ送信ラッパー
 * canCommMsg_tを入力し、canMsg.canIdへメッセージを送信する
 *
 * @param	canMsg：送信するCANの情報
 * @param   canID：Panel固有のCAN ID
 *
 * @return  CAN_OK / CAN err各種
 *
 * @note    ##
 *
 ******************************************************************************/
void vCanSendWrapper(canCommMsg_t canMsg)
{
    can_message_t tx_msg = {.data_length_code = 7,
                            .identifier = canMsg.ulCanId,
                            .flags = CAN_MSG_FLAG_NONE};

    // dataの下位バイトから情報を埋めていく
    tx_msg.data[0] = canMsg.bPanelId;
    tx_msg.data[1] = canMsg.bBtnFlag;
    tx_msg.data[2] = canMsg.bColorInfoR;
    tx_msg.data[3] = canMsg.bColorInfoG;
    tx_msg.data[4] = canMsg.bColorInfoB;
    tx_msg.data[5] = canMsg.bLightTime;
    tx_msg.data[6] = canMsg.bStartSwFlag;

    ESP_ERROR_CHECK(can_transmit(&tx_msg, portMAX_DELAY));
    ESP_LOGI(EXAMPLE_TAG, "Msg transmit - ID = %d", tx_msg.identifier);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/*****************************************************************************/
/**
 * can_message_t -> canCommMsg_t Converter
 *
 * @param	canMsg：ESPdrvのCANメッセージ形式
 * @param   canCommMsg：TB固有のCANメッセージ形式
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vCanMsgConv(can_message_t *canMsg, canCommMsg_t *canCommMsg)
{
    canCommMsg->ulCanId = canMsg->identifier;
    canCommMsg->bPanelId = canMsg->data[0];
    canCommMsg->bBtnFlag = canMsg->data[1];
    canCommMsg->bColorInfoR = canMsg->data[2];
    canCommMsg->bColorInfoG = canMsg->data[3];
    canCommMsg->bColorInfoB = canMsg->data[4];
    canCommMsg->bLightTime = canMsg->data[5];
    canCommMsg->bStartSwFlag = canMsg->data[6];
}