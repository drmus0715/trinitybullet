/*****************************************************************************/
/**
 * @file def_system.h
 * @comments System定義／各種設定
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Who           Date        Changes
 * ----- ------------- ----------- --------------------------------------------
 * 0.00  drmus0715     2019/08/14  First release
 *
 ******************************************************************************/
#ifndef SRC_DEF_SYSTEM_H
#define SRC_DEF_SYSTEM_H

/*****************************************************************************/
/* Include Files
******************************************************************************/
#include <Arduino.h>

/*****************************************************************************/
/* Macro
******************************************************************************/
#define COUNTOF(array) (sizeof(array) / sizeof(array[0]))

/*****************************************************************************/
/* TAG Definitions
******************************************************************************/

/*
 * CAN Massage Definitions
 * CAN rx, tx共通メッセージ形式
 */
typedef struct CAN_MESSAGE {
    uint32_t ulCanId;
    byte bPanelId;    // PanelID
    byte bColorInfoR; // 色情報 - R
    byte bColorInfoG; // 色情報 - G
    byte bColorInfoB; // 色情報 - B
    byte bBtnFlag;    // パネル押下許可／押下フラグ
    byte bLightTime;  // LED点灯時間
    byte bStartSwFlag;
} canCommMsg_t;

/*
 * Serial LED 情報格納構造体
 */
typedef struct SERIAL_LED_INFO {
    uint32_t ulOffTime; //消灯時間
    uint32_t ulColor;   //色情報（*.Colorクラス）
} ledInfo_t;

/*
 * Pin設定構造体定義
 */
typedef struct PIN_CONF {
    uint8_t pinNo;
    uint8_t ioFlag;
} pinConf_t;

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
// GPIO ピン定義
#define PIN_PANEL_SENSOR 9
#define PIN_SERIAL_LED 6

// GPIO ピン設定 ※順番変更禁止！！！
const pinConf_t sPinConfig[] = {
    {PIN_A0, INPUT},          {PIN_A1, INPUT}, {PIN_A2, INPUT},
    {PIN_A3, INPUT},          {PIN_A4, INPUT}, {PIN_PANEL_SENSOR, INPUT},
    {PIN_SERIAL_LED, OUTPUT},
};

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/

#endif