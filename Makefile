APP_NAME := glstation
BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include

ifeq ($(OS),Windows_NT)
  DETECTED_OS := Windows
  EXE_EXT := .exe
  CXXFLAGS_OS := -D_WIN32 -static-libgcc -static-libstdc++ -static
else
  DETECTED_OS := Linux
  EXE_EXT :=
  CXXFLAGS_OS :=
endif

CXX := g++
MKDIR := mkdir -p
RM := rm -rf

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -I$(INCLUDE_DIR) -DNDEBUG $(CXXFLAGS_OS)

rwildcard = $(foreach d,$(wildcard $(1)*),$(call rwildcard,$d/,$(2)) $(filter $(subst *,%,$(2)),$d))

SRCS := $(call rwildcard,$(SRC_DIR)/,*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run

all: $(BUILD_DIR)/$(APP_NAME)$(EXE_EXT)

$(BUILD_DIR)/$(APP_NAME)$(EXE_EXT): $(OBJS)
	@echo [LINK] $@
	@$(CXX) $(CXXFLAGS_OS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo [CXX] $<
	@$(MKDIR) $(dir $@)
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	@echo [CLEAN]
	@$(RM) $(BUILD_DIR) gls.csv

run: all
	@$(BUILD_DIR)/$(APP_NAME)$(EXE_EXT)


-include $(DEPS)
