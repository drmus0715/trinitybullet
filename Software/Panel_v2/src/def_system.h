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
#include <Adafruit_NeoPixel.h>

/*****************************************************************************/
/* Macro
******************************************************************************/
#define COUNTOF(array) (sizeof(array) / sizeof(array[0]))
#define SETCOLOR(r, g, b) (((uint32_t)r << 16) | ((uint32_t)g << 8) | b)

/*****************************************************************************/
/* TAG Definitions
******************************************************************************/

/********************************** enum *************************************/
/* LED Color Name */
enum COLOR {
    NOLIGHT = 0,
    // MAX輝度
    WHITE,
    RED,
    GREEN,
    BLUE,
    // 低輝度
    WHITE_L,
    RED_L,
    GREEN_L,
    BLUE_L,

    MAX_COLOR
};

/********************************** struct ***********************************/
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

/* LED Color Table 定義 */
typedef struct LED_COLOR_TBL {
    enum COLOR eColor;
    uint32_t ulColor;
} ledColor_t;

/*
 * Serial LED 情報格納構造体
 */
typedef struct SERIAL_LED_INFO {
    int32_t lOffTimeMs; //消灯時間: LEDの割り込みによって変わる？
    uint32_t ulColor;   //色情報（*.Colorクラス）
    byte bColorInfoR;   // 色情報 - R
    byte bColorInfoG;   // 色情報 - G
    byte bColorInfoB;   // 色情報 - B
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
// CAN
#define MASTER_CAN_ID 0x00

// GPIO ピン定義
#define PIN_PANEL_SENSOR 2
#define PIN_SERIAL_LED 6

// GPIO ピン設定 ※順番変更禁止！！！
const pinConf_t sPinConfig[] = {
    {PIN_A0, INPUT},          {PIN_A1, INPUT}, {PIN_A2, INPUT},
    {PIN_A3, INPUT},          {PIN_A4, INPUT}, {PIN_PANEL_SENSOR, INPUT},
    {PIN_SERIAL_LED, OUTPUT},
};

// LED Color Table
#define MAX_BR 255
#define LOW_BR 32
const ledColor_t sColorTbl[] = {
    {NOLIGHT, SETCOLOR(0, 0, 0)},
    {WHITE, SETCOLOR(MAX_BR, MAX_BR, MAX_BR)},
    {RED, SETCOLOR(MAX_BR, 0, 0)},
    {GREEN, SETCOLOR(0, MAX_BR, 0)},
    {BLUE, SETCOLOR(0, 0, MAX_BR)},

    {WHITE_L, SETCOLOR(LOW_BR, LOW_BR, LOW_BR)},
    {RED_L, SETCOLOR(LOW_BR, 0, 0)},
    {GREEN_L, SETCOLOR(0, LOW_BR, 0)},
    {BLUE_L, SETCOLOR(0, 0, LOW_BR)},
};

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/

#endif