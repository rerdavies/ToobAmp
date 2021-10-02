# compiler
CC = g++-8

# language file extension
EXT = cpp
HEXT = h

# source files directory
SRC_DIR = ./src

# program name
BUNDLE_NAME=ToobAmp.lv2
LIBFILE=ToobAmp.so


# default install paths
PREFIX = /usr/local
LV2_INSTALL_DIR = /usr/modep/lv2/$(BUNDLE_NAME)
LV2_BUILD_DIR=~/.lv2/$(BUNDLE_NAME)

# LV2 Bundle Resources
BUNDLE_SOURCE_DIR=./src/ToobAmp.lv2


# default compiler and linker flags
CFLAGS += -Wall -Wextra -Wno-reorder  -Wno-psabi -c -std=c++17 -fPIC  -fno-rtti 
CFLAGS += -Wno-deprecated-declarations
CFLAGS += -Werror=implicit-function-declaration -Werror=return-type
LDFLAGS += -Wl,--no-undefined 

# debug mode compiler and linker flags
ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g -Wall -c -DDEBUG
   LDFLAGS +=
else
   CFLAGS += -O3 -fvisibility=hidden
   LDFLAGS += -s
endif

ifeq ($(TESTBUILD), 1)
# CFLAGS += -Wconversion -Wsign-conversion
CFLAGS += -Werror -Wabi -Wcast-qual -Wclobbered -Wdisabled-optimization -Wfloat-equal -Wformat=2
CFLAGS += -Winit-self -Wmissing-declarations -Woverlength-strings -Wpointer-arith -Wredundant-decls -Wshadow
CFLAGS += -Wundef -Wuninitialized -Wunused -Wnested-externs
CFLAGS += -Wstrict-aliasing -fstrict-aliasing -Wstrict-overflow -fstrict-overflow
CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wwrite-strings -Wno-psabi
endif

# include paths
INCS = $(shell pkg-config --cflags lv2)

LIBS= -lm -lstdc++ 

# source and object files
SRC = $(wildcard $(SRC_DIR)/*.$(EXT)) $(wildcard $(SRC_DIR)/Filters/*.$(EXT)) 
HEADERS = $(wildcard $(SRC_DIR)/*.$(HEXT)) $(wildcard $(SRC_DIR)/Filters/*.$(HEXT)) 
OBJ = $(SRC:.$(EXT)=.o)

# default build
all: $(LIBFILE) 

#include compile dependencies
-include $(OBJ:.o=.d)

$(LIBFILE): $(OBJ)
	$(CC) $(OBJ) $(CLAGS) $(LDFLAGS) $(LIBS) -shared -o $@

# meta-rule to generate the object files
%.o: %.$(EXT)
	$(CC) $(INCS) $(CFLAGS) -o $@ $<
	$(CC) -MM $(CFLAGS) $< -MT $@ > $*.d

# install rule
install: 
	install -d $(LV2_INSTALL_DIR)
	install -m 755 $(LIBFILE) $(LV2_INSTALL_DIR)
	cp -r $(BUNDLE_SOURCE_DIR)/* $(LV2_INSTALL_DIR)


# clean rule
clean:
	@rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*/*.o $(SRC_DIR)/*.d $(SRC_DIR)/*/*.d $(PROG) $(LIBFILE)
	


