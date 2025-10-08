/**
 * @file messages.c
 * @brief BLE messaging implementation
 */

#include "messages.h"
#include <stdio.h>

/* ========================================================================
 * Function Implementations
 * ======================================================================== */

void tsync_msg_print(const tsync_msg *msg)
{
    if (msg == NULL) {
        printf("tsync_msg: NULL\n");
        return;
    }

    printf("tsync_msg: tx1=%lu, tx2=%lu, rx1=%lu, rx2=%lu\n",
           (unsigned long)msg->tx1,
           (unsigned long)msg->tx2,
           (unsigned long)msg->rx1,
           (unsigned long)msg->rx2);
}
