# 随机字符串生成器

使用系统调用的密码学安全随机数生成器（Linux: getrandom/urandom，Windows: BCryptGenRandom），基于 [str_random.cc](str_random.cc) 实现。支持多种字符集（数字、英文、中文、特殊符号）、可自定义字符集文件，直接传入自定义字符串，并可在生成前打印最终字符集。

## 功能
- CLI 生成随机字符串，可指定长度与数量
- 默认字符集：数字 + 英文字母
- 支持字符集选项：`dn`(数字)、`en`(英文)、`zh`(常用汉字)、`sp`(特殊符号)、自定义文件路径、直接输入字符串
- 支持每行输出多个字符串，便于批量生成
- 可选输出最终字符集，便于调试
- 版本：3.3.0

## 依赖
- C++17 编译器（示例使用 `g++`）
- Linux: 无额外依赖；Windows: 依赖系统自带的 bcrypt（`bcrypt.lib`）
- [CLI11.hpp v2.6.1](https://github.com/CLIUtils/CLI11/releases/download/v2.6.1/CLI11.hpp) 放到仓库根目录

## 构建
```bash
./build.sh
```
生成可执行文件 `out`。如果需要手动编译：
```bash
g++ -std=c++17 -O2 -Wall -Wextra -Werror -s -I . \
  str_random.cc -o out
```
Windows 可使用 [build_windows.bat](build_windows.bat)，会优先尝试 MSVC，其次 MinGW。

## 用法示例
- 生成 16 位字符串（默认字符集），输出 1 个：
```bash
./out 16
```

- 生成 20 位字符串 5 个，每行 2 个：
```bash
./out 20 5 -n 2
```

- 生成 12 位字符串，字符集为数字+特殊符号：
```bash
./out 12 -s dn sp
```

- 直接提供字符集字符串（会自动去重并移除空白）：
```bash
./out 10 -c "ABCxyz123" 5
```

- 生成前输出最终字符集（与 -s/-c 组合）：
```bash
./out 8 -s dn sp --show-charset
```

- 使用自定义字符集文件 `myset.txt`（会去除空白字符）：
```bash
./out 10 -s myset.txt
```

## CLI 帮助
```bash
./out -h

随机字符串生成器 (使用系统调用的密码学安全随机数)
% ./out -h                                                                       alc2@archsg
随机字符串生成器 (使用系统调用的密码学安全随机数) 


./out [OPTIONS] [length] [count]


POSITIONALS:
  length UINT [16]            生成的字符串长度 
  count INT [1]               生成的字符串数量 

OPTIONS:
  -h,     --help              Print this help message and exit 
  -v,     --version           显示版本信息 
  -s,     --set TEXT ...      字符集来源 (dn, en, zh, sp,或文件路径) 
  -c,     --charset TEXT      直接提供字符集字符串（可与 -s 组合） 
          --show-charset      输出最终字符集后再生成字符串 
  -n,     --per-line INT [1]  每行输出的字符串数量 
  -k,     --key-bits INT [0]  
                              等效密钥长度（比特数），根据字符集熵自动计算字符串长度 

## 字符集说明
- `dn`: 数字 `0-9`
- `en`: 英文大小写字母
- `zh`: 常用汉字（见 [charSet.hpp](charSet.hpp)）
- `sp`: 常见特殊符号
- 文件路径: 读取文件全部字符并剔除空白，重复字符会自动去重

## 其他
- 主代码： [str_random.cc](str_random.cc)
- 字符集定义： [charSet.hpp](charSet.hpp)
- 脚本： [build.sh](build.sh)