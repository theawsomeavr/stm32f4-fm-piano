all: m
CC = x86_64-w64-mingw32-g++
#CC = g++
OUT_NAME = piano.exe
OBJDIR  = obj
GOYCO_CPP_LIBS ?= ./libs

SOURCE_DIR = \
. \
$(GOYCO_CPP_LIBS)/mz_report \
$(GOYCO_CPP_LIBS)/rtmidi \
$(GOYCO_CPP_LIBS)/midi_parsee \

INCLUDE_DIR = $(SOURCE_DIR) \
$(GOYCO_CPP_LIBS)/raylib

FLAGS = -O3 $(foreach I_dir,$(INCLUDE_DIR),$(addsuffix $(I_dir),-I)) -std=c++20
LFLAGS = -static -static-libgcc -static-libstdc++ $(GOYCO_CPP_LIBS)/raylib/libraylib_win.a -lwinmm -lgdi32
#LFLAGS = $(GOYCO_CPP_LIBS)/raylib/libraylib_linux.a -lasound

SOURCES = $(foreach dir,$(SOURCE_DIR),$(wildcard $(dir)/*.cpp))
OBJECTS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(notdir $(basename $(SOURCES)))))
SRCPATH = $(sort $(dir $(SOURCES)))
vpath %.cpp $(SRCPATH)

m:
	@$(MAKE) -j4 $(OBJECTS)
	$(CC) -o $(OUT_NAME) $(OBJECTS) $(LFLAGS)

$(OBJDIR)/%.o: %.cpp
	$(CC) $(FLAGS) -o $@ -c $<

clean:
	-rm -f $(OUT_NAME)
	-rm $(OBJDIR)/*.*
