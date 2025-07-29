#ifndef LOG_H
#define LOG_H

/**
 * @brief 日志系统结构图 (紧凑版)
 *
 * +-------------------------------------------------------------+
 * | LogEventWrap(解析构触发器), LogEvent(日志事件), LogLevel(日志级别) |
 * +-------------------------------------------------------------+
 * | LoggerManager (日志管理类)                                  |
 * +-------------------------------------------------------------+
 * | Logger (日志器)                                             |
 * +-------------------------------------------------------------+
 * | LogAppender (日志输出适配器基类)                            |
 * |   -> FileLogAppender(文件), StdoutLogAppender(控制台), ... |
 * +-------------------------------------------------------------+
 * | LogFormatter (日志格式器)                                   |
 * +-------------------------------------------------------------+
 * | FormatItem (格式解析基类)                                   |
 * |   -> Message(内容), Level(级别), Elapse(耗时)              |
 * |   -> Name(日志名), ThreadId(线程ID), NewLine(换行)          |
 * |   -> DateTime(日期), Filename(文件名), Line(行号)          |
 * |   -> Tab(制表符), FiberId(协程ID), String(字面量)          |
 * +-------------------------------------------------------------+
 */

#include <unistd.h>        // UNIX标准定义
#include <cmath>           // 数学函数
#include <memory>          // 智能指针
#include <string>          // 字符串类
#include <cstdint>         // 标准整数类型
#include <ctime>           // 时间处理
#include <list>            // 链表容器
#include <iostream>        // 输入输出流
#include <tuple>           // 元组
#include <ostream>         // 输出流
#include <algorithm>       // 算法库
#include <sys/syscall.h>   // 系统调用
#include <sys/types.h>     // 系统类型定义
#include <sstream>         // 字符串流
#include <map>             // 映射容器

// 第三方库
#include "boost/asio.hpp"  // Boost异步IO库
#include <yaml-cpp/yaml.h> // YAML解析库

/**
 * 日志系统整体架构说明：
 * 大体流程在于我们会将日志信息封装成一个Event并输出到对应位置
 *
 * 核心组件：
 * - LogLevel: 日志级别定义
 * - LogEvent: 日志事件，包含日志的所有信息
 * - LogFormatter: 日志格式化器，定义输出格式
 * - LogAppender: 日志输出器，负责将日志输出到不同目标
 * - Logger: 日志器，管理日志的输出流程
 */

namespace sylar {

    /**
     * @brief 日志级别类
     * 定义了日志系统支持的所有级别
     */
    class LogLevel {
    public:
        enum Level {
            UNKNOWN = 0,  // 未知级别
            DEBUG = 1,    // 调试信息
            INFO = 2,     // 一般信息
            WARN = 3,     // 警告信息
            ERROR = 4,    // 错误信息
            FATAL = 5     // 致命错误
        };
        /// 将日志级别转换为字符串
        static const char* ToString(LogLevel::Level level);
    };

    /**
     * @brief 日志事件类
     * 封装单次日志记录的所有信息，包括时间、位置、内容等
     * 日志生成出来会被定义成LogEvent
     */
    class LogEvent {
    public:
        typedef std::shared_ptr<LogEvent> ptr;

        /**
         * @brief 构造函数
         * @param logName 日志器名称
         * @param level 日志级别
         * @param file 源文件名
         * @param line 行号
         * @param elapse 程序启动到现在的毫秒数
         * @param threadID 线程ID
         * @param fiber_id 协程ID
         * @param time 时间戳
         */
        LogEvent(const std::string& logName,LogLevel::Level level,const char* file,
              int32_t line,uint32_t elapse,uint32_t threadID,uint32_t fiber_id,uint64_t time);

        // Getter方法 - 获取日志事件的各种属性
        const char* getFile() const { return m_file;}                    ///< 获取文件名
        const std::string& getLogName() const {return m_logName;}        ///< 获取日志器名称
        int32_t getLine() const { return m_line;}                        ///< 获取行号
        uint32_t getElapse() const { return m_elapse;}                   ///< 获取程序运行时间
        uint32_t getThreadId() const { return m_threadId;}               ///< 获取线程ID
        uint32_t getFiberId() const { return m_fiberId;}                 ///< 获取协程ID
        uint64_t getTime() const { return m_time;}                       ///< 获取时间戳
        LogLevel::Level getLevel() const { return m_level;}              ///< 获取日志级别
        std::string getContent() const { return m_ss.str();}             ///< 获取日志内容
        std::stringstream& getSS() { return m_ss;}                       ///< 获取字符串流引用

    private:
        const char* m_file = nullptr;   ///< 源文件名
        int32_t m_line = 0;             ///< 行号
        uint32_t m_elapse = 0;          ///< 程序启动到现在的时间
        uint32_t m_threadId = 0;        ///< 线程ID
        uint32_t m_time = 0;            ///< 时间戳
        uint32_t m_fiberId = 0;         ///< 协程ID
        std::string m_content;          ///< 存储实际内容
        LogLevel::Level m_level;        ///< 日志级别
        std::string m_logName;          ///< 日志器名称
        std::stringstream m_ss;         ///< 字符流，用于构建日志内容
    };

    /**
     * @brief 日志格式化器类
     * 用于格式化日志输出，支持自定义格式模式
     * 将格式模式字符串解析为格式化项列表
     */
    class LogFormatter {
    public:
        typedef std::shared_ptr<LogFormatter> ptr;

        /**
         * @brief 构造函数
         * @param pattern 格式化模式字符串
         */
        LogFormatter(const std::string& pattern);

        /// 初始化格式化器，解析模式字符串
        void init();

        /**
         * @brief 格式化日志事件
         * @param event 日志事件对象
         * @return 格式化后的字符串
         */
        std::string format(LogEvent::ptr& event);

    public:
        /**
         * @brief 格式化项基类
         * 所有具体格式化项的抽象基类
         */
        class FormatItem {
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            virtual ~FormatItem() {}

            /**
             * @brief 格式化方法
             * @param os 输出流
             * @param event 日志事件
             */
            virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
        };

    private:
        std::string m_pattern;                  ///< 格式化模式字符串
        std::vector<FormatItem::ptr> m_items;   ///< 格式化项列表
        bool m_error = false;                   ///< 解析错误标志
    };

    /// 消息内容格式化项 - 输出日志消息内容
    class MessageFormatItem : public LogFormatter::FormatItem {
    public:
        MessageFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << "Message";
        }
    };

    /// 日志级别格式化项 - 输出日志级别
    class LevelFormatItem : public LogFormatter::FormatItem {
    public:
        LevelFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << LogLevel::ToString(event->getLevel());
        }
    };

    /// 程序运行时间格式化项 - 输出程序启动到现在的时间
    class ElapseFormatItem : public LogFormatter::FormatItem {
    public:
        ElapseFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << event -> getElapse();
        }
    };

    /// 日志器名称格式化项 - 输出日志器名称
    class NameFormatItem : public LogFormatter::FormatItem {
    public:
        NameFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << event -> getLogName();
        }
    };

    /// 线程ID格式化项 - 输出线程ID
    class ThreadIdFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadIdFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << event -> getThreadId();
        }
    };

    /// 协程ID格式化项 - 输出协程ID
    class FiberIdFormatItem : public LogFormatter::FormatItem {
    public:
        FiberIdFormatItem(const std::string& str = "") {}
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << event->getFiberId();
        }
    };

    /// 日期时间格式化项 - 输出格式化的日期时间
    class DateTimeFormatItem : public LogFormatter::FormatItem {
    public:
        /**
         * @brief 构造函数
         * @param format 时间格式字符串，默认为"%Y-%m-%d %H:%M:%S"
         */
        DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
            : m_format(format) {
            if (m_format.empty()) {
                m_format = "%Y-%m-%d %H:%M:%S";
            }
        }

        void format(std::ostream& os, LogEvent::ptr event) override {
            struct tm tm;
            time_t t = event->getTime();
            localtime_r(&t, &tm);  // 线程安全的时间转换
            char buf[64];
            strftime(buf, sizeof(buf), m_format.c_str(), &tm);
            os << buf;
        }
    private:
        std::string m_format;  ///< 时间格式字符串
    };

    /// 文件名格式化项 - 输出源文件名
    class FilenameFormatItem : public LogFormatter::FormatItem {
    public:
        FilenameFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << event->getFile();
        }
    };

    /// 行号格式化项 - 输出源代码行号
    class LineFormatItem : public LogFormatter::FormatItem {
    public:
        LineFormatItem(const std::string& str = "");
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << event->getLine();
        }
    };

    /// 换行符格式化项 - 输出换行符
    class NewLineFormatItem : public LogFormatter::FormatItem {
    public:
        NewLineFormatItem(const std::string& str = "") {}
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << std::endl;
        }
    };

    /// 字符串格式化项 - 输出固定字符串
    class StringFormatItem : public LogFormatter::FormatItem {
    public:
        StringFormatItem(const std::string& str)
            :m_string(str) {}
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << m_string;
        }
    private:
        std::string m_string;  ///< 要输出的固定字符串
    };

    /// 制表符格式化项 - 输出制表符
    class TabFormatItem : public LogFormatter::FormatItem {
    public:
        TabFormatItem(const std::string& str = "") {}
        void format(std::ostream& os, LogEvent::ptr event) override {
            os << "\t";
        }
    private:
        std::string m_string;  ///< 未使用的成员变量
    };

    /**
     * @brief 日志输出器基类
     * 定义日志输出的抽象接口，支持不同的输出目标
     */
    class LogAppender {
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        virtual ~LogAppender() {}

        /// 纯虚函数：输出日志事件
        virtual void log(LogEvent::ptr event) = 0;

        /// 设置格式化器
        void setFormatter(LogFormatter::ptr val) { m_formatter = val; }
        /// 虚函数：获取格式化器（实现有误）
        virtual void getFormatter_vir(LogFormatter::ptr val) { return ;}
        /// 获取格式化器
        LogFormatter::ptr getFormatter() const { return m_formatter; };
    protected:
        LogFormatter::ptr m_formatter;  ///< 日志格式化器
        LogLevel::Level m_level;        ///< 日志级别
    };

    /**
     * @brief 日志器类
     * 管理日志的输出流程，支持多个输出器
     */
    class Logger {
    public:
        typedef std::shared_ptr<Logger> ptr;
        /**
         * @brief 构造函数
         * @param name 日志器名称，默认为"root"
         */
        Logger(const std::string& name = "root");

        // Getter和Setter方法
        const std::string& getName() const { return m_name; };      ///< 获取日志器名称
        LogLevel::Level getLevel() const { return m_level; }        ///< 获取日志级别
        void setLevel(LogLevel::Level level) { m_level = level; }   ///< 设置日志级别

        /// 核心日志输出方法
        void log(LogEvent::ptr event);

        // 各级别日志输出方法
        void unknown(LogEvent::ptr event);  ///< 输出UNKNOWN级别日志
        void debug(LogEvent::ptr event);    ///< 输出DEBUG级别日志
        void info(LogEvent::ptr event);     ///< 输出INFO级别日志
        void warn(LogEvent::ptr event);     ///< 输出WARN级别日志
        void error(LogEvent::ptr event);    ///< 输出ERROR级别日志
        void fatal(LogEvent::ptr event);    ///< 输出FATAL级别日志

        // 输出器管理方法
        void addAppender(LogAppender::ptr appender);  ///< 添加输出器
        void delAppender(LogAppender::ptr appender);  ///< 删除输出器

    private:
        std::string m_name;                         ///< 日志器名称
        LogLevel::Level m_level;                    ///< 日志级别阈值
        std::list<LogAppender::ptr> m_appenders;    ///< 输出器列表
    };

    /**
     * @brief 控制台输出器
     * 将日志输出到标准输出（控制台）
     */
    class StdoutLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
        void log(LogEvent::ptr event) override;
    };

    /**
     * @brief 文件输出器
     * 将日志输出到指定文件
     */
    class FileLogAppender : public LogAppender {
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;

        /**
         * @brief 构造函数
         * @param filename 输出文件名
         */
        FileLogAppender (const std::string& filename);
        void log(LogEvent::ptr event) override;

    private:
        std::string m_filename;  ///< 输出文件名
    };

    /**
     * @brief 日志事件包装器
     * 使用RAII机制，在析构时自动输出日志
     * 提供流式日志写入接口
     */
    class LogEventWrap {
    public:
        /**
         * @brief 构造函数
         * @param event 日志事件对象
         * @param logger 日志器对象
         */
        LogEventWrap(LogEvent::ptr event, Logger::ptr logger);

        /**
         * @brief 析构函数
         * 利用RAII机制，自动调用logger输出日志
         */
        ~ LogEventWrap();

        /// 获取日志事件对象
        LogEvent::ptr getEvent() const { return m_event};

        /// 获取字符串流，用于流式写入日志内容
        std::stringstream &getSS();

    private:
        LogEvent::ptr m_event;   ///< 日志事件对象
        Logger::ptr m_logger;    ///< 日志器对象
    };
}

#endif //LOG_H
