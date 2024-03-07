#include "binance_market.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <fstream>
#include "block_queue.h"

namespace binance =  trading::market::binance;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace event = trading::event;
//------------------------------------------------------------------------------

void foo()
{
    // simulate expensive operation
    std::this_thread::sleep_for(std::chrono::seconds(1));
}


class FileSink {
    std::string file_name_;
    std::ofstream file_handler;

public:
    explicit FileSink(const std::string&& name): file_name_(name) {
        file_handler.open(name);
    }

    void write(const char*data, size_t size) {
        file_handler.write(data, size);
    }

    ~FileSink() {
        try {
            file_handler.close();
        }catch (std::exception& e) {
            spdlog::error("close file error {}", e.what());
        }
    }
};


std::string buildStreamPath(const std::vector<std::string> & symbols) {
    std::string ret{"/stream?streams="};
    ret += symbols[0] + "@aggTrade";
    for(int i = 1; i < symbols.size(); i++) {
        const std::string&  item = symbols[i];
        ret += "/" + item + "@aggTrade";
    }
    return ret;
}



int main(int argc, char** argv)
{
    // The io_context is required for all I/O
    net::io_context ioc;

    std::vector<std::string > symbols;
    symbols.emplace_back("btcusdt");
    symbols.emplace_back("ethusdt");
    symbols.emplace_back("opusdt");
    symbols.emplace_back("arbusdt");
    symbols.emplace_back("solusdt");
    symbols.emplace_back("rndrusdt");

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};
    // 现货行情
    event::Queue<event::RawSimdJsonMessage> q(1024*128);
    {
        event::TagType tag = event::BianceSpot;
        std::string host("stream.binance.com");
        std::string  port ("443");
        std::string path_s = buildStreamPath(symbols);
        std::cout<<"path " << path_s << "\n";
        std::string  path{std::move(path_s)};


        std::make_shared<binance::MarketTrading<event::Queue<event::RawSimdJsonMessage>>>(ioc, ctx, q, tag)->run(host, port, path);
    }
    {
        event::TagType tag = event::BianceFuture;
        std::string host("fstream.binance.com");
        std::string port ("443");
        std::string path{std::move(buildStreamPath(symbols))};

        std::make_shared<binance::MarketTrading<event::Queue<event::RawSimdJsonMessage>>>(ioc, ctx, q, tag)->run(host, port, path);
    }

    std::atomic<bool> stop = false;
    std::thread t([&]() {
        simdjson::ondemand::parser parser_;
        FileSink trade_file("/Users/zuoxiaoliang/project/market_data/binance_trade.data");
        FileSink future_trade_file("/Users/zuoxiaoliang/project/market_data/binance_future_trade.data");
        // FileSink ticker_file("/Users/zuoxiaoliang/project/cplusplus/tradingstrategy/book.txt");
        event::RawSimdJsonMessage item{};
        while(!stop.load()) {
            q.pop(item);
            char *data = item.data_;
            const size_t data_size = item.data_size_;
            const size_t all_size = item.simd_data_size_;
            try {
                simdjson::ondemand::document doc = parser_.iterate(data, data_size, all_size).take_value();
                simdjson::ondemand::object object = doc.get_object();
                if (auto stream = std::string_view(object["stream"]); stream.ends_with("aggTrade")) {
                    data[data_size] = '\n';
                    if(item.tag == event::BianceFuture) {
                        future_trade_file.write(data, data_size+1);
                    }else {
                        trade_file.write(data, data_size+1);
                    }
                }
            }catch(simdjson::simdjson_error& e) {
                spdlog::error("json faield {}", e.what());
            }
        }
    });
    // Run the I/O service. The call will return when
    // the socket is closed.
    ioc.run();
    stop.store(true);
    t.join();

    return EXIT_SUCCESS;
}