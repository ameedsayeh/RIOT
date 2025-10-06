/**
 * @file ble_operations.c
 * @brief BLE advertising, scanning, and connection operations implementation
 */

#include "ble_operations.h"
#include "ble_connection.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ztimer.h"
#include "random.h"
#include <host/ble_gap.h>
#include <host/ble_hs.h>

/* ========================================================================
 * Private Functions
 * ======================================================================== */

/**
 * @brief Filter and connect to discovered devices
 * @param disc Discovery descriptor
 */
static void filter_and_connect(const struct ble_gap_disc_desc *disc)
{
    ble_connection_t *conn = ble_conn_get_by_addr(&disc->addr);
    if (conn && conn->role == SLAVE && conn->state != BLE_CONN_STATE_CONNECTED) {
        ble_stop_scan();

        struct ble_gap_conn_params conn_params = {
            .scan_itvl = BLE_GAP_SCAN_ITVL_MS(SCN_WIN_MS),
            .scan_window = BLE_GAP_SCAN_WIN_MS(SCN_WIN_MS),
            .itvl_min = BLE_GAP_CONN_ITVL_MS(CONN_ITVL),
            .itvl_max = BLE_GAP_CONN_ITVL_MS(CONN_ITVL),
            .latency = 0,
            .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(20 * CONN_ITVL),
            .min_ce_len = 0,
            .max_ce_len = 0,
        };

        int rc = ble_gap_connect(get_addr_type(), &disc->addr, BLE_HS_FOREVER,
                                 &conn_params, connect_callback, NULL);
        if (rc != 0) {
            printf("Error initiating connection: %d\n", rc);
        }
    }
}

/* ========================================================================
 * Public Functions
 * ======================================================================== */

void ble_stop_advertise(void)
{
    if (ble_gap_adv_active()) {
        ble_gap_adv_stop();
    }
}

void ble_start_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params = (struct ble_gap_adv_params){
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = BLE_GAP_ADV_ITVL_MS(ADV_ITVL_MS),
        .itvl_max = BLE_GAP_ADV_ITVL_MS(ADV_ITVL_MS),
        .channel_map = 0,
        .filter_policy = 0,
        .high_duty_cycle = 0,
    };

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    /* fields for advertisement */
    fields.flags = BLE_HS_ADV_F_DISC_GEN;
    fields.name = (uint8_t *)get_device_name();
    fields.name_len = strlen(get_device_name());
    fields.name_is_complete = 1;
    /* Setting SVCs IDs */
    fields.uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(BLE_SVC_UUID) };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        printf("Error setting advertisement data: %d\n", rc);
    }
    assert(rc == 0);
    rc = ble_gap_adv_start(get_addr_type(), NULL, BLE_HS_FOREVER, &adv_params,
                           advertise_callback, NULL);
    if (rc != 0) {
        printf("Error starting advertising: %d\n", rc);
    }
    ztimer_sleep(ZTIMER_MSEC, ADV_ITVL_MS);
    ble_stop_advertise();
}

void ble_stop_scan(void)
{
    if (ble_gap_disc_active()) {
        ble_gap_disc_cancel();
    }
}

void ble_start_scan(void)
{
    struct ble_gap_disc_params scan_params;
    memset(&scan_params, 0, sizeof(scan_params));

    scan_params = (struct ble_gap_disc_params){
        .itvl = BLE_GAP_ADV_ITVL_MS(SCN_WIN_MS),
        .window = BLE_GAP_ADV_ITVL_MS(SCN_WIN_MS),
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 0
    };

    int rc = ble_gap_disc(get_addr_type(), BLE_HS_FOREVER, &scan_params,
                          scan_callback, NULL);
    if (rc != 0) {
        printf("Error starting scanning: %d\n", rc);
        return;
    }
    ztimer_sleep(ZTIMER_MSEC, SCN_WIN_MS);
    ble_stop_scan();
}

int advertise_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    printf("# GAP event %i\n", (int)event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* Update connection state */
        if (event->connect.status != 0) {
            printf("Connection failed; status=%d\n", event->connect.status);
            break;
        }
        printf("[Connected] ADV_CB\n");
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
            ble_conn_update_state(&desc.peer_ota_addr, event->connect.conn_handle,
                                  BLE_CONN_STATE_CONNECTED);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        /* Update connection state */
        printf("[Disconnected] ADV_CB\n");
        ble_connection_t *conn = ble_conn_get_by_handle(event->disconnect.conn.conn_handle);
        if (conn) {
            ble_conn_update_state(&conn->addr, BLE_HS_CONN_HANDLE_NONE,
                                  BLE_CONN_STATE_DISCONNECTED);
        }
        break;
    default:
        break;
    }
    return 0;
}

int connect_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    printf("# GAP event %i\n", (int)event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            printf("Connection failed; status=%d\n", event->connect.status);
            break;
        }
        printf("[Connected] CNCT_CB\n");
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
            ble_conn_update_state(&desc.peer_ota_addr, event->connect.conn_handle,
                                  BLE_CONN_STATE_CONNECTED);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        printf("[Disconnected] CNCT_CB\n");
        ble_connection_t *conn = ble_conn_get_by_handle(event->disconnect.conn.conn_handle);
        if (conn) {
            ble_conn_update_state(&conn->addr, BLE_HS_CONN_HANDLE_NONE,
                                  BLE_CONN_STATE_DISCONNECTED);
        }
        break;
    default:
        break;
    }
    return 0;
}

void scan_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        filter_and_connect(&event->disc);
        return;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        return;
    default:
        printf("[scan] Event type: %d\n", event->type);
        break;
    }
}

void *connection_manager_thread(void *arg)
{
    (void)arg;

    int mode = 0; /* 0: advertise, 1: scan */

    while (1) {
        uint32_t delay = random_uint32_range(100, 201); /* 100..200 ms */
        ztimer_sleep(ZTIMER_MSEC, delay);

        int continue_mode = 0;

        if (mode == 0 && ble_conn_has_unconnected_role(MASTER)) {
            continue_mode = 1;
        }
        else if (mode == 1 && ble_conn_has_unconnected_role(SLAVE)) {
            continue_mode = 1;
        }

        if (continue_mode) {
            if (mode == 0) {
                ble_start_advertise();
            }
            else {
                ble_start_scan();
            }
        }

        mode = 1 - mode;
    }
}
