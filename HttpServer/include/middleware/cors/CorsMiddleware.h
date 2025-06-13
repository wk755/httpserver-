#pragma once

#include "../Middleware.h"
#include "../../http/HttpRequest.h"
#include "../../http/HttpResponse.h"
#include "CorsConfig.h"

namespace http 
{
namespace middleware 
{

class CorsMiddleware : public Middleware 
{
public:
    explicit CorsMiddleware(const CorsConfig& config = CorsConfig::defaultConfig()); //可以显式传入一份 CorsConfig 配置，也可以使用默认配置（允许所有来源 *）
    
    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override;

    std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

private:
    bool isOriginAllowed(const std::string& origin) const;
    void handlePreflightRequest(const HttpRequest& request, HttpResponse& response);
    void addCorsHeaders(HttpResponse& response, const std::string& origin);

private:
    CorsConfig config_;
};

} // namespace middleware
} // namespace http