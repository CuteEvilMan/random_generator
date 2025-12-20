#include <algorithm>

#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CLI11.hpp"                            // 引入 CLI11 库
#include "charSet.hpp"                          // 引入默认字符集定义
#include "pcg_random.hpp"  // 引入 PCG 随机数生成器

const std::string VERSION = "1.1.0";
const std::string DEFAULT_CHARSET = std::string(digit) + std::string(en);

// PCG 随机数生成器包装类，支持自动注入真随机数
class AutoReseedPCG
{
   public:
    using result_type = uint64_t;  // 定义结果类型，符合 C++ 随机数生成器要求

   private:
    pcg64 generator_;                 // PCG 64位随机数生成器
    std::random_device true_random_;  // 真随机数源
    size_t operations_count_;         // 操作计数器
    size_t reseed_interval_;          // 重新播种间隔

    // 从真随机数源获取种子
    uint64_t get_true_random_seed()
    {
        // 使用 random_device 生成 64 位种子
        uint64_t seed = static_cast<uint64_t>(true_random_()) << 32 | true_random_();
        return seed;
    }

    // 重新播种
    void reseed()
    {
        uint64_t seed = get_true_random_seed();
        uint64_t seq = get_true_random_seed();
        generator_.seed(seed, seq);

#ifdef DEBUG_RESEED
        std::cerr << "[自动重新播种] 操作次数: " << operations_count_ << ", 新种子: " << seed
                  << ", 序列: " << seq << std::endl;
#endif
    }

   public:
    // 构造函数：初始化 PCG 生成器，设置重新播种间隔
    explicit AutoReseedPCG(size_t reseed_interval = 10000)
        : operations_count_(0), reseed_interval_(reseed_interval)
    {
        // 初始播种
        reseed();
    }

    // 生成随机数，自动检查是否需要重新播种
    template <typename T = uint64_t>
    T operator()()
    {
        // 每次调用增加计数器
        ++operations_count_;

        // 检查是否需要重新播种
        if (operations_count_ % reseed_interval_ == 0)
        {
            reseed();
        }

        // 返回随机数
        if constexpr (std::is_same_v<T, uint32_t>)
        {
            return static_cast<uint32_t>(generator_());
        }
        else
        {
            return generator_();
        }
    }

    // 获取最小值（用于 uniform_int_distribution）
    static constexpr uint64_t min() { return pcg64::min(); }

    // 获取最大值（用于 uniform_int_distribution）
    static constexpr uint64_t max() { return pcg64::max(); }

    // 手动触发重新播种
    void force_reseed() { reseed(); }

    // 设置重新播种间隔
    void set_reseed_interval(size_t interval) { reseed_interval_ = interval; }

    // 获取当前操作计数
    size_t get_operations_count() const { return operations_count_; }
};

// 新增函数：将 UTF-8 字符串拆分为单个字符（字符串向量）
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

// 函数：生成指定长度的随机字符串
// 修改：使用 AutoReseedPCG 替代 std::mt19937
std::string generate_random_string(size_t length, const std::vector<std::string>& charset,
                                   AutoReseedPCG& generator)
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
    CLI::App app{"随机字符串生成器 (使用 PCG 随机数生成器 + 自动真随机数注入)"};
    app.set_version_flag("-v,--version", VERSION, "显示版本信息");

    size_t length = 16;
    int count = 1;
    int per_line = 1;
    size_t reseed_interval = 10000;  // 默认每 10000 次操作重新播种
    std::vector<std::string> charset_sources;

    // 定义参数
    // 位置参数 1: 长度
    app.add_option("length", length, "生成的字符串长度")->default_val(16);

    // 位置参数 2: 数量
    app.add_option("count", count, "生成的字符串数量")->default_val(1);

    // 选项参数: -s/--set
    app.add_option("-s,--set", charset_sources, "字符集来源 (dn, en, zh, sp,或文件路径)")
        ->expected(1, -1);  // 允许至少 1 个，最多不限

    // 选项参数: -n/--per-line
    app.add_option("-n,--per-line", per_line, "每行输出的字符串数量")->default_val(1);

    // 新增选项参数: -r/--reseed-interval
    app.add_option("-r,--reseed-interval", reseed_interval, "自动重新播种间隔（操作次数）")
        ->default_val(10000);

    CLI11_PARSE(app, argc, argv);

    // 逻辑处理：构建最终的字符集字符串
    std::string final_charset_str{};

    if (charset_sources.empty())
    {
        final_charset_str = std::string(DEFAULT_CHARSET);
    }
    else
    {
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

    // 初始化 PCG 随机数引擎（带自动重新播种功能）
    AutoReseedPCG generator(reseed_interval);

    // 输出生成的字符串，支持每行多个字符串
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

    return 0;
}
