APPLICATION = ble_statconn
BOARD ?= nrf52dk

# CFLAGS += -DMYNEWT_VAL_BLE_MAX_CONNECTIONS=10
NIMBLE_MAX_CONN = 10
# include shell support
USEMODULE += shell_cmds_default
USEMODULE += ps

# # Include GNRC and RPL
# USEMODULE += auto_init_gnrc_netif
# USEMODULE += gnrc_ipv6_router_default
# USEMODULE += auto_init_gnrc_rpl
# USEMODULE += gnrc_rpl
# USEMODULE += gnrc_icmpv6_echo
# USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt

# # Setup Nimble
# USEMODULE += nimble_netif

# NimBLE stack + netif wrapper + handy AD builder
USEPKG   += nimble
USEMODULE += nimble_netif
USEMODULE += nimble_statconn
USEMODULE += bluetil_ad

# GNRC IPv6 so the netif becomes a real IP interface
# USEMODULE += gnrc_ipv6_default
USEMODULE += auto_init_gnrc_netif



RIOTBASE ?= $(CURDIR)/../../../../..
include $(RIOTBASE)/Makefile.include
