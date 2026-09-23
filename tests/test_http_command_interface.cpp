#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/error.hpp>
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/http_command_interface_client.hpp>

using namespace std::chrono_literals;

namespace
{

// A JSON-RPC server that takes one request per HTTP connection, like the digiBOX-WT's command
// interface. It answers, drops, stalls or answers with HTTP 400 or a body that is not JSON, as
// scripted per signal ID and attempt, and records the signal ID of each request in arrival order.
class fake_jsonrpc_http_server
{
    public:

        enum class action { answer, drop, stall, bad_status, not_json };

        fake_jsonrpc_http_server()
            : _acceptor(_ioc, {boost::asio::ip::make_address("127.0.0.1"), 0})
        {
            accept();
            _thread = std::thread([this] { _ioc.run(); });
        }

        ~fake_jsonrpc_http_server()
        {
            _ioc.stop();
            _thread.join();
        }

        std::uint16_t port() const
        {
            return _acceptor.local_endpoint().port();
        }

        // Sets what happens to each attempt of a signal's request; attempts past the list are answered.
        void script(const std::string& signal_id, std::vector<action> actions)
        {
            std::scoped_lock lock(_mutex);
            _script[signal_id] = std::move(actions);
        }

        std::vector<std::string> arrivals()
        {
            std::scoped_lock lock(_mutex);
            return _arrivals;
        }

    private:

        using request_type = boost::beast::http::request<boost::beast::http::string_body>;
        using response_type = boost::beast::http::response<boost::beast::http::string_body>;

        void accept()
        {
            _acceptor.async_accept(
                [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket)
                {
                    if (ec)
                        return;

                    read(std::make_shared<boost::beast::tcp_stream>(std::move(socket)));
                    accept();
                });
        }

        void read(std::shared_ptr<boost::beast::tcp_stream> stream)
        {
            auto buffer = std::make_shared<boost::beast::flat_buffer>();
            auto request = std::make_shared<request_type>();

            boost::beast::http::async_read(*stream, *buffer, *request,
                [this, stream, buffer, request](const boost::system::error_code& ec, std::size_t)
                {
                    if (!ec)
                        respond(stream, *request);
                });
        }

        void respond(std::shared_ptr<boost::beast::tcp_stream> stream, const request_type& request)
        {
            const auto body = nlohmann::json::parse(request.body());
            const std::string signal_id = body["params"][0];
            action next = action::answer;

            {
                std::scoped_lock lock(_mutex);
                const auto attempt = static_cast<std::size_t>(
                    std::count(_arrivals.begin(), _arrivals.end(), signal_id));
                _arrivals.push_back(signal_id);

                if (auto it = _script.find(signal_id); it != _script.end() && attempt < it->second.size())
                    next = it->second[attempt];
            }

            // the stream closes when the last reference to it goes
            if (next == action::drop)
                return;

            if (next == action::stall)
                return _stalled.push_back(stream);

            auto response = std::make_shared<response_type>(
                next == action::bad_status
                    ? boost::beast::http::status::bad_request
                    : boost::beast::http::status::ok,
                11);

            response->body() = next == action::not_json
                ? "Succeeded"
                : nlohmann::json({ { "jsonrpc", "2.0" }, { "id", body["id"] }, { "result", true } }).dump();
            response->prepare_payload();

            boost::beast::http::async_write(*stream, *response,
                [stream, response](const boost::system::error_code&, std::size_t) {});
        }

        boost::asio::io_context _ioc{1};
        boost::asio::ip::tcp::acceptor _acceptor;
        std::thread _thread;

        std::vector<std::shared_ptr<boost::beast::tcp_stream>> _stalled;

        std::mutex _mutex;
        std::map<std::string, std::vector<action>> _script;
        std::vector<std::string> _arrivals;
};

using action = fake_jsonrpc_http_server::action;

struct result
{
    std::string signal_id;
    boost::system::error_code ec;
    nlohmann::json response;
};

}

TEST(HttpCommandInterface, SendsRequestsOneAtATimeAndResendsOnlyUnansweredOnes)
{
    fake_jsonrpc_http_server server;
    server.script("b", { action::drop });
    server.script("c", { action::stall });
    server.script("d", { action::bad_status });
    server.script("e", { action::not_json });
    server.script("f", { action::drop, action::drop });

    boost::asio::io_context ioc{1};
    wss::detail::http_command_interface_client client{
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1"};

    std::vector<result> results;
    for (const std::string signal_id : { "a", "b", "c", "d", "e", "f", "g" })
        client.async_request("FAKE.subscribe", { signal_id },
            [&results, signal_id](const boost::system::error_code& ec, const nlohmann::json& response)
            {
                results.push_back({ signal_id, ec, response });
            });

    ioc.run_for(20s);

    // one request at a time: each request arrives after the previous one was answered or given up
    EXPECT_EQ(server.arrivals(),
        (std::vector<std::string>{ "a", "b", "b", "c", "c", "d", "e", "f", "f", "g" }));

    ASSERT_EQ(results.size(), 7u);

    EXPECT_EQ(results[0].signal_id, "a");
    EXPECT_FALSE(results[0].ec);
    EXPECT_EQ(results[0].response.value("result", false), true);

    EXPECT_EQ(results[1].signal_id, "b");
    EXPECT_FALSE(results[1].ec);

    EXPECT_EQ(results[2].signal_id, "c");
    EXPECT_FALSE(results[2].ec);

    EXPECT_EQ(results[3].signal_id, "d");
    EXPECT_EQ(results[3].ec, boost::beast::http::error::bad_status);

    EXPECT_EQ(results[4].signal_id, "e");
    EXPECT_FALSE(results[4].ec);
    EXPECT_TRUE(results[4].response.is_null());

    EXPECT_EQ(results[5].signal_id, "f");
    EXPECT_TRUE(results[5].ec);

    EXPECT_EQ(results[6].signal_id, "g");
    EXPECT_FALSE(results[6].ec);
}

TEST(HttpCommandInterface, CancelEndsQueuedAndStalledRequestsAtOnce)
{
    fake_jsonrpc_http_server server;
    server.script("a", { action::stall });

    boost::asio::io_context ioc{1};
    wss::detail::http_command_interface_client client{
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1"};

    std::vector<result> results;
    for (const std::string signal_id : { "a", "b" })
        client.async_request("FAKE.subscribe", { signal_id },
            [&results, signal_id](const boost::system::error_code& ec, const nlohmann::json& response)
            {
                results.push_back({ signal_id, ec, response });
            });

    ioc.run_for(500ms);
    ASSERT_EQ(server.arrivals(), std::vector<std::string>{ "a" });

    client.cancel();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].ec, boost::asio::error::operation_aborted);
    EXPECT_EQ(results[1].ec, boost::asio::error::operation_aborted);

    // no work is left behind: no resend of the stalled request, no timer
    const auto start = std::chrono::steady_clock::now();
    ioc.restart();
    ioc.run_for(10s);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" });
}
