include common/kext_info.mk

BUILD_PKG	:=	build_pkg
KEXTS		:=	WiiAudio WiiEXI WiiGraphics WiiPlatform WiiStorage WiiUSB
MKEXT_NAME	:=	Wii.mkext
ARCHIVE_ZIP	:= 	WiiIntosh-osx-drivers-$(KEXT_VERSION).zip

.PHONY: all clean

all:
	$(foreach kext,$(KEXTS),$(MAKE) -C $(kext) all &&) true

	rm -rf $(BUILD_PKG)
	$(foreach kext,$(KEXTS),mkdir -p $(BUILD_PKG)/Kexts/$(kext).kext &&) true
	$(foreach kext,$(KEXTS),cp -r $(kext)/build_kext/$(kext).kext $(BUILD_PKG)/Kexts &&) true

	./make-mkext.py $(BUILD_PKG)/Kexts $(MKEXT_NAME)
	cp $(MKEXT_NAME) $(BUILD_PKG)
	cd $(BUILD_PKG); zip -qry -FS ../$(ARCHIVE_ZIP) *

clean:
	rm -rf $(BUILD_PKG)
	rm -rf $(MKEXT_NAME)
	rm -rf $(ARCHIVE_ZIP)
	$(foreach kext,$(KEXTS),$(MAKE) -C $(kext) clean &&) true
