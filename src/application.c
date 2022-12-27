#ifndef VERSION
#define VERSION "vdev"
#endif

#include <twr.h>
#include <bcl.h>

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

static twr_scheduler_task_id_t display_update_task;

// GFX instance
static twr_gfx_t *gfx;

static enum {
    SYSTEM_INFO_PAGE,
    NETWORK_INFO_PAGE,
    REBOOT_PAGE,
    NUM_PAGES
} display_page_index;

static char hostname[20];
static char type[50];
static char memory[50];
static char uptime[30];

static char ipAddress[40];
static char subnet[40];
static char devicesConnected[5];

static void twr_get_system_info(uint64_t *id, const char *topic, void *value, void *param);
static void twr_get_network_info(uint64_t *id, const char *topic, void *value, void *param);

static const twr_radio_sub_t subs[] = {
    {"update/-/system/info", TWR_RADIO_SUB_PT_STRING, twr_get_system_info, NULL},
    {"update/-/network/info", TWR_RADIO_SUB_PT_STRING, twr_get_network_info, NULL}
};

static twr_tmp112_t temp;

static void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;
    // int percentage;

    if (twr_module_battery_get_voltage(&voltage))
    {
        twr_radio_pub_battery(&voltage);
    }


    // if (twr_module_battery_get_charge_level(&percentage))
    // {
    //     twr_radio_pub_string("%d", percentage);
    // }
}

static void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        float temperature = 0.0;
        twr_tmp112_get_temperature_celsius(&temp, &temperature);
        twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);
    }
}

static void twr_get_system_info(uint64_t *id, const char *topic, void *value, void *param)
{
    (void) id;
    (void) topic;
    (void) param;
    char *token[4];
    const char s[2] = ";";

    twr_log_info("%s: %s=%s", __func__, topic, (char *)value);

    token[0] = strtok(value, s);
    token[1] = strtok(NULL, s);
    token[2] = strtok(NULL, s);
    token[3] = strtok(NULL, s);

    strncpy(hostname, token[0], sizeof(hostname));
    strncpy(uptime, token[1], sizeof(uptime));
    strncpy(memory, token[2], sizeof(memory));
    strncpy(type, token[3], sizeof(type));

    twr_scheduler_plan_now(display_update_task);


}

static void twr_get_network_info(uint64_t *id, const char *topic, void *value, void *param)
{
    (void) id;
    (void) topic;
    (void) param;
    char *token[3];
    const char s[2] = ";";

    twr_log_debug("%s at %llu", __func__, twr_tick_get());
    token[0] = strtok(value, s);
    token[1] = strtok(NULL, s);
    token[2] = strtok(NULL, s);

    strncpy(ipAddress, token[0], sizeof(ipAddress));
    strncpy(subnet, token[1], sizeof(subnet));
    strncpy(devicesConnected, token[2], sizeof(devicesConnected));

    twr_scheduler_plan_now(display_update_task);


}

static void encoder_event_handler(twr_module_encoder_event_t event, void *event_param)
{
    (void)event_param;
    bool t = true;

    switch (event)
    {
    case TWR_MODULE_ENCODER_EVENT_ROTATION:
        if (twr_module_encoder_get_increment() < 0)
            display_page_index = (display_page_index + NUM_PAGES - 1) % NUM_PAGES;
        else
            display_page_index = (display_page_index + 1) % NUM_PAGES;
        twr_scheduler_plan_now(display_update_task);
        break;
    case TWR_MODULE_ENCODER_EVENT_CLICK:
        switch (display_page_index)
        {
        case SYSTEM_INFO_PAGE:
            twr_log_debug("send get/system/info");
            twr_radio_pub_bool("get/system/info", &t);
            break;
        case NETWORK_INFO_PAGE:
            twr_log_debug("send get/network/info");
            twr_radio_pub_bool("get/network/info", &t);
            break;
        case REBOOT_PAGE:
            twr_log_debug("send reboot/-/device");
            twr_radio_pub_bool("reboot/-/device", &t);
            break;
        case NUM_PAGES:
        default:
            break;
        }
        break;
    case TWR_MODULE_ENCODER_EVENT_PRESS:
    case TWR_MODULE_ENCODER_EVENT_HOLD:
    case TWR_MODULE_ENCODER_EVENT_RELEASE:
    case TWR_MODULE_ENCODER_EVENT_ERROR:
    default:
        return;
    }
}

static void lcd_page_system()
{
    twr_gfx_clear(gfx);
    twr_gfx_printf(gfx, 0, 0, true, "System info:");
    twr_gfx_printf(gfx, 0, 5, true, "------------------");
    twr_gfx_printf(gfx, 0, 15, true, "Hostname:");
    twr_gfx_printf(gfx, 0, 25, true, "%s", hostname);
    twr_gfx_printf(gfx, 0, 40, true, "Type:");
    twr_gfx_printf(gfx, 0, 50, true, "%s", type);
    twr_gfx_printf(gfx, 0, 65, true, "Free memory:");
    twr_gfx_printf(gfx, 0, 75, true, "%s", memory);
    twr_gfx_printf(gfx, 0, 90, true, "Uptime:");
    twr_gfx_printf(gfx, 0, 100, true, "%s", uptime);
}

static void lcd_page_network()
{
    twr_gfx_clear(gfx);
    twr_gfx_printf(gfx, 0, 0, true, "Network info(LAN):");
    twr_gfx_printf(gfx, 0, 5, true, "------------------");
    twr_gfx_printf(gfx, 0, 15, true, "IP address:");
    twr_gfx_printf(gfx, 0, 25, true, "%s", ipAddress);
    twr_gfx_printf(gfx, 0, 40, true, "Subnet:");
    twr_gfx_printf(gfx, 0, 50, true, "%s", subnet);
    twr_gfx_printf(gfx, 0, 90, true, "Connected devices:");
    twr_gfx_printf(gfx, 0, 100, true, "%s devices", devicesConnected);
}

static void lcd_reboot_page()
{
    twr_gfx_clear(gfx);
    twr_gfx_printf(gfx, 15, 30, true, "Reboot?");
}

static void display_update(void *param);

void application_init(void)
{

    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    twr_module_lcd_init();
    gfx = twr_module_lcd_get_gfx();
    twr_gfx_set_font(gfx, &twr_font_ubuntu_13);
    //twr_module_lcd_set_button_hold_time(300);

    twr_module_encoder_init();
    twr_module_encoder_set_event_handler(encoder_event_handler, NULL);

    display_update_task = twr_scheduler_register(display_update, NULL, 0);

    // initialize TMP112 sensor
    twr_tmp112_init(&temp, TWR_I2C_I2C0, TWR_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);

    // set measurement handler (call "tmp112_event_handler()" after measurement)
    twr_tmp112_set_event_handler(&temp, tmp112_event_handler, NULL);

    // automatically measure the temperature every 15 minutes
    twr_tmp112_set_update_interval(&temp, 5 * 60 * 1000);

    // Initialze battery module
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    //twr_radio_init(TWR_RADIO_MODE_NODE_LISTENING);
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_set_rx_timeout_for_sleeping_node(400);
    twr_radio_set_subs((twr_radio_sub_t *) subs, sizeof(subs)/sizeof(twr_radio_sub_t));

    twr_radio_pairing_request("turris-mon", VERSION);

    twr_log_debug("jedu");
}

static void display_update(void *param)
{
    (void)param;
    twr_log_debug("%s enter", __func__);
    twr_system_pll_enable();
    if (!twr_module_lcd_is_ready())
    {
        twr_log_debug("%s not ready", __func__);
        twr_scheduler_plan_current_from_now(10);
    }
    else
    {
        twr_gfx_set_font(gfx, &twr_font_ubuntu_13);

        switch (display_page_index)
        {
        case SYSTEM_INFO_PAGE:
            lcd_page_system();
            break;
        case NETWORK_INFO_PAGE:
            lcd_page_network();
            break;
        case REBOOT_PAGE:
            lcd_reboot_page();
            break;
        case NUM_PAGES:
        default:
            break;
        }

        twr_gfx_update(gfx);
    }
    twr_system_pll_disable();
    twr_log_debug("%s leave", __func__);
}
