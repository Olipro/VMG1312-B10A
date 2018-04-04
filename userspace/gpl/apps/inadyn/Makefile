INADYN_BASE = .
SRCDIR = $(INADYN_BASE)/src
OUTDIR=bin

LIBS= -lcms_msg $(CMS_COMMON_LIBS)
LIBS+= -Wl,-rpath,/lib:/lib/public:$(INSTALL_DIR)/lib:$(INSTALL_DIR)/lib/public -L$(INSTALL_DIR)/lib/public -lcms_util -lcms_msg $(CMS_COMMON_LIBS)
CFLAGS  += -DBRCM_CMS_BUILD -DCMS_LOG3		
#simple arch config. No smart detection.
#possible values: linux, mac, solaris, yourown
ifndef TARGET_ARCH
	TARGET_ARCH = linux
endif

OUTFILE=$(INSTALL_DIR)/bin/inadyn

# Make command to use for dependencies
MAKECMD=gmake

ifeq ($(TARGET_ARCH),solaris)
	ARCH_SPECIFIC_LIBS += -lsocket -lnsl
	ARCH_SPECIFIC_CFLAGS += 
endif

ifeq ($(TARGET_ARCH),linux)
	ARCH_SPECIFIC_LIBS += 
	ARCH_SPECIFIC_CFLAGS += 
endif

ifdef DEBUG
	CFLAGS += -g
endif

COMMON_OBJ=$(OUTDIR)/base64utils.o $(OUTDIR)/dyndns.o \
	$(OUTDIR)/errorcode.o $(OUTDIR)/get_cmd.o $(OUTDIR)/http_client.o \
	$(OUTDIR)/ip.o $(OUTDIR)/main.o $(OUTDIR)/os.o $(OUTDIR)/os_psos.o \
	$(OUTDIR)/os_unix.o $(OUTDIR)/os_windows.o $(OUTDIR)/tcp.o $(OUTDIR)/inadyn_cmd.o
OBJ=$(COMMON_OBJ) $(CFG_OBJ)

#modified by YYHuang 07/03/07
# 1. change "gcc" to $(CC) 
# 2. disable code of debug output:DBG_PRINT_DISABLED
# 3. add optimize flags
CFLAGS += -DDBG_PRINT_DISABLED -O2 -I$(BUILD_DIR)/userspace/public/include -I$(BUILD_DIR)/userspace/public/include/linux -I./
COMPILE=$(CC)  -Wall  -pedantic -c  $(ARCH_SPECIFIC_CFLAGS) $(CFLAGS) -o "$(OUTDIR)/$(*F).o" $(CFG_INC) "$<" $(LIBS)

LINK=$(CC) $(CFLAGS) -o "$(OUTFILE)" $(OBJ) $(CFG_LIB)  $(ARCH_SPECIFIC_LIBS) $(LIBS)
# Pattern rules
$(OUTDIR)/%.o : $(SRCDIR)/%.c
	$(COMPILE)

# Build rules
all: $(OUTFILE)

$(OUTFILE): $(OUTDIR)/$(TARGET_ARCH)  $(OBJ)
	$(LINK)

$(OUTDIR)/$(TARGET_ARCH):
	-mkdir -p "$@"

# Rebuild this project
rebuild: cleanall all

# Clean this project
clean:
	rm -f $(OUTFILE)
	rm -f $(OBJ)

# Clean this project and all dependencies
cleanall: clean


