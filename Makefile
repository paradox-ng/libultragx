#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

#---------------------------------------------------------------------------------
# Target platform: gamecube (default, primary) or wii (secondary).
#   make                -> GameCube .dol  (libultragx-gamecube.dol)
#   make PLATFORM=wii   -> Wii .dol       (libultragx-wii.dol)
# The GX/VI code is identical on both; wii_rules defines HW_RVL so the Wii-only
# bits (e.g. Wii remote input) compile in only for the Wii build.
#---------------------------------------------------------------------------------
PLATFORM	?=	gamecube
export PLATFORM

# Which app to build (one main per dir under source/apps): smoke | gfxdemo
APP		?=	smoke
export APP

ifeq ($(PLATFORM),wii)
include $(DEVKITPPC)/wii_rules
PLATFORM_LIBS	:=	-lwiiuse -lbte
else ifeq ($(PLATFORM),gamecube)
include $(DEVKITPPC)/gamecube_rules
PLATFORM_LIBS	:=
else
$(error Unknown PLATFORM '$(PLATFORM)' - use 'gamecube' or 'wii')
endif

#---------------------------------------------------------------------------------
# TARGET   is the name of the output (.dol/.elf)
# BUILD    holds object files & intermediates
# SOURCES  are directories containing source code
# INCLUDES are directories containing extra headers
#---------------------------------------------------------------------------------
TARGET		:=	libultragx-$(PLATFORM)
BUILD		:=	build_$(PLATFORM)
SOURCES		:=	source/apps/$(APP) source/platform source/ship source/config source/bridge \
				source/log source/utils/binarytools source/window/gui source/debug source/gfx \
				source/ship/resource source/ship/resource/archive source/ship/utils \
				source/fast source/fast/resource source/fast/debug \
				extern/prism/src/prism extern/prism/src/prism/utils
DATA		:=	data
INCLUDES	:=	source include \
				extern/prism/src extern/prism/src/prism extern/prism/src/prism/utils

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
# CVar name strings the adopted Fast3D interpreter expects as compile defines
# (libultraship sets these via cmake/cvars.cmake; we mirror the two it uses).
CVAR_DEFS	=	-DCVAR_INTERNAL_RESOLUTION='"gInternalResolution"' -DCVAR_MSAA_VALUE='"gMSAAValue"'
CFLAGS		=	-g -O2 -Wall -DIS_BIGENDIAN $(CVAR_DEFS) $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)
LDFLAGS		=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS		:=	$(PLATFORM_LIBS) -lfat -lz -logc -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries (top level, containing include and lib)
#---------------------------------------------------------------------------------
LIBDIRS		:=	$(PORTLIBS_PATH)/ppc

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless adding rules for new
# file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_BIN		:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
							$(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES			:=	$(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
					-I$(CURDIR)/include \
					-I$(CURDIR)/include/libultraship \
					-I$(CURDIR)/extern/json/single_include \
					-I$(CURDIR)/extern/prism/src \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	-L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr build_gamecube build_wii \
		libultragx-gamecube.elf libultragx-gamecube.dol \
		libultragx-wii.elf libultragx-wii.dol

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
