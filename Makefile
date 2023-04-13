
CXX = g++
CFLAGS = -g3 -Wall -std=c++11 -DNDEBUG -O3
SHARED = -fPIC --shared
INCLUDE_DIR ?= include/
TARGET = lperf.so

all: $(TARGET)

$(TARGET): lperf.cpp
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR) $(SHARED)  $^ -o $@

clean:
	rm -f $(TARGET)