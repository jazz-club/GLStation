APP_NAME := glstation
BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIR := include

rwildcard = $(foreach d,$(wildcard $(1)*),$(call rwildcard,$d/,$(2)) $(filter $(subst *,%,$(2)),$d))

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
ifneq ($(OS),Windows_NT)
  ifeq ($(findstring MINGW,$(UNAME_S)),)
    ifeq ($(findstring MSYS,$(UNAME_S)),)
      UNAME_S := $(UNAME_S)
    endif
  endif
endif

ifdef OS
  ifeq ($(OS),Windows_NT)
    _WIN := 1
  endif
endif
ifneq ($(findstring MINGW,$(UNAME_S)),)
  _WIN := 1
endif
ifneq ($(findstring MSYS,$(UNAME_S)),)
  _WIN := 1
endif

ifeq ($(_WIN),1)
  DETECTED_OS := Windows
  EXE_EXT := .exe
  CXX := g++
  MKDIR := mkdir -p
  RM := rm -rf
  CXXFLAGS_OS := -D_WIN32 -static-libgcc -static-libstdc++ -static
  LIBS := -lwinhttp
else
  DETECTED_OS := Linux
  EXE_EXT :=
  CXX := g++
  MKDIR := mkdir -p
  RM := rm -rf
  CXXFLAGS_OS :=
  LIBS := -lcurl
endif

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -I$(INCLUDE_DIR) -DNDEBUG $(CXXFLAGS_OS)

SRCS := $(call rwildcard,$(SRC_DIR)/,*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

.PHONY: all clean run info

all: dirs $(BUILD_DIR)/$(APP_NAME)$(EXE_EXT)

dirs:
	@$(MKDIR) $(BUILD_DIR)

$(BUILD_DIR)/$(APP_NAME)$(EXE_EXT): $(OBJS)
	@echo [LINK] $@
	@$(CXX) $(CXXFLAGS_OS) $(OBJS) -o $@ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo [CXX] $<
	@$(MKDIR) $(dir $@)
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	@echo [CLEAN]
	@$(RM) $(BUILD_DIR)
	@$(RM) *.s *.d *.log build_err*.txt

run: all
	@$(BUILD_DIR)/$(APP_NAME)$(EXE_EXT)

info:
	@echo Platform: $(DETECTED_OS)  CXX: $(CXX)  LIBS: $(LIBS)  Target: $(BUILD_DIR)/$(APP_NAME)$(EXE_EXT)

-include $(wildcard $(BUILD_DIR)/**/*.d)
