
/**
 * @file messages.h
 * @brief BLE messaging interface
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>

/* ========================================================================
 * Type Definitions
 * ======================================================================== */

typedef struct {
    uint32_t tx1;
    uint32_t tx2;
    uint32_t rx1;
    uint32_t rx2;
} tsync_msg;

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

/**
 * @brief Print the contents of a tsync_msg structure
 * @param msg Pointer to the tsync_msg structure to print
 */
void tsync_msg_print(const tsync_msg *msg);

#endif /* MESSAGES_H */
