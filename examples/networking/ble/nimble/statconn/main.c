#include <stdio.h>
#include <string.h>

#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "msg.h"
#include "net/bluetil/addr.h"
#include "nimble_statconn.h"
#include "shell.h"

// iPhone add: 75:BE:01:C9:67:90

#define MAIN_QUEUE_SIZE (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

uint16_t custom_write_data_val_handle;
uint16_t custom_notify_data_val_handle;

#define CUSTOM_SVC_UUID 0xff00
#define CUSTOM_NOTIFY_CHR_UUID 0xee00
#define CUSTOM_WRITE_CHR_UUID 0xee01

ble_uuid16_t custom_svc_uuid = BLE_UUID16_INIT(CUSTOM_SVC_UUID);
ble_uuid16_t custom_notify_chr_uuid = BLE_UUID16_INIT(CUSTOM_NOTIFY_CHR_UUID);
ble_uuid16_t custom_write_chr_uuid = BLE_UUID16_INIT(CUSTOM_WRITE_CHR_UUID);

int notify_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt, void *arg);
int write_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg);

const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {/* Custom Service */
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(CUSTOM_SVC_UUID),
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {
                 /* Custom Notify Characteristic */
                 .uuid = BLE_UUID16_DECLARE(CUSTOM_NOTIFY_CHR_UUID),
                 .access_cb = notify_access_cb,
                 .val_handle = &custom_notify_data_val_handle,
                 .flags = BLE_GATT_CHR_F_NOTIFY,
             },
             {
                 /* Custom Write Characteristic */
                 .uuid = BLE_UUID16_DECLARE(CUSTOM_WRITE_CHR_UUID),
                 .access_cb = write_access_cb,
                 .val_handle = &custom_write_data_val_handle,
                 .flags = BLE_GATT_CHR_F_WRITE,
             },
             {
                 0, /* no more characteristics in this service */
             },
         }},
    {
        0, /* no more services */
    },
};

int notify_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    (void)ctxt;

    printf("Notify access cb called\n");

    return 0;
}

int write_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    (void)ctxt;

    printf("Write access cb called\n");

    return 0;
}

// static int discover_dsc_cb(uint16_t conn_handle,
//                            const struct ble_gatt_error *error,
//                            uint16_t chr_val_handle,
//                            const struct ble_gatt_dsc *dsc, void *arg) {
//     (void)error;
//     (void)chr_val_handle;
//     uint16_t *ccc_handle = (uint16_t *)arg;

//     if (dsc != NULL) {
//         if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(
//                                            BLE_GATT_DSC_CLT_CFG_UUID16)) ==
//                                            0) {
//             *ccc_handle = dsc->handle;
//         }
//     } else {
//         // Manipulate the CCC descriptor to enable notifications
//         if (*ccc_handle != 0) {
//             uint8_t value[2] = {0x01, 0x00};
//             int rc = ble_gattc_write_flat(conn_handle, *ccc_handle, value,
//                                           sizeof(value), NULL, NULL);
//             if (rc != 0) {
//                 printf(
//                     "Failed to manipulate notify CCC, terminate
//                     connection\n");
//             }
//         } else {
//             printf("Failed to find notify CCC, terminate connection\n");
//         }
//     }

//     return 0;
// }

// int discover_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
//                     const struct ble_gatt_chr *chr, void *arg) {
//     (void)error;
//     uint16_t svc_end_handle = (uint16_t)(intptr_t)arg;

//     if (ble_uuid_cmp(&chr->uuid.u, &custom_notify_chr_uuid.u) == 0) {
//         static uint16_t ccc_handle = 0;
//         ble_gattc_disc_all_dscs(conn_handle, chr->val_handle, svc_end_handle,
//                                 discover_dsc_cb, &ccc_handle);
//     }

//     return 0;
// }

static void _print_evt(const char *msg, int handle, const uint8_t *addr) {
    printf("[ble] %s (%i|", msg, handle);
    if (addr) {
        bluetil_addr_print(addr);
    } else {
        printf("n/a");
    }
    puts(")");
}

int discover_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                    const struct ble_gatt_svc *service, void *arg) {
    printf("Discovering service...\n");

    if (service != NULL) {
        printf("Service discovered\n");
        // ble_gattc_disc_all_chrs(conn_handle, service->start_handle,
        //                         service->end_handle, discover_chr_cb,
        //                         (void *)(intptr_t)service->end_handle);
    }

    (void)arg;
    (void)error;
    (void)conn_handle;

    return 0;
}

static void _on_ble_evt(int handle, nimble_netif_event_t event,
                        const uint8_t *addr) {
    switch (event) {
        case NIMBLE_NETIF_CONNECTED_MASTER:
            _print_evt("CONNECTED master", handle, addr);
            break;
        case NIMBLE_NETIF_CONNECTED_SLAVE:
            _print_evt("CONNECTED slave", handle, addr);
            break;
        case NIMBLE_NETIF_CLOSED_MASTER:
            _print_evt("CLOSED master", handle, addr);
            break;
        case NIMBLE_NETIF_CLOSED_SLAVE:
            _print_evt("CLOSED slave", handle, addr);
            break;
        case NIMBLE_NETIF_CONN_UPDATED:
            _print_evt("UPDATED", handle, addr);
            break;
        default:
            /* do nothing */
            return;
    }
}

// /* Shell command: send a notification with provided text to all tracked
//  * connections */
// static int _cmd_notify(int argc, char **argv) {
//     if (argc < 2) {
//         puts("usage: notify <string>");
//         return 1;
//     }
//     const char *payload = argv[1];
//     size_t len = strlen(payload);
//     if (len > 244) { /* stay well below typical ATT MTU limits */
//         puts("error: payload too long (max 244 bytes)");
//         return 1;
//     }

//     if (_conn_count == 0) {
//         puts("no active connections");
//         return 0;
//     }

//     /* Build an mbuf for each connection (simplest) */
//     unsigned sent = 0;
//     for (unsigned i = 0; i < _conn_count; i++) {
//         struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, (uint16_t)len);
//         if (!om) {
//             puts("alloc mbuf failed");
//             break;
//         }
//         int rc = ble_gatts_notify_custom(_conns[i],
//                                          custom_notify_data_val_handle, om);
//         if (rc == 0) {
//             printf("notified conn=%u len=%u\n", _conns[i], (unsigned)len);
//             sent++;
//         } else {
//             printf("notify failed conn=%u rc=%d\n", _conns[i], rc);
//         }
//     }
//     printf("notifications sent: %u/%u\n", sent, _conn_count);
//     return 0;
// }

// static const shell_command_t _shell_cmds[] = {
//     {"notify", "send notification over custom notify characteristic",
//      _cmd_notify},
//     {NULL, NULL, NULL}};

int main(void) {
    puts("IPv6-over-BLE with statconn BLE connection manager");

    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    int rc;
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        printf("ble_gatts_count_cfg() rc = %d\n", rc);
        return -1;
    }
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        printf("ble_gatts_add_svcs() rc = %d\n", rc);
        return -1;
    }

    /* Reload the GATT server to link our added services */
    rc = ble_gatts_start();
    if (rc != 0) {
        printf("ble_gatts_start() rc = %d\n", rc);
        return -1;
    }

        /* register for BLE events */
    nimble_statconn_eventcb(_on_ble_evt);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should never be reached */
    return 0;
}
