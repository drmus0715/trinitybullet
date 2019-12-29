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
#define DEBUG_EN 1
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
#define LIGHT_TIME_CONV_NUM 1000 // Sec -> miliSec

// Serial LED
#define LED_COUNT 4
#define SERIAL_LED_SEND_INTERVAL_MS 100

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
// CAN
MCP_CAN CAN(SPI_CS_PIN); // Set CS to pin 10
uint32_t ulPanelId;
bool bCanIntrFlg = false;
canCommMsg_t canMsg; // 受送信するCAN Msg(割り込み対策)

// Serial LED
Adafruit_NeoPixel strip(LED_COUNT, PIN_SERIAL_LED, NEO_GRB + NEO_KHZ800);
ledInfo_t tLedinfo;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
void vInitGpio();           // GPIO Set Mode
uint32_t ulGetPanelCanID(); // CAN ID取得

/* CAN */
bool bInitCanDriver(uint32_t ulPanelId); // Init
// CAN Intr Function
void vCanIntrHandler();
// CAN Tx Wrapper
byte bCanSendWrapper(canCommMsg_t canMsg, uint32_t canId);
// CAN Rx
byte uCanReceiveInfo(canCommMsg_t *canMsg);
void vClearCanRxBuffer();

/* Serial LED */
void vSerialLedIntrHandler();
void vSerialLedLightUp(uint32_t ulColor);
void vConvCanMsg2LedInfo(canCommMsg_t *canMsg, ledInfo_t *ledinfo);

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
void setup()
{
    // UART Setup (For Debug)
    Serial.begin(115200);
    Serial.println("Hello, world! i'm Panel!");

    // GPIO Configure
    vInitGpio();

    // CANIDを変数に格納する
    ulPanelId = ulGetPanelCanID();
    Serial.print("This Panel ID : ");
    Serial.println(ulPanelId);

    // Init Function
    bInitCanDriver(ulPanelId);

    // Serial Led Send Timer
    MsTimer2::set(SERIAL_LED_SEND_INTERVAL_MS, vSerialLedIntrHandler);
}

void loop()
{
    bool bPanelPushFlg = false;
    bool bTimeoutFlg = false;

    /* CAN Msg 待機状態 */
    vClearCanRxBuffer(); // CAN_Rx Buffer Clear
    bCanIntrFlg = false; // CAN_Rx Flag Clear
    Serial.println("Wait CAN Message");
    while (bCanIntrFlg != true)
    {
        if (digitalRead(PIN_PANEL_SENSOR) != HIGH)
        {
            // LED点灯
            vSerialLedLightUp(sColorTbl[WHITE_L].ulColor);
            /*
             * 踏んでる時にMasterからCANが来たときの対策
             * 踏まれているときはCANのメッセージをリセットし続ける
             * 念の為Delayをおいて見る、ちょっと反応悪くなるかも
             */
            vClearCanRxBuffer(); // CAN_Rx Buffer Clear
            memset(&canMsg, 0x00, sizeof(canCommMsg_t));
            // delay(50);
        }
        else
        {
            // LED消灯
            vSerialLedLightUp(sColorTbl[NOLIGHT].ulColor);
        }
    }

    /* CAN受信 */
    bCanIntrFlg = false;
    uint32_t ulCanTimeOutCnt = CAN_RX_TIMEOUT_CNT;
    while (uCanReceiveInfo(&canMsg) != CAN_OK)
    {
        if (ulCanTimeOutCnt == 0)
        {
            Serial.println("[Error] CAN Receive Timeout!");
            break;
        }
        ulCanTimeOutCnt--;
    }

    /* LED情報セット */
    vConvCanMsg2LedInfo(&canMsg, &tLedinfo);

    /* パネル踏み待機 */
    if (canMsg.bBtnFlag == 1)
    { // BtnFlgが立っているときのみ
        Serial.println("Wait Push or Timeout");
        MsTimer2::start();
        while (1)
        {
            if (digitalRead(PIN_PANEL_SENSOR) != HIGH)
            {
                MsTimer2::stop(); // LEDへの送信／時間減算／LEDフェードを停止
                bPanelPushFlg = true;
                Serial.println("[Notice] Push Panel Sensor!");
                break;
            }
            if (tLedinfo.lOffTimeMs <= 0)
            {
                bTimeoutFlg = true;
                Serial.println("[Notice] Timeout!");
                delay(10);
                break;
            }
        }
        // LEDを消灯
        vSerialLedLightUp(sColorTbl[NOLIGHT].ulColor);

        /* パネルが踏まれたらCAN Msgを発⾏ */
        if (bPanelPushFlg)
        {
            Serial.println("Send CAN Msg");
            bCanSendWrapper(canMsg, MASTER_CAN_ID);
        }
    }
    else if (canMsg.bStartSwFlag == 1)
    {
        Serial.println("[StartSw]Wait Push");
        MsTimer2::start();
        /*
         * Start Sw
         * 受け取ったら点滅ループに入る
         * 点灯時間(lOffTimeMs)が0になったらタイマを更新
         * 押されたらタイマストップ、CAN Msgを送り返す（BtnFlagをつける）
         */
        while (1)
        {
            if (digitalRead(PIN_PANEL_SENSOR) != HIGH)
            {
                MsTimer2::stop(); // LEDへの送信／時間減算／LEDフェードを停止
                bPanelPushFlg = true;
                canMsg.bBtnFlag = 1;
                Serial.println("[Notice] Push Panel Sensor!");
                break;
            }
            if (tLedinfo.lOffTimeMs <= 0)
            {
                /* LED情報再セット */
                vConvCanMsg2LedInfo(&canMsg, &tLedinfo);
                MsTimer2::start();
            }
        }
        // LEDを消灯
        vSerialLedLightUp(sColorTbl[NOLIGHT].ulColor);

        /* パネルが踏まれたらCAN Msgを発⾏ */
        if (bPanelPushFlg)
        {
            Serial.println("Send CAN Msg");
            bCanSendWrapper(canMsg, MASTER_CAN_ID);
        }
    }
    else if (canMsg.bBtnFlag == 0 && canMsg.bStartSwFlag == 0) // デモ点灯用
    {
        MsTimer2::start();
        while (1)
        {
            // Serial.println(tLedinfo.lOffTimeMs);
            delay(10);
            if (tLedinfo.lOffTimeMs <= 0)
            {
                bTimeoutFlg = true;
                Serial.println("[Notice] Demo Timecount is 0");
                delay(10);
                break;
            }
        }
        // LEDを消灯
        vSerialLedLightUp(sColorTbl[NOLIGHT].ulColor);
    }
    else
    {
        /* NOP */
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
void vInitGpio()
{
    // pinMode設定
    for (unsigned int i = 0; i < COUNTOF(sPinConfig); i++)
    {
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
bool bInitCanDriver(uint32_t ulPanelId)
{
    if (CAN_OK ==
        CAN.begin(CAN_500KBPS, MCP_8MHz)) // init can bus : baudrate = 500k
    {
        Serial.println("CAN BUS Shield init ok!");
        CAN.init_Mask(0, 0, 0x3FF);     // todo
        CAN.init_Mask(1, 0, 0x3FF);     // todo
        CAN.init_Filt(0, 0, ulPanelId); // todo
        attachInterrupt(CAN_INTR_NO, vCanIntrHandler, FALLING);
    }
    else
    {
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
uint32_t ulGetPanelCanID()
{
    byte buf[5] = {};

    // GPIOから値を取得
    for (int i = 0; i < 5; i++)
    {
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

void vPrintCanMsg(canCommMsg_t canMsg)
{
    Serial.print("CanID: ");
    Serial.print(canMsg.ulCanId);
    Serial.print(" PanelId: ");
    Serial.print(canMsg.bPanelId);
    Serial.print(" BtnFlg: ");
    Serial.print(canMsg.bBtnFlag);
    Serial.print(" StartSwFlg: ");
    Serial.print(canMsg.bStartSwFlag);
    Serial.print(" ColorInf: R");
    Serial.print(canMsg.bColorInfoR);
    Serial.print("-G");
    Serial.print(canMsg.bColorInfoG);
    Serial.print("-B");
    Serial.print(canMsg.bColorInfoB);
    Serial.print(" LightTime: ");
    Serial.println(canMsg.bLightTime);
}
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
byte uCanReceiveInfo(canCommMsg_t *canMsg)
{
    byte len = 8;
    byte buf[8];
    byte bStatus;

    if (CAN_MSGAVAIL == CAN.checkReceive())
    { // CAN受信チェック
        // ReadData
        bStatus = CAN.readMsgBuf(&len, buf);

        Serial.print("[CAN rcv] len:");
        Serial.println(len);
        for (int i = 0; i < len; i++)
        {
            Serial.print(buf[i]);
            Serial.print(" ");
        }

        // 情報格納
        canMsg->ulCanId = CAN.getCanId();
        canMsg->bPanelId = buf[0];
        canMsg->bBtnFlag = buf[1];
        canMsg->bColorInfoR = buf[2];
        canMsg->bColorInfoG = buf[3];
        canMsg->bColorInfoB = buf[4];
        canMsg->bLightTime = buf[5];
        canMsg->bStartSwFlag = buf[6];
        vPrintCanMsg(*canMsg);
    }
    else
    {
        bStatus = CAN_FAIL;
    }

    return bStatus;
}

/*****************************************************************************/
/**
 * CANの受信バッファをクリアする
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vClearCanRxBuffer()
{
    byte len = 8;
    byte buf[8];

    while (CAN_MSGAVAIL == CAN.checkReceive())
    { // CAN受信チェック
        // ReadData
        CAN.readMsgBuf(&len, buf);

        Serial.print("[CAN Clr] len:");
        Serial.println(len);
    }
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
byte bCanSendWrapper(canCommMsg_t canMsg, uint32_t canId)
{
    byte bStatus;
    byte buf[8] = {};

    // bufの下位バイトから情報を埋めていく
    buf[0] = ulPanelId;
    buf[1] = canMsg.bBtnFlag;
    buf[2] = canMsg.bColorInfoR;
    buf[3] = canMsg.bColorInfoG;
    buf[4] = canMsg.bColorInfoB;
    // buf[5] = canMsg.bLightTime;
    buf[6] = canMsg.bStartSwFlag;

    // CAN経由でメッセージを送る
    bStatus = CAN.sendMsgBuf(canId, 0, 8, buf);
    Serial.print("Send CAN Message | ID:");
    Serial.print(canId);
    Serial.print(" buf[1]:");
    Serial.println(buf[1]);

    return bStatus;
}

/*****************************************************************************/
/**
 * Can MessageとLED Info変換
 *
 * @param	canCommMsg_t *canMsg
 * @param   ledInfo_t *ledinfo
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vConvCanMsg2LedInfo(canCommMsg_t *canMsg, ledInfo_t *ledinfo)
{
    ledinfo->ulColor =
        SETCOLOR(canMsg->bColorInfoR, canMsg->bColorInfoG, canMsg->bColorInfoB);
    ledinfo->lOffTimeMs = LIGHT_TIME_CONV_NUM * canMsg->bLightTime;
    ledinfo->bColorInfoR = canMsg->bColorInfoR;
    ledinfo->bColorInfoG = canMsg->bColorInfoG;
    ledinfo->bColorInfoB = canMsg->bColorInfoB;
}

/*****************************************************************************/
/**
 * Serial LEDを点灯
 *
 * @param	uint32_t (NeoPixcel *.Color クラス)
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vSerialLedLightUp(uint32_t ulColor)
{
    // LED色セット
    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
        strip.setPixelColor(i, ulColor);
    }
    // LED点灯
    strip.show();
    // Serial.print("[SerialLED] Send show Cmd: ");
    // Serial.println(tLedinfo.ulColor);
}

/*****************************************************************************/
/**
 * Serial LEDのタイマ割り込み関数
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    実行毎に以下のパラメータを変更します。
 *          tLedinfo.lOffTimeMs: (OffTimeMs / LED_SEND_INTERVAL_MS)ずつ減算
 *          tLedinfo.ulColor   : (格納された情報(r,g,b) /
 *消灯時間)ずつ減算※未実装
 *
 ******************************************************************************/
void vSerialLedIntrHandler()
{
    interrupts();
    // Serial.print("[SerialLED] Timer Handle Color:");
    // Serial.print(tLedinfo.ulColor);
    // Serial.print(" OffTimeMs:");
    // Serial.println(tLedinfo.lOffTimeMs);

    // 消灯時間が0でなければ
    if (tLedinfo.lOffTimeMs > 0)
    {

        // LED点灯
        vSerialLedLightUp(SETCOLOR(tLedinfo.bColorInfoR, tLedinfo.bColorInfoG,
                                   tLedinfo.bColorInfoB));

        // LED光量計算
        if (tLedinfo.bColorInfoR > 0)
            tLedinfo.bColorInfoR -=
                (canMsg.bColorInfoR /
                 ((LIGHT_TIME_CONV_NUM / SERIAL_LED_SEND_INTERVAL_MS) *
                  canMsg.bLightTime));
        if (tLedinfo.bColorInfoG > 0)
            tLedinfo.bColorInfoG -=
                (canMsg.bColorInfoG /
                 ((LIGHT_TIME_CONV_NUM / SERIAL_LED_SEND_INTERVAL_MS) *
                  canMsg.bLightTime));
        if (tLedinfo.bColorInfoB > 0)
            tLedinfo.bColorInfoB -=
                (canMsg.bColorInfoB /
                 ((LIGHT_TIME_CONV_NUM / SERIAL_LED_SEND_INTERVAL_MS) *
                  canMsg.bLightTime));
    }
    else if (tLedinfo.lOffTimeMs <= 0)
    {
        // タイマをストップ
        MsTimer2::stop();
        vSerialLedLightUp(sColorTbl[NOLIGHT].ulColor);
        Serial.println("[Timer] Stop Timer.");
    }
    // 消灯時間を呼び出し毎に減算
    tLedinfo.lOffTimeMs -= SERIAL_LED_SEND_INTERVAL_MS;
}