#pragma once

#include <cnet/utils/NonCopyable.h>
#include <cnet/utils/Date.h>
#include <cnet/utils/LogStream.h>
#include <string.h>
#include <functional>
#include <iostream>
namespace cnet
{
class Logger : public NonCopyable
{

  public:
    enum LogLevel
    {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS
    };
    // compile time calculation of basename of source file
    class SourceFile
    {
      public:
        template <int N>
        inline SourceFile(const char (&arr)[N])
            : data_(arr),
              size_(N - 1)
        {
            // std::cout<<data_<<std::endl;
            const char *slash = strrchr(data_, '/'); // builtin function
            if (slash)
            {
                data_ = slash + 1;
                size_ -= static_cast<int>(data_ - arr);
            }
        }

        explicit SourceFile(const char *filename)
            : data_(filename)
        {
            const char *slash = strrchr(filename, '/');
            if (slash)
            {
                data_ = slash + 1;
            }
            size_ = static_cast<int>(strlen(data_));
        }

        const char *data_;
        int size_;
    };
    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, bool isSysErr);
    Logger(SourceFile file, int line, LogLevel level, const char *func);
    ~Logger();
    LogStream &stream();
    static void setOutputFunction(std::function<void(const char *msg, const uint64_t len)> outputFunc, std::function<void()> flushFunc)
    {
        outputFunc_ = outputFunc;
        flushFunc_ = flushFunc;
    }

    static void setLogLevel(LogLevel level)
    {
        logLevel_ = level;
    }
    static LogLevel logLevel()
    {
        return logLevel_;
    }

  protected:
    void formatTime();
    static LogLevel logLevel_;
    static std::function<void(const char *msg, const uint64_t len)> outputFunc_;
    static std::function<void()> flushFunc_;
    LogStream logStream_;
    Date date_ = Date::date();
    SourceFile sourceFile_;
    int fileLine_;
    LogLevel level_;
};
#ifdef NDEBUG
#define LOG_TRACE \
    if (0)        \
    cnet::Logger(__FILE__, __LINE__, cnet::Logger::TRACE, __func__).stream()
#else
#define LOG_TRACE                                              \
    if (cnet::Logger::logLevel() <= cnet::Logger::TRACE) \
    cnet::Logger(__FILE__, __LINE__, cnet::Logger::TRACE, __func__).stream()
#endif
#define LOG_DEBUG                                              \
    if (cnet::Logger::logLevel() <= cnet::Logger::DEBUG) \
    cnet::Logger(__FILE__, __LINE__, cnet::Logger::DEBUG, __func__).stream()
#define LOG_INFO                                              \
    if (cnet::Logger::logLevel() <= cnet::Logger::INFO) \
    cnet::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN cnet::Logger(__FILE__, __LINE__, cnet::Logger::WARN).stream()
#define LOG_ERROR cnet::Logger(__FILE__, __LINE__, cnet::Logger::ERROR).stream()
#define LOG_FATAL cnet::Logger(__FILE__, __LINE__, cnet::Logger::FATAL).stream()
#define LOG_SYSERR cnet::Logger(__FILE__, __LINE__, true).stream()

const char *strerror_tl(int savedErrno);
} // namespace cnet
