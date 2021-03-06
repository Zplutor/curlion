#pragma once

#include <system_error>
#include <curl/curl.h>

namespace curlion {

inline const std::error_category& CurlMultiErrorCategory() {
    
    class CurlMultiErrorCategory : public std::error_category {
    public:
        const char* name() const _NOEXCEPT override {
            return "CURLMcode";
        }
        
        std::string message(int condition) const override {
            return std::string();
        }
    };
    
    static CurlMultiErrorCategory category;
    return category;
}
    

inline const std::error_category& CurlFormErrorCategory() {
    
    class CurlFormErrorCategory : public std::error_category {
    public:
        const char* name() const _NOEXCEPT override {
            return "CURLFORMcode";
        }
        
        std::string message(int condition) const override {
            return std::string();
        }
    };
    
    static CurlFormErrorCategory category;
    return category;
}

    
}
