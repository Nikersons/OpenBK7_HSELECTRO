// Custom driver: MyThermo
// Configures pins for CHT83xx + Battery and periodically sends UDP JSON broadcast

#include "../new_common.h"
#include "../new_cfg.h"
#include "../new_pins.h"
#include "../logging/logging.h"

#include "../driver/drv_battery.h"
#include "../hal/hal_wifi.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"

#define LOG_TAG "MyThermo"
#define MYTHERMO_UDP_PORT 34210
#define MYTHERMO_PERIOD_SECONDS 5

static int g_mythermo_socket = -1;
static struct sockaddr_in g_mythermo_addr;
static int g_mythermo_counter = 0;

// helper: configure fixed pins (called once on boot)
static void MyThermo_ConfigurePins(void) {
    // P7  - CHT83XX_DAT
    PIN_SetPinRoleForPinIndex(7, IOR_CHT83XX_DAT);
    // Set main temperature channel, for example ch1
    PIN_SetPinChannelForPinIndex(7, 1);
    // humidity channel on channels2, for example ch2
    PIN_SetPinChannel2ForPinIndex(7, 2);

    // P8  - CHT83XX_CLK
    PIN_SetPinRoleForPinIndex(8, IOR_CHT83XX_CLK);

    // P23 - battery ADC
    PIN_SetPinRoleForPinIndex(23, IOR_BAT_ADC);
    // optional: expose battery on some channel
    PIN_SetPinChannelForPinIndex(23, 3);

    // P26 - LED, assume active low -> LED_n
    PIN_SetPinRoleForPinIndex(26, IOR_LED_n);

    // P20 - button, assume active low -> Button_n
    PIN_SetPinRoleForPinIndex(20, IOR_Button_n);

    // Apply configuration to hardware
    PIN_SetupPins();
    CFG_Save_IfThereArePendingChanges();
}

// helper: open UDP broadcast socket
static void MyThermo_OpenSocket(void) {
    if (g_mythermo_socket >= 0) {
        return;
    }
    g_mythermo_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_mythermo_socket < 0) {
        addLogAdv(LOG_ERROR, LOG_FEATURE_GENERAL, LOG_TAG": socket() failed");
        return;
    }

    int broadcastEnable = 1;
    setsockopt(g_mythermo_socket, SOL_SOCKET, SO_BROADCAST,
               (void *)&broadcastEnable, sizeof(broadcastEnable));

    memset(&g_mythermo_addr, 0, sizeof(g_mythermo_addr));
    g_mythermo_addr.sin_family = AF_INET;
    g_mythermo_addr.sin_port = htons(MYTHERMO_UDP_PORT);
    g_mythermo_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL, LOG_TAG": UDP socket ready on port %d", MYTHERMO_UDP_PORT);
}

void MyThermo_Init() {
    addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL, LOG_TAG": Init");

    // configure pins and channels
    MyThermo_ConfigurePins();

    // open UDP broadcast socket
    MyThermo_OpenSocket();

    g_mythermo_counter = 0;
}

// called every second from main driver loop
void MyThermo_OnEverySecond(void) {
    g_mythermo_counter++;
    if (g_mythermo_counter < MYTHERMO_PERIOD_SECONDS) {
        return;
    }
    g_mythermo_counter = 0;

    // get temperature & humidity from channels bound to CHT83xx DAT pin (P7)
    float t = 0.0f;
    float h = 0.0f;

    int chTemp = PIN_GetPinChannelForPinIndex(7);
    int chHum  = PIN_GetPinChannel2ForPinIndex(7);

    if (chTemp >= 0) {
        t = CHANNEL_GetFinalValue(chTemp);
    }
    if (chHum >= 0) {
        h = CHANNEL_GetFinalValue(chHum);
    }

    // battery voltage and level from battery driver
    int mv = Battery_lastreading(OBK_BATT_VOLTAGE);   // mV
    int pct = Battery_lastreading(OBK_BATT_LEVEL);    // %

    float vbat = mv / 1000.0f;

    // Wi-Fi info
    char macStr[32];
    HAL_GetMACStr(macStr);
    int rssi = HAL_GetWifiStrength();

    const char *id = CFG_GetShortDeviceName();

    // build JSON
    char buf[256];
    os_snprintf(buf, sizeof(buf),
        "{\"id\":\"%s\",\"t\":%.2f,\"h\":%.2f,\"bat_v\":%.3f,\"bat_pct\":%d,\"rssi\":%d,\"mac\":\"%s\"}",
        id, t, h, vbat, pct, rssi, macStr);

    // send UDP broadcast
    if (g_mythermo_socket >= 0) {
        int sent = sendto(g_mythermo_socket, buf, os_strlen(buf), 0,
                          (struct sockaddr*)&g_mythermo_addr, sizeof(g_mythermo_addr));
        if (sent < 0) {
            addLogAdv(LOG_ERROR, LOG_FEATURE_GENERAL, LOG_TAG": sendto failed");
        } else {
            addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL, LOG_TAG": UDP sent: %s", buf);
        }
    }
}
