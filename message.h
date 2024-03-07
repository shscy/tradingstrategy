//
// Created by zuoxiaoliang on 2024/3/5.
//

#pragma once
#ifndef TRADINGSTRATEGY_MESSAGE_H
#define TRADINGSTRATEGY_MESSAGE_H
#include "folly/ProducerConsumerQueue.h"

#include <string>
#include "block_queue.h"


namespace trading::event {
    constexpr uint32_t depth_size = 5;

    template<class T>
    using Queue = queue<T>;

    using TagType = uint8_t;

    constexpr TagType BianceFuture = 1;
    constexpr TagType BianceSpot = 2;

    struct RawSimdJsonMessage {
        char*data_;
        size_t data_size_;
        size_t simd_data_size_;
        TagType tag;
    };
    // binance
    // 1 bookTicker
    // 2 trade

    // 用于从websocket获取的原始数据
    struct MemoryWebsocketMessage {
        void *data_;
        uint8_t type_;
    };




    struct BestBook {
        std::string symbol;
        float best_ask_price;
        float best_ask_size;
        float best_bid_price;
        float best_bid_size;
    };

    struct Trade {
        std::string symbol;
        uint64_t trade_time;
        float price;
        float size;
        int8_t dir = -1; // -1表示没有 0 表示sell 1 表示buy
    };

    struct Depth {
        float ask_price[depth_size];
        float ask_size[depth_size];
        float bid_price[depth_size];
        float bid_size[depth_size];
    };
}


#endif //TRADINGSTRATEGY_MESSAGE_H
