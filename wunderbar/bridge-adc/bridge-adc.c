#include <stdlib.h>
#include <string.h>

#include <twi_master.h>

#include "simble.h"
#include "indicator.h"

#include "adc121c02.h"


struct bridge_adc_ctx {
        struct service_desc;
        struct char_desc bridge_adc;
        uint16_t bridge_adc_value;
};


static void
bridge_adc_update(struct bridge_adc_ctx *ctx, uint16_t val)
{
        simble_srv_char_update(&ctx->bridge_adc, &val);
}

static void
bridge_adc_connected(struct service_desc *s)
{
        adc121c02_init();
}

static void
bridge_adc_disconnected(struct service_desc *s)
{
        adc121c02_stop();
}

static void
bridge_adc_read(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
        struct bridge_adc_ctx *ctx = (void *)s;

        ctx->bridge_adc_value = adc121c02_sample();
        *lenp = sizeof(ctx->bridge_adc_value);
        *valp = &ctx->bridge_adc_value;
}

static void
bridge_adc_init(struct bridge_adc_ctx *ctx)
{
        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
        simble_srv_char_add(ctx, &ctx->bridge_adc,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_ADC_CHAR,
                     u8"Bridge-Adc",
                     2);
        ctx->bridge_adc.read_cb = bridge_adc_read;
        /* srv_char_attach_format(&ctx->bridge_adc, */
        /*                        BLE_GATT_CPF_FORMAT_UINT16, */
        /*                        0, */
        /*                        0); */
        ctx->connect_cb = bridge_adc_connected;
        ctx->disconnect_cb = bridge_adc_disconnected;
        simble_srv_register(ctx);
}


static struct bridge_adc_ctx bridge_adc_ctx;

void
main(void)
{
        twi_master_init();

        simble_init("bridge-adc");
        ind_init();
        bridge_adc_init(&bridge_adc_ctx);
        simble_adv_start();

        simble_process_event_loop();
}
