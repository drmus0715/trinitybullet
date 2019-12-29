/*****************************************************************************/
/**
 * @file main.cpp
 * @comments Panel基板メインモジュール
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Who           Date        Changes
 * ----- ------------- ----------- --------------------------------------------
 * 0.00  drmus0715     2019/08/14  First release
 *
 ******************************************************************************/

/*****************************************************************************/
/* Include Files
******************************************************************************/
#include <Arduino.h>

// library Include
#include <SPI.h>
#include "mcp_can.h"
#include "MsTimer2.h"
#include "Adafruit_NeoPixel.h"

// User Include
#include "def_system.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
// CAN
const int SPI_CS_PIN = 10;
const int CAN_INTR_NO = 1;
#define CAN_RX_TIMEOUT_CNT 64
#define MASTER_CAN_ID 0x00

// Serial LED
#define SERIAL_LED_SEND_INTERVAL_MS 100

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
// CAN
MCP_CAN CAN(SPI_CS_PIN); // Set CS to pin 10
uint32_t ulPanelId = 0x0D;
bool bCanIntrFlg = false;

// Serial LED
Adafruit_NeoPixel *pixels;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
void vInitGpio();           // GPIO Set Mode
uint32_t ulGetPanelCanID(); // CAN ID取得

/* CAN */
bool bInitCanDriver(); // Init
// CAN Intr Function
void vCanIntrHandler();
// CAN Tx Wrapper
byte bCanSendWrapper(canCommMsg_t canMsg, uint32_t canId);
// CAN Rx
byte uCanReceiveInfo(canCommMsg_t *canMsg);

/* Serial LED */
void vSerialLedIntrHandler();

/*****************************************************************************/
/* Private Function
******************************************************************************/

/*****************************************************************************/
/**
 * setup, loop関数
 *
 * @param	##
 *
 * @return  ##
 *
 * @note		##
 *
 ******************************************************************************/
void setup() {
    // UART Setup (For Debug)
    Serial.begin(115200);
    Serial.println("Hello, world! i'm Panel!");

    // GPIO Configure
    vInitGpio();

    // CANIDを変数に格納する
    // ulCanId = ulGetPanelCanID();
    Serial.print("This Panel ID : ");
    Serial.println(ulPanelId);

    // Init Function
    bInitCanDriver();
    // todo: neopixel Init
}

void vPrintCanMsg(canCommMsg_t canMsg) {
    Serial.print("CanID: ");
    Serial.print(canMsg.ulCanId);
    Serial.print(" PanelId: ");
    Serial.print(canMsg.bPanelId);
    Serial.print(" BtnFlg: ");
    Serial.print(canMsg.bBtnFlag);
    Serial.print(" ColorInf: R");
    Serial.print(canMsg.bColorInfoR);
    Serial.print("-G");
    Serial.print(canMsg.bColorInfoG);
    Serial.print("-B");
    Serial.print(canMsg.bColorInfoB);
    Serial.print(" LightTime: ");
    Serial.println(canMsg.bLightTime);
}

bool fallingFlg = false;
void loop() {
    canCommMsg_t canMsg = {
        0,         // CanID (rcv only)
        ulPanelId, // PanelID
        255,       // R
        0,         // G
        0,         // B
        1,         // BtnFlag
        3          // LightTime
    };
    // canMsg.ulCanId = ulPanelId;

    // CAN受信
    if (bCanIntrFlg) {
        bCanIntrFlg = false;
        uCanReceiveInfo(&canMsg);
        Serial.print("[CAN Receive] ");
        vPrintCanMsg(canMsg);
    }

    // ボタン検知：CAN送信
    if (digitalRead(PIN_PANEL_SENSOR) == LOW) {
        fallingFlg = true;
    } else if ((digitalRead(PIN_PANEL_SENSOR) == HIGH) &&
               (fallingFlg == true)) {
        canMsg.bBtnFlag = 1;
        bCanSendWrapper(canMsg, MASTER_CAN_ID);
        Serial.print("[CAN Send] ");
        vPrintCanMsg(canMsg);
        fallingFlg = false;
    }
}

/*****************************************************************************/
/**
 * GPIOの初期化
 *
 * @param	##
 *
 * @return   ##
 *
 * @note		##
 *
 ******************************************************************************/
void vInitGpio() {
    // pinMode設定
    for (unsigned int i = 0; i < COUNTOF(sPinConfig); i++) {
        pinMode(sPinConfig[i].pinNo, sPinConfig[i].ioFlag);
    }
}

/*****************************************************************************/
/**
 * Can Driver 初期化
 *
 * @param	##
 *
 * @return   ##
 *
 * @note		##
 *
 ******************************************************************************/
bool bInitCanDriver() {
    if (CAN_OK ==
        CAN.begin(CAN_500KBPS, MCP_8MHz)) // init can bus : baudrate = 500k
    {
        Serial.println("CAN BUS Shield init ok!");
        CAN.init_Mask(0, 0, 0x3FF);     // todo
        CAN.init_Filt(0, 0, ulPanelId); // todo
        attachInterrupt(CAN_INTR_NO, vCanIntrHandler, FALLING);
    } else {
        Serial.println("CAN BUS Shield init fail");
        Serial.println("Init CAN BUS Shield again");
        delay(100);
        return false;
    }
    return true;
}

/*****************************************************************************/
/**
 * CAN ID取得
 *
 * @param   ##
 *
 * @return  uint32_t: CAN ID
 *
 * @note    ##
 *
 ******************************************************************************/
uint32_t ulGetPanelCanID() {
    byte buf[5] = {};

    // GPIOから値を取得
    for (int i = 0; i < 5; i++) {
        buf[i] = digitalRead(sPinConfig[i].pinNo);
        // Serial.print(sPinConfig[i].pinNo);
        // Serial.print(" pin is ");
        // Serial.println(buf[i]);
    }
    return ((buf[0] + (buf[1] << 1) + (buf[2] << 2) + (buf[3] << 3) +
             (buf[4] << 4)) &
            0b11111);
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
void vCanIntrHandler() { bCanIntrFlg = true; }

/*****************************************************************************/
/**
 * CANを受信する。
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
byte uCanReceiveInfo(canCommMsg_t *canMsg) {
    uint8_t len = 0;
    uint8_t buf[8];
    byte bStatus;

    if (CAN_MSGAVAIL == CAN.checkReceive()) { // CAN受信チェック
        // ReadData
        bStatus = CAN.readMsgBuf(&len, buf);

        // 情報格納
        canMsg->ulCanId = CAN.getCanId();
        canMsg->bPanelId = buf[0];
        canMsg->bBtnFlag = buf[1];
        canMsg->bColorInfoR = buf[2];
        canMsg->bColorInfoG = buf[3];
        canMsg->bColorInfoB = buf[4];
        canMsg->bLightTime = buf[5];
        canMsg->bStartSwFlag = buf[6];
    } else {
        bStatus = CAN_FAIL;
    }

    return bStatus;
}

/*****************************************************************************/
/**
 * CAN メッセージ送信ラッパー
 * canCommMsg_tを入力し、ESP32（ID:0x00）へ経由でCAN送信する
 *
 * @param	canMsg：送信するCANの情報
 * @param   canID：Panel固有のCAN ID
 *
 * @return  CAN_OK / CAN err各種
 *
 * @note    ##
 *
 ******************************************************************************/
byte bCanSendWrapper(canCommMsg_t canMsg, uint32_t canId) {
    byte bStatus;
    byte buf[8] = {};

    // bufの下位バイトから情報を埋めていく
    buf[0] = ulPanelId;
    buf[1] = canMsg.bBtnFlag;
    buf[2] = canMsg.bColorInfoR;
    buf[3] = canMsg.bColorInfoG;
    buf[4] = canMsg.bColorInfoB;
    buf[5] = canMsg.bLightTime;
    buf[6] = canMsg.bStartSwFlag;

    // CAN経由でメッセージを送る
    bStatus = CAN.sendMsgBuf(canId, 0, 8, buf);
    Serial.print("Send CAN Message | ID:");
    Serial.print(canId);
    Serial.print(" buf[1]:");
    Serial.println(buf[1]);

    return bStatus;
}