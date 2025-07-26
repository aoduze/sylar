//
// Created by admin on 2025/7/25.
//

#include "log.h"

namespace sylar {

Logger::Logger(const std::string& name) :m_name(name) {

}

void Logger::addAppender(LogAppender::ptr appender) {
    //将此输出目的地加入到队列中
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
        if (*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}


void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        for (auto& i : m_appenders) {
            i -> log(level, event);
        }
    }
}

void Logger::debug(LogEvent::ptr event) {
    debug(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
    debug(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
    debug(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
    debug(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
    debug(LogLevel::FATAL, event);
}

FileLogAppender::FileLogAppender(const std::string &filename) {}

voi8d FileLogAppender::reopen() {}

void Log(LogLevel::Level level, LogEvent::ptr event) {}
void Log(LogLevel::Level level, LogEvent::ptr event) {}

void StdoutLogAppender

}