# 自动检测平台
UNAME := $(shell uname -s)
LDFLAGS :=

ifeq ($(findstring MINGW,$(UNAME)),MINGW)
    LDFLAGS += -lbcrypt
endif
ifeq ($(findstring MSYS,$(UNAME)),MSYS)
    LDFLAGS += -lbcrypt
endif
ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)
    LDFLAGS += -lbcrypt
endif

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror -s -I .

SRC := str_random.cc
TARGET := out

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)
