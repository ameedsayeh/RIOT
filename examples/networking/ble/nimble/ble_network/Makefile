# name of your application
APPLICATION = ble_network

# If no BOARD is found in the environment, use this default:
BOARD ?= nrf52dk
UF2_SOFTDEV = DROP

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../..

NIMBLE_MAX_CONN = 10

USEMODULE += shell
USEMODULE += random
USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt

CFLAGS += -DMYNEWT_VAL_BLE_MAX_CONNECTIONS=10

DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1
WERROR = 0

FE:E1:13:54:7B:D8

C8:C2:EE:58:40:42

C3:3F:0C:B3:24:B4

E4:AE:C5:FA:3A:46

D9:87:11:41:43:5E


# MAC Setup for testing
# Per-port configuration
ifeq ($(PORT_LINUX),/dev/ttyACM0)
    DEVICE_LIST = X("C8:C2:EE:58:40:42","Master") \
                  X("C3:3F:0C:B3:24:B4","Slave") \
                  X("E4:AE:C5:FA:3A:46","Master") \
                  X("D9:87:11:41:43:5E","Slave")
endif

ifeq ($(PORT),/dev/ttyACM1)
    DEVICE_LIST = X("FE:E1:13:54:7B:D8","Slave") \
                  X("C3:3F:0C:B3:24:B4","Master") \
                  X("E4:AE:C5:FA:3A:46","Slave") \
                  X("D9:87:11:41:43:5E","Master")
endif

ifeq ($(PORT),/dev/ttyACM2)
    DEVICE_LIST = X("FE:E1:13:54:7B:D8","Master") \
                  X("C8:C2:EE:58:40:42","Slave") \
                  X("E4:AE:C5:FA:3A:46","Master") \
                  X("D9:87:11:41:43:5E","Slave")
endif

ifeq ($(PORT),/dev/ttyACM3)
    DEVICE_LIST = X("FE:E1:13:54:7B:D8","Slave") \
                  X("C8:C2:EE:58:40:42","Master") \
                  X("C3:3F:0C:B3:24:B4","Slave") \
                  X("D9:87:11:41:43:5E","Master")
endif

ifeq ($(PORT),/dev/ttyACM4)
    DEVICE_LIST = X("FE:E1:13:54:7B:D8","Master") \
                  X("C8:C2:EE:58:40:42","Slave") \
                  X("C3:3F:0C:B3:24:B4","Master") \
                  X("E4:AE:C5:FA:3A:46","Slave")
endif

# Only add -DDEVICE_LIST if it’s not empty
ifneq ($(strip $(DEVICE_LIST)),)
    CFLAGS += -DDEVICE_LIST='$(DEVICE_LIST)'
endif

include $(RIOTBASE)/Makefile.include
