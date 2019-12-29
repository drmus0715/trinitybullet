/*****************************************************************************/
/**
* @file drv_dfplayer.h
* @comments DFPlayerコマンド送信
*
* MODIFICATION HISTORY:
*
* Ver   Who           Date        Changes
* ----- ------------- ----------- --------------------------------------------
* 0.00  drmus0715     2019/08/25  First release
*
******************************************************************************/
#ifndef SRC_DRV_DFPLAYER_H
#define SRC_DRV_DFPLAYER_H
#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************/
/* Include Files
******************************************************************************/
#include "def_system.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* DFPlayer Configure */
#define DFPLAYER_DEFAULT_VOLUME 25

    /*****************************************************************************/
    /* TAG Definitions
******************************************************************************/
    // DFPlayerコマンド定義
    typedef enum DFPLAYER_PLAYLIST
    {
        SE_COUNTDOWN_1 = 1,
        SE_COUNTDOWN_2,
        SE_ENTRY,
        SE_PANEL,
        SE_FINISH,
        SE_PINCH,
        // SE_GAMEOVER,
        SE_DAMAGE,
    } ePlaylist_t;

    typedef struct DFPLAYER_CTRL_MSG
    {
        ePlaylist_t eSound;
        uint16_t uiVolume = DFPLAYER_DEFAULT_VOLUME; // volume : 0 to 30
    } dfpCtrlMsg_t;

    /*****************************************************************************/
    /* Variable Definitions
******************************************************************************/

    /*****************************************************************************/
    /* Function Prototypes
******************************************************************************/
    // Init
    esp_err_t lInitDfplayer();
    BOOL_t xSendDfplayerQueue(dfpCtrlMsg_t dfpCtrlMsg);

#ifdef __cplusplus
}
#endif
#endif