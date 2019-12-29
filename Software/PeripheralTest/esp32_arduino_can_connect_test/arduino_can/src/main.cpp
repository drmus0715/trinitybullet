/*****************************************************************************/
/**
 * @file main.cpp
 * @comments
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Who           Date        Changes
 * ----- ------------- ----------- --------------------------------------------
 * 0.01  drmus0715     2019/06/29  First release
 *
 ******************************************************************************/

/*****************************************************************************/
/* Include Files
******************************************************************************/
#include "mcp_can.h"
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <SPI.h>
#include <portmacro.h>
#include <semphr.h>

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* FreeRTOS TaskConfigure */
#define tskstacMAIN_CANRCVTASK 128
#define tskprioMAIN_CANRCVTASK 1

/* CAN Configure */
#define CAN_ID 0x000

#define DEBUG_EN 1

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
unsigned char Flag_Recv = 0;
unsigned char len = 0;
unsigned char buf[8];
char str[20];

SemaphoreHandle_t xCanRxSemphr;

MCP_CAN CAN(10); // Set CS to pin 10

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
static void prvCanRcvTask(void *pvParameters);
static void prvCanSendTask(void *pvParameters);

/* CAN Interrupt Function */
void prvCanIntrHandler();

/*****************************************************************************/
/* Public Function
******************************************************************************/

/*****************************************************************************/
/* Private Function
******************************************************************************/

/*****************************************************************************/
/**
 * Function description
 *
 * @param	[params] is ...
 *
 * @return  [Return] is ...
 *
 * @note    ##
 *
 ******************************************************************************/
void setup() {
    Serial.begin(115200);

    while (!Serial) {
        ;
    }

    xCanRxSemphr = xSemaphoreCreateBinary();

START_INIT:

    if (CAN_OK ==
        CAN.begin(CAN_500KBPS, MCP_8MHz)) // init can bus : baudrate = 500k
    {
        Serial.println("CAN BUS Shield init ok!");
        // CAN.init_Mask(0, 0, 0x3FF);
        // CAN.init_Filt(0, 0, 0x555);
        attachInterrupt(1, prvCanIntrHandler, FALLING);
    } else {
        Serial.println("CAN BUS Shield init fail");
        Serial.println("Init CAN BUS Shield again");
        delay(100);
        goto START_INIT;
    }

    xTaskCreate(prvCanRcvTask,
                (const char *)"prvCanRcvTask", //  Name
                tskstacMAIN_CANRCVTASK,        // Stack size
                NULL,
                tskprioMAIN_CANRCVTASK, // Priority
                NULL);

    xTaskCreate(prvCanSendTask,
                (const char *)"prvCanSend3Task", //  Name
                tskstacMAIN_CANRCVTASK,          // Stack size
                NULL,
                tskprioMAIN_CANRCVTASK, // Priority
                NULL);
}

void loop() {
    // Empty. Things are done in Tasks.
}

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
static void prvCanRcvTask(void *pvParameters) {
    Serial.println("Startup prvCanRcvTask");
    unsigned char len = 0;
    unsigned char buf[8];
    const TickType_t xCanRxBlockTime = pdMS_TO_TICKS(500);

    for (;;) {
        if (xSemaphoreTake(xCanRxSemphr, xCanRxBlockTime) == pdPASS) {
            if (CAN_MSGAVAIL == CAN.checkReceive()) // check if data coming
            {
                CAN.readMsgBuf(
                    &len,
                    buf); // read data,  len: data length, buf: data buf

                unsigned long canId = CAN.getCanId();

                Serial.println("-----------------------------");
                Serial.print("Get data from ID: 0x");
                Serial.println(canId, HEX);

                for (int i = 0; i < len; i++) // print the data
                {
                    Serial.print(buf[i], HEX);
                    Serial.print("\t");
                }
                Serial.println();
            }
        } else {
            /* code */
        }
    }
}

/*****************************************************************************/
/**
 * CANの割り込み関数
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void prvCanIntrHandler() {
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xCanRxSemphr, &xHigherPriorityTaskWoken);
    taskYIELD();
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
static void prvCanSendTask(void *pvParameters) {
    Serial.println("Startup prvCanSendTask");
    const unsigned char len = 8;
    const unsigned char buf[8] = {};
    bool fallingFlg = false;
    // const TickType_t xCanRxBlockTime = pdMS_TO_TICKS(500);

    pinMode(5, INPUT);

    for (;;) {
        if ((digitalRead(A0) == LOW)) {
            fallingFlg = true;
        } else if ((digitalRead(A0) == HIGH) && (fallingFlg == true)) {
            CAN.sendMsgBuf(CAN_ID, 0, len, buf);
            Serial.println("-----------------------------");
            fallingFlg = false;
        }
        vTaskDelay(1);
    }
}