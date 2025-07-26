#ifndef LOG_H
#define LOG_H

#include <math.h>
#include <memory>
#include <string>
#include <stdint.h>
#include <time.h>
#include <list>
#include <iostream>
#include <ostream>

//大体流程在于我们会将日志信息封装成亦歌Event并输出到对应位置
//

namespace sylar {

//日志生成出来会被定义成LogEvent
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent();
private:
    const char* m_file = nullptr;
    int32_t m_line = 0;     //行号
    uint32_t m_elapse = 0;  //程序启动到现在的时间
    int32_t m_threadId = 0;
    uint32_t m_time;        //时间戳
    uint32_t m_fiberId = 0; //存储协程ID
    std::string m_content;  //存储实际内容
};

    //用于格式匹配
    class LogFormatter {
    public:
        typedef std::shared_ptr<LogFormatter> ptr;

        std::string format(LogEvent::ptr event);
    private:
    };


//日志级别
class LogLevel {
public:
    enum Level {
        UNKNOWN = 0,
        ERROR = 1,
        INFO = 2,
        DEBUG = 3,
        WARN = 4,
        FATAL = 5
    };
};

    //日志输出目的地
    class LogAppender {
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        virtual ~LogAppender() {}

        virtual void log(LogLevel::Level level, LogEvent::ptr event);
        void setFormatter(LogFormatter::ptr val) { m_formatter = val; };
        LogFormatter::ptr getFormatter() const { return m_formatter; };
    protected:
        LogFormatter::ptr m_formatter;
        LogLevel::Level m_level;
    };


//日志器
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;
    //将日志级别转换为文本输出

    //用于记录日志,他接受日志级别和日志事件作为参数,并处理日志输出
    Logger(const std::string& name = "root");
    void log(LogLevel::Level level, LogEvent::ptr event);

    void unknown(LogEvent::ptr event);
    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level level) { m_level = level; }
private:
    std::string m_name;                         //日志名称
    LogLevel::Level m_level;                    //日志等级
    std::list<LogAppender::ptr> m_appenders;    //Appender集合
};

//输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    virtual void log(LogLevel::Level level, LogEvent::ptr event) override;
private:
};

//输出到文件
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender (const std::string& filename);
    virtual void log(LogLevel::Level level, LogEvent::ptr event) override;

    //重新打开文件,文件打开成功返回true
    bool reopen();
private:
    std::string m_filename;
    std::ostream m_stream;
};
}

#endif //LOG_H
