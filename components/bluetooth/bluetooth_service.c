#include "bluetooth_service.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"

static const char *TAG = "bluetooth";

static bluetooth_status_t s_status = {
    .enabled = false,
    .state = BLUETOOTH_LINK_IDLE,
};
static bluetooth_scan_result_t s_results[BLUETOOTH_MAX_SCAN_RESULTS];
static size_t s_result_count = 0;
static SemaphoreHandle_t s_lock = NULL;
static uint8_t s_own_addr_type = 0;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_host_started = false;
static SemaphoreHandle_t s_host_exited = NULL;
static SemaphoreHandle_t s_lifecycle_lock = NULL;
static uint32_t s_scan_packets = 0;
static uint32_t s_scan_named_packets = 0;
static uint32_t s_scan_event_type_count[8] = {0};
static size_t s_selected_result_index = SIZE_MAX;
static bool s_name_read_in_progress = false;

void ble_store_config_init(void);

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static void lock(void)
{
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    if (s_lock != NULL) xSemaphoreGive(s_lock);
}

static void bump_generation(void)
{
    ++s_status.generation;
}

static void format_addr(const uint8_t addr[6], char out[BLUETOOTH_ADDRESS_STR_MAX])
{
    snprintf(out, BLUETOOTH_ADDRESS_STR_MAX,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static int find_result(const ble_addr_t *addr)
{
    for (size_t i = 0; i < s_result_count; ++i) {
        if (s_results[i].addr_type == addr->type &&
            memcmp(s_results[i].addr, addr->val, sizeof(addr->val)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool extract_local_name(const uint8_t *data,
                               uint8_t data_len,
                               char *name,
                               size_t name_size)
{
    if (data == NULL || name == NULL || name_size == 0) return false;

    size_t offset = 0;
    while (offset < data_len) {
        uint8_t field_len = data[offset];
        if (field_len == 0) break;

        size_t total = (size_t)field_len + 1U;
        if (offset + total > data_len || field_len < 1) break;

        uint8_t type = data[offset + 1];
        if (type == BLE_HS_ADV_TYPE_COMP_NAME ||
            type == BLE_HS_ADV_TYPE_INCOMP_NAME) {
            size_t text_len = (size_t)field_len - 1U;
            if (text_len >= name_size) text_len = name_size - 1U;
            memcpy(name, &data[offset + 2], text_len);
            name[text_len] = '\0';
            return text_len > 0;
        }

        offset += total;
    }

    return false;
}

static void save_discovery(const struct ble_gap_disc_desc *disc)
{
    char name[BLUETOOTH_DEVICE_NAME_MAX] = {0};

    /* Some peripherals put Local Name only in the active-scan response.
     * Walk AD structures directly so a malformed/unrecognised unrelated AD
     * field cannot make ble_hs_adv_parse_fields() hide an otherwise valid
     * 0x08/0x09 Local Name field. */
    bool has_name = extract_local_name(
        disc->data, disc->length_data, name, sizeof(name));

    if (!has_name) {
        /* Keep the normal NimBLE parser as a second path. */
        struct ble_hs_adv_fields fields = {0};
        if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) == 0 &&
            fields.name != NULL && fields.name_len > 0) {
            size_t len = fields.name_len;
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, fields.name, len);
            name[len] = '\0';
            has_name = true;
        }
    }

    /* The Settings page is a pairing picker, so keep only connectable
     * advertisers. Scan Response (event type 4) is accepted only when we
     * already saw the same address advertising connectability. This mirrors
     * NimBLE central examples, which require ADV_IND / DIR_IND before connect. */
    bool is_connectable_adv =
        disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
        disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND;
    bool is_scan_response =
        disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP;

    lock();

    ++s_scan_packets;
    if (has_name) ++s_scan_named_packets;
    if (disc->event_type < (sizeof(s_scan_event_type_count) /
                            sizeof(s_scan_event_type_count[0]))) {
        ++s_scan_event_type_count[disc->event_type];
    }

    int idx = find_result(&disc->addr);

    if (!is_connectable_adv && !is_scan_response) {
        unlock();
        return;
    }

    /* A scan response is useful only when it belongs to a connectable
     * advertiser already present in our pairing list. */
    if (is_scan_response && idx < 0) {
        unlock();
        return;
    }

    bool is_new = idx < 0;

    if (is_new) {
        if (s_result_count >= BLUETOOTH_MAX_SCAN_RESULTS) {
            /* The settings page is a device picker, not a radio sniffer.
             * Prefer devices that actually advertise a Local Name. Otherwise
             * a room full of anonymous beacons/random-address advertisements
             * can occupy all visible slots before a useful peripheral appears. */
            int replace = -1;

            if (has_name) {
                for (size_t i = 0; i < s_result_count; ++i) {
                    if (strcmp(s_results[i].name, "BLE device") == 0) {
                        if (replace < 0 ||
                            s_results[i].rssi < s_results[replace].rssi) {
                            replace = (int)i;
                        }
                    }
                }
            }

            if (replace < 0) {
                int weakest = 0;
                for (size_t i = 1; i < s_result_count; ++i) {
                    if (s_results[i].rssi < s_results[weakest].rssi) {
                        weakest = (int)i;
                    }
                }

                /* Never evict a named device merely to show a stronger
                 * anonymous advertiser. */
                bool weakest_named =
                    strcmp(s_results[weakest].name, "BLE device") != 0;
                if (!has_name && weakest_named) {
                    unlock();
                    return;
                }
                if (disc->rssi <= s_results[weakest].rssi && !has_name) {
                    unlock();
                    return;
                }
                replace = weakest;
            }

            idx = replace;
        }
        else {
            idx = (int)s_result_count++;
        }
    }

    bluetooth_scan_result_t *dst = &s_results[idx];

    if (is_new || memcmp(dst->addr, disc->addr.val, sizeof(dst->addr)) != 0 ||
        dst->addr_type != disc->addr.type) {
        memset(dst, 0, sizeof(*dst));
        memcpy(dst->addr, disc->addr.val, sizeof(dst->addr));
        dst->addr_type = disc->addr.type;
        format_addr(dst->addr, dst->address);
        strlcpy(dst->name, "BLE device", sizeof(dst->name));
    }

    /* Always refresh signal strength, but never erase a previously discovered
     * name just because a later advertisement omits the Local Name field. */
    dst->rssi = disc->rssi;
    if (has_name && name[0] != '\0') {
        strlcpy(dst->name, name, sizeof(dst->name));
        ESP_LOGI(TAG,
                 "device name: %s (%s), event_type=%u rssi=%d data_len=%u",
                 dst->name,
                 dst->address,
                 (unsigned)disc->event_type,
                 (int)dst->rssi,
                 (unsigned)disc->length_data);
    }

    bump_generation();
    unlock();
}

static int device_name_read_cb(uint16_t conn_handle,
                               const struct ble_gatt_error *error,
                               struct ble_gatt_attr *attr,
                               void *arg)
{
    (void)arg;

    if (error->status == 0 && attr != NULL && attr->om != NULL) {
        uint16_t len = OS_MBUF_PKTLEN(attr->om);
        if (len >= BLUETOOTH_DEVICE_NAME_MAX) {
            len = BLUETOOTH_DEVICE_NAME_MAX - 1;
        }

        char name[BLUETOOTH_DEVICE_NAME_MAX] = {0};
        if (len > 0 && os_mbuf_copydata(attr->om, 0, len, name) == 0) {
            name[len] = '\0';

            lock();
            strlcpy(s_status.peer_name, name, sizeof(s_status.peer_name));
            if (s_selected_result_index < s_result_count) {
                strlcpy(s_results[s_selected_result_index].name,
                        name,
                        sizeof(s_results[s_selected_result_index].name));
            }
            bump_generation();
            unlock();

            ESP_LOGI(TAG, "GATT device name: %s", name);
        }
        return 0;
    }

    if (error->status == BLE_HS_EDONE) {
        s_name_read_in_progress = false;
        return 0;
    }

    s_name_read_in_progress = false;
    ESP_LOGW(TAG, "GATT device-name read failed: %d", error->status);
    return 0;
}

static void try_read_device_name(uint16_t conn_handle)
{
    if (s_name_read_in_progress) return;

    lock();
    bool needs_name =
        s_status.peer_name[0] == '\0' ||
        strcmp(s_status.peer_name, "BLE device") == 0;
    unlock();

    if (!needs_name) return;

    static const ble_uuid16_t device_name_uuid = BLE_UUID16_INIT(0x2A00);
    int rc = ble_gattc_read_by_uuid(conn_handle,
                                    0x0001,
                                    0xFFFF,
                                    &device_name_uuid.u,
                                    device_name_read_cb,
                                    NULL);
    if (rc == 0) {
        s_name_read_in_progress = true;
        ESP_LOGI(TAG, "reading GAP Device Name (0x2A00)");
    }
    else {
        ESP_LOGW(TAG, "could not start GAP Device Name read: %d", rc);
    }
}

static void host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    /* Wake the settings/UI task before the FreeRTOS host task exits. */
    if (s_host_exited) xSemaphoreGive(s_host_exited);
    nimble_port_freertos_deinit();
}

static void on_reset(int reason)
{
    lock();
    s_status.ready = false;
    s_status.scanning = false;
    s_status.connected = false;
    s_status.bonded = false;
    s_status.state = BLUETOOTH_LINK_IDLE;
    s_status.last_error = reason;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    bump_generation();
    unlock();
    ESP_LOGW(TAG, "NimBLE reset, reason=%d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &s_own_addr_type);

    lock();
    s_status.ready = rc == 0;
    s_status.last_error = rc;
    bump_generation();
    unlock();

    if (rc == 0) ESP_LOGI(TAG, "NimBLE ready");
    else ESP_LOGE(TAG, "NimBLE address setup failed: %d", rc);
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    struct ble_gap_conn_desc desc = {0};

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            save_discovery(&event->disc);
            return 0;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            lock();
            s_status.scanning = false;
            if (!s_status.connected) s_status.state = BLUETOOTH_LINK_IDLE;
            s_status.last_error = event->disc_complete.reason;
            bump_generation();
            unlock();
            ESP_LOGI(TAG,
                     "scan complete: reason=%d pairable=%u packets=%u named_packets=%u "
                     "adv_ind=%u dir_ind=%u scan_ind=%u nonconn=%u scan_rsp=%u",
                     event->disc_complete.reason,
                     (unsigned)s_result_count,
                     (unsigned)s_scan_packets,
                     (unsigned)s_scan_named_packets,
                     (unsigned)s_scan_event_type_count[0],
                     (unsigned)s_scan_event_type_count[1],
                     (unsigned)s_scan_event_type_count[2],
                     (unsigned)s_scan_event_type_count[3],
                     (unsigned)s_scan_event_type_count[4]);
            return 0;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status != 0) {
                lock();
                s_status.connected = false;
                s_status.bonded = false;
                s_status.state = BLUETOOTH_LINK_IDLE;
                s_status.last_error = event->connect.status;
                s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
                bump_generation();
                unlock();
                ESP_LOGW(TAG, "connect failed: %d", event->connect.status);
                return 0;
            }

            s_conn_handle = event->connect.conn_handle;
            if (ble_gap_conn_find(s_conn_handle, &desc) == 0) {
                lock();
                s_status.connected = true;
                s_status.state = BLUETOOTH_LINK_PAIRING;
                s_status.last_error = 0;
                s_status.bonded = desc.sec_state.bonded;
                format_addr(desc.peer_id_addr.val, s_status.peer_address);
                bump_generation();
                unlock();
            }

            try_read_device_name(s_conn_handle);

            /* Initiate security immediately. Just-Works peers complete without
             * user input; peers that need a passkey will stay connected but
             * report the security error for the next UI iteration. */
            {
                int rc = ble_gap_security_initiate(s_conn_handle);
                if (rc != 0) {
                    lock();
                    s_status.state = BLUETOOTH_LINK_CONNECTED;
                    s_status.last_error = rc;
                    bump_generation();
                    unlock();
                    ESP_LOGW(TAG, "security initiate returned %d", rc);
                }
            }
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
                lock();
                s_status.connected = true;
                s_status.bonded = desc.sec_state.bonded;
                s_status.state = BLUETOOTH_LINK_CONNECTED;
                s_status.last_error = event->enc_change.status;
                bump_generation();
                unlock();
                ESP_LOGI(TAG, "security changed: status=%d encrypted=%d bonded=%d",
                         event->enc_change.status,
                         desc.sec_state.encrypted,
                         desc.sec_state.bonded);
                if (event->enc_change.status == 0) {
                    try_read_device_name(event->enc_change.conn_handle);
                }
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            lock();
            s_status.connected = false;
            s_status.bonded = false;
            s_status.state = BLUETOOTH_LINK_IDLE;
            s_status.last_error = event->disconnect.reason;
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_name_read_in_progress = false;
            s_selected_result_index = SIZE_MAX;
            s_status.peer_name[0] = '\0';
            s_status.peer_address[0] = '\0';
            bump_generation();
            unlock();
            ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        default:
            return 0;
    }
}

esp_err_t bluetooth_service_init(void)
{
    if (s_status.initialized) return ESP_OK;

    /* NimBLE/BT controller uses NVS and needs a relatively large contiguous
     * chunk of internal DRAM. Initialize it before the display/UI/Wi-Fi stack
     * so the controller/HCI gets first choice of internal memory. */
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed before NimBLE: %s",
                 esp_err_to_name(nvs_err));
        return nvs_err;
    }

    ESP_LOGI(TAG, "before NimBLE: internal free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_host_exited) s_host_exited = xSemaphoreCreateBinary();
    if (s_lock == NULL || s_host_exited == NULL) return ESP_ERR_NO_MEM;
    (void)xSemaphoreTake(s_host_exited, 0);

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "nimble_port_init failed: %s; internal free=%u largest=%u",
                 esp_err_to_name(err),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        return err;
    }

    ESP_LOGI(TAG, "after NimBLE init: internal free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;

    /* Central/observer-only role: no local GAP peripheral service is needed.
     * Avoid ble_svc_gap_* here because those symbols are not linked when the
     * peripheral role is disabled in ESP-IDF 5.5.x. */
    ble_store_config_init();

    lock();
    s_status.initialized = true;
    s_status.enabled = true;
    bump_generation();
    unlock();

    if (!s_host_started) {
        s_host_started = true;
        nimble_port_freertos_init(host_task);
    }

    return ESP_OK;
}

esp_err_t bluetooth_service_set_enabled(bool enabled)
{
    /* Settings can call this repeatedly; serialize controller lifecycle. */
    if (!s_lifecycle_lock) {
        s_lifecycle_lock = xSemaphoreCreateMutex();
        if (!s_lifecycle_lock) return ESP_ERR_NO_MEM;
    }
    if (xSemaphoreTake(s_lifecycle_lock, pdMS_TO_TICKS(3000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = ESP_OK;
    if (enabled) {
        if (!s_status.initialized) {
            ESP_LOGI(TAG, "BLE ON before init: internal=%u largest=%u",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            err = bluetooth_service_init();
        }
        if (err == ESP_OK) {
            lock();
            s_status.enabled = true;
            bump_generation();
            unlock();
        }
    } else if (s_status.initialized) {
        (void)bluetooth_service_stop_scan();
        (void)bluetooth_service_disconnect();
        /* A connected peer may require a disconnect callback before the
         * host can stop; don't force deinit if the stack refuses to stop. */
        int rc = nimble_port_stop();
        if (rc != 0) {
            ESP_LOGE(TAG, "nimble_port_stop failed: %d", rc);
            err = ESP_FAIL;
        } else if (xSemaphoreTake(s_host_exited, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ESP_LOGE(TAG, "NimBLE host stop timed out; retain stack");
            err = ESP_ERR_TIMEOUT;
        } else {
            /* Host signals immediately before deleting its FreeRTOS task.
             * Allow that task to finish before destroying port resources. */
            vTaskDelay(pdMS_TO_TICKS(20));
            err = nimble_port_deinit();
            if (err == ESP_OK) {
                s_host_started = false;
                lock();
                s_status.initialized = false;
                s_status.enabled = false;
                s_status.ready = false;
                s_status.scanning = false;
                s_status.connected = false;
                s_status.bonded = false;
                s_status.state = BLUETOOTH_LINK_IDLE;
                s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
                s_result_count = 0;
                bump_generation();
                unlock();
                ESP_LOGI(TAG, "BLE OFF after deinit: internal=%u largest=%u",
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            } else {
                ESP_LOGE(TAG, "nimble_port_deinit failed: %s",
                         esp_err_to_name(err));
            }
        }
    } else {
        lock();
        s_status.enabled = false;
        bump_generation();
        unlock();
    }

    xSemaphoreGive(s_lifecycle_lock);
    return err;
}

void bluetooth_service_get_status(bluetooth_status_t *status)
{
    if (status == NULL) return;
    lock();
    *status = s_status;
    unlock();
}

esp_err_t bluetooth_service_start_scan(void)
{
    bluetooth_status_t status;
    bluetooth_service_get_status(&status);
    if (!status.initialized || !status.ready) return ESP_ERR_INVALID_STATE;
    if (!status.enabled) return ESP_ERR_INVALID_STATE;
    if (status.connected) return ESP_ERR_INVALID_STATE;

    (void)ble_gap_disc_cancel();

    lock();
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    s_scan_packets = 0;
    s_scan_named_packets = 0;
    memset(s_scan_event_type_count, 0, sizeof(s_scan_event_type_count));
    s_status.scanning = true;
    s_status.state = BLUETOOTH_LINK_SCANNING;
    s_status.last_error = 0;
    bump_generation();
    unlock();

    struct ble_gap_disc_params params = {0};
    /* Match ESP-IDF's BLE HID host active-scan timing. A non-zero scan
     * window is important here because many nearby devices expose their
     * Local Name only in the scan-response packet. */
    params.filter_duplicates = 0;
    params.passive = 0;
    /* ESP32-S3 Wi-Fi and BLE share one 2.4 GHz radio. Espressif recommends
     * interval == window in coexistence scenarios so BLE can reacquire RF
     * time during the same scan window after Wi-Fi activity. */
    params.itvl = 0x50;
    params.window = 0x50;
    params.filter_policy = 0;
    params.limited = 0;

    int rc = ble_gap_disc(s_own_addr_type, 7000, &params, gap_event_cb, NULL);
    if (rc != 0) {
        lock();
        s_status.scanning = false;
        s_status.state = BLUETOOTH_LINK_IDLE;
        s_status.last_error = rc;
        bump_generation();
        unlock();
        ESP_LOGE(TAG, "scan start failed: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "scan started");
    return ESP_OK;
}

esp_err_t bluetooth_service_stop_scan(void)
{
    int rc = ble_gap_disc_cancel();
    lock();
    s_status.scanning = false;
    if (!s_status.connected) s_status.state = BLUETOOTH_LINK_IDLE;
    bump_generation();
    unlock();

    return (rc == 0 || rc == BLE_HS_EALREADY) ? ESP_OK : ESP_FAIL;
}

size_t bluetooth_service_get_scan_results(bluetooth_scan_result_t *results,
                                          size_t capacity)
{
    lock();
    size_t count = s_result_count;
    if (results != NULL && capacity > 0) {
        if (count > capacity) count = capacity;
        memcpy(results, s_results, count * sizeof(results[0]));
    }
    unlock();
    return count;
}

esp_err_t bluetooth_service_connect(size_t index)
{
    bluetooth_scan_result_t peer;

    lock();
    if (!s_status.enabled || !s_status.ready || index >= s_result_count) {
        unlock();
        return ESP_ERR_INVALID_ARG;
    }
    peer = s_results[index];
    s_selected_result_index = index;
    strlcpy(s_status.peer_name, peer.name, sizeof(s_status.peer_name));
    strlcpy(s_status.peer_address, peer.address, sizeof(s_status.peer_address));
    s_status.scanning = false;
    s_status.state = BLUETOOTH_LINK_CONNECTING;
    s_status.last_error = 0;
    bump_generation();
    unlock();

    (void)ble_gap_disc_cancel();

    ble_addr_t addr = {
        .type = peer.addr_type,
    };
    memcpy(addr.val, peer.addr, sizeof(addr.val));

    int rc = ble_gap_connect(s_own_addr_type, &addr, 15000,
                             NULL, gap_event_cb, NULL);
    if (rc != 0) {
        lock();
        s_status.state = BLUETOOTH_LINK_IDLE;
        s_status.last_error = rc;
        bump_generation();
        unlock();
        ESP_LOGE(TAG, "connect start failed: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "connecting to %s (%s)", peer.name, peer.address);
    return ESP_OK;
}

esp_err_t bluetooth_service_disconnect(void)
{
    uint16_t handle = s_conn_handle;
    if (handle == BLE_HS_CONN_HANDLE_NONE) return ESP_OK;

    int rc = ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t bluetooth_service_forget_peer(void)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return ESP_ERR_INVALID_STATE;

    struct ble_gap_conn_desc desc = {0};
    int rc = ble_gap_conn_find(s_conn_handle, &desc);
    if (rc != 0) return ESP_FAIL;

    rc = ble_store_util_delete_peer(&desc.peer_id_addr);
    if (rc != 0) return ESP_FAIL;

    lock();
    s_status.bonded = false;
    bump_generation();
    unlock();
    return ESP_OK;
}
