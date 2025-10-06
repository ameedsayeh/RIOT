
#include <stdio.h>
#include <string.h>

#include "mutex.h"
#include "random.h"
#include "thread.h"
#include "ztimer.h"
// NimBLE
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

#include "host/util/util.h"

#define BLE_SVC_UUID 0xabcd
#define MAX_CONNECTIONS 8
#define MIN_PERIOD 150
#define MAX_PERIOD 300

const char *device_name = "Ameed BLE";
uint8_t address;
int period;

typedef struct {
    uint16_t conn_handle;
    ble_addr_t peer_addr;  // peer_addr.type, peer_addr.val[6]
    uint8_t role;          // BLE_GAP_ROLE_MASTER or BLE_GAP_ROLE_SLAVE
} ble_connection_info;

// Thread-safe connections list
static mutex_t connections_mutex = MUTEX_INIT;
static ble_connection_info connections[MAX_CONNECTIONS];
static int connections_count = 0;
static char scan_adv_stack[THREAD_STACKSIZE_DEFAULT];

static void init_connections(void) {
    mutex_lock(&connections_mutex);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
    connections_count = 0;
    mutex_unlock(&connections_mutex);
}

// Adds handle if space exists and not already present.
// Returns 0 on success, 1 if already present, -1 if full/invalid.
int add_connection(ble_connection_info conn) {
    mutex_lock(&connections_mutex);

    // Check duplicate
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn.conn_handle ||
            ble_addr_cmp(&connections[i].peer_addr, &conn.peer_addr) == 0) {
            mutex_unlock(&connections_mutex);
            printf("Duplicate: %u\n", conn.conn_handle);
            ble_gap_terminate(conn.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            ble_gap_terminate(connections[i].conn_handle,
                              BLE_ERR_REM_USER_CONN_TERM);
            return 1;
        }
    }

    // Find empty slot
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            connections[i].conn_handle = conn.conn_handle;
            connections[i].peer_addr = conn.peer_addr;
            connections[i].role = conn.role;
            connections_count++;
            mutex_unlock(&connections_mutex);
            printf("Added: %u, total: %d\n", conn.conn_handle,
                   connections_count);
            return 0;
        }
    }

    mutex_unlock(&connections_mutex);
    printf("Can't add: %u, total: %d\n", conn.conn_handle, connections_count);
    return -1;  // Full
}

// Removes handle if present.
// Returns 0 on success, -1 if not found.
int remove_connection(ble_connection_info conn) {
    mutex_lock(&connections_mutex);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn.conn_handle) {
            connections[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
            connections[i].peer_addr = (ble_addr_t){0};
            connections[i].role = 0;

            if (connections_count > 0) {
                connections_count--;
            }
            mutex_unlock(&connections_mutex);
            printf("Removed: %u, total: %d\n", conn.conn_handle,
                   connections_count);
            return 0;
        }
    }
    mutex_unlock(&connections_mutex);
    printf("Not found: %u, total: %d\n", conn.conn_handle, connections_count);
    return -1;
}

void advertise(void);
void scan(void);

// Advertise
void start_ble(void);
void stop_ble(void);
void update_period(void);

void sleep_ms(int ms) { ztimer_sleep(ZTIMER_MSEC, ms); }

void wait_for_terminal(void) {
    sleep_ms(5000);
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
    // printf(" address=%02X:%02X:%02X:%02X:%02X:%02X",
    // desc->peer_ota_addr.val[5],
    //        desc->peer_ota_addr.val[4], desc->peer_ota_addr.val[3],
    //        desc->peer_ota_addr.val[2], desc->peer_ota_addr.val[1],
    //        desc->peer_ota_addr.val[0]);
    printf(" conn_interval=%d", desc->conn_itvl);
    // printf(" conn_latency=%d", desc->conn_latency);
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
                printf("[Incoming Connection] ");
                print_conn_desc(&conn_desc);
                ble_connection_info conn_info = {
                    .conn_handle = conn_desc.conn_handle,
                    .peer_addr = conn_desc.peer_ota_addr,
                    .role = conn_desc.role};
                add_connection(conn_info);
            } else {
                printf("[Incoming Connection Failure]\n");
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            printf("[Incoming Disconnection]\n");
            ble_connection_info conn_info = {
                .conn_handle = event->disconnect.conn.conn_handle};
            remove_connection(conn_info);
            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            // printf("[Adv complete]\n");
            sleep_ms(period);
            scan();
            return 0;
    }

    printf("[adv] Event type: %d\n", event->type);

    return 0;
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

    rc = ble_gap_adv_start(address, NULL, BLE_HS_FOREVER, &adv_params,
                           advertise_callback, NULL);
    if (rc != 0) {
        printf("Error starting advertising: %d\n", rc);
    }
}

int connect_callback(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            printf("[Outgoing connection] %d\n", event->connect.status);
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
                ble_connection_info conn_info = {
                    .conn_handle = desc.conn_handle,
                    .peer_addr = desc.peer_ota_addr,
                    .role = desc.role};
                add_connection(conn_info);
            }
            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            printf("[Outgoing disconnection] %d\n", event->disconnect.reason);
            ble_connection_info conn_info = {
                .conn_handle = event->disconnect.conn.conn_handle,
            };
            remove_connection(conn_info);
            return 0;
    }

    printf("[connect] Event type: %d\n", event->type);

    return 0;
}

// Scan
int scan_callback(struct ble_gap_event *event, void *arg) {
    (void)arg;

    struct ble_hs_adv_fields parsed_fields;
    memset(&parsed_fields, 0, sizeof(parsed_fields));

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            struct ble_gap_conn_desc conn_desc;
            // Already connected
            if (ble_gap_conn_find_by_addr(&event->disc.addr, &conn_desc) == 0) {
                return 0;
            }

            ble_hs_adv_parse_fields(&parsed_fields, event->disc.data,
                                    event->disc.length_data);

            const ble_uuid16_t expected_service_id =
                BLE_UUID16_INIT(BLE_SVC_UUID);
            int cmp_result =
                ble_uuid_cmp(&expected_service_id.u, &parsed_fields.uuids16->u);
            if (cmp_result == 0) {
                ble_gap_disc_cancel();
                ble_gap_connect(address, &(event->disc.addr), BLE_HS_FOREVER,
                                NULL, connect_callback, NULL);
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

void update_period(void) {
    period = random_uint32_range(MIN_PERIOD, MAX_PERIOD + 1);
}

void print_connections(void) {
    mutex_lock(&connections_mutex);
    printf("Connections (%d):\n", connections_count);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            printf(
                "- Handle: %u, Role: %s, Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                connections[i].conn_handle,
                (connections[i].role == BLE_GAP_ROLE_MASTER) ? "MASTER"
                                                             : "SLAVE",
                connections[i].peer_addr.val[5],
                connections[i].peer_addr.val[4],
                connections[i].peer_addr.val[3],
                connections[i].peer_addr.val[2],
                connections[i].peer_addr.val[1],
                connections[i].peer_addr.val[0]);
        }
    }
    mutex_unlock(&connections_mutex);
}

void start_ble(void) {
    while (1) {
        update_period();
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
}

int main(void) {
    wait_for_terminal();
    set_gap_device_name();
    prepare_address();
    init_connections();

    thread_create(scan_adv_stack, sizeof(scan_adv_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  (thread_task_func_t)start_ble, NULL, "scan_adv");

    int x = 0;
    while (1) {
        sleep_ms(5000);
        printf("os_msys_num_free(): %d\n", os_msys_num_free());
        if (x % 6 == 0) {
            print_connections();
        }
        x++;
    }

    return 0;
}
