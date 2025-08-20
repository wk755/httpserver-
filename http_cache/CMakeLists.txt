add_library(http_cache STATIC
  src/MemoryCacheLRU.cpp
  src/CacheMiddleware.cpp
)

target_include_directories(http_cache
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}     # 让 CacheMiddleware.cpp 能包含你的 HttpRequest/HttpResponse 头
)

target_compile_features(http_cache PUBLIC cxx_std_17)
