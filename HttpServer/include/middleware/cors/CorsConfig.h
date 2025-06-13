#pragma once

#include <string>
#include <vector>

namespace http 
{
namespace middleware 
{

struct CorsConfig 
{
    std::vector<std::string> allowedOrigins; //允许哪些来源的请求跨域访问你服务器。
    std::vector<std::string> allowedMethods; //允许哪些 HTTP 方法用于跨域请求。
    std::vector<std::string> allowedHeaders; //允许客户端使用哪些自定义请求头。
    bool allowCredentials = false; //是否允许跨域请求携带 Cookie / Authorization 等凭据。

    int maxAge = 3600; //请求结果，缓存时间3600s，3600 秒 = 1 小时，表示 1 小时内不需要重复发预检请求。
    
    //默认的跨域配置
    static CorsConfig defaultConfig() 
    {
        CorsConfig config;
        config.allowedOrigins = {"*"};
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        config.allowedHeaders = {"Content-Type", "Authorization"};
        return config;
    }
};

} // namespace middleware
} // namespace http