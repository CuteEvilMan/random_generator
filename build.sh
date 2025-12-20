#!/bin/bash

# 编译脚本 - 使用 PCG 随机数生成器的随机字符串生成器

echo "正在编译 str_random.cc ..."

# 编译命令 - 添加 -Wno-unused-but-set-parameter 来抑制 PCG 库的警告
g++ -std=c++17 -O2 -Wall -Wextra \
    -Werror \
    -s \
    -I./pcg-cpp-0.98/include \
    str_random.cc \
    -o out

# 检查编译是否成功
if [ $? -eq 0 ]; then
    echo "✓ 编译成功！可执行文件: out"
    echo ""
    echo "使用示例:"
    echo "  ./out 16 10              # 生成10个长度为16的随机字符串"
    echo "  ./out 20 5 -s zh         # 使用中文字符集"
    echo "  ./out 12 100 -r 1000     # 每1000次操作重新播种"
    echo ""
    
else
    echo "✗ 编译失败！"
    exit 1
fi
