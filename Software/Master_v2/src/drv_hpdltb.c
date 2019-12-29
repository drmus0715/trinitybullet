/*****************************************************************************/
/**
* @file drv_hpdltb.c
* @comments HPDLTB 通信用モジュール
*
* MODIFICATION HISTORY:
*
* Ver   Who           Date        Changes
* ----- ------------- ----------- --------------------------------------------
* 0.01  drmus0715     2019/10/13  First release
*
******************************************************************************/

/*****************************************************************************/
/* Include Files
******************************************************************************/
/* FreeRTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Standard Lib Includes */
#include <stdio.h>
#include <stdlib.h>

/* ESP-IDF Includes */
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "def_system.h"
#include "drv_hpdltb.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* FreeRTOS TaskConfigure */
#define tskstacDRV_HPDLTB 4096
#define tskprioDRV_HPDLTB 9

/* Queue Configure */
#define QUEUE_HPDLTB_SIZE 16
#define QUEUE_HPDLTB_WAIT portMAX_DELAY

/* I2C Configure */
#define I2C_MASTER_SCL_IO GPIO_NUM_22 /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO GPIO_NUM_21 /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0      /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0   /*!< I2C master doesn't need buffer */

#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */

#define HPDLTB_ADDR 0x1E

static const char *TAG = "HPDLTB";

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
// Semaphore

// Task Handler
TaskHandle_t xHpdltbTask;

// Queue
QueueHandle_t xHpdltbQueue = NULL;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
// Task
static void prvHpdltbTask(void *pvParameters);

// I2C
esp_err_t i2c_master_init(void);
static esp_err_t i2c_master_write_slave(i2c_port_t i2c_num, uint8_t *data_wr, size_t size);

/*****************************************************************************/
/* Public Function
******************************************************************************/

/*****************************************************************************/
/**
* HPDLTB 送信モジュール 初期化
*
* @param    [params] is ...
*
* @return   pdPASS / pdFAIL
*         
* @note     ##
*
******************************************************************************/
esp_err_t lInitHpdltb()
{
    BOOL_t xStatus;

    // Install CAN driver
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "Driver installed");

    // Create Queue
    xHpdltbQueue = xQueueCreate(QUEUE_HPDLTB_SIZE, sizeof(hpdltb_t));
    configASSERT(xHpdltbQueue);

    // Create Task
    xStatus = xTaskCreatePinnedToCore(
        prvHpdltbTask, "HPDLTB", tskstacDRV_HPDLTB, NULL,
        tskprioDRV_HPDLTB, NULL, tskNO_AFFINITY);
    configASSERT(xStatus);

    return (xStatus == pdPASS ? ESP_OK : ESP_FAIL);
}

/*****************************************************************************/
/**
 * CAN Tx Queueへ送信
 *
 * @param    hpdltb_t：canで送信するメッセージ
 *
 * @return   pdPASS / pdFAIL
 *
 * @note		##
 *
 ******************************************************************************/
BOOL_t xSendHpdltbQueue(hpdltb_t hptldbMsg)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xHpdltbQueue, &hptldbMsg, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/* Private Function
******************************************************************************/
/*****************************************************************************/
/**
* HPDLTB 受信タスク
* 受信したらI2CでHPDLTBへ送信します。
*
* @param    [params] is ...
*
* @return   [Return] is ...
*         
* @note     ##
*
******************************************************************************/
static void prvHpdltbTask(void *pvParameters)
{
    esp_err_t ret;
    hpdltb_t rcvBuff;
    uint8_t sendData[4] = {};

    for (;;)
    {
        // Queueから受信
        xQueueReceive(xHpdltbQueue, &rcvBuff, portMAX_DELAY);
        // ESP_LOGI(TAG, "send I2C data:%d,%d,%d,%d", rcvBuff.eMsg, rcvBuff.bHp, rcvBuff.bTimeH, rcvBuff.bTimeL);
        /*
         * データ変換
         * [0] -> EVENT
         * [1] -> HP
         * [2] -> TIME_H
         * [3] -> TIME_L
         */
        sendData[0] = rcvBuff.eMsg;
        sendData[1] = rcvBuff.bHp;
        sendData[2] = rcvBuff.bTimeH;
        sendData[3] = rcvBuff.bTimeL;

        // 書き込み
        ret = i2c_master_write_slave(I2C_MASTER_NUM, sendData, sizeof(uint8_t[4]));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed Senc Data(I2C) errCode:%d", ret);
        }
    }
}

/*****************************************************************************/
/**
* I2C 設定
*
* @param    [params] is ...
*
* @return   [Return] is ...
*         
* @note     ##
*
******************************************************************************/
esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief Test code to write esp-i2c-slave
 *        Master device write data to slave(both esp32),
 *        the data will be stored in slave buffer.
 *        We can read them out from slave buffer.
 *
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write n bytes + ack  | stop |
 * --------|---------------------------|----------------------|------|
 *
 */
static esp_err_t i2c_master_write_slave(i2c_port_t i2c_num, uint8_t *data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    // hpdltbのアドレスにした
    i2c_master_write_byte(cmd, (HPDLTB_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}