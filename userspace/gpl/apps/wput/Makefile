#wput makefile
PACKAGE     = wput
prefix      = /usr/local
datadir     = $(prefix)/share
mandir	    = $(datadir)/man/man1
exec_prefix = ${prefix}
bindir=${exec_prefix}/bin


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common



#
# GPL apps and libs are only allowed to include header files from the
# gpl and public directories
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/gpl/include  \
                         -I$(BUILD_DIR)/userspace/gpl/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR)


#
# GPL apps and libs are only allowed to link with libraries from the
# gpl and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/gpl:/lib/public




all clean:
	cd po && $(MAKE) $(MAKEDEFS) $@
	cd src && $(MAKE) $(MAKEDEFS) $@
	
win-clean:
	cd src && $(MAKE) $(MAKEDEFS) $@

install: all
	cd po && $(MAKE) $(MAKEDEFS) $@
	install -m0755 wput $(bindir)	
	install -m0644 doc/wput.1.gz $(mandir)
	@echo "----------------"
	@echo "Wput installed. See 'wput -h' or 'man wput' for usage information."
	@echo "Further documentation is located in the doc/USAGE.* files."
	@echo 
	@echo "Wput is not perfect, so please report any bugs you notice (see BUGS-section"
	@echo "in the manpage)."
	@echo "----------------"
uninstall:
	rm -f $(bindir)/wput
	rm -f $(mandir)/wput.1.gz
	
