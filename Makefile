# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Iinclude -O2
DEBUG_FLAGS = -g -O0 -DDEBUG -gdwarf-4
# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# Files
# This automatically finds all .cpp files in src/
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# This converts src/filename.cpp to obj/filename.o
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Target executable
TARGET = yabr

# Default rule
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# Rule to compile .cpp files into .o files in the obj directory
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create the obj directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Debugging
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: TARGET = yabr-debug
debug: all

# Clean up build files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean

