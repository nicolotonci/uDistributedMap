CXXFLAGS            += -std=c++17
ifdef DEBUG
    OPTIMIZE_FLAGS  += -g -fno-inline-functions -DVERBOSE
else
    OPTIMIZE_FLAGS  += -O3 -finline-functions -DNDEBUG
endif
ifdef BLOCKING_MODE
    CXXFLAGS        += -DBLOCKING_MODE
endif
ifdef FF_HOME
    INCS            += -I$(FF_HOME)
else
    INCS            += -I ~/SPM/fastflow
endif
ifdef CEREAL_HOME
    INCS            += -I$(CEREAL_HOME)
else
    INCS            += -I include
endif
ifdef LOCAL
	CXXFLAGS += -DLOCAL
else
	CXXFLAGS += -DREMOTE
endif

CXXFLAGS            += -Wall
INCS                += -Isrc/
LIBS                 = -pthread
INCLUDES             = $(INCS)

SOURCES              = $(wildcard *.cpp)
TARGET               = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 
.SUFFIXES: .c .cpp .o

%.d: %.cpp
	set -e; $(CXX) -MM $(INCLUDES) $(CXXFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@
%.d: %.c
	set -e; $(CC) -MM $(INCLUDES) $(CFLAGS)  $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@
%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c -o $@ $<
%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

all: $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET) *.d ./socket*

include $(OBJS:.o=.d)
