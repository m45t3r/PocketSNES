# Define the applications properties here:

TARGET = ./dist/PocketSNES.dge

CROSS_COMPILE ?= mipsel-linux-

CC  := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
STRIP := $(CROSS_COMPILE)strip

SYSROOT := $(shell $(CC) --print-sysroot)
SDL_CFLAGS := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

INCLUDE = -I pocketsnes \
		-I sal/linux/include -I sal/include \
		-I pocketsnes/include \
		-I menu -I pocketsnes/linux -I pocketsnes/snes9x

CFLAGS = $(INCLUDE) -DRC_OPTIMIZED -DGCW_ZERO -DGCW_JOYSTICK -D__LINUX__ -D__DINGUX__ -DFOREVER_16_BIT -DFOREVER_16_BIT_SOUND -DLAGFIX
# CFLAGS += -ggdb3 -Og
CFLAGS += -Ofast -fdata-sections -ffunction-sections -mips32r2 -mno-mips16 -mplt -mno-shared
CFLAGS += -fomit-frame-pointer -fno-builtin -fno-common -flto=4
CFLAGS += -DFAST_LSB_WORD_ACCESS
CFLAGS += $(SDL_CFLAGS)
ifdef PROFILE_GEN
CFLAGS += -fprofile-generate -fprofile-dir=/media/data/local/home/profile/pocketsnes
else ifdef PROFILE_USE
CFLAGS += -fprofile-use -fprofile-dir=./profile
endif

CXXFLAGS = $(CFLAGS) -std=gnu++03 -fno-exceptions -fno-rtti -fno-math-errno -fno-threadsafe-statics

LDFLAGS = $(CXXFLAGS) -lz -lpng $(SDL_LIBS) -Wl,-O1,--sort-common,--as-needed
ifdef HUGE_PAGES
LDFLAGS += -Wl,-zcommon-page-size=2097152 -Wl,-zmax-page-size=2097152 -lhugetlbfs
endif
ifndef PROFILE_GEN
LDFLAGS += -Wl,--gc-sections -s
endif

# Find all source files
SOURCE = pocketsnes/snes9x menu sal/linux sal
SRC_CPP = $(foreach dir, $(SOURCE), $(wildcard $(dir)/*.cpp))
SRC_C   = $(foreach dir, $(SOURCE), $(wildcard $(dir)/*.c))
OBJ_CPP = $(patsubst %.cpp, %.o, $(SRC_CPP))
OBJ_C   = $(patsubst %.c, %.o, $(SRC_C))
OBJS    = $(OBJ_CPP) $(OBJ_C)

.PHONY : all
all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CMD)$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@
ifdef HUGE_PAGES
	hugeedit --text --data $(TARGET)
endif

.PHONY: opk
opk: $(TARGET)
ifdef HUGE_PAGES
	opk/make_opk.sh PocketSNES_new.opk
else
	opk/make_opk.sh
endif

%.o: %.c
	$(CMD)$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CMD)$(CXX) $(CFLAGS) -c $< -o $@

.PHONY : clean
clean :
	$(CMD)rm -f $(OBJS) $(TARGET)
	$(CMD)rm -rf .opk_data $(TARGET).opk dist/pocketsnes.ipk
