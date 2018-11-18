#include <cnet/utils/Logger.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>
#include <sys/syscall.h>

namespace cnet {
// helper class for known string length at compile time
    class T {
    public:
        T(const char *str, unsigned len)
                : str_(str),
                  len_(len) {
            assert(strlen(str) == len_);
        }

        const char *str_;
        const unsigned len_;
    };

    const char *strerror_tl(int savedErrno) {
        return strerror(savedErrno);
    }

    inline LogStream &operator<<(LogStream &s, T v) {
        s.append(v.str_, v.len_);
        return s;
    }

    inline LogStream &operator<<(LogStream &s, const Logger::SourceFile &v) {
        s.append(v.data_, v.size_);
        return s;
    }
} // namespace cnet
using namespace cnet;

void defaultOutputFunction(const char *msg, const uint64_t len) {
    fwrite(msg, 1, len, stdout);
}

void defaultFlushFunction() {
    fflush(stdout);
}

static __thread uint64_t lastSecond_ = 0;
static __thread char lastTimeString_[32] = {0};
#ifdef __linux__
static __thread pid_t threadId_ = 0;
#else
static __thread uint64_t threadId_ = 0;
#endif
//   static __thread LogStream logStream_;
#ifdef RELEASE
Logger::LogLevel Logger::logLevel_ = LogLevel::INFO;
#else
Logger::LogLevel Logger::logLevel_ = LogLevel::DEBUG;
#endif
std::function<void(const char *msg, const uint64_t len)> Logger::outputFunc_ = defaultOutputFunction;
std::function<void()> Logger::flushFunc_ = defaultFlushFunction;

void Logger::formatTime() {
    uint64_t now = date_.microSecondsSinceEpoch();
    uint64_t microSec = now % 1000000;
    now = now / 1000000;
    if (now != lastSecond_) {
        lastSecond_ = now;
        strncpy(lastTimeString_, date_.toFormattedString(false).c_str(), sizeof(lastTimeString_) - 1);
    }
    logStream_ << T(lastTimeString_, 17);
    char tmp[32];
#ifdef __linux__
    sprintf(tmp, ".%06lu UTC ", microSec);
#else
    sprintf(tmp, ".%06llu UTC ", microSec);
#endif
    logStream_ << T(tmp, 12);
#ifdef __linux__
    if (threadId_ == 0)
        threadId_ = static_cast<pid_t>(::syscall(SYS_gettid));
    logStream_ << threadId_;
#else
    if (threadId_ == 0) {
        pthread_threadid_np(NULL, &threadId_);
    }
    logStream_ << threadId_;
#endif
}

static const char *logLevelStr[Logger::LogLevel::NUM_LOG_LEVELS] = {
        " TRACE ",
        " DEBUG ",
        " INFO  ",
        " WARN  ",
        " ERROR ",
        " FATAL ",
};

Logger::Logger(SourceFile file, int line)
        : sourceFile_(file),
          fileLine_(line),
          level_(INFO) {
    formatTime();
    logStream_ << T(logLevelStr[level_], 7);
}

Logger::Logger(SourceFile file, int line, LogLevel level)
        : sourceFile_(file),
          fileLine_(line),
          level_(level) {
    formatTime();
    logStream_ << T(logLevelStr[level_], 7);
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char *func)
        : sourceFile_(file),
          fileLine_(line),
          level_(level) {
    formatTime();
    logStream_ << T(logLevelStr[level_], 7) << "[" << func << "] ";
}

Logger::Logger(SourceFile file, int line, bool isSysErr)
        : sourceFile_(file),
          fileLine_(line),
          level_(FATAL) {
    formatTime();
    if (errno != 0) {
        logStream_ << T(logLevelStr[level_], 7);
        logStream_ << strerror(errno) << " (errno=" << errno << ") ";
    }
}

Logger::~Logger() {
    logStream_ << T(" - ", 3) << sourceFile_ << ':' << fileLine_ << '\n';
    if (Logger::outputFunc_)
        Logger::outputFunc_(logStream_.bufferData(), logStream_.bufferLength());
    else
        defaultOutputFunction(logStream_.bufferData(), logStream_.bufferLength());
    if (level_ >= ERROR)
        Logger::flushFunc_();
    //logStream_.resetBuffer();
}

LogStream &Logger::stream() {
    return logStream_;
}
