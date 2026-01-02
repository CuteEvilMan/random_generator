@echo off
setlocal

rem Windows 构建脚本（优先使用 cl，其次 g++/MinGW）

where cl >NUL 2>NUL
if %errorlevel%==0 goto use_cl

where g++ >NUL 2>NUL
if %errorlevel%==0 goto use_gpp

echo 未找到可用的 C++ 编译器（需要 cl 或 g++）。
exit /b 1

:use_cl
set "CXXFLAGS=/std:c++17 /O2 /W4 /EHsc /utf-8 /nologo"
cl %CXXFLAGS% /I . str_random.cc bcrypt.lib /Fe:out.exe
if %errorlevel% neq 0 exit /b 1
echo ✓ 编译成功 (cl) -> out.exe
exit /b 0

:use_gpp
set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -Werror -s -I ."
g++ %CXXFLAGS% str_random.cc -lbcrypt -o out.exe
if %errorlevel% neq 0 exit /b 1
echo ✓ 编译成功 (g++) -> out.exe
exit /b 0
