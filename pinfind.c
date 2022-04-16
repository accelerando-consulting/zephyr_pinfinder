/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#if CONFIG_APP_USE_LED
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,{0});
#endif

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#if CONFIG_APP_USE_GPIO0
static const struct device *gpio0=NULL;
#endif

#if CONFIG_APP_USE_GPIO1
static const struct device *gpio1=NULL;
#endif

#if CONFIG_APP_USE_LED
void setup_led() 
{
	if (!led.port || !device_is_ready(led.port)) {
		LOG_WRN("LED device %s is not available; ignoring it\n", led.port->name);
		led.port = NULL;
	}
	if (led.port) {
		int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			LOG_ERR("Error %d: failed to configure LED device %s pin %d\n",
				ret, led.port->name, led.pin);
			led.port = NULL;
		} else {
			LOG_INF("Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}
}
#endif

void setup_gpio(int instance, const struct device **dev_r, gpio_port_value_t *skip_pins, gpio_port_value_t *output_pins, gpio_port_value_t *pins) 
{
	const struct device *dev;
	LOG_INF("setup GPIO%d", instance);
	*pins = 0xFFFFFFFF;
	*skip_pins = 0;

	if (instance == 0) {
		dev = device_get_binding("GPIO_0");
	}
	else if (instance == 1) {
		dev = device_get_binding("GPIO_1");
	}
	else {
		LOG_ERR("Unhandled GPIO instance %d", instance);
		return;
	}
	if (!dev) {
		LOG_ERR("Could not get GPIO instance %d", instance);
		return;
	}
	*dev_r = dev;
	
	if (!device_is_ready(dev)) {
		LOG_ERR("Error: gpio instance %d is not ready\n", instance);
		return;
	}
#ifdef CONFIG_APP_USE_LED
	if (dev == led.port) {
		LOG_INF("Ignoring LED pin GPIO%d:%d", instance, led.pin);
		*skip_pins |= (1<<led.pin);
	}
#endif
	
	if (instance == 0) {
#ifdef NRF52832_XXAA
		LOG_INF("Skipping pins 18,30,31 on NRF52832 GPIO0");
		*skip_pins |= (1<<18);
		*skip_pins |= (1<<30);
		*skip_pins |= (1<<31);
#endif
		if (strcmp(CONFIG_BOARD, "accelerando_redsoil") == 0) {
			LOG_INF("Skip pin 30 on accelerando_redsoil GPIO0");
			*skip_pins |= (1<<30);
		}
	}
	else if (instance == 1) {
#ifdef NRF52840_XXAA
		*skip_pins |= 0xFFFF0000;
#endif
	}
	
	for (int p=0; p<31;p++) {
		if (*skip_pins & (1<<p)) {
			continue;
		}
		if (*output_pins & (1<<p)) {
			LOG_INF("Configure pin GPIO%d:%d as OUTPUT", instance, p);
			int ret = gpio_pin_configure(dev, p, GPIO_OUTPUT);
			if (ret != 0) {
				LOG_ERR("Could not set pin GPIO%d:%d as output: error %d",
					instance, p, ret);
			}
			gpio_pin_set(dev, p, 0);
			continue;
		}
			
		LOG_INF("Configure pin GPIO%d:%d as INPUT", instance, p);
		int ret = gpio_pin_configure(dev, p, GPIO_INPUT|GPIO_PULL_UP);
		if (ret != 0) {
			LOG_WRN("Error %d: failed to configure GPIO%d:%d\n",
				ret, instance, p);
		}
		else {
			LOG_INF("    pin GPIO%d:%d set as input", instance, p);
		}
	}
}

void poll_gpio(int instance, const struct device *dev, gpio_port_value_t *skip_pins, gpio_port_value_t *pins) 
{
	gpio_port_value_t new_pins;
	int ret = gpio_port_get(dev, &new_pins);

	if (ret != 0) {
		LOG_ERR("Port read error for GPIO%d:%d", instance, ret);
		return;
	}
	new_pins |= *skip_pins; // ignore skipped pins

	if (new_pins == *pins) {
		// no change
		return;
	}
	
	LOG_INF("Input state change GPIO%d %08X => %08X", instance, *pins, new_pins);
	for (int p=0; p<32; p++) {
		if (*skip_pins & (1<<p)) continue;
				
		if ((new_pins&(1<<p)) && !((*pins)&(1<<p))) {
			LOG_INF("    pin GPIO%d:%d went HIGH", instance, p);
		}
		if (((*pins)&(1<<p)) && !(new_pins&(1<<p))) {
			LOG_INF("    pin GPIO%d:%d went LOW", instance, p);
		}
	}
	*pins = new_pins;
}


void main(void)
{
	//k_sleep(K_MSEC(2000));
	printk("pinfind indahaus");
	//k_sleep(K_MSEC(1000));
	LOG_INF("Pinfind will log a message when GPIO pins change state");
	
#if CONFIG_APP_USE_LED
	setup_led();
#endif

#if CONFIG_APP_USE_GPIO0
	gpio_port_value_t pins_0, skip_pins_0, output_pins_0;
#if CONFIG_APP_LED_MASK_GPIO0
	output_pins_0 = CONFIG_APP_LED_MASK_GPIO0;
#endif
	setup_gpio(0, &gpio0, &skip_pins_0, &output_pins_0, &pins_0);
#endif

#if CONFIG_APP_USE_GPIO1
	gpio_port_value_t pins_1, skip_pins_1, output_pins_1;
#if CONFIG_APP_LED_MASK_GPIO1
	output_pins_1 = CONFIG_APP_LED_MASK_GPIO1;
#endif
	setup_gpio(1, &gpio1, &skip_pins_1, &output_pins_1, &pins_1);
#endif
		
	LOG_INF("GPIO ready");
	//k_sleep(K_MSEC(1000));

#if CONFIG_APP_USE_LED
	uint64_t loops = 0;
	int led_val = 0;
#endif
	
	while (true) {
#if CONFIG_APP_USE_GPIO0
		poll_gpio(0, gpio0, &skip_pins_0, &pins_0);
#endif
#if CONFIG_APP_USE_GPIO1
		poll_gpio(1, gpio1, &skip_pins_1, &pins_1);
#endif

#if CONFIG_APP_USE_LED
		if (led.port && ((loops % 500000) == 0)) {
			led_val = !led_val;
			gpio_pin_set_dt(&led, led_val);
#if CONFIG_APP_USE_GPIO0 && CONFIG_APP_LED_MASK_GPIO0
			for (int i=0;i<32;i++) {
				if (CONFIG_APP_LED_MASK_GPIO0 & (1<<i)) {
					gpio_pin_set(gpio0, i, led_val);
				}
			}
#endif
#if CONFIG_APP_USE_GPIO1 && CONFIG_APP_LED_MASK_GPIO1
			for (int i=0;i<32;i++) {
				if (CONFIG_APP_LED_MASK_GPIO1 & (1<<i)) {
					gpio_pin_set(gpio1, i, led_val);
				}
			}
#endif
		}
		if (++loops>=1000000) {
			LOG_INF("Still here");
			loops=0;
		}
#endif
		//k_sleep(K_MSEC(10));
	}
}
