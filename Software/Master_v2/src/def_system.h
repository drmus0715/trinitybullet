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
/* FreeRTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/*****************************************************************************/
/* Macro
******************************************************************************/
#define COUNTOF(array) (sizeof(array) / sizeof(array[0]))
#define SETCOLOR(r, g, b) (((uint32_t)r << 16) | ((uint32_t)g << 8) | b)

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* プレイヤー名最大長 */
#define configPLAYERNAME_LENGTH 64

/* 難易度別ブロック時間 */
#define configWAIT_EASY pdMS_TO_TICKS(1500)
#define configWAIT_NORMAL pdMS_TO_TICKS(1000)
#define configWAIT_HARD pdMS_TO_TICKS(600)
#define configWAIT_LUNATIC pdMS_TO_TICKS(300)

/* スレーブ(Panel)固定値 */
#define configPANEL_NUM 25

/*****************************************************************************/
/* TAG Definitions
******************************************************************************/
typedef BaseType_t BOOL_t;
typedef uint8_t byte;

/*
 * Player Infomarion Definitions
 * 難易度、チームのenum定義です。
 */
typedef enum DIFFICULTY
{
    DFCLT_EASY = 1,
    DFCLT_NORMAL,
    DFCLT_HARD,
    DFCLT_LUNATIC,
    MAX_DFCLT
} eDfclt_t;

typedef enum TEAM_COLOR
{
    TEAM_RED = 1,
    TEAM_GREEN,
    TEAM_BLUE,
    MAX_TEAM
} eTeamcl_t;

/* LED Color Name */
enum COLOR
{
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

/*
 * CAN Massage Definitions
 * CAN rx, tx共通メッセージ形式
 */
typedef struct CAN_MESSAGE
{
    uint32_t ulCanId; // 送信先／受信元
    byte bPanelId;    // PanelID（Panelからの返答のみに使用）
    byte bColorInfoR; // 色情報 - R
    byte bColorInfoG; // 色情報 - G
    byte bColorInfoB; // 色情報 - B
    byte bBtnFlag;    // パネル押下許可／押下フラグ
    byte bLightTime;  // LED点灯時間
    byte bStartSwFlag;
} canCommMsg_t;

/*
 * Player Infomation
 * プレイヤー情報格納形式
 */
typedef struct PLAYER_INFO
{
    BOOL_t startFlg;
    char name[configPLAYERNAME_LENGTH];
    eDfclt_t difficulty;
    eTeamcl_t team;
} playerInfo_t;

/* Game情報格納構造体 */
struct GAME_INFO
{
    eTeamcl_t team;
    eDfclt_t difficuty;
    char name[configPLAYERNAME_LENGTH];
    uint32_t redPoint;
    uint32_t bluePoint;
    uint32_t greenPoint;
    int32_t hitPoint;
    uint32_t remainingTime;
};

/* Panel enum定義 */
enum PANEL_NO
{
    PANEL_1 = 1,
    PANEL_2,
    PANEL_3,
    PANEL_4,
    PANEL_5,
    PANEL_6,
    PANEL_7,
    PANEL_8,
    PANEL_9,
    PANEL_10,
    PANEL_11,
    PANEL_12,
    PANEL_13,
    PANEL_14,
    PANEL_15,
    PANEL_16,
    PANEL_17,
    PANEL_18,
    PANEL_19,
    PANEL_20,
    PANEL_21,
    PANEL_22,
    PANEL_23,
    PANEL_24,
    PANEL_25,
    MAX_PANEL_NUM
};

/* LED Color Table 定義 */
typedef struct LED_COLOR_TBL
{
    enum COLOR eColor;
    uint32_t ulColor;
} ledColor_t;

// LED Color Table
#define MAX_BR 255
#define HALF_BR 128
#define LOW_BR 32
// const ledColor_t sColorTbl[] = {
//     {NOLIGHT, SETCOLOR(0, 0, 0)},
//     {WHITE, SETCOLOR(MAX_BR, MAX_BR, MAX_BR)},
//     {RED, SETCOLOR(MAX_BR, 0, 0)},
//     {GREEN, SETCOLOR(0, MAX_BR, 0)},
//     {BLUE, SETCOLOR(0, 0, MAX_BR)},

//     {WHITE_L, SETCOLOR(LOW_BR, LOW_BR, LOW_BR)},
//     {RED_L, SETCOLOR(LOW_BR, 0, 0)},
//     {GREEN_L, SETCOLOR(0, LOW_BR, 0)},
//     {BLUE_L, SETCOLOR(0, 0, LOW_BR)},
// };

/* Hit Point Table */
struct HIT_POINT_TBL
{
    eDfclt_t difficulty;
    int strong;
    int same;
    int weak;
};

/* Weak point Table */
struct WEAK_POINT_TBL
{
    eTeamcl_t team;
    enum COLOR strong;
    enum COLOR same;
    enum COLOR weak;
};

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/

#endif