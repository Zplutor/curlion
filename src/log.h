#pragma once

#include <functional>
#include <string>

/**
 CURLION_VERBOSE macro controls whether to print debug information to stdout.
 Enable it by adding CURLION_VERBOSE=1 macro to the project.
 
 Be aware of that enabling this macro would produce much output that flood the console easily.
 So it should be used for debug purpose only. 
 */
#ifndef CURLION_VERBOSE
#define CURLION_VERBOSE 0
#endif

#if CURLION_VERBOSE
#include <memory>
#include <sstream>
#endif

namespace curlion {
    
using Logger = std::function<void(const std::string&)>;

#if CURLION_VERBOSE

/**
Sets a customized logger to replace the default logger.
*/
void SetLogger(Logger logger);
    
class LoggerProxy {
public:
    LoggerProxy();

    LoggerProxy(LoggerProxy&& other) : stream_(std::move(other.stream_)) {
        
    }
    
    ~LoggerProxy() {
        
        if (stream_) {
            *stream_ << std::endl;
            WriteLog(stream_->str());
        }
    }
    
    template<typename Type>
    LoggerProxy operator<<(const Type& value) {
        *stream_ << value;
        return std::move(*this);
    }
    
private:
    static void WriteLog(const std::string& log);

private:
    std::unique_ptr<std::ostringstream> stream_;
};
    

inline LoggerProxy Log() {
    return LoggerProxy();
}
    
#else
    
class LoggerProxy {
public:
    template<typename Type>
    LoggerProxy operator<<(const Type& value) {
        return *this;
    }
};
    
inline LoggerProxy Log() {
    return LoggerProxy();
}

inline void SetLogger(Logger logger) {

}
    
#endif
    
}