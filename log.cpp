//
// Created by admin on 2025/7/25.
// 日志系统实现文件
//

#include "log.h"

namespace sylar {
    /**
     * @brief 将日志级别枚举转换为字符串
     * @param level 日志级别枚举值
     * @return 对应的字符串表示
     */
    const char* ToString(LogLevel::Level level) {
        switch (level) {
// 使用宏简化重复代码，为每个日志级别生成case语句
#define XX(name) \
case LogLevel::name: \
return #name; \
break;

            XX(DEBUG);   // 调试级别
            XX(INFO);    // 信息级别
            XX(WARN);    // 警告级别
            XX(ERROR);   // 错误级别
            XX(FATAL);   // 致命错误级别

            //销毁宏，避免污染命名空间
#undef XX
            default:
            return "UNKONWN";  // 未知级别（注意：这里有拼写错误）
        }
        return "UNKNOWN";
    }

    /**
     * @brief LogEvent构造函数 - 创建日志事件对象
     * @param level 日志级别
     * @param file 源文件名
     * @param line 行号
     * @param elapse 程序启动到现在的毫秒数
     * @param threadID 线程ID
     * @param fiber_id 协程ID
     * @param time 时间戳
     */
    LogEvent::LogEvent(LogLevel::Level level
                ,const char* file, int32_t line, uint32_t elapse
                , uint32_t threadID, uint32_t fiber_id, uint64_t time)
        :m_level(level)
        ,m_file(file)
        ,m_line(line)
        ,m_elapse(elapse)
        ,m_threadId(threadID)
        ,m_fiberId(fiber_id)
        ,m_time(time){
    }

    /**
     * @brief Logger构造函数 - 创建日志器
     * @param name 日志器名称，默认级别为DEBUG
     */
    Logger::Logger(const std::string& name) :m_name(name), m_level(LogLevel::DEBUG) {}

    /**
     * @brief 输出日志 - 核心日志输出方法
     * @param event 日志事件对象
     * 只有当事件级别>=日志器级别时才会输出
     */
    void Logger::log(LogEvent::ptr event) {
        if (event -> getLevel() >= m_level) {
            for (auto& it : m_appenders) {
                //利用多态进行输出
                //当it走到不同的Appender时就输出到对应控制台
                it->log(event);
            }
        }
    }

    /**
     * @brief 添加日志输出器
     * @param appender 输出器智能指针
     */
    void Logger::addAppender(LogAppender::ptr appender) {
        m_appenders.push_back(appender);
    }

    /**
     * @brief 删除日志输出器
     * @param appender 要删除的输出器智能指针
     */
    void Logger::delAppender(LogAppender::ptr appender) {
        for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
            if (*it == appender) {
                m_appenders.erase(it);
                break;
            }
        }
    }

    /**
     * @brief 控制台输出器 - 将日志输出到标准输出
     * @param event 日志事件对象
     */
    void StdoutLogAppender::log(LogEvent::ptr event) {
        //格式化时间（已注释的旧实现）
        /*const std::string format = "%Y-%m-%d %H:%M:%S";
        struct tm tm;
        time_t t = event->getTime();
        localtime_r(&t, &tm);
        char tm_str[32];
        strftime(tm_str, sizeof(tm_str), format.c_str(), &tm);

        std::cout << event -> getLevel() << " 以及一些其他的属性" << std::endl;
        */
        // 使用格式化器格式化日志并输出到控制台
        std::cout<< m_formatter->format(event) << std::endl;
    }

    /**
     * @brief 文件输出器构造函数
     * @param filename 输出文件名
     */
    FileLogAppender::FileLogAppender(const std::string &filename)
        : m_filename(filename) {}

    /**
     * @brief 文件输出器 - 将日志输出到文件（当前为占位实现）
     * @param event 日志事件对象
     */
    void FileLogAppender::log(LogEvent::ptr event) {
        std::cout<<"输出到文件" << std::endl;
    }

    /**
     * @brief 日志格式化器构造函数
     * @param pattern 格式化模式字符串
     * 构造时自动调用init()解析模式字符串
     */
    LogFormatter::LogFormatter(const std::string &pattern)
        : m_pattern(pattern) { init(); }

    /**
     * 格式化模式解析说明：
     * 我们需要将模板字符串解析成 符号：子串：解析方式 的结构
     * 例如这个模板 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
     * 可以解析成：
     * 符号    子串                   解析方式  注释
     * "d"    "%Y-%m-%d %H:%M:%S"    1 		#当前时间
     * "T"    ""                     1  		#制表（4空格）
     * "t"	 ""						1	    #线程ID
     * "T"    ""                     1 		#制表（4空格）
     * "F"    ""                     1		#协程ID
     * "T"    ""                     1 		#制表（4空格）
     * "["    ""                     0		#普通字符
     * "p"    ""                     1		#日志级别
     * "]"    ""                     0		#普通字符
     * "T"    ""                     1  		#制表（4空格）
     * "["    ""                     0		#普通字符
     * "c"    ""                     1		#日志器名称
     * "]"    ""                     0		#普通字符
     * "T"    ""                     1 		#制表（4空格）
     * "f"    ""                     1		#文件名称
     * ":"    ""                     0		#普通字符
     * "l"    ""                     1		#行号
     * "T"    ""                     1 		#制表（4空格）
     * "m"    ""                     1		#消息
     * "n"    ""                     1 		#换行
     */

    // 格式化项工厂映射表：格式符号 -> 对应的FormatItem创建函数
    static std::map<std::string,std::function<LogFormatter::FormatItem::ptr(const std::string& str)> > s_format_items = {
// 使用宏简化重复的映射定义
#define XX(str, C) \
{#str, [](const std::string& fmt) { \
return LogFormatter::FormatItem::ptr(new C(fmt));}}

        XX(m, MessageFormatItem),    // 消息内容
        XX(p, LevelFormatItem),      // 日志级别
        XX(r, ElapseFormatItem),     // 启动时间
        XX(c, NameFormatItem),       // 日志器名称
        XX(t, ThreadIdFormatItem),   // 线程ID
        XX(n, NewLineFormatItem),    // 换行符
        XX(d, DateTimeFormatItem),   // 日期时间
        XX(f, FilenameFormatItem),   // 文件名
        XX(l, LineFormatItem),       // 行号
        XX(T, TabFormatItem),        // 制表符
        XX(F, FiberIdFormatItem),    // 协程ID
#undef XX
    };

    /**
     * @brief 初始化格式化器 - 解析格式化模式字符串
     * 将模式字符串解析为格式化项列表，支持%符号转义和{}参数
     */
    void LogFormatter::init() {
        // 存储解析结果的向量：<格式符号, 参数, 类型(0=普通字符,1=格式符)>
        std::vector<std::tuple<std::string,std::string,int> > vec;
        std::string nstr;  // 临时存储普通字符串

        // 遍历模式字符串进行解析
        for (auto it = 0 ; it < m_pattern.size(); ++it) {
            if (m_pattern[it] != '%') {
                // 普通字符，直接添加到临时字符串
                nstr.append(1,m_pattern[it]);
                continue;
            }
            // 处理%%转义：两个%，第二个作为普通字符
            if ((it + 1) < m_pattern.size()) {
                if (m_pattern[it + 1] == '%') {
                    nstr.append(1,'%');
                    it++;
                    continue;
                }
            }

            // 遇到单个%，开始解析格式符
            size_t n = it + 1;		// 跳过'%',从下一个字符开始解析
            int fmt_status = 0;		// 大括号状态: 0=未进入, 1=已进入大括号
            size_t fmt_begin = 0;	// 大括号开始位置

            std::string str;  // 格式符号
            std::string fmt;  // 格式参数（大括号内容）
            // 解析格式符和参数的主循环
            while (n < m_pattern.size()) {
                // 如果未进入大括号且遇到非字母非大括号字符，格式符结束
                if (!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{'
                    && m_pattern[n] != '}')) {
                    str = m_pattern.substr(it + 1, n - it - 1 );
                    break;
                    }
                if (fmt_status == 0 ) {
                    // 遇到左大括号，开始解析参数
                    if (m_pattern[n] == '{') {
                        fmt_status = 1;  // 标记进入大括号状态
                        str = m_pattern.substr(it + 1, n - it - 1 );  // 提取格式符
                        fmt_begin = n;   // 记录大括号开始位置
                        ++n;
                        continue;
                    }
                }else if (fmt_status == 1) {
                    // 在大括号内，遇到右大括号结束参数解析
                    if (m_pattern[n] == '}') {
                        fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1 );  // 提取参数
                        fmt_status = 0;  // 退出大括号状态
                        ++n;
                        break;  // 找完一组格式符+参数，退出循环
                    }
                }
                ++n;
                // 到达字符串末尾的处理
                if (n == m_pattern.size()) {
                    if (str.empty()) {
                        str = m_pattern.substr(it + 1);  // 提取剩余部分作为格式符
                    }
                }
            }
            // 解析完成后的处理
            if(fmt_status == 0) {
                // 正常解析完成
                if(!nstr.empty()) {
                    // 保存之前累积的普通字符（如 '['  ']'  ':'）
                    vec.push_back(std::make_tuple(nstr, std::string(), 0));
                    nstr.clear();
                }
                // 保存解析到的格式符和参数
                vec.push_back(std::make_tuple(str, fmt, 1));
                // 调整索引位置继续向后遍历
                it = n - 1;
            } else if(fmt_status == 1) {
                // 解析错误：没有找到与'{'相对应的'}'
                std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(it) << std::endl;
                vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
            }
        }

        // 处理最后剩余的普通字符
        if(!nstr.empty()) {
            vec.push_back(std::make_tuple(nstr, "", 0));
        }

        // 调试输出：显示解析结果
        for(auto& it : vec) {
            std::cout
                << std::get<0>(it)  // 格式符或普通字符
                << " : " << std::get<1>(it)  // 参数
                << " : " << std::get<2>(it)  // 类型(0=普通字符,1=格式符)
                << std::endl;
        }

        // 根据解析结果创建FormatItem对象
        for(auto& i : vec) {
            if(std::get<2>(i) == 0) {
                // 普通字符，创建StringFormatItem
                m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
            } else {
                // 格式符，查找对应的FormatItem创建函数
                auto it = s_format_items.find(std::get<0>(i));
                if(it == s_format_items.end()) {
                    // 未知格式符，创建错误提示
                    m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                } else {
                    // 找到对应的创建函数，创建FormatItem
                    m_items.push_back(it->second(std::get<1>(i)));
                }
            }
        }
    }

    /**
     * @brief 格式化日志事件
     * @param event 日志事件对象
     * @return 格式化后的字符串
     */
    std::string LogFormatter::format(LogEvent::ptr& event) {
        std::stringstream ss;
        // 遍历所有格式化项，依次格式化
        for(auto& i : m_items) {
            i->format(ss, event);
        }
        return ss.str();
    }

    /**
     * @brief 测试主函数 - 演示日志系统的基本使用
     */
    int main(int argc, char** argv) {
        // 创建日志事件
        LogEvent::ptr event(new LogEvent(
            LogLevel::INFO,        // 日志级别
            __FILE__,              // 当前文件名
            __LINE__,              // 当前行号
            1234567,               // 程序运行时间
            syscall(SYS_gettid),   // 线程ID
            0,                     // 协程ID
            time(0)                // 当前时间戳
            ));

        // 创建日志器
        Logger::ptr lg(new Logger("XYZ"));
        // 创建格式化器，定义输出格式
        LogFormatter::ptr formatter(new LogFormatter(
            "%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
        // 创建控制台输出器
        StdoutLogAppender::ptr appender(new StdoutLogAppender());
        appender->setFormatter(formatter);
        lg->addAppender(appender);
        // 输出日志
        lg->log(event);
        return 0;
    }

    /**
     * @brief LogEventWrap构造函数 - 日志事件包装器
     * @param event 日志事件
     * @param logger 日志器
     * 用于RAII模式，析构时自动输出日志
     */
    LogEventWrap::LogEventWrap(LogEvent::ptr event, Logger::ptr logger)
        : m_event(event),m_logger(logger) {}

    /**
     * @brief LogEventWrap析构函数 - 自动输出日志
     * 利用RAII机制，在对象销毁时自动调用logger输出日志
     */
    LogEventWrap::~LogEventWrap() {
        m_logger -> log( m_event);
    }

    /**
     * @brief 获取日志事件的字符串流
     * @return 字符串流引用，用于写入日志内容
     */
    std::stringstream &LogEventWrap::getSS() { return m_event -> getSS(); }

// 定义宏来简化Level操作（未完成）
#define LOG_LEVEL


}
