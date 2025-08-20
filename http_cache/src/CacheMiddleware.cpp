#include "http_cache/include/CacheMiddleware.h"
#include "http_cache/include/CacheKey.h"
#include "http_cache/include/CacheEntry.h"

// 你的项目头
#include "HttpServer/include/http/HttpRequest.h"
#include "HttpServer/include/http/HttpResponse.h"

// 用于读取序列化后的响应
#include <muduo/net/Buffer.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

using http::HttpRequest;
using http::HttpResponse;

namespace { // 工具 & 解析

inline std::string trim(std::string s) {
  auto notspace = [](int ch){ return !std::isspace(ch); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
  return s;
}
inline std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}

struct ParsedResp {
  std::string version;  // "HTTP/1.1"
  int         statusCode{200};
  std::string statusMessage;
  std::vector<std::pair<std::string,std::string>> headers;
  std::string body;
  bool        hasNoStore{false};
};

bool parseSerializedResponse(const HttpResponse& resp, ParsedResp* out) {
  muduo::net::Buffer buf;
  resp.appendToBuffer(&buf);
  const char* p = buf.peek();
  size_t n = buf.readableBytes();
  if (n < 7) return false;

  const char* end = p + n;
  // 寻找 CRLFCRLF
  const char* hdr_end = nullptr;
  for (const char* it = p; it + 3 < end; ++it) {
    if (it[0]=='\r' && it[1]=='\n' && it[2]=='\r' && it[3]=='\n') { hdr_end = it; break; }
  }
  if (!hdr_end) return false;

  // 状态行
  const char* line_end = std::search(p, end, "\r\n", "\r\n"+2);
  if (line_end == end) return false;
  std::string statusLine(p, line_end);
  {
    std::istringstream iss(statusLine);
    iss >> out->version;
    iss >> out->statusCode;
    std::string rest; std::getline(iss, rest);
    out->statusMessage = trim(rest);
  }

  // 头部
  const char* hcur = line_end + 2;
  while (hcur < hdr_end) {
    const char* ln_end = std::search(hcur, hdr_end, "\r\n", "\r\n"+2);
    if (ln_end == hdr_end) break;
    std::string line(hcur, ln_end);
    hcur = ln_end + 2;

    auto pos = line.find(':');
    if (pos != std::string::npos) {
      std::string k = trim(line.substr(0, pos));
      std::string v = trim(line.substr(pos + 1));
      if (!k.empty()) {
        if (lower(k) == "cache-control" && v.find("no-store") != std::string::npos)
          out->hasNoStore = true;
        out->headers.emplace_back(std::move(k), std::move(v));
      }
    }
  }

  // body
  const char* body_start = hdr_end + 4;
  if (body_start < end) out->body.assign(body_start, end);
  return true;
}

// 方法枚举 -> 字符串
std::string methodToString(const HttpRequest& req) {
  switch (req.method()) {
    case HttpRequest::kGet:     return "GET";
    case HttpRequest::kHead:    return "HEAD";
    case HttpRequest::kPost:    return "POST";
    case HttpRequest::kPut:     return "PUT";
    case HttpRequest::kDelete:  return "DELETE";
    case HttpRequest::kOptions: return "OPTIONS";
    default: return "UNKNOWN";
  }
}

} // anonymous namespace

// ====== CacheMiddleware 成员实现（全部在命名空间 http::cache 内） ======
namespace http::cache {

// ---- 适配层：类内 static 的定义（务必带 CacheMiddleware:: 前缀） ----
std::string CacheMiddleware::getMethod(const http::HttpRequest& req) {
  return methodToString(req);
}
std::string CacheMiddleware::getPathWithQuery(const http::HttpRequest& req) {
  // 目前你的 HttpRequest 无公开 query 访问器，这里只用 path
  return req.path();
}
std::string CacheMiddleware::getHeader(const http::HttpRequest& req, const std::string& k) {
  return req.getHeader(k);
}

int CacheMiddleware::getStatus(const http::HttpResponse& resp) {
  return static_cast<int>(resp.getStatusCode());
}
std::string CacheMiddleware::getBody(const http::HttpResponse& resp) {
  ParsedResp pr; if (parseSerializedResponse(resp, &pr)) return pr.body; return {};
}
void CacheMiddleware::addHeader(http::HttpResponse* resp, const std::string& k, const std::string& v) {
  resp->addHeader(k, v);
}
bool CacheMiddleware::hasNoStore(const http::HttpResponse& resp) {
  ParsedResp pr; if (!parseSerializedResponse(resp, &pr)) return false; return pr.hasNoStore;
}
std::vector<std::pair<std::string,std::string>>
CacheMiddleware::dumpHeaders(const http::HttpResponse& resp) {
  ParsedResp pr; if (!parseSerializedResponse(resp, &pr)) return {}; return pr.headers;
}
std::string CacheMiddleware::dumpStatusLine(const http::HttpResponse& resp) {
  ParsedResp pr; if (!parseSerializedResponse(resp, &pr)) return "HTTP/1.1 200 OK";
  return pr.version + " " + std::to_string(pr.statusCode) + " " + pr.statusMessage;
}
void CacheMiddleware::setFromEntry(const CachedEntry& e, http::HttpResponse* out) {
  // 解析 e.statusLine -> 版本/码/消息
  std::string ver = "HTTP/1.1"; int code = 200; std::string msg = "OK";
  {
    std::istringstream iss(e.statusLine);
    iss >> ver;
    iss >> code;
    std::string rest; std::getline(iss, rest);
    msg = trim(rest);
  }
  out->setStatusLine(ver, static_cast<HttpResponse::HttpStatusCode>(code), msg);
  for (auto &h: e.headers) out->addHeader(h.first, h.second);
  out->setBody(e.body);
  out->addHeader("Age", "0");
  out->addHeader("X-Cache", "HIT");
}

// ---- 策略 & key ----
bool CacheMiddleware::isCacheableRequest(const HttpRequest& req, const CachePolicy& p) {
  if (p.respectAuthorization && !getHeader(req, "authorization").empty()) return false;
  auto m = getMethod(req);
  if (m == "GET")  return p.cacheGET;
  if (m == "HEAD") return p.cacheHEAD;
  return false;
}

bool CacheMiddleware::isCacheableResponse(const HttpResponse& resp, const CachePolicy& p) {
  if (p.respectNoStore && hasNoStore(resp)) return false;
  int sc = getStatus(resp);
  return (sc==200 && p.cache200) || (sc==301 && p.cache301) || (sc==404 && p.cache404);
}

CacheKey CacheMiddleware::makeKey(const HttpRequest& req, const CachePolicy& p) {
  CacheKey k;
  k.method       = getMethod(req);
  k.pathAndQuery = getPathWithQuery(req);
  if (p.varyAcceptEncoding) k.acceptEncoding = getHeader(req, "accept-encoding");
  return k;
}

CachedEntry CacheMiddleware::pack(const HttpResponse& resp,
                                  std::chrono::steady_clock::time_point now,
                                  const CachePolicy& p) {
  CachedEntry e;
  e.statusLine = dumpStatusLine(resp);
  e.headers    = dumpHeaders(resp);
  e.body       = getBody(resp);
  e.hardExpire = now + p.ttl;
  e.softExpire = e.hardExpire + p.staleWhileRevalidate;
  return e;
}

// ---- 钩子 ----
bool CacheMiddleware::before(const HttpRequest& req, HttpResponse* resp) {
  if (!isCacheableRequest(req, policy_)) return false;

  auto key = makeKey(req, policy_);
  auto now = std::chrono::steady_clock::now();

  auto hit = store_->get(key);
  if (!hit) return false;

  if (now < hit->hardExpire) {
    setFromEntry(*hit, resp);
    return true; // 新鲜命中
  }
  if (now < hit->softExpire) {
    setFromEntry(*hit, resp);
    addHeader(resp, "Warning", "110 - Response is Stale");
    return true; // 软过期命中
  }
  return false; // 彻底过期
}

void CacheMiddleware::after(const HttpRequest& req, const HttpResponse& resp) {
  if (!isCacheableRequest(req, policy_))  return;
  if (!isCacheableResponse(resp, policy_)) return;

  auto e = pack(resp, std::chrono::steady_clock::now(), policy_);
  if (e.bytes() > policy_.maxObjectBytes) return;

  store_->set(makeKey(req, policy_), e);
}

} // namespace http::cache
