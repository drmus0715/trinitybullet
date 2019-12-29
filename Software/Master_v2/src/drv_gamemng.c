/*****************************************************************************/
/**
 * @file drv_gamemng.c
 * @comments GameManager通信モジュールです。
 *
 * MODIFICATION HISTORY:
 *
 * Ver   Who           Date        Changes
 * ----- ------------- ----------- --------------------------------------------
 * 0.01  drmus0715     2019/08/31  First release
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
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/* Standard Lib Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ESP-IDF Includes */
#include <sys/param.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"

/* LwIP Includes */
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

/* JSON library */
#include "jsmn.h"

/* User Includes */
#include "def_system.h"
#include "drv_gamemng.h"
#include "ctrl_main.h"

/*****************************************************************************/
/* Constant Definitions
******************************************************************************/
/* Wi-Fi Configuration */
#define WIFI_SSID "escape_gmAP"
#define WIFI_PASS "1qaz2wsx"

/* Network Configration */
#define MAKE_IP4ADDR(a, b, c, d) PP_HTONL(LWIP_MAKEU32(a, b, c, d))
tcpip_adapter_ip_info_t sIpinfo = {
    .ip.addr = MAKE_IP4ADDR(192, 168, 10, 64),
    .netmask.addr = MAKE_IP4ADDR(255, 255, 255, 0),
    .gw.addr = MAKE_IP4ADDR(192, 168, 10, 1),
};
#define PORT_GAMEMNG 50000

/* WebServer Configration */
#define WEBSERVER_HOSTNAME "192.168.10.104"
#define WEBSERVER_PORT 4567
#define WEBSERVER_POST_DEST "/result"

#define WEBSERVER_POSTDATA_SIZE 1024

/* JSMN Configration */
#define JSMN_TOKENS_NUM 32

/* TX_MSG */
const char *CMD_GMMSG_FIN_PREPARE = "esp_wait";
const char *CMD_GMMSG_GAME_START = "esp_gamestart";
const char *CMD_GMMSG_PLAYER_ENTRY = "esp_playerEntry";

const char *CMD_GMMSG_FAIL = "Bad command";

/*****************************************************************************/
/* Variable Definitions
******************************************************************************/
/* FreeRTOS event group to signal when we are connected & ready to make a
 * request */
static EventGroupHandle_t wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;

static const char *TAG = "GameMng";
static const char *TAG_H = "HttpClient";

QueueHandle_t xGamemngTxQueue = NULL;
QueueHandle_t xHttpPostQueue = NULL;

/*****************************************************************************/
/* Function Prototypes
******************************************************************************/
// Task
static void tcp_server_task(void *pvParameters);
static void http_client_task(void *pvParameters);

// Wi-Fi Function
static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wait_for_ip();

BOOL_t prvParsePlayerInfo(char *buf, int len);

/*****************************************************************************/
/* Public Function
******************************************************************************/

/*****************************************************************************/
/**
 * Init Wi-Fi & ...
 *
 * @param	##
 *
 * @return   esp_err
 *
 * @note    ##
 *
 ******************************************************************************/
esp_err_t lInitGameMng()
{
    BOOL_t xStatus;
    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi();
    wait_for_ip();

    xStatus = xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    configASSERT(xStatus);

    xGamemngTxQueue = xQueueCreate(10, sizeof(enum MSG_TYPE));
    if (xGamemngTxQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed creating xGamemngTxQueue");
        return ESP_FAIL;
    }

    xHttpPostQueue = xQueueCreate(10, sizeof(struct GAME_INFO));
    if (xHttpPostQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed creating xHttpPostQueue");
        return ESP_FAIL;
    }

    xStatus = xTaskCreate(http_client_task, "http_client", 4096, NULL, 5, NULL);
    configASSERT(xStatus);

    return (xStatus == pdPASS ? ESP_OK : ESP_FAIL);
}

/*****************************************************************************/
/**
 * GamemngTx Queueへ送信
 *
 * @param    enum MSG_TYPE: メッセージのタイプ
 *
 * @return   pdPASS / pdFAIL
 *
 * @note    ##
 *
 ******************************************************************************/
BOOL_t xSendGamemngTxQueue(enum MSG_TYPE eTxMsg)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xGamemngTxQueue, &eTxMsg, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/**
 * GamemngTx Queueへ送信
 *
 * @param    enum MSG_TYPE: メッセージのタイプ
 *
 * @return   pdPASS / pdFAIL
 *
 * @note    ##
 *
 ******************************************************************************/
BOOL_t xSendHttpPostQueue(struct GAME_INFO sGameInfo)
{
    BOOL_t xStatus;
    xStatus = xQueueSendToBack(xHttpPostQueue, &sGameInfo, portMAX_DELAY);
    return xStatus;
}

/*****************************************************************************/
/* Private Function
******************************************************************************/

/*****************************************************************************/
/**
 * httpのエベントハンドラ
 *
 * @param	[params] is ...
 *
 * @return   [Return] is ...
 *
 * @note		##
 *
 ******************************************************************************/
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG_H, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG_H, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG_H, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG_H, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                 evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG_H, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // Write out data
            // printf("%.*s", evt->data_len, (char*)evt->data);
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG_H, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_H, "HTTP_EVENT_DISCONNECTED");
        // int mbedtls_err = 0;
        // esp_err_t err =
        //     esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        // if (err != 0) {
        //     ESP_LOGI(TAG_H, "Last esp error code: 0x%x", err);
        //     ESP_LOGI(TAG_H, "Last mbedtls failure: 0x%x", mbedtls_err);
        // }
        break;
    }
    return ESP_OK;
}

/*****************************************************************************/
/**
 * えいち・てぃー・てぃー・ペー の くらいあんと？の タヌク
 *
 * @param	[params] is ...
 *
 * @return   [Return] is ...
 *
 * @note		##
 *
 ******************************************************************************/
static void http_client_task(void *pvParameters)
{
    esp_err_t err;
    struct GAME_INFO sGameInfo = {};
    bool bRetryFlag = false;
    bool RetryCnt = 0;
    char post_data[WEBSERVER_POSTDATA_SIZE] = {};
    esp_http_client_config_t config = {
        .host = WEBSERVER_HOSTNAME,
        .path = WEBSERVER_POST_DEST,
        .port = WEBSERVER_PORT,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .is_async = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
        ESP_LOGE(TAG_H, "[Critical Error] Failed Initialize HTTP Client");

    for (;;)
    {
        // RetryFlagがtrueのとき、ブロックせずに再度行う。

        if (bRetryFlag != true)
        {
            // キューを受信
            xQueueReceive(xHttpPostQueue, &sGameInfo, portMAX_DELAY);
        }
        else if (bRetryFlag == true)
        {
            client = esp_http_client_init(&config);
            RetryCnt++;
        }

        // postするデータを格納
        sprintf(post_data,
                "team=%d&difficulty=%d&name=%s&redPoint=%d&"
                "bluePoint=%d&greenPoint=%d&hitPoint=%d&remainingTime=%d",
                sGameInfo.team, sGameInfo.difficuty, sGameInfo.name,
                sGameInfo.redPoint, sGameInfo.bluePoint, sGameInfo.greenPoint,
                sGameInfo.hitPoint, sGameInfo.remainingTime);
        // const char *post_data = "field1=value1&field2=value2";
        ESP_LOGI(TAG, "send | %s", post_data);

        esp_http_client_set_url(client, "/result");
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        if (esp_http_client_set_post_field(client, post_data,
                                           strlen(post_data)) != ESP_OK)
        {
            ESP_LOGE(TAG, "failed http_set_post | %s", post_data);
        }
        err = esp_http_client_perform(client);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG_H, "HTTP POST Status = %d, content_length = %d",
                     esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));
            RetryCnt = 0;
            bRetryFlag = false;
        }
        else
        {
            ESP_LOGE(TAG_H, "HTTP POST request failed: %s",
                     esp_err_to_name(err));
            /*
             * Retry: HTTP connections
             * note: 回数でタイムアウトしたい…
             */
            esp_http_client_cleanup(client);
            bRetryFlag = true;
            ESP_LOGE(TAG_H, "Retrying HTTP Client... Count:%d", RetryCnt);
        }
    }
}

/*****************************************************************************/
/**
 * TCP server の タスク
 *
 * @param	[params] is ...
 *
 * @return   [Return] is ...
 *
 * @note		##
 *
 ******************************************************************************/
static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    enum MSG_TYPE eTxMsg;

    while (1)
    { // Task Loop
        while (1)
        { // Connection Failed, Retry
            /* Create Socket */
            struct sockaddr_in destAddr;
            destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            destAddr.sin_family = AF_INET;
            destAddr.sin_port = htons(PORT_GAMEMNG);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
            inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

            int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
            if (listen_sock < 0)
            {
                ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Socket created");

            /* Bind Socket */
            int err = bind(listen_sock, (struct sockaddr *)&destAddr,
                           sizeof(destAddr));
            if (err != 0)
            {
                /*
                 * ここでエラーが頻発する。errno:112
                 * でもエラーが起きても試行すれば行けそう
                 */
                ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Socket binded");

            /* Listen Socket */
            err = listen(listen_sock, 1);
            if (err != 0)
            {
                ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Socket listening");

            /* Accept Socket */
            struct sockaddr_in6
                sourceAddr; // Large enough for both IPv4 or IPv6
            uint addrLen = sizeof(sourceAddr);
            int sock =
                accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
            if (sock < 0)
            {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Socket accepted");

            // Put the socket in non-blocking mode:
            if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0)
            {
                ESP_LOGE(TAG, "Failed to put the socket in non-blocking");
                break;
            }

            while (!(sock < 0) || !(listen_sock < 0))
            { // Connect Client Loop
                int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                // Error occured during receiving
                if (len < 0 && errno != EAGAIN)
                {
                    ESP_LOGE(TAG, "recv failed: errno %d", errno);
                    break;
                }
                // Connection closed
                else if (len == 0)
                {
                    ESP_LOGI(TAG, "Connection closed");
                    break;
                }
                // Data received
                else if (len > 0)
                {
                    // Get the sender's ip address as string
                    if (sourceAddr.sin6_family == PF_INET)
                    {
                        inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)
                                        ->sin_addr.s_addr,
                                    addr_str, sizeof(addr_str) - 1);
                    }
                    else if (sourceAddr.sin6_family == PF_INET6)
                    {
                        inet6_ntoa_r(sourceAddr.sin6_addr, addr_str,
                                     sizeof(addr_str) - 1);
                    }

                    rx_buffer[len] = 0; // Null-terminate whatever we received
                                        // and treat like a string
                    ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);

                    // parse Player Infomation & Send Main
                    if (prvParsePlayerInfo(rx_buffer, len) != pdPASS)
                    {
                        ESP_LOGE(TAG, "Cannot Parse recvData.");
                        err = send(sock, CMD_GMMSG_FAIL, strlen(CMD_GMMSG_FAIL),
                                   0);
                        if (err < 0)
                        {
                            ESP_LOGE(TAG,
                                     "Error occured during sending: errno %d",
                                     errno);
                        }
                    }
                    ESP_LOGI(TAG, "%s", rx_buffer);
                }

                /*
                 * GMへコマンド送信
                 * Queueにメッセージがあったらコマンドを送信する
                 */
                if (xQueueReceive(xGamemngTxQueue, &eTxMsg, 0) ==
                    pdPASS)
                { // Non-Blocking
                    switch (eTxMsg)
                    {
                    case GMMSG_FIN_PREPARE:
                        err = send(sock, CMD_GMMSG_FIN_PREPARE,
                                   strlen(CMD_GMMSG_FIN_PREPARE), 0);
                        break;
                    case GMMSG_GAME_START:
                        err = send(sock, CMD_GMMSG_GAME_START,
                                   strlen(CMD_GMMSG_GAME_START), 0);
                        break;
                    case GMMSG_ENTRY:
                        err = send(sock, CMD_GMMSG_PLAYER_ENTRY,
                                   strlen(CMD_GMMSG_PLAYER_ENTRY), 0);
                        break;
                    default:
                        ESP_LOGE(TAG, "Not exist GMMSG");
                        break;
                    }
                    if (err < 0)
                    {
                        ESP_LOGE(TAG, "Error occured during sending: errno %d",
                                 errno);
                        break;
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (sock != -1)
            {
                // コネクションが終了したとき
                ESP_LOGE(TAG, "Shutting down socket and restarting...");
                if (shutdown(sock, 0) < 0)
                    ESP_LOGE(TAG, "Failed shutdown(sock)");
                if (close(sock) < 0)
                    ESP_LOGE(TAG, "Failed close(sock)");
                if (shutdown(listen_sock, 0) < 0)
                    ESP_LOGE(TAG, "Failed shutdown(listen_sock)");
                if (close(listen_sock) < 0)
                    ESP_LOGE(TAG, "Failed close(listen_sock)");
            }
        }
    }
    vTaskDelete(NULL);
}

/*****************************************************************************/
/**
 * プレイヤー情報のパース（JSON）
 *  jsoneq()はパース用関数です。ｵｳﾁｬｸ…
 *
 * @param	[params] is ...
 *
 * @return  [Return] is ...
 *
 * @note    ##
 *
 ******************************************************************************/
static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0)
    {
        return 0;
    }
    return -1;
}

BOOL_t prvParsePlayerInfo(char *buf, int len)
{
    // BOOL_t xStatus = pdFAIL;
    char strBuf[128] = {};
    playerInfo_t player = {};

    /* JSON Parse用 */
    jsmn_parser json;
    jsmntok_t tokens[JSMN_TOKENS_NUM] = {};
    int r = 0;

    /* Initialize Json Parser: やらないと死ぬ*/
    jsmn_init(&json);

    r = jsmn_parse(&json, buf, len, tokens, JSMN_TOKENS_NUM); // パースするよー

    /* Error Check */
    if (r < 0)
    {
        ESP_LOGE(TAG, "JSON: Failed JSON Parse errno:%d", r);
        return pdFAIL;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || tokens[0].type != JSMN_OBJECT)
    {
        ESP_LOGE(TAG, "JSON: Not Json Object");
        return pdFAIL;
    }

    /* Loop over all keys of the root object */
    for (int i = 1; i < r; i++)
    {
        if (jsoneq(buf, &tokens[i], "startFlag") == 0)
        {
            strncpy(strBuf, buf + tokens[i + 1].start,
                    tokens[i + 1].end - tokens[i + 1].start);

            switch (strBuf[0])
            { // StartFlag
            case 't':
            case '1':
                player.startFlg = pdTRUE;
                break;
            case 'f':
            case '0':
                player.startFlg = pdFALSE;
                break;
            default:
                ESP_LOGE(TAG, "JSON: Obj: startFlag: ERROR_NUM->%d",
                         player.startFlg);
                return pdFAIL;
                break;
            }

            ESP_LOGI(TAG, "JSON: Obj: startFlag: %s",
                     player.startFlg ? "true" : "false");
            i++;
        }
        else if (jsoneq(buf, &tokens[i], "name") == 0)
        {
            strncpy(player.name, buf + tokens[i + 1].start,
                    tokens[i + 1].end - tokens[i + 1].start);
            ESP_LOGI(TAG, "JSON: Obj: name: %s", player.name);
            i++;
        }
        else if (jsoneq(buf, &tokens[i], "difficulty") == 0)
        {
            strncpy(strBuf, buf + tokens[i + 1].start,
                    tokens[i + 1].end - tokens[i + 1].start);
            player.difficulty = atoi(strBuf);

            /* 難易度が正しい値か */
            if (player.difficulty < DFCLT_EASY ||
                MAX_DFCLT < player.difficulty)
            {
                ESP_LOGE(TAG, "JSON: Obj: difficulty: ERROR_NUM->%d",
                         player.difficulty);
                return pdFAIL;
            }
            else
            {
                ESP_LOGI(TAG, "JSON: Obj: difficulty: %d", player.difficulty);
                i++;
            }
        }
        else if (jsoneq(buf, &tokens[i], "team") == 0)
        {
            strncpy(strBuf, buf + tokens[i + 1].start,
                    tokens[i + 1].end - tokens[i + 1].start);
            player.team = atoi(strBuf);

            /* チームがが正しい値か */
            if (player.team < TEAM_RED || MAX_TEAM < player.team)
            {
                ESP_LOGE(TAG, "JSON: Obj: team: ERROR_NUM->%d", player.team);
                return pdFAIL;
            }
            else
            {
                ESP_LOGI(TAG, "JSON: Obj: team: %d", player.team);
                i++;
            }
        }
        else
        {
            ESP_LOGI(TAG, "Unexpected key: %.*s",
                     tokens[i].end - tokens[i].start, buf + tokens[i].start);
            return pdFAIL;
        }
    }

    // ctrl_mainへ送信
    return xSendPlayerInfoQueue(player);
}

/*****************************************************************************/
/**
 * IP待つ
 * 多分これいらないけど面倒なので・・・
 *
 * @param	[params] is ...
 *
 * @return   [Return] is ...
 *
 * @note		##
 *
 ******************************************************************************/
static void wait_for_ip()
{
    uint32_t bits = IPV4_GOTIP_BIT | IPV6_GOTIP_BIT;

    ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(wifi_event_group, bits, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}

/*****************************************************************************/
/**
 * うぃふぃーのせとあぷ
 *
 * @param	[params] is ...
 *
 * @return   [Return] is ...
 *
 * @note		##
 *
 ******************************************************************************/
static void initialise_wifi(void)
{
    tcpip_adapter_init();

    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

    /* Static IP Addr */
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &sIpinfo);

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = WIFI_SSID,
                .password = WIFI_PASS,
            },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...",
             wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/*****************************************************************************/
/**
 * event_handler
 * 考えるのがめんどいのでサンプルままになっております
 *
 * @param	[params] is ...
 *
 * @return   [Return] is ...
 *
 * @note		##
 *
 ******************************************************************************/
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        /* enable ipv6 */
        tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
         * auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
        xEventGroupClearBits(wifi_event_group, IPV6_GOTIP_BIT);
        break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        xEventGroupSetBits(wifi_event_group, IPV6_GOTIP_BIT);
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP6");

        char *ip6 = ip6addr_ntoa(&event->event_info.got_ip6.ip6_info.ip);
        ESP_LOGI(TAG, "IPv6: %s", ip6);
    default:
        break;
    }
    return ESP_OK;
}