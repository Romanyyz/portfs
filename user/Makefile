TARGET = portfs_tool.out

PROJECT_ROOT := $(abspath ..)

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O3
CXXFLAGS += -I$(PROJECT_ROOT)/common

SRCS = main.cpp UIManager.cpp StorageManager.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
