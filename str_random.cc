#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "Bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#endif

#include "CLI11.hpp"                            // 引入 CLI11 库
#include "charSet.hpp"                          // 引入默认字符集定义

const std::string VERSION = "3.2.0";
const std::string DEFAULT_CHARSET = std::string(digit) + std::string(en);

// 大小限制常量
const size_t MAX_OUTPUT_SIZE = 10 * 1024 * 1024;  // 10 MB
const size_t CHUNK_SIZE = 1024;                     // 1 KB

// 使用系统调用获取密码学安全的随机数（Linux: getrandom/urandom；Windows: BCryptGenRandom）
class SystemRandomGenerator
{
   public:
    using result_type = uint64_t;

    SystemRandomGenerator() = default;

    template <typename T = uint64_t>
    T operator()()
    {
        T value{};
        fill_random_bytes(reinterpret_cast<unsigned char*>(&value), sizeof(T));
        return value;
    }

    static constexpr uint64_t min() { return 0; }
    static constexpr uint64_t max() { return UINT64_MAX; }

   private:
    // 先尝试 getrandom；若内核不支持则回落到 /dev/urandom；Windows 使用 BCryptGenRandom
    static void fill_random_bytes(unsigned char* buffer, size_t size)
    {
    #ifdef _WIN32
            NTSTATUS status = BCryptGenRandom(nullptr, buffer, static_cast<ULONG>(size),
                                              BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if (status != STATUS_SUCCESS)
            {
                throw std::runtime_error("BCryptGenRandom 失败");
            }
    #else
        size_t filled = 0;
        while (filled < size)
        {
            ssize_t ret = getrandom(buffer + filled, size - filled, GRND_NONBLOCK);
            if (ret < 0)
            {
                if (errno == EINTR)
                {
                    continue;  // 被信号中断，重试
                }
                if (errno == ENOSYS)
                {
                    fill_from_urandom(buffer + filled, size - filled);
                    return;
                }
                throw std::runtime_error("getrandom 失败: " + std::string(std::strerror(errno)));
            }
            filled += static_cast<size_t>(ret);
        }
#endif
    }

#ifndef _WIN32
    static void fill_from_urandom(unsigned char* buffer, size_t size)
    {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0)
        {
            throw std::runtime_error("无法打开 /dev/urandom: " + std::string(std::strerror(errno)));
        }

        size_t read_bytes = 0;
        while (read_bytes < size)
        {
            ssize_t ret = read(fd, buffer + read_bytes, size - read_bytes);
            if (ret < 0)
            {
                if (errno == EINTR)
                {
                    continue;  // 被信号中断，重试
                }
                close(fd);
                throw std::runtime_error("读取 /dev/urandom 失败: " + std::string(std::strerror(errno)));
            }
            if (ret == 0)
            {
                close(fd);
                throw std::runtime_error("读取 /dev/urandom 意外返回 0 字节");
            }
            read_bytes += static_cast<size_t>(ret);
        }

        close(fd);
    }
#endif
};

// 将 UTF-8 字符串拆分为单个字符（字符串向量）
std::vector<std::string> split_utf8_string(const std::string_view& str)
{
    std::vector<std::string> chars;
    for (size_t i = 0; i < str.length();)
    {
        unsigned char c = static_cast<unsigned char>(str[i]);
        size_t n = 1;

        // 根据 UTF-8 首字节判断字符长度
        if ((c & 0x80) == 0)
            n = 1;  // ASCII
        else if ((c & 0xE0) == 0xC0)
            n = 2;  // 2字节字符
        else if ((c & 0xF0) == 0xE0)
            n = 3;  // 3字节字符 (汉字通常在这里)
        else if ((c & 0xF8) == 0xF0)
            n = 4;  // 4字节字符

        // 边界检查，防止非法 UTF-8 导致越界
        if (i + n > str.length())
            n = 1;

        chars.emplace_back(str.substr(i, n));
        i += n;
    }
    return chars;
}

// 函数：从文件读取字符集
std::string load_charset_from_file(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开字符集文件: " + filename);
    }

    // 读取文件所有内容到字符串
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string charset = buffer.str();

    // 移除空白字符，并确保字符集非空
    // 修复：使用 lambda 强制转换为 unsigned char，避免 isspace 处理汉字字节时的未定义行为
    charset.erase(std::remove_if(charset.begin(), charset.end(),
                                 [](unsigned char c) { return std::isspace(c); }),
                  charset.end());

    if (charset.empty())
    {
        throw std::runtime_error("字符集文件为空或只包含空白字符。");
    }

    return charset;
}

// 函数：生成指定长度的随机字符串，使用系统调用的密码学安全随机数生成器
std::string generate_random_string(size_t length, const std::vector<std::string>& charset,
                                   SystemRandomGenerator& generator)
{
    if (charset.empty())
    {
        return "";
    }

    // 定义均匀分布
    // 范围是 [0, charset.size() - 1]
    std::uniform_int_distribution<size_t> distribution(0, charset.size() - 1);

    std::string random_string;
    random_string.reserve(length);  // 预留空间

    // 循环生成字符串
    for (size_t i = 0; i < length; ++i)
    {
        // 从分布中获取一个随机索引
        size_t random_index = distribution(generator);

        // 将字符集对应索引的完整字符（string）添加到结果中
        random_string += charset[random_index];
    }

    return random_string;
}

// 主函数，处理命令行参数
int main(int argc, char* argv[])
{
    CLI::App app{"随机字符串生成器 (使用系统调用的密码学安全随机数)"};
    app.set_version_flag("-v,--version", VERSION, "显示版本信息");

    size_t length = 16;
    int count = 1;
    int per_line = 1;
    int key_bits = 0;  // 等效密钥长度（比特数），0 表示未指定
    std::string charset_literal;
    std::vector<std::string> charset_sources;
    bool show_charset = false;

    // 定义参数
    // 位置参数 1: 长度
    app.add_option("length", length, "生成的字符串长度")->default_val(16);

    // 位置参数 2: 数量
    app.add_option("count", count, "生成的字符串数量")->default_val(1);

    // 选项参数: -s/--set
    app.add_option("-s,--set", charset_sources, "字符集来源 (dn, en, zh, sp,或文件路径)")
        ->expected(1, -1);  // 允许至少 1 个，最多不限

    // 选项参数: -c/--charset
    app.add_option("-c,--charset", charset_literal, "直接提供字符集字符串（可与 -s 组合）");

    // 选项参数: --show-charset
    app.add_flag("--show-charset", show_charset, "输出最终字符集后再生成字符串");

    // 选项参数: -n/--per-line
    app.add_option("-n,--per-line", per_line, "每行输出的字符串数量")->default_val(1);

    // 选项参数: -k/--key-bits
    app.add_option("-k,--key-bits", key_bits, "等效密钥长度（比特数），根据字符集熵自动计算字符串长度")
        ->default_val(0);

    CLI11_PARSE(app, argc, argv);

    // 逻辑处理：构建最终的字符集字符串
    std::string final_charset_str{};

    if (charset_sources.empty() && charset_literal.empty())
    {
        final_charset_str = std::string(DEFAULT_CHARSET);
    }
    else
    {
        if (!charset_literal.empty())
        {
            final_charset_str += charset_literal;
        }

        for (const auto& source : charset_sources)
        {
            if (source == "dn")
            {
                final_charset_str += std::string(digit);
            }
            else if (source == "en")
            {
                final_charset_str += std::string(en);
            }
            else if (source == "zh")
            {
                final_charset_str += std::string(zh);
            }
            else if (source == "sp")
            {
                final_charset_str += std::string(special);
            }
            else
            {
                try
                {
                    final_charset_str += load_charset_from_file(source);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "警告: " << e.what() << " (跳过)\n";
                }
            }
        }
    }

    // 统一清理空白字符
    final_charset_str.erase(std::remove_if(final_charset_str.begin(), final_charset_str.end(),
                                           [](unsigned char c) { return std::isspace(c); }),
                            final_charset_str.end());

    // 预处理：将字符集解析为字符向量
    std::vector<std::string> charset_vec = split_utf8_string(final_charset_str);

    // 去重逻辑：排序并移除重复字符
    std::sort(charset_vec.begin(), charset_vec.end());
    charset_vec.erase(std::unique(charset_vec.begin(), charset_vec.end()), charset_vec.end());

    if (charset_vec.empty())
    {
        std::cerr << "错误: 有效字符集为空。\n";
        return 1;
    }

    if (show_charset)
    {
        std::cerr << "字符集(" << charset_vec.size() << "): ";
        for (const auto& ch : charset_vec)
        {
            std::cerr << ch;
        }
        std::cerr << "\n";
    }

    // 如果指定了等效密钥长度，根据字符集熵计算所需字符串长度
    if (key_bits > 0)
    {
        // 计算字符集的熵（每个字符提供的比特数）
        double entropy_per_char = std::log2(static_cast<double>(charset_vec.size()));
        
        // 计算需要的字符数以达到指定的密钥强度
        length = static_cast<size_t>(std::ceil(key_bits / entropy_per_char));
        
        // 输出信息
        std::cerr << "字符集大小: " << charset_vec.size() << " 个字符\n";
        std::cerr << "每字符熵: " << entropy_per_char << " 比特\n";
        std::cerr << "目标密钥强度: " << key_bits << " 比特\n";
        std::cerr << "计算得到的字符串长度: " << length << " 个字符\n";
        std::cerr << "实际密钥强度: " << (length * entropy_per_char) << " 比特\n";
        std::cerr << "---\n";
    }

    // 初始化系统调用的密码学安全随机数生成器
    SystemRandomGenerator generator;

    // 估算总输出大小
    // 计算字符集中每个字符的平均字节数
    size_t total_charset_bytes = 0;
    for (const auto& ch : charset_vec)
    {
        total_charset_bytes += ch.size();
    }
    double avg_bytes_per_char = static_cast<double>(total_charset_bytes) / charset_vec.size();

    // 估算每个随机字符串的字节数
    size_t estimated_string_bytes = static_cast<size_t>(length * avg_bytes_per_char);

    // 估算分隔符的字节数（空格或换行）
    size_t separator_bytes = count - 1;  // count-1 个分隔符（空格）
    separator_bytes += (count / per_line);  // 换行符数量

    // 估算总输出大小
    size_t estimated_total_size = estimated_string_bytes * count + separator_bytes;

    // 检查是否超过 10 MB 限制
    if (estimated_total_size > MAX_OUTPUT_SIZE)
    {
        std::cerr << "错误: 估算输出大小 (" << estimated_total_size << " 字节, 约 "
                  << (estimated_total_size / 1024.0 / 1024.0) << " MB) 超过 10 MB 限制。\n";
        std::cerr << "请减少字符串长度或数量。\n";
        return 1;
    }

    // 输出生成的字符串
    // 如果估算大小小于等于 1 KB，直接输出
    if (estimated_total_size <= CHUNK_SIZE)
    {
        for (int i = 0; i < count; ++i)
        {
            if (i > 0 && i % per_line == 0)
            {
                std::cout << "\n";
            }
            else if (i > 0)
            {
                std::cout << " ";
            }
            std::cout << generate_random_string(length, charset_vec, generator);
        }
        std::cout << "\n";
    }
    else
    {
        // 大于 1 KB，使用分块输出
        std::string buffer;
        buffer.reserve(CHUNK_SIZE * 2);  // 预留缓冲区

        for (int i = 0; i < count; ++i)
        {
            // 添加分隔符
            if (i > 0 && i % per_line == 0)
            {
                buffer += "\n";
            }
            else if (i > 0)
            {
                buffer += " ";
            }

            // 生成并添加随机字符串
            buffer += generate_random_string(length, charset_vec, generator);

            // 如果缓冲区大于 1 KB，输出并清空
            if (buffer.size() >= CHUNK_SIZE)
            {
                std::cout << buffer;
                std::cout.flush();  // 立即刷新输出
                buffer.clear();
            }
        }

        // 输出剩余内容
        if (!buffer.empty())
        {
            std::cout << buffer;
        }
        std::cout << "\n";
    }

    return 0;
}
