#include <stdio.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <rmw_microros/rmw_microros.h>

#include "pico/stdlib.h"
//#include "pico_uart_transports.h"
#include "pico/cyw43_arch.h"

#include "picow_udp_transports.h" 

#if 0
extern bool picow_udp_transport_open(struct uxrCustomTransport * transport);
extern bool picow_udp_transport_close(struct uxrCustomTransport * transport);
extern size_t picow_udp_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
extern size_t picow_udp_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);
#endif

rcl_publisher_t publisher;
std_msgs__msg__Int32 msg;

char ssid[] = "riplab";
char pass[] = "Aut0m@tion";

struct SensorReading {
    uint64_t timestamp_us;
    int value;
};

struct SensorReading readSensor() {
    struct SensorReading reading;
    reading.timestamp_us = time_us_64(); // microseconds since boot
    reading.value = 35;               // fake sensor value
    return reading;
}

void timer_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    rcl_ret_t ret = rcl_publish(&publisher, &msg, NULL);
    struct SensorReading reading = readSensor();
    msg.data = reading.value;
}


int main()
{
    #if 0
    rmw_uros_set_custom_transport(
		true,
		NULL,
		pico_serial_transport_open,
		pico_serial_transport_close,
		pico_serial_transport_write,
		pico_serial_transport_read
	);
#endif

    //gpio_init(LED_PIN);
    //gpio_set_dir(LED_PIN, GPIO_OUT);
    
    stdio_init_all();
    cyw43_arch_init_with_country(CYW43_COUNTRY_USA);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    cyw43_arch_enable_sta_mode();
    cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000);
    
    rmw_uros_set_custom_transport(
	false,          // must be false for UDP
	&picow_params, //NULL,
	picow_udp_transport_open,
	picow_udp_transport_close,
	picow_udp_transport_write,
	picow_udp_transport_read
	);
    
    rcl_timer_t timer;
    rcl_node_t node;
    rcl_allocator_t allocator;
    rclc_support_t support;
    rclc_executor_t executor;

    allocator = rcl_get_default_allocator();

    // Wait for agent successful ping for 1 minutes.
    const int timeout_ms = 1000;
    const uint8_t attempts = 60;
    rcl_ret_t ret = 0;
    //uint8_t blink_led = 1;

    int loop = 0;
    for ( ; loop < attempts; loop++) {
        printf("uros_ping_agent: loop[%d]\n", loop);
        ret = rmw_uros_ping_agent(timeout_ms, 1);


        if (ret == RCL_RET_OK)
        {
            // Reachable agent, exiting loop.
            //cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            break;
        }
        //cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, blink_led);
        //blink_led = blink_led ? 0 : 1;
    }
    if (loop == attempts) {
        // Unreachable agent, exiting program.
        //cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        return ret;
    }

    rclc_support_init(&support, 0, NULL, &allocator);

    rclc_node_init_default(&node, "pico_node", "", &support);
    rclc_publisher_init_default(
        &publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "pico_publisher");

    rclc_timer_init_default2(
        &timer,
        &support,
        RCL_MS_TO_NS(1000),
        timer_callback,
        1);

    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_timer(&executor, &timer);

    //gpio_put(LED_PIN, 1);

    msg.data = 0;
    while (true)
    {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
    }
    cyw43_arch_deinit();
    
    return 0;
}
