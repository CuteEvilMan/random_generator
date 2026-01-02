#!/usr/bin/env bash
set -euo pipefail

echo "正在编译 str_random.cc ..."

UNAME=$(uname -s || echo unknown)
LDFLAGS=""

# Windows(MinGW/MSYS) 需要链接 bcrypt
if echo "$UNAME" | grep -qiE "mingw|msys|cygwin"; then
    LDFLAGS+=" -lbcrypt"
fi

g++ -std=c++17 -O2 -Wall -Wextra -Werror -s -I . \
    str_random.cc -o out $LDFLAGS

echo "✓ 编译成功！可执行文件: out"
