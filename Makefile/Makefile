######################################
# target
######################################
TARGET = MC-S001_Console

######################################
# building variables
######################################
# debug build
ifeq ($(release), y)
DEBUG = 0
else
DEBUG = 1
endif
# optimization
OPT ?= -O0

#######################################
# Parallel compilation jobs
#######################################
# Valid J range: minimum is 1 (= serial, no parallel).
# Best value is around the CPU logical core count; going far above it
# does not help and may slow the build down.
# Default is 8; override on command line with: make J=16
J ?= 8
MAKEFLAGS += -j$(J)

#######################################
# Build path
#######################################
BUILD_DIR = build

#####################################
# chip platform info
######################################
TARGET_STARTUP  := n32h47x_48x
## NTFx CODE START Define ##
DEFS += -DN32H473
DEFS += -DUSE_STDPERIPH_DRIVER
## NTFx CODE END Define ##
######################################
# Algo libs
######################################
#USELIB = 0

######################################
#TOOLS CHAIN
######################################
# The gcc path can be defined by 3 methods:
# 1.Define it in make command via GCC_PATH variable (> make GCC_PATH="D:\\Program Files (x86)\\GNU Arm Embedded Toolchain\\10 2020-q4-major\\bin\\")
# 2.Added it to the PATH environment variable
# 3.Define it in makefile ,such as :
#GCC_PATH = 'D:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2020-q4-major\bin\'
ifdef GCC_PATH
CROSS_COMPILE = $(GCC_PATH)arm-none-eabi-
else 
CROSS_COMPILE = arm-none-eabi-
endif


######################################
# C sources
######################################
## NTFx CODE START C sources ##
C_DIRS += ../USER/src
C_DIRS += ../Driver/CMSIS/core
C_DIRS += ../Driver/CMSIS/device

SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_dbg.c
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_vrefbuf.c
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_flash.c
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_rcc.c
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/misc.c
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_dma.c
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_gpio.c

## NTFx CODE END C sources ##

# Handwritten application and board adapters (outside generated regions).
C_DIRS += ../USER/src/App
C_DIRS += ../USER/src/Board
C_DIRS += ../USER/src/Modules/log
C_DIRS += ../USER/src/Modules/lcd
SRC_OBJS_DIRS += ../Driver/n32h47x_48x_std_periph_driver/src/n32h47x_48x_usart.c

SRC_OBJS_DIRS += $(foreach DIR, $(C_DIRS), $(wildcard $(DIR)/*.c))
C_SOURCES = $(SRC_OBJS_DIRS)

######################################
# ASM sources
######################################
ASM_SOURCES =../Driver/CMSIS/device/startup/startup_$(TARGET_STARTUP)_gcc.s

######################################
# C includes
######################################
## NTFx CODE START C includes ##
C_INCS += ../USER/inc
C_INCS += ../Driver/CMSIS/core
C_INCS += ../Driver/CMSIS/device
C_INCS += ../Driver/n32h47x_48x_std_periph_driver/inc
## NTFx CODE END C includes ##


INCS_OBJS_DIR = $(foreach DIR2, $(C_INCS), $(wildcard $(DIR2)/*.h))
INCS_OBJS_PATH = $(sort $(dir $(INCS_OBJS_DIR)))
C_INCLUDES = $(foreach DIR2, $(INCS_OBJS_PATH), "-I$(abspath $(DIR2))")

######################################
# Lib files
######################################
ifeq ($(USELIB), 1)
SRC_LIB_DIR += ../Driver/n32h47x_48x_algo_lib/lib
INC_LIB_DIR += ../Driver/n32h47x_48x_algo_lib/inc
LIB_SOURCES = $(foreach DIR3, $(SRC_LIB_DIR), $(wildcard $(DIR3)/*.lib))
LIB_SOURCES_L = $(addprefix -L,$(LIB_SOURCES))
C_LIBS = $(LIB_SOURCES_L)
LIB_INCS = $(foreach DIR4, $(INC_LIB_DIR), $(wildcard $(DIR4)/*.h))
LIB_INCS_PATH = $(sort $(dir $(LIB_INCS)))
LIB_INCS_PATH_I = $(addprefix -I,$(LIB_INCS_PATH))
C_INCLUDES += $(LIB_INCS_PATH_I)
endif

######################################
# Compile & Link flags
######################################
# cpu
CPU = -mcpu=cortex-m4
# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

#CFLAGS
CFLAGS += $(MCU) -Wall
CFLAGS += $(OPT)
CFLAGS += -ffunction-sections -fdata-sections
#DEBUG
ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif
# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
#LFLAGS
LFLAGS += $(MCU)
LFLAGS += -Wl,--gc-sections 
LFLAGS += --specs=nosys.specs
LFLAGS += --specs=nano.specs
LFLAGS += -Xlinker -Map=$(BUILD_DIR)/$(TARGET).map
# link script
LDSCRIPT =MC-S001_Console.ld

######################################
# Objects
######################################
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	@echo CC  $(notdir $<)
	@"$(CROSS_COMPILE)gcc" $(CFLAGS) $(DEFS) $(C_INCLUDES) $(C_LIBS) -c -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) "$(abspath $<)" -o $@
    
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo AS  $(notdir $<)
	"$(CROSS_COMPILE)gcc" $(CFLAGS) -c "$(abspath $<)" -o $@
    
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo LD  $(TARGET).elf
	@"$(CROSS_COMPILE)gcc" $(OBJECTS) $(LFLAGS) -T$(LDSCRIPT) -o $@
	@"$(CROSS_COMPILE)size" $@
	
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	"$(CROSS_COMPILE)objcopy" -O binary -S $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	"$(CROSS_COMPILE)objcopy" -O ihex -S $< $@
    
$(BUILD_DIR):
	@-mkdir $@


#######################################
# clean up
#######################################
ifeq ($(OS),Windows_NT)
    RMDIR = rmdir /S /Q
else
    RMDIR = rm -rf
endif
clean:
	-$(RMDIR) $(BUILD_DIR)

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

#######################################
# download .hex/.bin by jlink
#######################################
# Local default; may be overridden with JLINK_PATH=... (with or without /).
JLINK_PATH ?= D:/Jlink/app/JLink
JLINK_SPEED ?= 100
ifneq ($(strip $(JLINK_PATH)),)
JK_DPATH = $(patsubst %/,%,$(subst \,/,$(JLINK_PATH)))/
else
JK_DPATH =
endif

.PHONY: all download download_jlink reset reset_jlink debug_jlink erase_jlink

#Chip type
CHIP_TYPE = N32H473CC

# Build both HEX and BIN before accessing the board. Build failure stops download.
download: download_jlink

download_jlink: all
	@"$(JK_DPATH)JLink.exe" -device $(CHIP_TYPE) -if SWD -speed $(JLINK_SPEED) -autoconnect 1 -ExitOnError 1 -NoGui 1 -CommanderScript MC-S001_Console.jlink
	@echo "Download Completed!"

# Reset and run the current firmware; no build or flash operation.
reset: reset_jlink

reset_jlink:
	@"$(JK_DPATH)JLink.exe" -device $(CHIP_TYPE) -if SWD -speed $(JLINK_SPEED) -autoconnect 1 -ExitOnError 1 -NoGui 1 -CommanderScript MC-S001_Console_reset.jlink
	@echo "Reset Completed!"

debug_jlink:
	@"$(JK_DPATH)JLinkGDBServer.exe" -select USB -device $(CHIP_TYPE) -if SWD -speed auto -noir -LocalhostOnly

erase_jlink:
	@"$(JK_DPATH)JLink.exe" -device $(CHIP_TYPE) -if SWD -speed 4000 -autoconnect 1 -CommanderScript MC-S001_Console_erase.jlink
	@echo "JLink Mass Erase Completed!"

#######################################
# download .bin by CMSIS-DAP (NSLINK)
#######################################
# The openocd.exe path can be defined by 3 methods:
# 1.Define it in make command via OPENOCD_PATH variable
# 2.Added it to the PATH environment variable
# 3.Define it in makefile ,such as :
#OPENOCD_PATH = D:/installVScodeBatTest/openocd-v0.12.0-i686-w64-mingw32/bin
ifdef OPENOCD_PATH
OPENOCD = $(OPENOCD_PATH)openocd.exe
else
OPENOCD = openocd.exe
endif

# OpenOCD config file for CMSIS-DAP
OPENOCD_CFG ?= n32h47x_48x_cmsisdap.cfg

# Reset source used by the CMSIS-DAP flow ONLY. The J-Link targets above do not
# read this: JLink.exe does its own reset from tt-to-cs6.jlink.
# Exactly one source is used, no fallback:
#   0(default) - SYSRESETREQ  
#   1 - HW RESET    
#   2 - VECTRESET    
# NOTE: -c runs before -f on purpose. The cfg reads CMSISDAP_RESET_TYPE while
# it is being sourced, so setting it afterwards would leave it at its default.
CMSISDAP_RESET_TYPE ?= 0

download_cmsisdap:
	@"$(OPENOCD)" -d0 -c "set CMSISDAP_RESET_TYPE $(CMSISDAP_RESET_TYPE)" -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "flash_program_bin $(BUILD_DIR)/$(TARGET).bin 0x08000000" \
		-c "reset run" \
		-c "shutdown" 2>nul
	@echo "CMSIS-DAP Download Completed!"

debug_cmsisdap:
	@echo "======================================="
	@echo "Starting CMSIS-DAP GDB Server"
	@echo "Chip: $(CHIP_TYPE)"
	@echo "GDB port: 3333"
	@echo "Telnet port: 4444"
	@echo "Connect: arm-none-eabi-gdb $(BUILD_DIR)/$(TARGET).elf"
	@echo "  (gdb) target extended-remote localhost:3333"
	@echo "======================================="
	@"$(OPENOCD)" -d0 "set CMSISDAP_RESET_TYPE $(CMSISDAP_RESET_TYPE)" -f $(OPENOCD_CFG)

erase_cmsisdap:
	@"$(OPENOCD)" -d0 -c "set CMSISDAP_RESET_TYPE $(CMSISDAP_RESET_TYPE)" -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset init" \
		-c "flash_mass_erase" \
		-c "reset run" \
		-c "shutdown" 2>nul
	@echo "CMSIS-DAP Mass Erase Completed!"

# *** EOF ***
