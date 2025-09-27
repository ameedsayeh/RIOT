
#include <stdio.h>
#include <string.h>
#include "random.h"
#include "ztimer.h"
// NimBLE
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

#include "host/util/util.h"

#define BLE_SVC_UUID 0xabcd

const char *device_name = "Ameed BLE";
uint8_t address;

void wait_for_terminal(void) {
    ztimer_sleep(ZTIMER_MSEC, 5000);
    printf("Device is ready!\n");
}

void set_gap_device_name(void) {
    // Setting GAP device name
    printf("Setting device name to: %s\n", device_name);
    int rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        printf("Error setting device name: %d\n", rc);
    }
    assert(rc == 0);
}

void prepare_address(void) {
    int rc;
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &address);
    assert(rc == 0);
}

void print_conn_desc(struct ble_gap_conn_desc *desc) {
    printf("handle=[%d]", desc->conn_handle);
    printf(" address=%02X:%02X:%02X:%02X:%02X:%02X", desc->peer_ota_addr.val[5],
           desc->peer_ota_addr.val[4], desc->peer_ota_addr.val[3],
           desc->peer_ota_addr.val[2], desc->peer_ota_addr.val[1],
           desc->peer_ota_addr.val[0]);
    printf(" conn_interval=%d", desc->conn_itvl);
    printf(" conn_latency=%d", desc->conn_latency);
    printf(" Role=%s\n",
           (desc->role == BLE_GAP_ROLE_MASTER) ? "MASTER" : "SLAVE");
}

// Advertise
int advertise_callback(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            struct ble_gap_conn_desc conn_desc;
            int rc = ble_gap_conn_find(event->connect.conn_handle, &conn_desc);
            if (rc == 0) {
                printf("[Connected] ");
                print_conn_desc(&conn_desc);
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            printf("[Disconnected] ");
            print_conn_desc(&event->disconnect.conn);
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            return 0;
    }

    printf("[adv] Event type: %d\n", event->type);

    return 0;
}

int connect_callback(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            printf("Connected %d\n", event->connect.status);
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            printf("Disconnected\n");
            return 0;
    }

    printf("[connect] Event type: %d\n", event->type);

    return 0;
}

// Scan
int scan_callback(struct ble_gap_event *event, void *arg) {
    (void)arg;

    // int uuid_cmp_result;
    struct ble_hs_adv_fields parsed_fields;
    memset(&parsed_fields, 0, sizeof(parsed_fields));

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            struct ble_gap_conn_desc conn_desc;
            // Already connected
            if (ble_gap_conn_find_by_addr(&event->disc.addr, &conn_desc) == 0) {
                // printf("Found already connected device: ");
                // print_conn_desc(&conn_desc);
                return 0;
            }

            ble_hs_adv_parse_fields(&parsed_fields, event->disc.data,
                                    event->disc.length_data);

            const ble_uuid16_t expected_service_id =
                BLE_UUID16_INIT(BLE_SVC_UUID);
            int cmp_result =
                ble_uuid_cmp(&expected_service_id.u, &parsed_fields.uuids16->u);
            if (cmp_result == 0) {
                printf("Found device advertising the service\n");
                ble_gap_disc_cancel();
                // connect to it
                int rc = ble_gap_connect(address, &(event->disc.addr),
                                         BLE_HS_FOREVER, NULL, connect_callback,
                                         NULL);
                if (rc != 0) {
                    printf("Error connecting to device: rc=%d\n", rc);
                } else {
                    printf("Connecting...\n");
                }
            }

            return 0;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            return 0;
    }

    printf("[scan] Event type: %d\n", event->type);
    return 0;
}

void scan(void) {
    struct ble_gap_disc_params disc_params = {.itvl = 10000,
                                              .window = 100,
                                              .filter_policy = 0,
                                              .limited = 0,
                                              .passive = 0,
                                              .filter_duplicates = 1};

    int rc = ble_gap_disc(address, BLE_HS_FOREVER, &disc_params, scan_callback,
                          NULL);
    if (rc != 0) {
        printf("Error starting scanning: rc=%d\n", rc);
    }
}

void advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    // Clear the adv_params and fields
    memset(&adv_params, 0, sizeof(adv_params));
    memset(&fields, 0, sizeof(fields));

    // Settings adv_params
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // fields for advertisement
    fields.flags = BLE_HS_ADV_F_DISC_GEN;
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    // Setting SVCs IDs
    fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(BLE_SVC_UUID)};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        printf("Error setting advertisement data: %d\n", rc);
    }
    assert(rc == 0);

    rc = ble_gap_adv_start(address, NULL, BLE_HS_FOREVER, &adv_params, advertise_callback,
                           NULL);
    if (rc != 0) {
        printf("Error starting advertising: %d\n", rc);
    }
    assert(rc == 0);
}

int main(void) {
    wait_for_terminal();
    set_gap_device_name();
    prepare_address();
    int period;
    while (1) {
        period = random_uint32_range(100, 301);
        if (ble_gap_disc_active() == 0 && ble_gap_adv_active() == 0) {
            advertise();
            ztimer_sleep(ZTIMER_MSEC, period);
            ble_gap_adv_stop();
            ztimer_sleep(ZTIMER_MSEC, period);
        } else {
            printf("Problem after advertise!\n");
            ztimer_sleep(ZTIMER_MSEC, period);
        }

        if (ble_gap_disc_active() == 0 && ble_gap_adv_active() == 0) {
            scan();
            ztimer_sleep(ZTIMER_MSEC, period);
            ble_gap_disc_cancel();
            ztimer_sleep(ZTIMER_MSEC, period);
        } else {
            printf("Problem after scan!\n");
            ztimer_sleep(ZTIMER_MSEC, period);
        }
    }

    return 0;
}
