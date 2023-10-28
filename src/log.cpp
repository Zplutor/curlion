#include "log.h"

#if CURLION_VERBOSE
#include <chrono>
#include <iomanip>
#include <iostream>

namespace curlion {
namespace {

Logger g_logger = [](const std::string& log) {
    std::cout << log;
};

}


void SetLogger(Logger logger) {
    g_logger = std::move(logger);
}


LoggerProxy::LoggerProxy() : stream_(new std::ostringstream()) {

    std::time_t time = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &time);
    *stream_ << "curlion> " << std::put_time(&tm, "%H:%M:%S") << ' ';
}


void LoggerProxy::WriteLog(const std::string& log) {
    g_logger(log);
}

}

#endif