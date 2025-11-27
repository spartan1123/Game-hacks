# Kernel Driver Makefile
# Requires Windows Driver Kit (WDK)

DRIVER_NAME = GameMemoryReader
TARGET_TYPE = DRIVER
DRIVER_TYPE = WDM

SOURCES = kernel_driver.c

INCLUDES = $(DDK_INC_PATH)

TARGETLIBS = $(DDK_LIB_PATH)\wdfldr.lib \
             $(DDK_LIB_PATH)\ntoskrnl.lib \
             $(DDK_LIB_PATH)\hal.lib

MSC_WARNING_LEVEL = /W4 /WX

C_DEFINES = $(C_DEFINES) -DUNICODE -D_UNICODE

# Build using WDK build environment
all:
	@echo "Building kernel driver..."
	build -cZ

clean:
	@echo "Cleaning driver build..."
	build -cZ -clean

.PHONY: all clean
