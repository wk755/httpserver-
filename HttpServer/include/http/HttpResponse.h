#pragma once

#include <muduo/net/TcpServer.h>


namespace http
{

class HttpResponse 
{
public:
    enum HttpStatusCode
    {
        //状态码
        kUnknown, //未知状态
        k200Ok = 200, //成功
        k204NoContent = 204, //成功处理，但无返回内容，例如delete操作
        k301MovedPermanently = 301, //所请求资源更新url，在location给出新地址
        k400BadRequest = 400, //客户端请求存在问题，服务器无法处理
        k401Unauthorized = 401, //请求未通过身份认证
        k403Forbidden = 403, //客户端缺乏权限
        k404NotFound = 404, //服务器不存在资源
        k409Conflict = 409, //请求冲突
        k500InternalServerError = 500, //服务器内部错误
    };

    HttpResponse(bool close = true)
        : statusCode_(kUnknown)
        , closeConnection_(close)
    {}

    void setVersion(std::string version)
    { httpVersion_ = version; }
    void setStatusCode(HttpStatusCode code)
    { statusCode_ = code; }

    HttpStatusCode getStatusCode() const
    { return statusCode_; }

    void setStatusMessage(const std::string message)
    { statusMessage_ = message; }

    void setCloseConnection(bool on)
    { closeConnection_ = on; }

    bool closeConnection() const
    { return closeConnection_; }
    
    void setContentType(const std::string& contentType)
    { addHeader("Content-Type", contentType); }

    void setContentLength(uint64_t length)
    { addHeader("Content-Length", std::to_string(length)); }

    void addHeader(const std::string& key, const std::string& value)
    { headers_[key] = value; }
    
    void setBody(const std::string& body)
    { 
        body_ = body;
        // body_ += "\0";
    }

    void setStatusLine(const std::string& version,
                         HttpStatusCode statusCode,
                         const std::string& statusMessage); //状态行

    void setErrorHeader(){}

    void appendToBuffer(muduo::net::Buffer* outputBuf) const;
private:
    std::string                        httpVersion_; 
    HttpStatusCode                     statusCode_;
    std::string                        statusMessage_;
    bool                               closeConnection_;
    std::map<std::string, std::string> headers_;
    std::string                        body_;
    bool                               isFile_;
};

} // namespace http