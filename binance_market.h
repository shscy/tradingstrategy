#pragma once


#include <iostream>
#include<string_view>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>

#include "spdlog/spdlog.h"
#include "message.h"
#include "simdjson.h"


namespace trading::market::binance {
    using namespace std::chrono_literals;

    namespace beast = boost::beast; // from <boost/beast.hpp>
    namespace http = beast::http; // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
    namespace net = boost::asio; // from <boost/asio.hpp>
    namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
    using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

    // Report a failure
    void
    fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }


    template<class T>
    class MarketTrading : public std::enable_shared_from_this<MarketTrading<T>> {
        tcp::resolver resolver_;
        websocket::stream<
            beast::ssl_stream<beast::tcp_stream> > ws_;
        boost::asio::steady_timer timer_;
        beast::flat_buffer buffer_;
        std::string host_;
        std::string path_;
        std::string port_;

        bool stop_{false};
        net::io_context &ioc_;
        ssl::context &ctx_;
        T &queue_;
        simdjson::ondemand::parser parser_;
        event::TagType tag_;

    public:
        // Resolver and socket require an io_context
        explicit
        MarketTrading(net::io_context &ioc, ssl::context &ctx, T &queue, event::TagType tag)
            : resolver_(net::make_strand(ioc)),
              ioc_(ioc),
              ctx_(ctx),
              tag_(tag),
              ws_(net::make_strand(ioc), ctx),
              queue_(queue),
              timer_(net::make_strand(ioc)) {
            spdlog::info("spdlog info");
        }

        // Start the asynchronous operation
        void
        run(
            std::string host,
            std::string port,
            std::string path) {
            spdlog::info("start to run");
            // Save these for later
            host_ = host;
            port_ = port;
            path_ = path;


            // Look up the domain name
            resolver_.async_resolve(
                host,
                port,
                beast::bind_front_handler(
                    &MarketTrading::on_resolve,
                    this->shared_from_this()));
        }

        void
        on_resolve(
            beast::error_code ec,
            tcp::resolver::results_type results) {
            if (ec)
                return fail(ec, "resolve");

            // Set a timeout on the operation
            beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(10));

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(ws_).async_connect(
                results,
                beast::bind_front_handler(
                    &MarketTrading::on_connect,
                    this->shared_from_this()));
        }

        void
        on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
            if (ec)
                return fail(ec, "connect");

            // Set a timeout on the operation
            beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(10));

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(
                ws_.next_layer().native_handle(),
                host_.c_str())) {
                ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                                       net::error::get_ssl_category());
                return fail(ec, "connect");
            }

            // Update the host_ string. This will provide the value of the
            // Host HTTP header during the WebSocket handshake.
            // See https://tools.ietf.org/html/rfc7230#section-5.4
            host_ += ':' + std::to_string(ep.port());

            // Perform the SSL handshake
            ws_.next_layer().async_handshake(
                ssl::stream_base::client,
                beast::bind_front_handler(
                    &MarketTrading::on_ssl_handshake,
                    this->shared_from_this()));
        }

        void
        on_ssl_handshake(beast::error_code ec) {
            if (ec)
                return fail(ec, "ssl_handshake");

            // Turn off the timeout on the tcp_stream, because
            // the websocket stream has its own timeout system.
            beast::get_lowest_layer(ws_).expires_never();

            websocket::stream_base::timeout opt{
                std::chrono::seconds(5), // 握手超时
                std::chrono::seconds(5), // 空闲超时
                false
            };

            ws_.set_option(opt);
            // Set suggested timeout settings for the websocket
            //        ws_.set_option(
            //                websocket::stream_base::timeout::suggested(
            //                        beast::role_type::client));

            // Set a decorator to change the User-Agent of the handshake
            ws_.set_option(websocket::stream_base::decorator(
                [](websocket::request_type &req) {
                    req.set(http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-client-async-ssl");
                }));


            // Perform the websocket handshake
            ws_.async_handshake(host_, path_,
                                beast::bind_front_handler(
                                    &MarketTrading::on_handshake,
                                    this->shared_from_this()));
        }

        void
        on_handshake(beast::error_code ec) {
            if (ec)
                return fail(ec, "handshake");


            // Send the message
            ws_.async_read(
                buffer_,
                beast::bind_front_handler(
                    &MarketTrading::on_read,
                    this->shared_from_this()));
        }

        void
        on_write(
            beast::error_code ec,
            std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            if (ec)
                return fail(ec, "write");

            // Read a message into our buffer
            ws_.async_read(
                buffer_,
                beast::bind_front_handler(
                    &MarketTrading::on_read,
                    this->shared_from_this()));
        }

        void
        on_read(
            beast::error_code ec,
            std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);

            if (ec)
                return fail(ec, "read");
            try {
                timer_.cancel();
            } catch (const std::exception &ex) {
            }
            // bool ok = false;
            //uint8_t  type_ = 0;
            //void *dest = parse_json(ok, type_);
            auto size = buffer_.size() + simdjson::SIMDJSON_PADDING;
            char *data = new char[size];
            std::memcpy(data, buffer_.data().data(), buffer_.size());
            queue_.write(trading::event::RawSimdJsonMessage{
                .data_ = data,
                .data_size_ = buffer_.size(),
                .simd_data_size_ = size,
                .tag = tag_,
            });

            buffer_.clear();
            ws_.async_read(
                buffer_, [self = this->shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
                    self->on_read(ec, bytes_transferred);
                });
        }

        void
        on_close(beast::error_code ec) {
            if (ec)
                return fail(ec, "on_close");
            spdlog::error("read timeout close success");
        }


        ~MarketTrading() {
            std::cout << "shutdown " << "\n";
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(500ms);
            std::make_shared<MarketTrading>(ioc_, ctx_, queue_, tag_)->run(host_, port_, path_);
        }
    };
}
