cmake_minimum_required(VERSION 3.23)
project(tradingstrategy)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (THREADS_PREFER_PTHREAD_FL ON)
find_package (Threads REQUIRED)

find_package(OpenSSL)
if (NOT MSVC)
    link_libraries(${OPENSSL_LIBRARIES})
endif()

add_subdirectory(third_party/spdlog)


find_package(Boost 1.84.0 REQUIRED COMPONENTS system thread)
IF(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
ENDIF(Boost_FOUND)

find_package(Folly REQUIRED)
find_package(gflags CONFIG REQUIRED)

add_executable(market_save market_sink.cpp
        shutdown.cpp
        shutdown.h
        message.cpp
        message.h
        binance_market.h
        simdjson.cpp
)
target_link_libraries(market_save ${Boost_LIBRARIES}   Folly::folly  spdlog::spdlog)


add_executable(bargainHunting main.cpp
        shutdown.cpp
        shutdown.h
        message.cpp
        message.h
        binance_market.h
        simdjson.cpp
        block_queue.h
)
target_link_libraries(bargainHunting ${Boost_LIBRARIES}   Folly::folly  spdlog::spdlog)