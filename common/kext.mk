#
# Toolchain configuration.
#
DARLING_SHELL	:= 	sudo darling shell
DARLING_CLT 	:= 	/Library/Developer/DarlingCLT/usr/bin
AS 			:= 	$(DARLING_CLT)/as
CC 			:= 	$(DARLING_CLT)/powerpc-apple-darwin10-gcc-4.2.1
CXX 		:= 	$(DARLING_CLT)/powerpc-apple-darwin10-g++-4.2.1
LD 			:= 	$(DARLING_CLT)/ld_classic

#
# Toolchain flags.
#
ASFLAGS	=
CFLAGS  = 	-fno-builtin -fno-common -mlong-branch -finline -fno-keep-inline-functions
CFLAGS	+=	-fmessage-length=0  -force_cpusubtype_ALL -static -nostdinc -nostdlib -r
CFLAGS	+=	-D__KERNEL__ -DKERNEL -DDEBUG -Wall -mmacosx-version-min=10.2 $(INCLUDE)
CXXFLAGS	=	$(CFLAGS) -x c++ -fapple-kext -fno-rtti -fno-exceptions -fcheck-new

#
# Folders. These must be relative to the kext Makefiles, Darling can't deal with absolute paths.
#
BUILD			:=	build
BUILD_KEXT	:= build_kext
INCLUDES	:=	../MacPPCKernelSDK/Headers ../include $(SOURCES)

#
# Source and object files.
#
KMOD_CFILE		:= 	$(BUILD)/$(KEXT_NAME)-info.c
CFILES				:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c)) $(KMOD_CFILE)
CXXFILES			:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
OFILES				:=	$(CFILES:%.c=$(BUILD)/%.o) $(CXXFILES:%.cpp=$(BUILD)/%.o)
KEXT_BIN			:=	$(BUILD)/$(KEXT_NAME)
KEXT_PLIST		:=	$(BUILD)/$(KEXT_NAME)-Info.plist
KEXT_BUNDLE		:=	$(BUILD_KEXT)/$(KEXT_NAME).kext

INCLUDE				:=	$(foreach dir,$(INCLUDES),-I$(dir))
DEPENDENCIES 	:= 	$(OFILES:.o=.d)

include ../common/kext_info.mk

.PHONY: $(BUILD) $(KEXT_BUNDLE) clean

all: $(KEXT_BUNDLE)

clean:
	rm -rf $(BUILD)
	rm -rf $(BUILD_KEXT)

# kmod info
$(KMOD_CFILE): ../common/kmod_info.c
	@test -d $(dir $@) || mkdir $(dir $@)
	@cat $< | sed -e s/__BUNDLE__/$(KEXT_BUNDLE_ID)/ \
		-e s/__MODULE__/$(KEXT_NAME)/ -e 's/-info[.]c//' \
		-e s/__VERSION__/$(KEXT_VERSION)/ > $@

# C source
$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(DARLING_SHELL) $(CC) $(CFLAGS) -c $< -o $@

# C++ source
$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(DARLING_SHELL) $(CXX) $(CXXFLAGS) -c $< -o $@

# Kext binary
$(KEXT_BIN): $(OFILES)
	$(DARLING_SHELL) $(CC)  $^ $(CFLAGS) -o $@

# Kext plist
$(KEXT_PLIST): Info.plist
	@test -d $(dir $@) || mkdir $(dir $@)
	@cat $< | sed -e s/__BUNDLE__/$(KEXT_BUNDLE_ID)/ \
		-e s/__MODULE__/$(KEXT_NAME)/ \
		-e s/__VERSION__/$(KEXT_VERSION)/ > $@

# Kext bundle
$(KEXT_BUNDLE): $(KEXT_BIN) $(KEXT_PLIST)
	@test -d $(KEXT_BUNDLE) || mkdir -p $(KEXT_BUNDLE)
	@test -d $(KEXT_BUNDLE)/Contents || mkdir $(KEXT_BUNDLE)/Contents
	@test -d $(KEXT_BUNDLE)/Contents/MacOS || mkdir $(KEXT_BUNDLE)/Contents/MacOS

	@cp -f $(KEXT_PLIST) $(KEXT_BUNDLE)/Contents/Info.plist
	@cp -f $(KEXT_BIN) $(KEXT_BUNDLE)/Contents/MacOS/$(KEXT_NAME)

-include $(DEPENDENCIES)
