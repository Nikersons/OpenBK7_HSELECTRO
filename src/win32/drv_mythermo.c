#include "../new_common.h"
#include "../new_cfg.h"
#include "../logging/logging.h"
#include "../new_pins.h"
#include "../driver/drv_i2c.h"
#include "../driver/drv_battery.h"
#include "../driver/drv_cht83xx.h"

#define LOG_TAG "MYTHERMO"

// Период обновления в миллисекундах
#define MYTHERMO_UPDATE_PERIOD 5000

static int g_mythermo_timer = -1;

// ---------- ЧТЕНИЕ ДАННЫХ ----------------
static void MYTHERMO_ReadTask(int param) {
    float t = 0, h = 0;
    float vbat = 0;

    // Читаем температуру/влажность
    bool ok = CHT83XX_Read(&t, &h);
    if (!ok) {
        addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL, LOG_TAG": sensor read FAIL");
    } else {
        addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL,
            LOG_TAG": Temperature=%.2f C, Humidity=%.2f %%", t, h);
    }

    // Читаем батарею
    vbat = Battery_GetVoltage();
    addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL,
        LOG_TAG": Battery=%.3f V", vbat);

    // --------- ТУТ МОЖНО ОТПРАВИТЬ ДАННЫЕ ПО WI-FI ---------

    // HTTP пример:
    // HTTP_Request("http://192.168.1.50/update?t=%f&h=%f&b=%f", t, h, vbat);

    // MQTT пример:
    // MQTT_PublishMain("mythermo/data", "{\"t\":%.2f,\"h\":%.2f,\"b\":%.3f}", t, h, vbat);
}

// ---------- ИНИЦИАЛИЗАЦИЯ ---------------
void DRV_MyThermo_Init() {

    addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL, LOG_TAG": Init");

    // ---- 1. Назначаем роли пинов ----

    // I2C
    PIN_SetPinRole(7, I2C_SDA);
    PIN_SetPinRole(8, I2C_SCL);

    // Батарея
    PIN_SetPinRole(23, PIN_ROLE_ADC);

    // Светодиод
    PIN_SetPinRole(26, PIN_ROLE_LED);

    // Кнопка
    PIN_SetPinRole(20, PIN_ROLE_BUTTON);

    // Применяем изменения
    PIN_ConfigAllPins();

    // ---- 2. Инициализация драйверов ----
    I2C_Init();
    Battery_Init();
    CHT83XX_Init();   // Драйвер встроенный в ОВК

    // ---- 3. Старт таймера ----
    g_mythermo_timer = HAL_TimerCreate(
        MYTHERMO_UPDATE_PERIOD,
        MYTHERMO_ReadTask,
        0,
        true
    );

    addLogAdv(LOG_INFO, LOG_FEATURE_GENERAL, LOG_TAG": Started");
}

// ----- Регистрация драйвера в системе -----
#include "../new_pwr.h"
OBK_REGISTER_DRIVER(DRV_MyThermo_Init);
