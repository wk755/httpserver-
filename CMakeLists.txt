cmake_minimum_required(VERSION 3.10)
project(simple_server)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 头文件路径（保持你的写法）
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/HttpServer/include
    ${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/include
    /usr/include/mysql-cppconn-8
    /usr/include/mysql
    ${OPENSSL_INCLUDE_DIR}
)

# mysqlcppconn 库位置（保持你的查找逻辑）
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

# 源文件收集（保持你的写法）
file(GLOB_RECURSE HTTP_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/HttpServer/src/*.cpp"
)
file(GLOB_RECURSE GOMOKU_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/*.cpp"
)

# 主文件（保持你的写法）
set(MAIN_SRC "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/main.cpp")
if(NOT EXISTS ${MAIN_SRC})
    file(WRITE ${MAIN_SRC}
"#include \"../include/GomokuServer.h\"
#include <iostream>
int main(int argc, char* argv[]) {
    try {
        GomokuServer server;
        server.start();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << \"Error: \" << e.what() << std::endl;
        return 1;
    }
}
")
endif()

# ===== 新增：把 HttpCache（以及可选 StaticServe）拉进来 =====
add_subdirectory(http_cache)
if (EXISTS ${PROJECT_SOURCE_DIR}/StaticServe/CMakeLists.txt)
  add_subdirectory(StaticServe)
endif()

# 可执行文件
add_executable(simple_server
    ${MAIN_SRC}
    ${HTTP_SERVER_SRC}
    ${GOMOKU_SERVER_SRC}
)

# 如果你找到了 mysqlcppconn 的绝对路径就用它，否则用库名
if (MYSQLCPPCONN_LIBRARY)
  set(MYSQLCPPCONN_LIB ${MYSQLCPPCONN_LIBRARY})
else()
  set(MYSQLCPPCONN_LIB mysqlcppconn)
endif()

# ===== 重新整理链接：用 OpenSSL 的导入目标，使用线程包，并把 http_cache 链上 =====
target_link_libraries(simple_server
    PRIVATE
      muduo_net
      muduo_base
      ${MYSQLCPPCONN_LIB}
      mysqlclient
      Threads::Threads
      OpenSSL::SSL
      OpenSSL::Crypto
      http_cache
)

# 如果 StaticServe 子库存在，也一起链接
if (TARGET static_serve)
  target_link_libraries(simple_server PRIVATE static_serve)
endif()

# 调试信息（保持你的输出）
message(STATUS "Include directories:")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "  ${dir}")
endforeach()
message(STATUS "Main source file: ${MAIN_SRC}")
message(STATUS "HTTP Server sources: ${HTTP_SERVER_SRC}")
message(STATUS "Gomoku Server sources: ${GOMOKU_SERVER_SRC}")
