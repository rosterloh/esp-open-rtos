/* http_get - Retrieves a web page over HTTP GET.
 *
 * See http_get_ssl for a TLS-enabled version.
 *
 * This sample code is in the public domain.,
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "ssid_config.h"

#define WEB_SERVER "chainxor.org"
#define WEB_PORT 80
#define WEB_URL "http://chainxor.org/"

void http_get_task(void *pvParameters)
{
    int successes = 0, failures = 0;
    printf("HTTP get task starting...\r\n");

    while(1) {
        const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res;

        printf("Running DNS lookup for %s...\r\n", WEB_SERVER);
        int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);

        if(err != 0 || res == NULL) {
            printf("DNS lookup failed err=%d res=%p\r\n", err, res);
            if(res)
                freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_RATE_MS);
            failures++;
            continue;
        }
        /* Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        struct in_addr *addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        printf("DNS lookup succeeded. IP=%s\r\n", inet_ntoa(*addr));

        int s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            printf("... Failed to allocate socket.\r\n");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_RATE_MS);
            failures++;
            continue;
        }

        printf("... allocated socket\r\n");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            close(s);
            freeaddrinfo(res);
            printf("... socket connect failed.\r\n");
            vTaskDelay(4000 / portTICK_RATE_MS);
            failures++;
            continue;
        }

        printf("... connected\r\n");
        freeaddrinfo(res);

        const char *req =
            "GET "WEB_URL"\r\n"
            "User-Agent: esp-open-rtos/0.1 esp8266\r\n"
            "\r\n";
        if (write(s, req, strlen(req)) < 0) {
            printf("... socket send failed\r\n");
            close(s);
            vTaskDelay(4000 / portTICK_RATE_MS);
            failures++;
            continue;
        }
        printf("... socket send success\r\n");

        static char recv_buf[128];
        int r;
        do {
            bzero(recv_buf, 128);
            r = read(s, recv_buf, 127);
            if(r > 0) {
                printf("%s", recv_buf);
            }
        } while(r > 0);

        printf("... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
        if(r != 0)
            failures++;
        else
            successes++;
        close(s);
        printf("successes = %d failures = %d\r\n", successes, failures);
        for(int countdown = 10; countdown >= 0; countdown--) {
            printf("%d... ", countdown);
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
        printf("\r\nStarting again!\r\n");
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&http_get_task, (const char * const)"get_task", 256, NULL, 2, NULL);
}

