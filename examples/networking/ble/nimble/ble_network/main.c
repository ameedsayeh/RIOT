#include <stdio.h>

#include "msg.h"
#include "mutex.h"
#include "shell.h"
#include "thread.h"
#include "ztimer.h"

// NimBLE
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

typedef enum {
    BLE_CONN_INIT = 0,
    BLE_CONN_STATE_CONNECTED,
    BLE_CONN_STATE_DISCONNECTED
} ble_conn_state_t;

typedef enum { MASTER = 0,
               SLAVE } ble_conn_role_t;

typedef struct {
    ble_addr_t addr;
    ble_conn_role_t role;
    uint16_t conn_handle;
    ble_conn_state_t state;
    uint8_t in_use;
} ble_connection_t;

#define MAIN_QUEUE_SIZE (8)
#define MAX_CONNECTIONS (10)
#define ADV_ITVL_MS     (90U)
#define SCN_WIN_MS      (100U)
#define CONN_ITVL       (75U)
#define CONN_TIMEOUT_MS (600U)

#define BLE_SVC_UUID    0xff00
#define NOTIFY_CHR_UUID 0xee00
#define WRITE_CHR_UUID  0xee01

/* Convert UUIDs to ble_uuid16_t */
// ble_uuid16_t custom_svc_uuid = BLE_UUID16_INIT(CUSTOM_SVC_UUID);
// ble_uuid16_t custom_notify_chr_uuid =
// BLE_UUID16_INIT(CUSTOM_NOTIFY_CHR_UUID); ble_uuid16_t custom_write_chr_uuid =
// BLE_UUID16_INIT(CUSTOM_WRITE_CHR_UUID);

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static char _conn_mgr_stack[THREAD_STACKSIZE_DEFAULT];

// Connections list
static ble_connection_t _conns[MAX_CONNECTIONS];
// Lock to protect the list
static mutex_t _conns_lock = MUTEX_INIT;
uint8_t addr_type;
const char *device_name = "BLE_Device";

static const char *_state_str(ble_conn_state_t s)
{
    switch (s) {
    case BLE_CONN_INIT:
        return "init";
    case BLE_CONN_STATE_CONNECTED:
        return "connected";
    case BLE_CONN_STATE_DISCONNECTED:
        return "disconnected";
    default:
        return "?";
    }
}

static const char *_role_str(ble_conn_role_t r)
{
    return (r == MASTER) ? "master" : "slave";
}

static int _parse_addr(const char *s, ble_addr_t *out)
{
    unsigned int b[6];
    if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x", &b[0], &b[1], &b[2], &b[3], &b[4],
               &b[5]) != 6) {
        return -1;
    }
    /* BLE convention: bytes in order as entered */
    for (int i = 0; i < 6; i++) {
        out->val[5 - i] = (uint8_t)b[i];
    }
    out->type = BLE_ADDR_PUBLIC; /* adjust if you expect random addresses */
    return 0;
}

static int _ble_addr_t_equal(const ble_addr_t *a, const ble_addr_t *b)
{
    return (memcmp(a->val, b->val, 6) == 0);
}

static int _add_connection(const ble_addr_t *addr, ble_conn_role_t role)
{
    mutex_lock(&_conns_lock);

    /* duplicate check */
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _ble_addr_t_equal(&_conns[i].addr, addr)) {
            mutex_unlock(&_conns_lock);
            return 1; /* already present */
        }
    }
    /* find free slot */
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!_conns[i].in_use) {
            _conns[i].addr = *addr;
            _conns[i].role = role;
            _conns[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
            _conns[i].state = BLE_CONN_INIT;
            _conns[i].in_use = 1;
            mutex_unlock(&_conns_lock);
            return 0; /* success */
        }
    }

    mutex_unlock(&_conns_lock);
    return -1; /* full */
}

static int _remove_connection(const ble_addr_t *addr)
{
    int ret = -1;
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _ble_addr_t_equal(&_conns[i].addr, addr)) {
            _conns[i].in_use = 0;
            _conns[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
            _conns[i].state = BLE_CONN_STATE_DISCONNECTED;
            ret = 0;
            break;
        }
    }
    mutex_unlock(&_conns_lock);
    return ret;
}

static void _print_addr(const ble_addr_t *a)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X", a->val[5], a->val[4], a->val[3],
           a->val[2], a->val[1], a->val[0]);
}

static void _print_addr_from_bytes(const uint8_t *addr)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static int cmd_addm(int argc, char **argv)
{
    if (argc != 2) {
        puts("usage: addm <AA:BB:CC:DD:EE:FF>");
        return 1;
    }
    ble_addr_t addr;
    if (_parse_addr(argv[1], &addr) != 0) {
        puts("error: invalid address format");
        return 1;
    }
    int rc = _add_connection(&addr, MASTER);
    if (rc == 0) {
        printf("added (master role) ");
        _print_addr(&addr);
        puts("");
        return 0;
    }
    if (rc == 1) {
        puts("already present");
        return 0;
    }
    puts("error: list full");
    return 1;
}

static int cmd_adds(int argc, char **argv)
{
    if (argc != 2) {
        puts("usage: adds <AA:BB:CC:DD:EE:FF>");
        return 1;
    }
    ble_addr_t addr;
    if (_parse_addr(argv[1], &addr) != 0) {
        puts("error: invalid address format");
        return 1;
    }
    int rc = _add_connection(&addr, SLAVE);
    if (rc == 0) {
        printf("added (slave role) ");
        _print_addr(&addr);
        puts("");
        return 0;
    }
    if (rc == 1) {
        puts("already present");
        return 0;
    }
    puts("error: list full");
    return 1;
}

static int cmd_rm(int argc, char **argv)
{
    if (argc != 2) {
        puts("usage: rm <AA:BB:CC:DD:EE:FF>");
        return 1;
    }
    ble_addr_t addr;
    if (_parse_addr(argv[1], &addr) != 0) {
        puts("error: invalid address format");
        return 1;
    }
    if (_remove_connection(&addr) == 0) {
        printf("removed ");
        _print_addr(&addr);
        puts("");
        return 0;
    }
    puts("not found");
    return 1;
}

static int cmd_list(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        puts("usage: list");
        return 1;
    }
    int count = 0;
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use) {
            printf("%d: ", i);
            _print_addr(&_conns[i].addr);
            printf(" role=%s state=%s handle=%u\n", _role_str(_conns[i].role),
                   _state_str(_conns[i].state),
                   (unsigned)_conns[i].conn_handle);
            count++;
        }
    }
    mutex_unlock(&_conns_lock);
    if (count == 0) {
        puts("empty");
    }
    return 0;
}

static int advertise_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    printf("# GAP event %i\n", (int)event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        // Update connection state
        if (event->connect.status != 0) {
            printf("Connection failed; status=%d\n", event->connect.status);
            break;
        }

        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
            mutex_lock(&_conns_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (_conns[i].in_use && _conns[i].role == MASTER &&
                    _conns[i].state != BLE_CONN_STATE_CONNECTED && _ble_addr_t_equal(&desc.peer_ota_addr, &_conns[i].addr)) {
                    _conns[i].conn_handle = event->connect.conn_handle;
                    _conns[i].state = BLE_CONN_STATE_CONNECTED;
                    break;
                }
            }
            mutex_unlock(&_conns_lock);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        // Update connection state
        printf("Disconnected\n");
        mutex_lock(&_conns_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (_conns[i].in_use && _conns[i].role == MASTER &&
                _conns[i].state == BLE_CONN_STATE_CONNECTED &&
                _conns[i].conn_handle == event->disconnect.conn.conn_handle) {
                _conns[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
                _conns[i].state = BLE_CONN_STATE_DISCONNECTED;
                break;
            }
        }
        mutex_unlock(&_conns_lock);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        printf("ADV COMPLETE\n");
        break;
    default:
        break;
    }
    return 0;
}

static void stop_advertise()
{
    if (ble_gap_adv_active()) {
        ble_gap_adv_stop();
    }
}

static void advertise()
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
    // fields for advertisement
    fields.flags = BLE_HS_ADV_F_DISC_GEN;
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    // Setting SVCs IDs
    fields.uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(BLE_SVC_UUID) };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        printf("Error setting advertisement data: %d\n", rc);
    }
    assert(rc == 0);
    rc = ble_gap_adv_start(addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           advertise_callback, NULL);
    if (rc != 0) {
        printf("Error starting advertising: %d\n", rc);
    }
    ztimer_sleep(ZTIMER_MSEC, ADV_ITVL_MS);
    stop_advertise();
}

int connect_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    printf("# GAP event %i\n", (int)event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        printf("Connected\n");
        if (event->connect.status != 0) {
            printf("Connection failed; status=%d\n", event->connect.status);
            break;
        }
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
            mutex_lock(&_conns_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (_conns[i].in_use && _conns[i].role == SLAVE &&
                    _conns[i].state != BLE_CONN_STATE_CONNECTED && _ble_addr_t_equal(&desc.peer_ota_addr, &_conns[i].addr)) {
                    _conns[i].conn_handle = event->connect.conn_handle;
                    _conns[i].state = BLE_CONN_STATE_CONNECTED;
                    break;
                }
            }
            mutex_unlock(&_conns_lock);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        printf("Disconnected\n");
        mutex_lock(&_conns_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (_conns[i].in_use && _conns[i].role == SLAVE &&
                _conns[i].state == BLE_CONN_STATE_CONNECTED &&
                _conns[i].conn_handle == event->disconnect.conn.conn_handle) {
                _conns[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
                _conns[i].state = BLE_CONN_STATE_DISCONNECTED;
                break;
            }
        }
        mutex_unlock(&_conns_lock);
        break;
    default:
        break;
    }

    return 0;
}

static void stop_scan();

static void filter_and_connect(const struct ble_gap_disc_desc *disc)
{
    mutex_lock(&_conns_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (_conns[i].in_use && _conns[i].role == SLAVE && _ble_addr_t_equal(&disc->addr, &_conns[i].addr) &&
            _conns[i].state != BLE_CONN_STATE_CONNECTED) {
            stop_scan();

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

            int rc = ble_gap_connect(addr_type, &disc->addr, BLE_HS_FOREVER, &conn_params,
                                     connect_callback, NULL);
            if (rc != 0) {
                printf("Error initiating connection: %d\n", rc);
            }
            else {
                printf("Connecting to device: ");
                _print_addr(&disc->addr);
                printf("\n");
            }

            break;
        }
    }
    mutex_unlock(&_conns_lock);
}

static void scan_callback(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    struct ble_hs_adv_fields parsed_fields;
    memset(&parsed_fields, 0, sizeof(parsed_fields));

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        filter_and_connect(&event->disc);
        return;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        return;
    }

    printf("[scan] Event type: %d\n", event->type);
    return;
}

static void stop_scan()
{
    if (ble_gap_disc_active()) {
        ble_gap_disc_cancel();
    }
}

static void scan()
{
    struct ble_gap_disc_params scan_params;
    memset(&scan_params, 0, sizeof(scan_params));

    scan_params =
        (struct ble_gap_disc_params){ .itvl = BLE_GAP_ADV_ITVL_MS(SCN_WIN_MS),
                                      .window = BLE_GAP_ADV_ITVL_MS(SCN_WIN_MS),
                                      .filter_policy = 0,
                                      .limited = 0,
                                      .passive = 0,
                                      .filter_duplicates = 0 };

    int rc = ble_gap_disc(addr_type, BLE_HS_FOREVER, &scan_params,
                          scan_callback, NULL);
    if (rc != 0) {
        printf("Error starting scanning: %d\n", rc);
        return;
    }
    ztimer_sleep(ZTIMER_MSEC, SCN_WIN_MS);
    stop_scan();
}

static void *_connection_manager(void *arg)
{
    (void)arg;

    int mode = 0; // 0: advertise, 1: scan

    while (1) {
        uint32_t delay = random_uint32_range(100, 201); /* 100..200 ms */
        ztimer_sleep(ZTIMER_MSEC, delay);

        int continue_mode = 0;

        mutex_lock(&_conns_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (_conns[i].in_use &&
                _conns[i].state != BLE_CONN_STATE_CONNECTED) {
                if (mode == 0 && _conns[i].role == MASTER) {
                    continue_mode = 1;
                    break;
                }
                else if (mode == 1 && _conns[i].role == SLAVE) {
                    continue_mode = 1;
                    break;
                }
            }
        }
        mutex_unlock(&_conns_lock);

        if (continue_mode) {
            if (mode == 0) {
                advertise();
            }
            else {
                scan();
            }
        }

        mode = 1 - mode;
    }
}

void show_own_address(void)
{
    uint8_t addr[6];
    int rc;

    rc = ble_hs_id_copy_addr(BLE_ADDR_RANDOM, addr, NULL);
    if (rc == 0) {
        printf("Static random address: ");
        _print_addr_from_bytes(&addr);
        printf("\n");
        return;
    }

    printf("Failed to read own address (rc=%d)\n", rc);
}

// implement init_ble
static void init_ble(void)
{
    // Setting GAP device name
    printf("Setting device name to: %s\n", device_name);
    int rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
        printf("Error setting device name: %d\n", rc);
    }
    assert(rc == 0);
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &addr_type);
    assert(rc == 0);

    show_own_address();
}

static const shell_command_t shell_commands[] = {
    { "addm", "Add master BLE address", cmd_addm },
    { "adds", "Add slave BLE address", cmd_adds },
    { "rm", "Remove BLE address", cmd_rm },
    { "list", "List connections", cmd_list },
    { NULL, NULL, NULL }
};

int main(void)
{
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    init_ble();

    thread_create(_conn_mgr_stack, sizeof(_conn_mgr_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  _connection_manager, NULL, "conn_mgr");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    // Never reached
    return 0;
}
