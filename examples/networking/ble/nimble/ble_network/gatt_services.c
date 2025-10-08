/**
 * @file gatt_services.c
 * @brief GATT services implementation
 */

#include "gatt_services.h"
#include "config.h"
#include "messages.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include "ztimer.h"

/* ========================================================================
 * Global Variables
 * ======================================================================== */

/* Value handles for GATT characteristics */
uint16_t custom_write_data_val_handle;

/* Message storage */
#define MAX_MESSAGE_LEN 128

/* Time synchronization state */
static uint32_t stored_tx1 = 0;
static uint32_t stored_tx2 = 0;
// static uint16_t sync_conn_handle = 0;
static bool sync_active = false;

/* Thread data structure for sending messages */
typedef struct {
    uint16_t conn_handle;
    tsync_msg msg;
} send_thread_data_t;

/* Single thread stack and data */
static char send_thread_stack[THREAD_STACKSIZE_DEFAULT];
static send_thread_data_t thread_data;

/* Thread function for sending messages */
static void *send_msg_thread(void *arg)
{
    send_thread_data_t *data = (send_thread_data_t *)arg;

    // printf("[THREAD] Sending message in new thread for handle %d\n", data->conn_handle);
    int rc = send_tsync_msg(data->conn_handle, data->msg);
    if (rc != 0) {
        printf("[THREAD] Failed to send message: %d\n", rc);
    }

    return NULL;
}

/* ========================================================================
 * GATT Characteristic Access Callbacks
 * ======================================================================== */

int write_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    printf("[GATT] Write characteristic access callback op=%d\n", ctxt->op);
}

/* ========================================================================
 * GATT Services Definition
 * ======================================================================== */

const struct ble_gatt_svc_def gatt_svcs[] = {
    { /* Custom Service */
      .type = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid = BLE_UUID16_DECLARE(CUSTOM_SVC_UUID),
      .characteristics = (struct ble_gatt_chr_def[]){
          {
              /* Custom Write Characteristic */
              .uuid = BLE_UUID16_DECLARE(CUSTOM_WRITE_CHR_UUID),
              .access_cb = write_access_cb,
              .val_handle = &custom_write_data_val_handle,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
          },
          {
              0, /* no more characteristics in this service */
          } } },
    {
        0, /* no more services */
    },
};

/* ========================================================================
 * GATT Services Initialization
 * ======================================================================== */

int gatt_services_init(void)
{
    int rc;

    /* Count the configuration */
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        printf("ble_gatts_count_cfg() failed: %d\n", rc);
        return rc;
    }

    /* Add services to the GATT server */
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        printf("ble_gatts_add_svcs() failed: %d\n", rc);
        return rc;
    }

    /* Start GATT server */
    rc = ble_gatts_start();
    if (rc != 0) {
        printf("ble_gatts_start() failed: %d\n", rc);
        return rc;
    }

    return 0;
}

/* ========================================================================
 * Communication Functions
 * ======================================================================== */

int start_sync(uint16_t conn_handle)
{
    /* Initialize sync protocol */
    stored_tx1 = 0;
    stored_tx2 = 0;

    sync_active = true;

    /* Create initial message with all zeros */
    tsync_msg msg = { .tx1 = 0, .tx2 = 0, .rx1 = 0, .rx2 = 0 };
    return send_tsync_msg(conn_handle, msg);
}

int send_tsync_msg(uint16_t conn_handle, tsync_msg msg)
{
    size_t len = sizeof(tsync_msg);
    struct os_mbuf *om;

    /* Convert struct to mbuf */
    om = ble_hs_mbuf_from_flat(&msg, len);

    if (!om) {
        printf("[GATT] Failed to allocate mbuf for notification\n");
        return -1;
    }

    int rc = ble_gattc_notify_custom(conn_handle, custom_write_data_val_handle, om);
    if (rc != 0) {
        printf("[GATT] Failed to send data to handle %d: %d\n", conn_handle, rc);
        return rc;
    }

    // printf("[GATT] Sent data to handle %d\n", conn_handle);
    return 0;
}

/* ========================================================================
 * Time Synchronization Protocol Functions
 * ======================================================================== */

void handle_sync_tx_event(uint16_t conn_handle, uint32_t timestamp)
{
    if (stored_tx1 == 0 && sync_active) {
        /* This is the first message being sent */
        stored_tx1 = timestamp;
        // printf("[SYNC] TX1 recorded: %lu\n", (unsigned long)stored_tx1);
        // printf("[SYNC] TX1 recorded: %lu (first message sent)\n", (unsigned long)stored_tx1);
    }
    else if (!sync_active) {
        /* This is the second message being sent (response) */
        stored_tx2 = timestamp;
        // printf("[SYNC] TX2 recorded: %lu\n", (unsigned long)stored_tx2);
        // printf("[SYNC] TX2 recorded: %lu (response sent)\n", (unsigned long)stored_tx2);
    }
}

void handle_sync_rx_event(uint16_t conn_handle, struct os_mbuf *om, uint32_t timestamp)
{
    /* Parse the received tsync message */
    tsync_msg received_msg;
    uint16_t copy_len;

    int rc = ble_hs_mbuf_to_flat(om, &received_msg, sizeof(tsync_msg), &copy_len);
    if (rc != 0 || copy_len != sizeof(tsync_msg)) {
        printf("[SYNC] Failed to decode tsync message\n");
        return;
    }

    // printf("[SYNC] Received message from handle %d: ", conn_handle);
    // tsync_msg_print(&received_msg);

    /* Check which stage of the protocol we're in */
    if (received_msg.tx1 == 0 && received_msg.tx2 == 0 &&
        received_msg.rx1 == 0 && received_msg.rx2 == 0) {
        /* First message received - respond with rx2 filled */
        // printf("[SYNC] RX2 recorded: %lu (first message received)\n", (unsigned long)timestamp);

        thread_data.conn_handle = conn_handle;
        thread_data.msg = (tsync_msg){ .tx1 = 0, .tx2 = 0, .rx1 = 0, .rx2 = timestamp };

        kernel_pid_t pid = thread_create(send_thread_stack, sizeof(send_thread_stack),
                                         THREAD_PRIORITY_MAIN - 1, 0,
                                         send_msg_thread, &thread_data, "send_msg");
        if (pid <= KERNEL_PID_UNDEF) {
            printf("[SYNC] Failed to create send thread\n");
        }
    }
    else if (received_msg.tx1 != 0 && received_msg.rx1 != 0 && received_msg.rx2 != 0 && received_msg.tx2 == 0) {
        /* Third message received - complete the protocol */
        // printf("[SYNC] Final message received, completing protocol\n");

        thread_data.conn_handle = conn_handle;
        thread_data.msg = received_msg;
        thread_data.msg.tx2 = stored_tx2; /* Add our stored tx2 */

        // printf("[SYNC] Sending final message: ");
        // tsync_msg_print(&thread_data.msg);

        // ((rx2-tx1) + (tx2-rx1)) / 2
        tsync_msg_print(&thread_data.msg);
        uint32_t sync_offset = ((stored_tx2 - received_msg.tx1) + (received_msg.tx2 - received_msg.rx1)) / 2;
        printf("%lu\n", (unsigned long)sync_offset);

        kernel_pid_t pid = thread_create(send_thread_stack, sizeof(send_thread_stack),
                                         THREAD_PRIORITY_MAIN - 1, 0,
                                         send_msg_thread, &thread_data, "send_msg");
        if (pid <= KERNEL_PID_UNDEF) {
            printf("[SYNC] Failed to create send thread\n");
        }
        stored_tx1 = 0;
        stored_tx2 = 0;
        sync_active = false;
    }
    else if (received_msg.rx2 != 0 && received_msg.tx1 == 0 && received_msg.rx1 == 0 && received_msg.tx2 == 0) {
        /* Second message received - add rx1 and tx1, send back */
        // printf("[SYNC] RX1 recorded: %lu (response received)\n", (unsigned long)timestamp);

        thread_data.conn_handle = conn_handle;
        thread_data.msg = received_msg;
        thread_data.msg.tx1 = stored_tx1;
        thread_data.msg.rx1 = timestamp;

        // printf("[SYNC] Sending third message: ");
        // tsync_msg_print(&thread_data.msg);

        kernel_pid_t pid = thread_create(send_thread_stack, sizeof(send_thread_stack),
                                         THREAD_PRIORITY_MAIN - 1, 0,
                                         send_msg_thread, &thread_data, "send_msg");
        if (pid <= KERNEL_PID_UNDEF) {
            printf("[SYNC] Failed to create send thread\n");
        }
    }
    else if (received_msg.tx1 != 0 && received_msg.tx2 != 0 &&
             received_msg.rx1 != 0 && received_msg.rx2 != 0) {
        /* Final complete message received */
        // printf("[SYNC] Complete synchronization message received:\n");
        // tsync_msg_print(&received_msg);

        // ((rx2-tx1) + (tx2-rx1)) / 2
        tsync_msg_print(&received_msg);
        uint32_t sync_offset = ((received_msg.rx2 - received_msg.tx1) + (received_msg.tx2 - received_msg.rx1)) / 2;
        printf("%lu\n", (unsigned long)sync_offset);

        sync_active = false;
        stored_tx1 = 0;
        stored_tx2 = 0;
    }
}
