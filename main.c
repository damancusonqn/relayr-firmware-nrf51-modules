#include <stdlib.h>
#include <string.h>

#include <ble.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>


struct ble_gap_advdata {
        uint8_t length;
        uint8_t data[BLE_GAP_ADV_MAX_SIZE];
};

struct ble_gap_ad_header {
        uint8_t payload_length;
        uint8_t type;
} __packed;

struct ble_gap_ad_flags {
        struct ble_gap_ad_header;
        uint8_t flags;
} __packed;

struct ble_gap_ad_name {
        struct ble_gap_ad_header;
        uint8_t name[BLE_GAP_DEVNAME_MAX_LEN];
} __packed;


enum onboard_led {
        ONBOARD_LED_OFF = 0,
        ONBOARD_LED_ON = 1,
        ONBOARD_LED_TOGGLE = -1,
};

static void
onboard_led(enum onboard_led set)
{
        const int led_pin = 29;

        /* set to ouput */
        NRF_GPIO->PIN_CNF[led_pin] = GPIO_PIN_CNF_DIR_Output | GPIO_PIN_CNF_INPUT_Disconnect;
        switch (set) {
        case ONBOARD_LED_ON:
                NRF_GPIO->OUTSET = (1 << led_pin);
                break;
        case ONBOARD_LED_OFF:
                NRF_GPIO->OUTCLR = (1 << led_pin);
                break;
        case ONBOARD_LED_TOGGLE:
                NRF_GPIO->OUT ^= (1 << led_pin);
                break;
        }
}

static uint32_t
ble_add_advdata(const struct ble_gap_ad_header *data, struct ble_gap_advdata *advdata)
{
        size_t elem_length = data->payload_length + sizeof(*data);
        size_t final_length = advdata->length + elem_length;

        if (final_length > sizeof(advdata->data))
                return NRF_ERROR_DATA_SIZE;

        memcpy(&advdata->data[advdata->length], data, elem_length);
        advdata->data[advdata->length] = elem_length - 1;

        advdata->length += elem_length;

        return NRF_SUCCESS;
}

static void
ble_adv_start(void)
{
        struct ble_gap_advdata advdata = { .length = 0 };

        struct ble_gap_ad_flags flags = {
                .payload_length = 1,
                .type = BLE_GAP_AD_TYPE_FLAGS,
                .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        };
        ble_add_advdata(&flags, &advdata);

        struct ble_gap_ad_name name;
        uint16_t namelen = sizeof(advdata);
        if (sd_ble_gap_device_name_get(name.name, &namelen) != NRF_SUCCESS)
                namelen = 0;
        name.type =  BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
        name.payload_length = namelen;
        ble_add_advdata(&name, &advdata);

        sd_ble_gap_adv_data_set(advdata.data, advdata.length, NULL, 0);

        ble_gap_adv_params_t adv_params = {
                .type = BLE_GAP_ADV_TYPE_ADV_IND,
                .fp = BLE_GAP_ADV_FP_ANY,
                .interval = 0x400,
        };
        sd_ble_gap_adv_start(&adv_params);
        onboard_led(ONBOARD_LED_ON);
}

static void
ble_init(const char *name)
{
        sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL); /* XXX assertion handler */

        ble_enable_params_t ble_params = {
                .gatts_enable_params = {
                        .service_changed = 1,
                },
        };
        sd_ble_enable(&ble_params);

        ble_gap_conn_sec_mode_t mode;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&mode);
        sd_ble_gap_device_name_set(&mode, (const uint8_t *)name, strlen(name));

}

static void
ble_srv_tx_init(void)
{
        uint16_t srv_handle;
        ble_uuid_t srv_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = 0x1804};
        sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                 &srv_uuid,
                                 &srv_handle);

        ble_gatts_char_handles_t chr_handles;
        ble_gatts_char_md_t char_meta = {.char_props = {.read = 1}};
        ble_uuid_t chr_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = 0x2a07};
        ble_gatts_attr_md_t chr_attr_meta = {
                .vloc = BLE_GATTS_VLOC_STACK,
        };
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&chr_attr_meta.write_perm);
        uint8_t tx_val = 0;
        ble_gatts_attr_t chr_attr = {
                .p_uuid = &chr_uuid,
                .p_attr_md = &chr_attr_meta,
                .init_offs = 0,
                .init_len = sizeof(tx_val),
                .max_len = sizeof(tx_val),
                .p_value = &tx_val,
        };
        sd_ble_gatts_characteristic_add(srv_handle,
                                        &char_meta,
                                        &chr_attr,
                                        &chr_handles);
}

static uint8_t
get_vendor_uuid_class(void)
{
        ble_uuid128_t vendorid = {{0x13,0xb0,0xa0,0x71,0xfe,0x62,0x4c,0x01,0xaa,0x4d,0xd8,0x03,0,0,0x0b,0xd0}};
        static uint8_t vendor_type = BLE_UUID_TYPE_UNKNOWN;

        if (vendor_type != BLE_UUID_TYPE_UNKNOWN)
                return (vendor_type);

        sd_ble_uuid_vs_add(&vendorid, &vendor_type);
        return (vendor_type);
}

enum vendor_uuid {
        VENDOR_UUID_SENSOR_SERVICE = 0x1801,
        VENDOR_UUID_TEMP_CHAR = 0x2301,
        VENDOR_UUID_HUMID_CHAR = 0x2302,
        VENDOR_UUID_ACCEL_CHAR = 0x2303,
        VENDOR_UUID_GYRO_CHAR = 0x2304,
        VENDOR_UUID_SOUND_CHAR = 0x2305,
        VENDOR_UUID_BRIGHT_CHAR = 0x2306,
        VENDOR_UUID_COLOR_CHAR = 0x2307,
        VENDOR_UUID_PROX_CHAR = 0x2308,
};

static void
ble_srv_temp_init(void)
{
        uint16_t srv_handle;
        ble_uuid_t srv_uuid = {.type = get_vendor_uuid_class(),
                               .uuid = VENDOR_UUID_SENSOR_SERVICE};
        sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                 &srv_uuid,
                                 &srv_handle);

        ble_gatts_char_handles_t chr_handles;
        ble_gatts_char_pf_t fmt = {
                .format = BLE_GATT_CPF_FORMAT_SINT16,
                .exponent = 0,
                .unit = 0x272f,
        };
        const char *descstr = u8"Temperature";
        ble_gatts_char_md_t char_meta = {
                .char_props = {.read = 1},
                .p_char_user_desc = (uint8_t *)descstr,
                .char_user_desc_size = strlen(descstr),
                .char_user_desc_max_size = strlen(descstr),
                .p_char_pf = &fmt,
        };
        ble_uuid_t chr_uuid = {
                .type = get_vendor_uuid_class(),
                .uuid = VENDOR_UUID_TEMP_CHAR
        };
        ble_gatts_attr_md_t chr_attr_meta = {
                .vloc = BLE_GATTS_VLOC_STACK,
        };
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&chr_attr_meta.write_perm);
        uint16_t temp_val = 0x1234;
        ble_gatts_attr_t chr_attr = {
                .p_uuid = &chr_uuid,
                .p_attr_md = &chr_attr_meta,
                .init_offs = 0,
                .init_len = sizeof(temp_val),
                .max_len = sizeof(temp_val),
                .p_value = (uint8_t *)&temp_val,
        };
        sd_ble_gatts_characteristic_add(srv_handle,
                                        &char_meta,
                                        &chr_attr,
                                        &chr_handles);
}

static void
ble_app_disconnected(void)
{
        ble_adv_start();
}

static void
process_event_loop(void)
{
        for (;;) {
                uint32_t r;
                uint32_t evt;

                sd_app_evt_wait();
                r = sd_evt_get(&evt);

                struct {
                        ble_evt_t evt;
                        uint8_t buf[GATT_MTU_SIZE_DEFAULT];
                } evt_buf;
                uint16_t len = sizeof(evt_buf);

                sd_app_evt_wait();
                r = sd_ble_evt_get((uint8_t *)&evt_buf, &len);
                if (r == NRF_SUCCESS) {
                        switch (evt_buf.evt.header.evt_id) {
                        case BLE_GAP_EVT_CONNECTED:
                                onboard_led(ONBOARD_LED_OFF);
                                break;
                        case BLE_GAP_EVT_DISCONNECTED:
                                ble_app_disconnected();
                                break;
                        }
                }
        }
}


void
main(void)
{
        ble_init("foo");
        ble_srv_tx_init();
        ble_srv_temp_init();
        ble_adv_start();

        process_event_loop();
}
