#include "binance_market.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <fstream>


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


int main(int argc, char** argv)
{
    // Check command line arguments.

    std::string host("stream.binance.com");
    std::string  port ("443");
    std::string  path ( "/stream?streams=btcusdt@aggTrade");
    std::string  text ("{ \"method\": \"SUBSCRIBE\", \"params\": [ \"btcusdt@aggTrade\" ], \"id\": 1 }");

    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};

    // This holds the root certificate used for verification
    //load_root_certificates(ctx);
    folly::ProducerConsumerQueue<event::RawSimdJsonMessage>  q(1024*128);
    // Launch the asynchronous operation
    std::make_shared<binance::MarketTrading>(ioc, ctx, q)->run(host, port, path);

    std::atomic<bool> stop = false;
    std::thread t([&]() {
        simdjson::ondemand::parser parser_;
        FileSink trade_file("/Users/zuoxiaoliang/project/cplusplus/tradingstrategy/trade.text");
        FileSink ticker_file("/Users/zuoxiaoliang/project/cplusplus/tradingstrategy/book.txt");

        while(!stop.load()) {
            auto* item = q.frontPtr();
            if(item == nullptr) {
                continue;
            }
            char *data = item->data_;
            const size_t data_size = item->data_size_;
            const size_t all_size = item->simd_data_size_;
            try {
                simdjson::ondemand::document doc = parser_.iterate(data, data_size, all_size).take_value();
                simdjson::ondemand::object object = doc.get_object();
                if (auto stream = std::string_view(object["stream"]); stream.ends_with("bookTicker")) {
                    data[data_size] = '\n';
                    ticker_file.write(data, data_size+1);
                } else if (stream.ends_with("aggTrade")) {
                    data[data_size] = '\n';
                    trade_file.write(data, data_size+1);
                }
            }catch(simdjson::simdjson_error& e) {
                spdlog::error("json faield {}", e.what());
            }
            q.popFront();

        }
    });
    // Run the I/O service. The call will return when
    // the socket is closed.
    ioc.run();
    stop.store(true);
    t.join();

    return EXIT_SUCCESS;
}