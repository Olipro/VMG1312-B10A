ifneq ($(strip $(BRCMDRIVERS_DIR)),)
OPEN_OBJS =  wl/sys/wl_linux_dslcpe.o wl/sys/wl_linux.o emf/igs/osl_linux.o 
include $(BRCMDRIVERS_DIR)/broadcom/net/wl_4.12L08/makefile.wlan
else
include ../makefile.release
endif

