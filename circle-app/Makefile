#
# Makefile
#

# -include ../Config.mk

CIRCLEHOME ?= ../circle

OBJS	= main.o kernel.o miniorgan.o

LIBS	= $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/usb/gadget/libusbgadget.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/libcircle.a \
	  ../common/libcommonsynth.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
