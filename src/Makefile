CIRCLEHOME = ../third_party/circle
STDLIB_SUPPORT = 1

DEBUG ?= 0
SMSBARE_SIZE_OPT ?= 1
SMSBARE_BUILD_BID ?=
SMSBARE_BUILD_OWNER ?=
SMSBARE_KERNEL_MAX_SIZE ?= 0x500000
SMSBARE_EMBED_INITRAMFS ?= 0
SMSBARE_EMBED_INITRAMFS_PATH ?= ../build/initramfs/sms-embedded.cpio

DEFINE += -DSMSBARE_ENABLE_DEBUG_OVERLAY=$(DEBUG)
DEFINE += -DSMSBARE_REQUIRE_CPIO_BIOS=0
DEFINE += -DSMSBARE_SIZE_OPT=$(SMSBARE_SIZE_OPT)
DEFINE += '-DSMSBARE_BUILD_BID="$(SMSBARE_BUILD_BID)"'
DEFINE += '-DSMSBARE_BUILD_OWNER="$(SMSBARE_BUILD_OWNER)"'
DEFINE += -DKERNEL_MAX_SIZE=$(SMSBARE_KERNEL_MAX_SIZE)
DEFINE += -DSMSBARE_EMBED_INITRAMFS=$(SMSBARE_EMBED_INITRAMFS)
DEFINE += '-DSMSBARE_EMBED_INITRAMFS_PATH="$(SMSBARE_EMBED_INITRAMFS_PATH)"'
DEFINE += -DSMSBARE_BACKEND_FAMILY_SMS=1
DEFINE += '-DSMSBARE_BACKEND_DEFAULT="sms.smsplus"'
DEFINE += -DDOS -DLSB_FIRST -DSMSBARE_BAREMETAL -DSKIP_STDIO_REDEFINES

SMS_BACKEND_DIR := backend/sms
SMSPLUS_MODULE_DIR := $(SMS_BACKEND_DIR)/modules/smsplus
SMSPLUS_UPSTREAM_DIR := ../third_party/smsplus

SMSPLUS_INCLUDE_DIRS := \
	-I./$(SMS_BACKEND_DIR) \
	-I./$(SMSPLUS_MODULE_DIR) \
	-I$(SMSPLUS_UPSTREAM_DIR) \
	-I$(SMSPLUS_UPSTREAM_DIR)/cpu \
	-I$(SMSPLUS_UPSTREAM_DIR)/sound \
	-I$(SMSPLUS_UPSTREAM_DIR)/unzip

LOCAL_INCLUDE_DIRS := \
	-I. \
	-I./tools/locale \
	-I./tools/debug \
	-I./tools/menu \
	-I./tools/file_browser \
	-I./tools/render \
	-I./tools/input \
	-I./tools/media \
	-I./tools

INCLUDE += $(SMSPLUS_INCLUDE_DIRS) $(LOCAL_INCLUDE_DIRS)
CFLAGS += -w
CXXFLAGS += -w

ifeq ($(SMSBARE_SIZE_OPT),1)
OPTIMIZE := -Os
GC_SECTIONS := 1
endif

OBJS = \
	main.o \
	kernel.o \
	combo_screen_device.o \
	tools/input/keyboard_input.o \
	tools/input/usb_hid_input_queue.o \
	tools/usb_hid_gamepad.o \
	tools/usb_hid_keyboard.o \
	tools/render/gfx_textbox.o \
	tools/render/viewport_screenshot.o \
	tools/menu/combo_menu.o \
	tools/menu/combo_menu_items.o \
	tools/media/media_catalog.o \
	tools/file_browser/combo_file_browser.o

ifeq ($(DEBUG),1)
OBJS += tools/debug/combo_debug_overlay.o
endif

OBJS += \
	tools/locale/combo_locale.o \
	tools/locale/combo_locale_pt.o \
	tools/locale/combo_locale_en.o \
	tools/locale/combo_locale_es.o \
	backend/core/registry.o

SMSPLUS_UPSTREAM_OBJS = \
	../third_party/smsplus/system.o \
	../third_party/smsplus/sms.o \
	../third_party/smsplus/pio.o \
	../third_party/smsplus/memz80.o \
	../third_party/smsplus/vdp.o \
	../third_party/smsplus/tms.o \
	../third_party/smsplus/render.o \
	../third_party/smsplus/sound/sound.o \
	../third_party/smsplus/sound/emu2413.o \
	../third_party/smsplus/sound/fmintf.o \
	../third_party/smsplus/sound/ym2413.o \
	../third_party/smsplus/cpu/z80.o

SMSPLUS_LOCAL_OBJS = \
	$(SMSPLUS_MODULE_DIR)/smsplus_bare_psg.o \
	$(SMSPLUS_MODULE_DIR)/smsplus_math_shim.o

SMS_RUNTIME_OBJS = \
	$(SMS_BACKEND_DIR)/backend.o \
	$(SMSPLUS_MODULE_DIR)/module.o \
	$(SMSPLUS_MODULE_DIR)/smsplus_bare_stubs.o \
	$(SMSPLUS_LOCAL_OBJS) \
	$(SMSPLUS_UPSTREAM_OBJS)

OBJS += $(SMS_RUNTIME_OBJS)

BACKEND_MODULE_SMS_OBJS = \
	$(SMSPLUS_MODULE_DIR)/module.o \
	$(SMSPLUS_MODULE_DIR)/smsplus_bare_stubs.o \
	$(SMSPLUS_LOCAL_OBJS) \
	$(SMSPLUS_UPSTREAM_OBJS)

LIBS = $(CIRCLEHOME)/lib/sound/libsound.a \
	   $(CIRCLEHOME)/lib/sched/libsched.a \
	   $(CIRCLEHOME)/lib/usb/libusb.a \
	   $(CIRCLEHOME)/lib/input/libinput.a \
	   $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	   $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	   $(CIRCLEHOME)/lib/fs/libfs.a \
	   $(CIRCLEHOME)/lib/libcircle.a

# Rules.mk clean handles current dir globs; include nested tool objects/deps.
EXTRACLEAN += tools/*.o tools/*.d tools/*/*.o tools/*/*.d
EXTRACLEAN += backend/*.o backend/*.d backend/*/*.o backend/*/*.d backend/*/*/*.o backend/*/*/*.d backend/*/*/*/*.o backend/*/*/*/*.d

include $(CIRCLEHOME)/Rules.mk

ifeq ($(SMSBARE_EMBED_INITRAMFS),1)
$(SMSPLUS_MODULE_DIR)/module.o: $(SMSBARE_EMBED_INITRAMFS_PATH)
endif

$(CIRCLEHOME)/addon/SDCard/libsdcard.a:
	$(MAKE) -C $(CIRCLEHOME)/addon/SDCard PREFIX64=$(PREFIX64)

$(CIRCLEHOME)/addon/fatfs/libfatfs.a:
	$(MAKE) -C $(CIRCLEHOME)/addon/fatfs PREFIX64=$(PREFIX64) EXTRAINCLUDE=-I$(CURDIR)/fatfs_compat

.PHONY: backend-module

backend-module: $(BACKEND_MODULE_SMS_OBJS)
	@mkdir -p ../artifacts/backend/sms
	$(LD) -r -o ../artifacts/backend/sms/sms.smsplus.mod.elf $(BACKEND_MODULE_SMS_OBJS)

-include $(DEPS)
