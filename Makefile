CXX=g++
CXXFLAGS=-O0 -g3 -ggdb -Wall -Wpedantic -Wconversion -std=c++14 -DDEBUG -c -MD
SOURCE_ROOT=./
LDFLAGS=-lpthread

SOURCE_DIRS = $(SOURCE_ROOT)

INCLUDE_DIRS := $(patsubst %, -I%, $(SOURCE_DIRS))

# pass include dirs to gcc via environment to not bloat output with -Idirectives
export C_INCLUDE_PATH:=$(subst $(space),$(colon),$(realpath $(INCLUDE_DIRS)))

CXXSOURCES := $(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)*.cpp))
CXXOBJECTS=$(CXXSOURCES:.cpp=.o)

EXECUTABLE=app

all: $(CXXSOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(CXXOBJECTS)
	$(CXX) $(CXXOBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

PHONY: clean debug

clean:
	rm -rf ./*.d ./*.o ./$(EXECUTABLE) $(CXXOBJECTS)

include $(wildcard $(addsuffix /*.d, $(dir $(CXXOBJECTS))))

debug:
	@echo $(CXXOBJECTS)
