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

#include <ws-streaming/ws-streaming.hpp>
#include <ws-streaming/detail/http_command_interface_client.hpp>
#include <ws-streaming/detail/peer.hpp>

using namespace std::chrono_literals;

namespace
{

// A JSON-RPC server that takes one request per HTTP connection.
// It answers, drops, stalls or answers with HTTP 400 or a body that is not JSON, as
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

// For tests that do not exercise the timeout: no machine, however slow, reaches it, so it never resends a request.
// It also outlasts every run_for limit below, so a timer left behind keeps the work from ending and is caught.
constexpr std::chrono::hours long_timeout{1};

struct result
{
    std::string signal_id;
    boost::system::error_code ec;
    nlohmann::json response;
};

// A WebSocket Streaming peer that announces signal "a" and a "jsonrpc-http" command interface on
// the given port, written as a JSON integer. It accepts the WebSocket upgrade on a raw socket;
// the client only checks the status line.
class fake_http_peer
{
    public:

        explicit fake_http_peer(std::uint16_t command_port)
            : _command_port(command_port)
            , _acceptor(_ioc, {boost::asio::ip::make_address("127.0.0.1"), 0})
        {
            _acceptor.async_accept(
                [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket)
                {
                    if (!ec)
                        upgrade(std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket)));
                });

            _thread = std::thread([this] { _ioc.run(); });
        }

        ~fake_http_peer()
        {
            _ioc.stop();
            _thread.join();
        }

        std::string url() const
        {
            return "ws://127.0.0.1:" + std::to_string(_acceptor.local_endpoint().port()) + "/";
        }

        // closes the WebSocket from the peer's side
        void close()
        {
            boost::asio::post(_ioc, [this] { if (_peer) _peer->stop(); });
        }

        // tells the client that signal "a" is subscribed, as a device does once it has run the subscribe request
        void acknowledge_subscribe()
        {
            boost::asio::post(_ioc, [this] { if (_peer) _peer->send_metadata(1, "subscribe", { { "signalId", "a" } }); });
        }

    private:

        void upgrade(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
        {
            auto buffer = std::make_shared<boost::asio::streambuf>();

            boost::asio::async_read_until(*socket, *buffer, "\r\n\r\n",
                [this, socket, buffer](const boost::system::error_code& ec, std::size_t)
                {
                    if (ec)
                        return;

                    boost::asio::async_write(*socket, boost::asio::buffer(_upgrade_response),
                        [this, socket](const boost::system::error_code& ec, std::size_t)
                        {
                            if (!ec)
                                start(std::move(*socket));
                        });
                });
        }

        void start(boost::asio::ip::tcp::socket socket)
        {
            _peer = std::make_shared<wss::detail::peer>(std::move(socket), false);
            _peer->run();

            _peer->send_metadata(0, "apiVersion", { { "version", "1.0.0" } });
            _peer->send_metadata(0, "init", {
                { "streamId", "FAKE" },
                { "commandInterfaces", { { "jsonrpc-http", {
                    { "httpMethod", "POST" },
                    { "httpPath", "/" },
                    { "httpVersion", "1.1" },
                    { "port", _command_port },
                } } } },
            });
            _peer->send_metadata(0, "available", { { "signalIds", { "a" } } });
        }

        const std::uint16_t _command_port;
        const std::string _upgrade_response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: fake\r\n"
            "\r\n";

        boost::asio::io_context _ioc{1};
        boost::asio::ip::tcp::acceptor _acceptor;
        std::thread _thread;

        std::shared_ptr<wss::detail::peer> _peer;
};

// A client connection that subscribes signal "a" as soon as the peer announces it. Like the LT client
// module with a subscription it holds, it releases the subscription when the signal reports itself
// unsubscribed, which a closing connection does for each subscribed signal.
struct subscribing_client
{
    explicit subscribing_client(const std::string& url)
    {
        client.async_connect(url,
            [this](const boost::system::error_code& ec, wss::connection_ptr c)
            {
                ASSERT_FALSE(ec) << ec.message();
                connection = c;
                on_available = c->on_available.connect(
                    [this](wss::remote_signal_ptr signal)
                    {
                        // the signal itself raises these, so it is alive while they run
                        on_subscribed = signal->on_subscribed.connect([this] { subscribed = true; });
                        on_unsubscribed = signal->on_unsubscribed.connect(
                            [this, s = signal.get()] { subscribed = false; s->unsubscribe(); });
                        signal->subscribe();
                    });
            });
    }

    template <typename Predicate>
    bool run_until(Predicate done, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!done() && std::chrono::steady_clock::now() < deadline)
            ioc.run_for(10ms);
        return done();
    }

    boost::asio::io_context ioc{1};
    wss::client client{ioc.get_executor()};
    wss::connection_ptr connection;
    bool subscribed = false;
    boost::signals2::scoped_connection on_available;
    boost::signals2::scoped_connection on_subscribed;
    boost::signals2::scoped_connection on_unsubscribed;
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

    // an attempt given up on is canceled, so nothing is left waiting once the last request completes
    EXPECT_TRUE(ioc.stopped()) << "work was left behind: an attempt given up on, or a timer, is still pending";

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

TEST(HttpCommandInterface, RequestMadeInAHandlerQueuesBehindTheOthers)
{
    fake_jsonrpc_http_server server;

    boost::asio::io_context ioc{1};
    wss::detail::http_command_interface_client client{
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1", long_timeout};

    std::vector<std::string> completed;
    client.async_request("FAKE.subscribe", { "a" },
        [&](const boost::system::error_code&, const nlohmann::json&)
        {
            completed.push_back("a");
            client.async_request("FAKE.subscribe", { "c" },
                [&completed](const boost::system::error_code&, const nlohmann::json&) { completed.push_back("c"); });
        });
    client.async_request("FAKE.subscribe", { "b" },
        [&completed](const boost::system::error_code&, const nlohmann::json&) { completed.push_back("b"); });

    ioc.run_for(10s);

    // "b" was queued before "c" was requested, so it goes first, and only once
    EXPECT_EQ(server.arrivals(), (std::vector<std::string>{ "a", "b", "c" }));
    EXPECT_EQ(completed, (std::vector<std::string>{ "a", "b", "c" }));
}

TEST(HttpCommandInterface, CancelEndsQueuedAndStalledRequestsAtOnce)
{
    fake_jsonrpc_http_server server;
    server.script("a", { action::stall });

    boost::asio::io_context ioc{1};
    wss::detail::http_command_interface_client client{
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1", long_timeout};

    std::vector<result> results;
    for (const std::string signal_id : { "a", "b" })
        client.async_request("FAKE.subscribe", { signal_id },
            [&results, signal_id](const boost::system::error_code& ec, const nlohmann::json& response)
            {
                results.push_back({ signal_id, ec, response });
            });

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (server.arrivals().empty() && std::chrono::steady_clock::now() < deadline)
        ioc.run_for(10ms);
    ASSERT_EQ(server.arrivals(), std::vector<std::string>{ "a" });

    client.cancel();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].ec, boost::asio::error::operation_aborted);
    EXPECT_EQ(results[1].ec, boost::asio::error::operation_aborted);

    // no work is left behind: no resend of the stalled request, no timer; run_for returns as soon as the
    // canceled operations complete, however long the system takes to report them
    ioc.restart();
    ioc.run_for(10s);
    EXPECT_TRUE(ioc.stopped()) << "work was left behind: a resend, a timer or the stalled attempt is still pending";
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" });
}

TEST(HttpCommandInterface, DestroyingTheClientEndsAStalledRequest)
{
    fake_jsonrpc_http_server server;
    server.script("a", { action::stall, action::stall });

    boost::asio::io_context ioc{1};
    auto client = std::make_unique<wss::detail::http_command_interface_client>(
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1", long_timeout);

    bool called = false;
    client->async_request("FAKE.subscribe", { "a" },
        [&called](const boost::system::error_code&, const nlohmann::json&) { called = true; });

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (server.arrivals().empty() && std::chrono::steady_clock::now() < deadline)
        ioc.run_for(10ms);
    ASSERT_EQ(server.arrivals(), std::vector<std::string>{ "a" });

    client.reset();

    // no work is left behind: no resend, no timer, no attempt waiting for its answer; run_for returns as soon
    // as the operations the destructor canceled complete, however long the system takes to report them
    ioc.restart();
    ioc.run_for(10s);
    EXPECT_TRUE(ioc.stopped()) << "work was left behind: a resend, a timer or the stalled attempt is still pending";
    EXPECT_FALSE(called);
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" });
}

TEST(HttpCommandInterface, DestroyingTheClientWhileItsTimeoutIsQueuedIsSafe)
{
    fake_jsonrpc_http_server server;

    boost::asio::io_context ioc{1};

    // a zero timeout has already expired when ioc starts to run, however slow the machine
    auto client = std::make_unique<wss::detail::http_command_interface_client>(
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1", 0s);

    // expired before the client's timeout; expired timers run in expiry order, so this one goes first and
    // destroys the client while the client's timeout handler is already queued, which then runs after that
    boost::asio::steady_timer destroyer{ioc};
    destroyer.expires_at(boost::asio::steady_timer::clock_type::now() - 1s);
    destroyer.async_wait([&client](const boost::system::error_code&) { client.reset(); });

    bool called = false;
    client->async_request("FAKE.subscribe", { "a" },
        [&called](const boost::system::error_code&, const nlohmann::json&) { called = true; });

    ioc.run_for(5s);

    ASSERT_FALSE(client);
    EXPECT_FALSE(called);
    EXPECT_TRUE(server.arrivals().empty()) << "the request was sent after the client was destroyed";
}

TEST(HttpCommandInterface, DestroyingTheClientBeforeItsRequestIsSentSendsNothing)
{
    fake_jsonrpc_http_server server;

    boost::asio::io_context ioc{1};
    auto client = std::make_unique<wss::detail::http_command_interface_client>(
        ioc.get_executor(), "127.0.0.1", std::to_string(server.port()), "POST", "/", "1.1");

    bool called = false;
    client->async_request("FAKE.subscribe", { "a" },
        [&called](const boost::system::error_code&, const nlohmann::json&) { called = true; });

    // the resolver finishes on its own thread and queues its handler, so the destructor cancels after that
    std::this_thread::sleep_for(200ms);
    client.reset();

    ioc.run_for(5s);
    EXPECT_FALSE(called);
    EXPECT_TRUE(server.arrivals().empty()) << "the request was sent after the client was destroyed";
}

TEST(HttpCommandInterface, IntegerPortReachesTheServer)
{
    fake_jsonrpc_http_server server;
    fake_http_peer peer{server.port()};
    subscribing_client client{peer.url()};

    EXPECT_TRUE(client.run_until([&] { return !server.arrivals().empty(); }, 5s));
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" });
}

TEST(HttpCommandInterface, CloseDoesNotWaitForAStalledRequest)
{
    fake_jsonrpc_http_server server;
    server.script("a", { action::stall });
    fake_http_peer peer{server.port()};
    subscribing_client client{peer.url()};

    ASSERT_TRUE(client.run_until([&] { return !server.arrivals().empty(); }, 5s));

    client.connection->close();

    // the close cancels the stalled request: it is not resent once its timeout passes, and no work is left
    client.ioc.run_for(10s);
    EXPECT_TRUE(client.ioc.stopped()) << "work was left behind after the close";
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" });
}

TEST(HttpCommandInterface, PeerCloseEndsAStalledRequest)
{
    fake_jsonrpc_http_server server;
    server.script("a", { action::stall });
    fake_http_peer peer{server.port()};
    subscribing_client client{peer.url()};

    ASSERT_TRUE(client.run_until([&] { return !server.arrivals().empty(); }, 5s));

    peer.close();

    // the connection cancels the stalled request once it sees the close: no resend, and no work is left
    client.ioc.run_for(10s);
    EXPECT_TRUE(client.ioc.stopped()) << "work was left behind after the close";
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" });
}

// A closing connection reports each subscribed signal unsubscribed, and a client that then releases its
// subscription, as the LT client module does, makes an unsubscribe request while the connection closes.
// The connection cancels its requests after that, so the request does not go out to the peer that is gone.
TEST(HttpCommandInterface, UnsubscribeMadeWhileThePeerClosesIsNotSent)
{
    fake_jsonrpc_http_server server;
    fake_http_peer peer{server.port()};
    subscribing_client client{peer.url()};

    ASSERT_TRUE(client.run_until([&] { return !server.arrivals().empty(); }, 5s));
    peer.acknowledge_subscribe();
    ASSERT_TRUE(client.run_until([&] { return client.subscribed; }, 5s));

    peer.close();

    client.ioc.run_for(10s);
    EXPECT_TRUE(client.ioc.stopped()) << "work was left behind after the close";
    EXPECT_EQ(server.arrivals(), std::vector<std::string>{ "a" }) << "the unsubscribe made during the close was sent";
}
