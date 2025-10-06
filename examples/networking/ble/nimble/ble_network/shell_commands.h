/**
 * @file shell_commands.h
 * @brief Shell command interface for BLE network management
 */

#ifndef SHELL_COMMANDS_H
#define SHELL_COMMANDS_H

#include "shell.h"

/* ========================================================================
 * Function Prototypes
 * ======================================================================== */

/**
 * @brief Add master BLE address command
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, 1 on error
 */
int cmd_addm(int argc, char **argv);

/**
 * @brief Add slave BLE address command
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, 1 on error
 */
int cmd_adds(int argc, char **argv);

/**
 * @brief Remove BLE address command
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, 1 on error
 */
int cmd_rm(int argc, char **argv);

/**
 * @brief List connections command
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, 1 on error
 */
int cmd_list(int argc, char **argv);

/**
 * @brief Get shell commands array
 * @return Pointer to shell commands array
 */
const shell_command_t *get_shell_commands(void);

#endif /* SHELL_COMMANDS_H */
