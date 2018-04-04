EXE = httpd
dynamic: generic_exe_install


httpd:
	cp httpd_$(PROFILE)_save httpd

	mkdir -p $(INSTALL_DIR)/webs
	mkdir -p $(INSTALL_DIR)/webs/CA
	cp -rf html/CA/* $(INSTALL_DIR)/webs/CA
	mkdir -p $(INSTALL_DIR)/webs/Brick
	cp -rf html/Common/* $(INSTALL_DIR)/webs/Brick
	cp -rf html/ZyXEL_Brick/* $(INSTALL_DIR)/webs/Brick
	cp -rf html/ZyXEL_Brick/pages/VMG1312-B10A/* $(INSTALL_DIR)/webs/Brick/pages


clean:
	rm -f httpd


CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
