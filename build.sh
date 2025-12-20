#!/bin/bash

# 编译脚本 - 使用 PCG 随机数生成器的随机字符串生成器

echo "正在编译 str_random.cc ..."

g++ -std=c++17 -O2 -Wall -Wextra \
    -Werror \
    -s \
    -I .\
    str_random.cc \
    -o out

# 检查编译是否成功
if [ $? -eq 0 ]; then
    echo "✓ 编译成功！可执行文件: out"
    
    
else
    echo "✗ 编译失败！"
    exit 1
fi
