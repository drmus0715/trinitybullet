/*****************************************************************************/
/**
 * @file ctrl_main.cpp
 * @comments ##
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Who           Date        Changes
 * ----- ------------- ----------- --------------------------------------------
 * 0.01  drmus0715     2019/09/03  First release
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
#include "freertos/queue.h"
#include "freertos/timers.h"

/* Standard Lib Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ESP-IDF Includes */
#include <sys/param.h>
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"

/* User Includes */
#include "def_system.h"
#include "drv_gamemng.h"
#include "drv_can.h"
#include "drv_dfplayer.h"
#include "drv_hpdltb.h"
#include "ctrl_main.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* FreeRTOS TaskConfigure */
#define tskstacMAIN_CTRLTASK 4096
#define tskprioCTRL_TXTASK 5
#define tskprioCTRL_RXTASK 5

/* Queue Configure */
#define QUEUE_CTRL_RX_SIZE 16
#define QUEUE_CTRL_RX_WAIT portMAX_DELAY
#define QUEUE_CTRL_RX_WAIT_NOWPLAYING pdMS_TO_TICKS(1000)

#define QUEUE_PLAYER_RX_SIZE 4
#define QUEUE_PLAYER_RX_WAIT pdMS_TO_TICKS(100) // デモLED点灯間隔

/* Timer Configure */
#define TIMER_GAMEMNG_COUNT_TICK pdMS_TO_TICKS(100) // 0.1sタイマ
#define TIMER_INIT_TIME 500                         // 60秒をカウントする(600 Count)
#define TIMER_SPEEDUP_COEFFICIENT 0.8               // スピードアップ時の係数
#define TIMER_SPEEDUP_THRESHOLD 250                 // スピードアップの時間しきい値

/* Game Configure */
#define gameconfSTART_SW_PANEL_NUM PANEL_13
#define gameconfSTART_SW_LIGHT_TIME 2  // second
#define gameconfINIT_HIT_POINT 5       // 体力上限
#define gameconfLIGHT_TIME 2           // second, ゲーム中のLED点灯時間
#define gameconfLIGHT_TIME_EASY 2      // second, ゲーム中のLED点灯時間
#define gameconfLIGHT_TIME_NORMAL 2    // second, ゲーム中のLED点灯時間
#define gameconfLIGHT_TIME_HARD 1.5    // second, ゲーム中のLED点灯時間
#define gameconfLIGHT_TIME_LUNAITC 1.5 // second, ゲーム中のLED点灯時間

/* Demo Blink Configure */
#define DEMO_HALOWEEN_MODE 0

/* ESPLOGGER Configure */
#define TAG "CtrlMain"

/*
 * Constant Table
 * [0]は空とし、enumの値と対応させる。
 */
const struct HIT_POINT_TBL csHpTbl[] = {{},
                                        {DFCLT_EASY, 0, 0, 0},
                                        {DFCLT_NORMAL, 1, 0, -1},
                                        {DFCLT_HARD, 1, 0, -2},
                                        {DFCLT_LUNATIC, 0, 0, -4}};
const struct WEAK_POINT_TBL csWeakTbl[] = {{},
                                           {TEAM_RED, GREEN, RED, BLUE},
                                           {TEAM_GREEN, BLUE, GREEN, RED},
                                           {TEAM_BLUE, RED, BLUE, GREEN}};

/*****************************************************************************/
/* TAG Definitions
******************************************************************************/

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
// Semaphore

// Task Handler
TaskHandle_t xCtrlRxTask;
TaskHandle_t xCtrlTxTask;

// Queue
QueueHandle_t xCtrlRxCanQueue = NULL;
QueueHandle_t xPlayerInfoQueue = NULL;

// Timer
TimerHandle_t xGameMngTimer;
int sulTimer;

// Flags(static)
/*
 * Timer Flag: ゲームプレイ時にpdTRUEへ
 * カウント0 or 体力0でpdFALSEに戻す
 */
static BOOL_t xTimerFlag = pdFALSE;

// for Game
static struct GAME_INFO stGameInfo;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
// Task
static void prvCtrlMainRxTask(void *pxParameters);
static void prvCtrlMainTxTask(void *pxParameters);

// Timer Handles
void prvGameTimerHandle(TimerHandle_t xTimer);

// Some Functions
void vLightOnAllPanel(uint32_t ulLightTime, TickType_t xInterval);
BOOL_t xSendStartNotifyToPanel(eTeamcl_t eTeam);
enum COLOR eDetectPanelColor(canCommMsg_t *canMsg);
int iCalcHitPoint(eTeamcl_t eTeam, eDfclt_t eDfclt, enum COLOR eColor);
void vGameStartSequence(eTeamcl_t eTeam);
void vGameFinishSequence();

/*****************************************************************************/
/* Public Function
******************************************************************************/

/*****************************************************************************/
/**
 * CtrlMain初期化
 *
 * @param    ##
 *
 * @return   pdPASS / pdFAIL
 *
 * @note     ##
 *
 ******************************************************************************/
esp_err_t lInitCtrlMainFunction()
{
    BOOL_t xStatus;

    // Create Queue
    xCtrlRxCanQueue = xQueueCreate(QUEUE_CTRL_RX_SIZE, sizeof(canCommMsg_t));
    configASSERT(xCtrlRxCanQueue);
    xPlayerInfoQueue = xQueueCreate(QUEUE_CTRL_RX_SIZE, sizeof(playerInfo_t));
    configASSERT(xPlayerInfoQueue);

    // Create Timer
    xGameMngTimer = xTimerCreate("GameMngTimer", TIMER_GAMEMNG_COUNT_TICK,
                                 pdTRUE, NULL, prvGameTimerHandle);
    configASSERT(xGameMngTimer);

    // Create Task
    xStatus = xTaskCreatePinnedToCore(
        prvCtrlMainRxTask, "prvCtrlMainRxTask", tskstacMAIN_CTRLTASK, NULL,
        tskprioCTRL_RXTASK, &xCtrlRxTask, tskNO_AFFINITY);
    configASSERT(xStatus);

    xStatus = xTaskCreatePinnedToCore(
        prvCtrlMainTxTask, "prvCtrlMainTxTask", tskstacMAIN_CTRLTASK, NULL,
        tskprioCTRL_TXTASK, &xCtrlTxTask, tskNO_AFFINITY);
    configASSERT(xStatus);

    return (xStatus == pdPASS ? ESP_OK : ESP_FAIL);
}

/*****************************************************************************/
/**
 * Player InfoをQueueへ送信
 *
 * @param    canCommMsg_t：canで送信するメッセージ
 *
 * @return   pdPASS / pdFAIL
 *
 * @note		##
 *
 ******************************************************************************/
BOOL_t xSendPlayerInfoQueue(playerInfo_t playerInfo)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xPlayerInfoQueue, &playerInfo, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/**
 * Can MsgをCtrlMain Queueへ送信
 *
 * @param    canCommMsg_t：canで送信するメッセージ
 *
 * @return   pdPASS / pdFAIL
 *
 * @note		##
 *
 ******************************************************************************/
BOOL_t xSendCtrlCanRxQueue(canCommMsg_t canRxMsg)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xCtrlRxCanQueue, &canRxMsg, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/* Private Function
******************************************************************************/

/*****************************************************************************/
/**
 * ゲームのシーケンスを作るタスク。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
static void prvCtrlMainTxTask(void *pxParameters)
{
    playerInfo_t playerInfo = {};
    canCommMsg_t canMsg = {};
    dfpCtrlMsg_t dfpMsg = {};
    hpdltb_t tHpdltb = {};
    enum COLOR eColor;
    uint32_t ulPanelIdBuff = 0;
    TickType_t xDelay = configWAIT_EASY;

    // 起動前全点灯
    vLightOnAllPanel(3, pdMS_TO_TICKS(50));
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (;;)
    {
        // スタート通知待ち
        xSendGamemngTxQueue(GMMSG_FIN_PREPARE);

        tHpdltb.eMsg = HPDLTB_BLANK;
        xSendHpdltbQueue(tHpdltb);

        while (xQueueReceive(xPlayerInfoQueue, &playerInfo, QUEUE_PLAYER_RX_WAIT) != pdPASS)
        {
            // Demo Blink
            // バッファクリア
            memset(&canMsg, 0x00, sizeof(canCommMsg_t));

            // 固定値
            canMsg.bPanelId = 0;
            canMsg.bBtnFlag = 0;
            canMsg.bStartSwFlag = 0;
            canMsg.bLightTime = 1;

            // ランダム要素
            canMsg.ulCanId = (uint32_t)random(PANEL_1, MAX_PANEL_NUM);

#if DEMO_HALOWEEN_MODE == 1
            if (random(0, 2) == 1)
            { // Orange
                canMsg.bColorInfoR = 235;
                canMsg.bColorInfoG = 97;
                canMsg.bColorInfoB = 0;
            }
            else
            { // Purple
                canMsg.bColorInfoR = 106;
                canMsg.bColorInfoG = 51;
                canMsg.bColorInfoB = 134;
            }

#else
            canMsg.bColorInfoR = (uint32_t)random(0, MAX_BR);
            canMsg.bColorInfoG = (uint32_t)random(0, MAX_BR);
            canMsg.bColorInfoB = (uint32_t)random(0, MAX_BR);
#endif

            // 送信
            xSendCanTxQueue(canMsg);
        }
        // デモ点灯の後若干待ちを入れる
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG, "Player Info | startFlg:%d name:%s diffculty:%d team:%d",
                 playerInfo.startFlg, playerInfo.name, playerInfo.difficulty,
                 playerInfo.team);

        /* ゲーム情報をコピー */
        stGameInfo.difficuty = playerInfo.difficulty;
        stGameInfo.team = playerInfo.team;
        sprintf(stGameInfo.name, playerInfo.name);

        /* 難易度別にDelayを設定する */
        switch (stGameInfo.difficuty)
        {
        case DFCLT_EASY:
            xDelay = configWAIT_EASY;
            break;
        case DFCLT_NORMAL:
            xDelay = configWAIT_NORMAL;
            break;
        case DFCLT_HARD:
            xDelay = configWAIT_HARD;
            break;
        case DFCLT_LUNATIC:
            xDelay = configWAIT_LUNATIC;
            break;
        default:
            break;
        }

        if (playerInfo.startFlg == pdTRUE)
        {
            // CANのキューをリセットしておく
            xQueueReset(xCtrlRxCanQueue);

            // スタートSWのメッセージを送信
            xSendStartNotifyToPanel(playerInfo.team);

            // GameMngにPlayerEntry表示
            xSendGamemngTxQueue(GMMSG_ENTRY);

            // Entry再生
            dfpMsg.eSound = SE_ENTRY;
            dfpMsg.uiVolume = DFPLAYER_DEFAULT_VOLUME;
            xSendDfplayerQueue(dfpMsg);

            // 押下待ち
            xQueueReceive(xCtrlRxCanQueue, &canMsg, QUEUE_CTRL_RX_WAIT);
            ESP_LOGD(TAG, "Receive CAN Msg | Panel:%d", canMsg.bPanelId);

            // CAN MsgがPANEL_13でBtnFlg==1のとき
            if (canMsg.bBtnFlag == 1 && canMsg.bPanelId == PANEL_13)
            {
                // ゲームスタート通知(GM? Web?)
                xSendGamemngTxQueue(GMMSG_GAME_START);

                /*
                 * ゲームスタートカウントダウン
                 */
                vGameStartSequence(playerInfo.team);

                // タイマ開始（ゲームスタート、カウント開始）
                xTimerFlag = pdTRUE;
                sulTimer = TIMER_INIT_TIME;
                ESP_LOGI(TAG, "Start Timer");
                xTimerReset(xGameMngTimer, portMAX_DELAY);

                // 受信タスク動作開始
                xTaskNotifyGive(xCtrlRxTask);

                // バッファクリア
                memset(&canMsg, 0x00, sizeof(canCommMsg_t));

                // ゲーム中フラグON(タイマ != 0, 体力 != 0)
                while (xTimerFlag == pdTRUE)
                {
                    // パネルランダム生成
                    do
                    {
                        ulPanelIdBuff = (uint32_t)random(PANEL_1, MAX_PANEL_NUM);
                    } while (canMsg.ulCanId == ulPanelIdBuff);

                    // バッファクリア
                    memset(&canMsg, 0x00, sizeof(canCommMsg_t));

                    canMsg.ulCanId = ulPanelIdBuff;
                    // canMsg.ulCanId = PANEL_6;

                    // 色ランダム生成
                    eColor = (enum COLOR)random(RED, BLUE + 1);
                    switch (eColor)
                    {
                    case RED:
                        canMsg.bColorInfoR = MAX_BR;
                        break;
                    case GREEN:
                        canMsg.bColorInfoG = MAX_BR;
                        break;
                    case BLUE:
                        canMsg.bColorInfoB = MAX_BR;
                        break;
                    default:
                        break;
                    }

                    // 押下許可フラグ
                    canMsg.bBtnFlag = 1;

                    // LED点灯時間
                    // 難易度別に設定する
                    switch (stGameInfo.difficuty)
                    {
                    case DFCLT_EASY:
                        canMsg.bLightTime = gameconfLIGHT_TIME_EASY;
                        break;
                    case DFCLT_NORMAL:
                        canMsg.bLightTime = gameconfLIGHT_TIME_NORMAL;
                        break;
                    case DFCLT_HARD:
                        canMsg.bLightTime = gameconfLIGHT_TIME_HARD;
                        break;
                    case DFCLT_LUNATIC:
                        canMsg.bLightTime = gameconfLIGHT_TIME_LUNAITC;
                        break;
                    default:
                        break;
                    }

                    // 残り時間がn秒になったらゲームスピードを早くする
                    if (sulTimer < TIMER_SPEEDUP_THRESHOLD)
                    {
                        /* 難易度別にDelayを設定する */
                        switch (stGameInfo.difficuty)
                        {
                        case DFCLT_EASY:
                            xDelay = configWAIT_EASY * TIMER_SPEEDUP_COEFFICIENT;
                            break;
                        case DFCLT_NORMAL:
                            xDelay = configWAIT_NORMAL * TIMER_SPEEDUP_COEFFICIENT;
                            break;
                        case DFCLT_HARD:
                            xDelay = configWAIT_HARD * TIMER_SPEEDUP_COEFFICIENT;
                            break;
                        case DFCLT_LUNATIC:
                            // Lunaticはソフランなし
                            // xDelay = configWAIT_LUNATIC * TIMER_SPEEDUP_COEFFICIENT;
                            break;
                        default:
                            break;
                        }
                    }

                    // CAN送信
                    if (xSendCanTxQueue(canMsg) != pdPASS)
                    {
                        ESP_LOGE(TAG, "Failed Send CanMsg panelNo:%d",
                                 canMsg.ulCanId);
                    }

                    // Wait(難易度で変化する)
                    vTaskDelay(xDelay);
                }
            }
        }

        // タイマ終了

        // 終了シーケンス
        vGameFinishSequence();

        // スコア格納待ち
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 結果をWebserverへ送信(踏んだパネル数、プレイヤー情報、残り時間※!=0でゲームオーバー)
        xSendHttpPostQueue(stGameInfo);
        xSendGamemngTxQueue(GMMSG_SCORE);

        // ゲーム中に来たゲームスタート通知を削除する
        xQueueReset(xPlayerInfoQueue);

        // ゲーム終了後
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*****************************************************************************/
/**
 * CANのメッセージを受け取って計算するタスク。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
static void prvCtrlMainRxTask(void *pxParameters)
{
    canCommMsg_t canMsg = {};
    dfpCtrlMsg_t dfpMsg;
    enum COLOR eColor;
    BOOL_t xHp1SeFlg = pdFALSE;
    BOOL_t xStatus;

    for (;;)
    {
        // ゲームスタート待ち
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "(RxTask)Game Start");

        // スコアをリセット
        stGameInfo.redPoint = 0;
        stGameInfo.bluePoint = 0;
        stGameInfo.greenPoint = 0;
        stGameInfo.remainingTime = 0;
        stGameInfo.hitPoint = gameconfINIT_HIT_POINT;

        // フラグを0に
        xHp1SeFlg = pdFALSE;

        // hpdltb_t tHpdltb;

        // tHpdltb.bHp = (uint8_t)stGameInfo.hitPoint;
        // tHpdltb.bTimeH = (uint8_t)sulTimer;
        // tHpdltb.bTimeL = 0;
        // xSendHpdltbQueue(tHpdltb);

        // ゲームスタート
        while (xTimerFlag == pdTRUE)
        {
            // CAN受信待ち
            if (xQueueReceive(xCtrlRxCanQueue, &canMsg,
                              QUEUE_CTRL_RX_WAIT_NOWPLAYING) == pdTRUE)
            {
                ESP_LOGI(TAG, "Receive CAN Msg | Panel:%d", canMsg.bPanelId);

                // 押されたときの音を再生
                /* ダメージ音を付けたいので体力計算のときに鳴らす */
                // dfpMsg.eSound = SE_PANEL;
                // dfpMsg.uiVolume = 10;
                // xSendDfplayerQueue(dfpMsg);

                // 色判別
                eColor = eDetectPanelColor(&canMsg);
                switch (eColor)
                {
                case RED:
                    stGameInfo.redPoint++;
                    break;
                case GREEN:
                    stGameInfo.greenPoint++;
                    break;
                case BLUE:
                    stGameInfo.bluePoint++;
                    break;
                default:
                    ESP_LOGI(TAG, "(RxTask) Failed Color undetected panel:%d",
                             canMsg.bPanelId);
                    break;
                }

                // 体力計算
                stGameInfo.hitPoint += iCalcHitPoint(
                    stGameInfo.team, stGameInfo.difficuty, eColor);

                /* 体力が5より多くなったら5に戻す */
                if (stGameInfo.hitPoint > gameconfINIT_HIT_POINT)
                {
                    stGameInfo.hitPoint = gameconfINIT_HIT_POINT;
                }

                // もし体力が0ならタイマフラグをpdFALSEへ
                if (stGameInfo.hitPoint <= 0)
                {
                    xTimerFlag = pdFALSE;
                }

                // もし体力が1なら音声を鳴らす
                if (stGameInfo.hitPoint == 1 && xHp1SeFlg == pdFALSE)
                {
                    dfpMsg.eSound = SE_PINCH;
                    xSendDfplayerQueue(dfpMsg);
                    xHp1SeFlg = pdTRUE;
                }
                else if (stGameInfo.hitPoint != 1 && xHp1SeFlg == pdTRUE)
                {
                    xHp1SeFlg = pdFALSE;
                }

                ESP_LOGI(TAG, "(RxTask) stGameInfo={point R%dG%dB%d, hp%d}",
                         stGameInfo.redPoint, stGameInfo.greenPoint,
                         stGameInfo.bluePoint, stGameInfo.hitPoint);
            }
        }
        // 残り時間を格納
        stGameInfo.remainingTime = sulTimer;
        if (sulTimer < 0)
            ESP_LOGE(TAG, "(RxTask) Failed Timer count is 0");

        // CtrlTxTaskへスコア確定を通知
        xStatus = xTaskNotifyGive(xCtrlTxTask);
        if (xStatus != pdPASS)
        {
            ESP_LOGE(TAG, "Failed xTaskNotifyGive(xCtrlTxTask)");
        }
    }
}

/*****************************************************************************/
/**
 * ゲームのカウント毎に呼び出される関数です。
 * カウント変数から1ずつ減算します。
 * 0になったらカウントフラグをpdFALSEにします。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void prvGameTimerHandle(TimerHandle_t xTimer)
{
    hpdltb_t tHpdltb;
    sulTimer--;
    ESP_LOGD(TAG, "Timer Count:%d", sulTimer);

    // todo: 時間送信
    tHpdltb.eMsg = HPDLTB_NORMAL;
    tHpdltb.bHp = (uint8_t)stGameInfo.hitPoint;
    tHpdltb.bTimeH = (uint8_t)(sulTimer / 10);
    tHpdltb.bTimeL = (uint8_t)(sulTimer - ((int)(sulTimer / 10) * 10));
    xSendHpdltbQueue(tHpdltb);

    if (sulTimer == 0)
    { // カウント０
        ESP_LOGI(TAG, "Stop Timer (Time up)");
        xTimerFlag = pdFALSE;
        xTimerStop(xTimer, portMAX_DELAY);
    }
    else if (sulTimer > 0 && xTimerFlag == pdFALSE)
    { // 体力切れ
        ESP_LOGI(TAG, "Stop Timer (Game over)");
        xTimerStop(xTimer, portMAX_DELAY);
    }
    else if (sulTimer < 0)
    {
        ESP_LOGE(TAG, "Timer Count Error sulTimer:%d", sulTimer);
    }
}

/*****************************************************************************/
/**
 * 起動時に一回すべてのパネルを点灯させます。
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vLightOnAllPanel(uint32_t ulLightTime, TickType_t xInterval)
{
    canCommMsg_t canMsg = {};

    for (int i = PANEL_1; i < MAX_PANEL_NUM; i++)
    {
        // canMsgへ情報を格納
        canMsg.ulCanId = i;
        canMsg.bPanelId = 0x00; //別になんでもよい
        canMsg.bBtnFlag = 0;    // StartSwFlagとの切り分け
        canMsg.bColorInfoR = MAX_BR;
        canMsg.bColorInfoG = MAX_BR;
        canMsg.bColorInfoB = MAX_BR;
        canMsg.bLightTime = (byte)ulLightTime;
        canMsg.bStartSwFlag = 0;

        xSendCanTxQueue(canMsg);
        vTaskDelay(xInterval);
    }
}

/*****************************************************************************/
/**
 * GMからのプレイヤー情報受け取り後に、PANEL_13へスタート通知を送信します
 * その後、PANEL_13はスタートの動作を行うはず…です。
 *
 * @param	pvParametersはNULLです。
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
BOOL_t xSendStartNotifyToPanel(eTeamcl_t eTeam)
{
    canCommMsg_t canMsg;

    // canMsgへ情報を格納
    canMsg.ulCanId = gameconfSTART_SW_PANEL_NUM;
    canMsg.bPanelId = 0x00; //別になんでもよい
    canMsg.bBtnFlag = 0;    // StartSwFlagとの切り分け
    canMsg.bColorInfoR = HALF_BR;
    canMsg.bColorInfoG = HALF_BR;
    canMsg.bColorInfoB = HALF_BR;
    canMsg.bLightTime = gameconfSTART_SW_LIGHT_TIME;
    canMsg.bStartSwFlag = 1;

    // チーム色を格納する
    switch (eTeam)
    {
    case TEAM_RED:
        canMsg.bColorInfoR = MAX_BR;
        break;
    case TEAM_GREEN:
        canMsg.bColorInfoG = MAX_BR;
        break;
    case TEAM_BLUE:
        canMsg.bColorInfoB = MAX_BR;
        break;

    default:
        break;
    }

    // 送信
    return xSendCanTxQueue(canMsg);
}

/*****************************************************************************/
/**
 * ゲームスタートシーケンス！
 * かっこよくするぞ～～～～
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vGameStartSequence(eTeamcl_t eTeam)
{
    hpdltb_t tHpdltb = {};
    dfpCtrlMsg_t tDfpMsg = {};
    canCommMsg_t tCanMsg{};
    byte bColorR = HALF_BR;
    byte bColorG = HALF_BR;
    byte bColorB = HALF_BR;
    const uint32_t culCDPanel1[8] = {PANEL_7, PANEL_8, PANEL_9, PANEL_12, PANEL_14, PANEL_17, PANEL_18, PANEL_19};
    const uint32_t culCDPanel2[15] = {PANEL_1, PANEL_2, PANEL_3, PANEL_4, PANEL_5, PANEL_6,
                                      PANEL_10, PANEL_11, PANEL_16, PANEL_20,
                                      PANEL_21, PANEL_22, PANEL_23, PANEL_24, PANEL_25};
    tDfpMsg.uiVolume = DFPLAYER_DEFAULT_VOLUME;
    tHpdltb.eMsg = HPDLTB_COUNT;

    // チーム色を格納
    switch (eTeam)
    {
    case TEAM_RED:
        bColorR = MAX_BR;
        break;
    case TEAM_GREEN:
        bColorG = MAX_BR;
        break;
    case TEAM_BLUE:
        bColorB = MAX_BR;
        break;
    default:
        break;
    }

    // Seq.1 SE:Countdown1
    tDfpMsg.eSound = SE_COUNTDOWN_1;
    xSendDfplayerQueue(tDfpMsg);
    tHpdltb.bTimeL = 0x03; // 3表示
    xSendHpdltbQueue(tHpdltb);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Seq.2 パネル中枠点灯, SE:Countdown1
    tDfpMsg.eSound = SE_COUNTDOWN_1;
    xSendDfplayerQueue(tDfpMsg);
    tHpdltb.bTimeL = 0x02; // 2表示
    xSendHpdltbQueue(tHpdltb);
    for (int i = 0; i < 8; i++)
    {
        tCanMsg.bBtnFlag = 0;
        tCanMsg.bColorInfoR = bColorR;
        tCanMsg.bColorInfoG = bColorG;
        tCanMsg.bColorInfoB = bColorB;
        tCanMsg.bLightTime = 4;
        tCanMsg.bPanelId = 0;
        tCanMsg.bStartSwFlag = 0;
        tCanMsg.ulCanId = culCDPanel1[i];
        xSendCanTxQueue(tCanMsg);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Seq.3 パネル中枠点灯, SE:Countdown1
    tDfpMsg.eSound = SE_COUNTDOWN_1;
    xSendDfplayerQueue(tDfpMsg);
    tHpdltb.bTimeL = 0x01; // 1表示
    xSendHpdltbQueue(tHpdltb);
    for (int i = 0; i < 15; i++)
    {
        tCanMsg.bBtnFlag = 0;
        tCanMsg.bColorInfoR = bColorR;
        tCanMsg.bColorInfoG = bColorG;
        tCanMsg.bColorInfoB = bColorB;
        tCanMsg.bLightTime = 3;
        tCanMsg.bPanelId = 0;
        tCanMsg.bStartSwFlag = 0;
        tCanMsg.ulCanId = culCDPanel2[i];
        xSendCanTxQueue(tCanMsg);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Seq.4 パネル中枠点灯, SE:Countdown2
    tDfpMsg.eSound = SE_COUNTDOWN_2;
    xSendDfplayerQueue(tDfpMsg);
    tHpdltb.bTimeL = 0xFF; // GO表示
    xSendHpdltbQueue(tHpdltb);
    for (int i = PANEL_1; i < MAX_PANEL_NUM; i++)
    {
        tCanMsg.bBtnFlag = 0;
        tCanMsg.bColorInfoR = 0;
        tCanMsg.bColorInfoG = 0;
        tCanMsg.bColorInfoB = 0;
        tCanMsg.bLightTime = 0;
        tCanMsg.bPanelId = 0;
        tCanMsg.bStartSwFlag = 0;
        tCanMsg.ulCanId = i;
        xSendCanTxQueue(tCanMsg);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
}

/*****************************************************************************/
/**
 * Finishシーケンス
 *
 * @param	##
 *
 * @return  ##
 *
 * @note    ##
 *
 ******************************************************************************/
void vGameFinishSequence()
{
    hpdltb_t tHpdltb = {};
    dfpCtrlMsg_t tDfpMsg = {};
    canCommMsg_t tCanMsg{};
    tDfpMsg.uiVolume = DFPLAYER_DEFAULT_VOLUME;
    tHpdltb.eMsg = HPDLTB_FINISH;

    // Seq.1 SE:Finish
    tDfpMsg.eSound = SE_FINISH;
    xSendDfplayerQueue(tDfpMsg);
    xSendHpdltbQueue(tHpdltb);

    vTaskDelay(pdMS_TO_TICKS(2000));

    for (int i = 0; i < MAX_PANEL_NUM; i++)
    {
        tCanMsg.bBtnFlag = 0;
        tCanMsg.bColorInfoR = MAX_BR;
        tCanMsg.bColorInfoG = MAX_BR;
        tCanMsg.bColorInfoB = MAX_BR;
        tCanMsg.bLightTime = 2;
        tCanMsg.bPanelId = 0;
        tCanMsg.bStartSwFlag = 0;
        tCanMsg.ulCanId = i;
        xSendCanTxQueue(tCanMsg);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*****************************************************************************/
/**
 * 色の判別
 * 返ってきたCanMSGが何色のやつかを判別します
 *
 * @param	canCommMsg_t
 *
 * @return  enum COLOR
 *
 * @note    ##
 *
 ******************************************************************************/
enum COLOR eDetectPanelColor(canCommMsg_t *canMsg)
{
    if (canMsg->bColorInfoR > 0 && canMsg->bColorInfoG == 0 &&
        canMsg->bColorInfoB == 0)
    {
        return RED;
    }
    else if (canMsg->bColorInfoR == 0 && canMsg->bColorInfoG > 0 &&
             canMsg->bColorInfoB == 0)
    {
        return GREEN;
    }
    else if (canMsg->bColorInfoR == 0 && canMsg->bColorInfoG == 0 &&
             canMsg->bColorInfoB > 0)
    {
        return BLUE;
    }
    // ここまで来たらエラー
    ESP_LOGE(TAG, "Failed Color undetected panel:%d", canMsg->bPanelId);
    return NOLIGHT;
}

/*****************************************************************************/
/**
 * 体力計算
 * 踏んだ色＋チーム色＋難易度から、体力の増減を返します。
 *
 * @param	enum COLOR eColor
 * @param   eTeamcl_t eTeam
 * @param   eDfclt_t eDfclt
 *
 * @return  int     戻り値を現在の体力に足せばいいと思います。
 *
 * @note    ##
 *
 ******************************************************************************/
int iCalcHitPoint(eTeamcl_t eTeam, eDfclt_t eDfclt, enum COLOR eColor)
{
    dfpCtrlMsg_t dfpMsg = {};
    // // 所属チームの弱点をコピー
    // enum COLOR strong, same, weak;
    // strong = csWeakTbl[eTeam].strong

    // switch (eColor) {
    // case csWeakTbl[eTeam].strong:
    //     return csHpTbl[eDfclt].strong;
    //     break;
    // case :

    //     break;
    // case csWeakTbl[eTeam].weak:
    //     break;
    // default:
    //     return 0;
    // }

    if (eColor == csWeakTbl[eTeam].strong)
    {
        dfpMsg.eSound = SE_PANEL;
        xSendDfplayerQueue(dfpMsg);
        return csHpTbl[eDfclt].strong;
    }
    else if (eColor == csWeakTbl[eTeam].same)
    {
        dfpMsg.eSound = SE_PANEL;
        xSendDfplayerQueue(dfpMsg);
        return csHpTbl[eDfclt].same;
    }
    else if (eColor == csWeakTbl[eTeam].weak)
    {
        dfpMsg.eSound = SE_DAMAGE;
        xSendDfplayerQueue(dfpMsg);
        return csHpTbl[eDfclt].weak;
    }
    return 0;
}