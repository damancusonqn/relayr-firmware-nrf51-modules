#include "simble.h"
#include "onboard-led.h"
#include "rtc.h"

struct my_service_ctx {
        struct service_desc;
        struct char_desc my_char;
        uint8_t my_sensor_value;
};

static void
template_connect_cb(struct service_desc *s)
{
	// struct my_service_ctx *ctx = s;
}

static void
template_disconnect_cb(struct service_desc *s)
{
	// struct my_service_ctx *ctx = s;
}

static void
my_char_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	*valp = &ctx->my_sensor_value;
	*lenp = 1;
}

static void
my_char_write_cb(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len)
{
	struct my_service_ctx *ctx = (struct my_service_ctx *)s;
	ctx->my_sensor_value = *(uint8_t*)val;
}

static void
my_service_init(struct my_service_ctx *ctx)
{
	ctx->my_sensor_value = 0;
	// init the service context
	simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_SENSOR_SERVICE);
	// add a characteristic to our service
	simble_srv_char_add(ctx, &ctx->my_char,
		simble_get_vendor_uuid_class(), VENDOR_UUID_IR_CHAR,
		u8"my characteristic",
		1); // size in bytes
	// BLE callbacks (optional)
	ctx->connect_cb = template_connect_cb;
	ctx->disconnect_cb = template_disconnect_cb;
	ctx->my_char.read_cb = my_char_read_cb;
	ctx->my_char.write_cb = my_char_write_cb;
	simble_srv_register(ctx); // register our service
}

static struct my_service_ctx my_service_ctx;


static void
notif_timer_cb(struct rtc_ctx *ctx)
{
  NRF_GPIO->OUT ^= (1 << 1);
  //simble_srv_char_update(&ctx->temp, &ctx->last_reading);
}

void
main(void)
{
	simble_init("relayr"); // init BLE library

  NRF_GPIO->PIN_CNF[2] = GPIO_PIN_CNF_DIR_Output;
  NRF_GPIO->PIN_CNF[1] = GPIO_PIN_CNF_DIR_Output;

  //Set the timer parameters and initialize it.
  struct rtc_ctx rtc_ctx = {
      .rtc_x[0].period = 500,
      .rtc_x[0].enabled = 1,
      .rtc_x[0].cb = notif_timer_cb
  };
  rtc_init(&rtc_ctx);

	my_service_init(&my_service_ctx);
	simble_adv_start(); // start advertising
	simble_process_event_loop(); // main loop (stuck here)
}
