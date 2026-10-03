#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLUETOOTH_MAX_SCAN_RESULTS 8
#define BLUETOOTH_DEVICE_NAME_MAX  32
#define BLUETOOTH_ADDRESS_STR_MAX  18

typedef enum {
    BLUETOOTH_LINK_IDLE = 0,
    BLUETOOTH_LINK_SCANNING,
    BLUETOOTH_LINK_CONNECTING,
    BLUETOOTH_LINK_PAIRING,
    BLUETOOTH_LINK_CONNECTED,
} bluetooth_link_state_t;

typedef struct {
    char name[BLUETOOTH_DEVICE_NAME_MAX];
    char address[BLUETOOTH_ADDRESS_STR_MAX];
    int8_t rssi;
    uint8_t addr_type;
    uint8_t addr[6];
} bluetooth_scan_result_t;

typedef struct {
    bool initialized;
    bool ready;
    bool enabled;
    bool scanning;
    bool connected;
    bool bonded;
    bluetooth_link_state_t state;
    char peer_name[BLUETOOTH_DEVICE_NAME_MAX];
    char peer_address[BLUETOOTH_ADDRESS_STR_MAX];
    int last_error;
    uint32_t generation;
} bluetooth_status_t;

esp_err_t bluetooth_service_init(void);
esp_err_t bluetooth_service_set_enabled(bool enabled);
void bluetooth_service_get_status(bluetooth_status_t *status);

esp_err_t bluetooth_service_start_scan(void);
esp_err_t bluetooth_service_stop_scan(void);
size_t bluetooth_service_get_scan_results(bluetooth_scan_result_t *results,
                                          size_t capacity);

esp_err_t bluetooth_service_connect(size_t index);
esp_err_t bluetooth_service_disconnect(void);
esp_err_t bluetooth_service_forget_peer(void);

#ifdef __cplusplus
}
#endif
