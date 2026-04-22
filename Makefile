include common/kext_info.mk

BUILD_PKG	:=	build_pkg_$(OSX_VERSION)
KEXTS		:=	WiiAudio WiiEXI WiiGraphics WiiPlatform WiiStorage WiiUSB
MKEXT_NAME	:=	Wii_$(OSX_VERSION).mkext
ARCHIVE_ZIP	:= 	Wiintosh-osx-drivers-$(OSX_VERSION)-$(KEXT_VERSION).zip

.PHONY: all clean

all:
	@echo "OSX target version: $(OSX_VERSION)"
	$(foreach kext,$(KEXTS),$(MAKE) -C $(kext) all &&) true

	rm -rf $(BUILD_PKG)
	$(foreach kext,$(KEXTS),mkdir -p $(BUILD_PKG)/Kexts/$(kext).kext &&) true
	$(foreach kext,$(KEXTS),cp -r $(kext)/build_kext_$(OSX_VERSION)/$(kext).kext $(BUILD_PKG)/Kexts &&) true

package:
	@echo "OSX target version: $(OSX_VERSION)"
	./make-mkext.py $(BUILD_PKG)/Kexts $(BUILD_PKG)/$(MKEXT_NAME)
	cd $(BUILD_PKG); zip -qry -FS ../$(ARCHIVE_ZIP) *

clean:
	rm -rf $(BUILD_PKG)*
	rm -rf *.mkext
	rm -rf *.zip
	$(foreach kext,$(KEXTS),$(MAKE) -C $(kext) clean &&) true
