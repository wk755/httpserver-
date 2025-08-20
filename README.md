# httpserver-
本项目设计并实现了一套高性能 C++ HttpServer 框架，支持 HTTP/HTTPS 双协议、多线程异步 I/O、动态路由与中间件机制，并基于该框架开发了一个在线五子棋对战小程序，包含用户登录注册、菜单导航、人机/对战逻辑与胜负判定等功能模块

框架梳理：
一. http模块
实际上就是负责定义http通信报文段，请求报文，响应报文，然后对其进行包装解析，然后基于muduo库构建一个httserver服务器；
httpcontext：顾名思义，负责报文的解析；httprequest：解析原始请求字节流，封装成请求行+请求头+请求体的请求报文段；httpresponse：将封装成状态行+响应头+响应体的响应报文段解释成原始响应字节流发送回回去；数据流传输部分用到了muduo库提供的buffer指针，muduo::net::Buffer 是 muduo 为网络场景做的高效字节缓冲区。即muduo 开辟了一块 buffer 区，然后用户用指针在 buffer 里读，减少拷贝；buffer 在用户态；数据是从 socket 读来的。

