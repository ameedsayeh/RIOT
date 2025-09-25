# name of your application
APPLICATION = ble_multihop

# If no BOARD is found in the environment, use this default:
BOARD ?= nrf52dk
UF2_SOFTDEV = DROP

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../..

# configure and use Nimble
USEMODULE += nimble_svc_gap
# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
