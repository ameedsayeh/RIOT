/**
 * @file main.c
 * @brief BLE Network Management Application - Main Entry Point
 */

/* Standard C library includes */
#include <stdio.h>

/* RIOT OS includes */
#include "msg.h"
#include "shell.h"
#include "thread.h"

/* Application modules */
#include "config.h"
#include "ble_connection.h"
#include "ble_operations.h"
#include "shell_commands.h"
#include "gatt_services.h"

/* ========================================================================
 * Global Variables
 * ======================================================================== */

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static char _conn_mgr_stack[THREAD_STACKSIZE_DEFAULT];

/* ========================================================================
 * Main Function
 * ======================================================================== */

int main(void)
{
    ztimer_sleep(ZTIMER_MSEC, 5000); /* wait for console to be ready */
                                     /* Initialize message queue */

    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    /* Initialize BLE connection management */
    ble_conn_init();

    /* Initialize BLE stack */
    init_ble();

    /* Load predefined connections */
    load_connections();

    /* Start connection manager thread */
    thread_create(_conn_mgr_stack, sizeof(_conn_mgr_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  connection_manager_thread, NULL, "conn_mgr");

    /* Start interactive shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(get_shell_commands(), line_buf, SHELL_DEFAULT_BUFSIZE);

    /* Never reached */
    return 0;
}
