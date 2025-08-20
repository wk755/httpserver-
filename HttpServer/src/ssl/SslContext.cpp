#include "../../include/ssl/SslContext.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>

namespace ssl
{
SslContext::SslContext(const SslConfig& config)
    : ctx_(nullptr)
    , config_(config)
{

}

SslContext::~SslContext()
{
    if (ctx_)
    {
        SSL_CTX_free(ctx_);
    }
}

bool SslContext::initialize()
{
    // 初始化 OpenSSL
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | 


                    OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);

    // 创建 SSL 上下文
    const SSL_METHOD* method = TLS_server_method(); //返回一个通用的服务器端方法（支持 TLS1.0 ~ TLS1.3）,tls是升级版
    ctx_ = SSL_CTX_new(method); //基于该方法创建一个新的 SSL_CTX 上下文对象。
    if (!ctx_)
    {
        handleSslError("Failed to create SSL context");
        return false;
    }

    // 设置 SSL 选项
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | 
                  SSL_OP_NO_COMPRESSION |
                  SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(ctx_, options);

    // 加载证书和私钥
    if (!loadCertificates())
    {
        return false;
    }

    // 设置协议版本
    if (!setupProtocol())
    {
        return false;
    }

    // 设置会话缓存
    setupSessionCache();

    LOG_INFO << "SSL context initialized successfully";
    return true;
}

bool SslContext::loadCertificates()
{
    // 加载证书
    if (SSL_CTX_use_certificate_file(ctx_,
     config_.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load server certificate");
        return false;
    }

    // 加载私钥
    if (SSL_CTX_use_PrivateKey_file(ctx_, 
        config_.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load private key");
        return false;
    }

    // 验证私钥
    if (!SSL_CTX_check_private_key(ctx_))
    {
        handleSslError("Private key does not match the certificate");
        return false;
    }

    // 加载证书链
    if (!config_.getCertificateChainFile().empty())
    {
        if (SSL_CTX_use_certificate_chain_file(ctx_,
            config_.getCertificateChainFile().c_str()) <= 0)
        {
            handleSslError("Failed to load certificate chain");
            return false;
        }
    }

    return true;
}

bool SslContext::setupProtocol()
{
    long opts = SSL_CTX_get_options(ctx_);
    opts |= SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
    SSL_CTX_set_options(ctx_, opts);

    // 2) 用“最小/最大版本”明确协议区间
    //    默认允许 TLS1.2 起；若你在 SslConfig 里设置了 1.3，则提升下限到 1.3
    int minVer = TLS1_2_VERSION;
#ifdef TLS1_3_VERSION
    if (config_.getProtocolVersion() == SSLVersion::TLS_1_3) {
        minVer = TLS1_3_VERSION;
    }
#endif
    if (SSL_CTX_set_min_proto_version(ctx_, minVer) != 1) {
        LOG_ERROR << "Failed to set min TLS version";
        return false;
    }
    // 如果想“只允许 1.2”，再加一条最大版本限制：
    // SSL_CTX_set_max_proto_version(ctx_, TLS1_2_VERSION);

    // 3) 配置密码套件
    // ≤ TLS1.2 走 cipher_list（注意把拼写修成 !MD5）
    const std::string& c = config_.getCipherList();
    if (!c.empty()) {
        if (SSL_CTX_set_cipher_list(ctx_, c.c_str()) != 1) {
            LOG_ERROR << "Failed to set cipher list: " << c;
            return false;
        }
    }

#ifdef TLS1_3_VERSION
    // TLS1.3 套件需单独设置（可保留默认，也可指定一组常见的）
    if (minVer <= TLS1_3_VERSION) {
        // 这行可选；不设也会有合理默认
        SSL_CTX_set_ciphersuites(ctx_,
            "TLS_AES_128_GCM_SHA256:"
            "TLS_CHACHA20_POLY1305_SHA256:"
            "TLS_AES_256_GCM_SHA384");
    }
#endif
    return true;
}

void SslContext::setupSessionCache()
{
    // 1. 把会话缓存模式设置为“服务器端缓存”
    SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);
    // 2. 设置会话缓存的最大条目数
    SSL_CTX_sess_set_cache_size(ctx_, config_.getSessionCacheSize());
    // 3. 设置会话超时时间（秒），超时后缓存的会话将被丢弃
    SSL_CTX_set_timeout(ctx_, config_.getSessionTimeout());
}

void SslContext::handleSslError(const char* msg)
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    LOG_ERROR << msg << ": " << buf;
}

}; // namespace ssl