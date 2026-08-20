#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/endian/conversion.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/json_rpc_exception.hpp>
#include <ws-streaming/metadata.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/detail/command_interface_client_factory.hpp>
#include <ws-streaming/detail/in_band_command_interface_client.hpp>
#include <ws-streaming/detail/linear_table.hpp>
#include <ws-streaming/detail/peer.hpp>
#include <ws-streaming/detail/registered_local_signal.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/semver.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

#if WS_STREAMING_ENABLE_TLS
#include <ws-streaming/detail/peer_tls.hpp>
#endif

using namespace std::placeholders;

wss::connection::connection(
        boost::asio::ip::tcp::socket&& socket,
        bool is_client,
        std::string local_stream_id,
        bool use_tcp_protocol)
    : _is_client{is_client}
    , _peer{std::make_shared<detail::peer>(std::move(socket), is_client, use_tcp_protocol)}
    , _executor{_peer->socket().get_executor()}
    , _local_stream_id{local_stream_id}
{
    _command_interfaces["jsonrpc"] = { { "httpMethod", "" } };
}

#if WS_STREAMING_ENABLE_TLS

wss::connection::connection(
        boost::asio::ssl::stream<boost::beast::tcp_stream>&& stream,
        bool is_client,
        std::string local_stream_id,
        bool use_tcp_protocol)
    : _is_client{is_client}
    , _peer{std::make_shared<detail::peer_tls>(std::move(stream), is_client, use_tcp_protocol)}
    , _executor{_peer->socket().get_executor()}
    , _local_stream_id{local_stream_id}
{
    _command_interfaces["jsonrpc"] = { { "httpMethod", "" } };
}

#endif

wss::connection::~connection()
{
    _peer->stop();
}

void wss::connection::register_external_command_interface(
    const std::string& id,
    const nlohmann::json& params)
{
    _command_interfaces[id] = params;
}

void wss::connection::run()
{
    auto self_weak = weak_from_this();

    _on_peer_data_received = _peer->on_data_received.connect(
        [self_weak](unsigned signo, const std::uint8_t* data, std::size_t size) {
            if (auto self = self_weak.lock())
                self->on_peer_data_received(signo, data, size);
        });

    _on_peer_metadata_received = _peer->on_metadata_received.connect(
        [self_weak](unsigned signo, const std::string& method, const nlohmann::json& params) {
            if (auto self = self_weak.lock())
                self->on_peer_metadata_received(signo, method, params);
        });

    _on_peer_closed = _peer->on_closed.connect(
        [self_weak](const boost::system::error_code& ec) {
            if (auto self = self_weak.lock())
                self->on_peer_closed(ec);
        });

    _peer->run();

    if (!_is_client)
        do_hello();
}

void wss::connection::run(const void *data, std::size_t size)
{
    auto self_weak = weak_from_this();

    _on_peer_data_received = _peer->on_data_received.connect(
        [self_weak](unsigned signo, const std::uint8_t* data, std::size_t size)
        {
            if (auto self = self_weak.lock())
                self->on_peer_data_received(signo, data, size);
        });

    _on_peer_metadata_received = _peer->on_metadata_received.connect(
        [self_weak](unsigned signo, const std::string& method, const nlohmann::json& params)
        {
            if (auto self = self_weak.lock())
                self->on_peer_metadata_received(signo, method, params);
        });

    _on_peer_closed = _peer->on_closed.connect(
        [self_weak](const boost::system::error_code& ec)
        {
            if (auto self = self_weak.lock())
                self->on_peer_closed(ec);
        });

    _peer->run(data, size);

    if (!_is_client)
        do_hello();
}

void wss::connection::close()
{
    _peer->stop();
}

void wss::connection::add_local_signal(local_signal& signal)
{
    auto [entry, added] = detail::local_signal_container::add_local_signal(signal);

    if (added)
    {
        auto rule = signal.metadata().rule();
        entry->is_explicit = rule == rule_types::explicit_rule;

        if (rule == rule_types::linear_rule)
            entry->table = std::make_shared<detail::linear_table>(signal.metadata());

        auto table_id = signal.metadata().table_id();
        if (!table_id.empty() && table_id != signal.id())
        {
            auto domain_entry = find_local_signal(table_id);
            if (domain_entry)
            {
                entry->domain_signo = domain_entry->signo;
                entry->domain_table = domain_entry->table;
            }
        }

        if (_hello_sent)
            _peer->send_metadata(0, "available", { { "signalIds", { signal.id() } } });
    }
}

void wss::connection::remove_local_signal(local_signal& signal)
{
    unsigned signo = detail::local_signal_container::remove_local_signal(signal);

    if (signo)
        _peer->send_metadata(0, "unavailable", { { "signalIds", { signal.id() } } });
}

wss::remote_signal_ptr
wss::connection::find_remote_signal(const std::string& id) const
{
    const auto *entry = detail::remote_signal_container::find_remote_signal(id);
    return entry ? entry->signal : nullptr;
}

const boost::asio::any_io_executor& wss::connection::executor() const noexcept
{
    return _peer->socket().get_executor();
}

const boost::asio::ip::tcp::socket& wss::connection::socket() const noexcept
{
    return _peer->socket();
}

const std::string& wss::connection::local_stream_id() const noexcept
{
    return _local_stream_id;
}

nlohmann::json wss::connection::do_command_interface(
    const std::string& method,
    const nlohmann::json& params)
{
    if (method == _local_stream_id + ".subscribe")
        return do_command_interface_subscribe(params);

    else if (method == _local_stream_id + ".unsubscribe")
        return do_command_interface_unsubscribe(params);

    throw json_rpc_exception(
        json_rpc_exception::method_not_found,
        "method not found");
}

void wss::connection::do_hello()
{
    _peer->send_metadata(
        0,
        "apiVersion",
        {
            { "version", "2.0.0" }
        });

    _peer->send_metadata(
        0,
        "init",
        {
            { "streamId", _local_stream_id },
            { "commandInterfaces", _command_interfaces },
        });

    auto signal_ids = nlohmann::json::array();

    for (std::shared_ptr<detail::registered_local_signal> signal : local_signals())
        signal_ids.emplace_back(signal->signal.id());

    if (!signal_ids.empty())
        _peer->send_metadata(
            0,
            "available",
            {
                { "signalIds", signal_ids }
            });

    _hello_sent = true;
}

void wss::connection::on_peer_data_received(
    unsigned signo,
    const std::uint8_t *data,
    std::size_t size)
{
    auto *signal = detail::remote_signal_container::find_remote_signal(signo);
    if (signal)
        signal->signal->handle_data(data, size);
}

void wss::connection::on_peer_metadata_received(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    if (method == "subscribe")
        handle_subscribe(signo, params);
    else if (method == "unsubscribe")
        handle_unsubscribe(signo, params);
    else if (signo)
        dispatch_metadata(signo, method, params);
    else if (method == "apiVersion")
        handle_api_version(params);
    else if (method == "init")
        handle_init(params);
    else if (method == "available")
        handle_available(params);
    else if (method == "unavailable")
        handle_unavailable(params);
    else if (method == "request")
        handle_command_interface_request(params);
    else if (method == "response")
        handle_command_interface_response(params);
}

void wss::connection::on_peer_closed(
    const boost::system::error_code& ec)
{
    _on_peer_data_received.disconnect();
    _on_peer_metadata_received.disconnect();
    _on_peer_closed.disconnect();

    clear_remote_signals(
        [this](remote_signal_ptr signal)
        {
            on_unavailable(signal);
        });

    clear_local_signals();

    on_disconnected(ec);
}

void wss::connection::refresh_local_signal_bindings(
    detail::registered_local_signal& entry)
{
    auto rule = entry.signal.metadata().rule();

    entry.is_explicit = rule == rule_types::explicit_rule;

    if (rule == rule_types::linear_rule)
    {
        if (entry.table)
            entry.table->update(entry.signal.metadata());
        else
            entry.table = std::make_shared<detail::linear_table>(entry.signal.metadata());
    }

    else
        entry.table.reset();

    auto table_id = entry.signal.metadata().table_id();
    entry.domain_signo = 0;
    entry.domain_table.reset();

    if (!table_id.empty() && table_id != entry.signal.id())
    {
        auto domain_entry = find_local_signal(table_id);

        if (domain_entry)
        {
            entry.domain_signo = domain_entry->signo;
            entry.domain_table = domain_entry->table;
        }
    }
}

void wss::connection::on_local_signal_metadata_changed(
    detail::registered_local_signal& entry)
{
    refresh_local_signal_bindings(entry);

    entry.value_index = 0;
    _peer->send_metadata(entry.signo, "signal", entry.signal.metadata().json());
}

void wss::connection::on_local_signal_data_published(
    std::shared_ptr<detail::registered_local_signal> entry,
    std::int64_t domain_value,
    std::size_t sample_count,
    const void *data,
    std::size_t size)
{
    auto domain_table = entry->domain_table.lock();

    if (domain_table)
    {
        std::int64_t index
            = entry->is_explicit
                ? entry->value_index
                : domain_table->driven_index();

        if (domain_value != domain_table->value_at(index))
        {
            domain_table->set(index, domain_value);

            detail::streaming_protocol::linear_payload payload;
            payload.sample_index = boost::endian::native_to_little(index);
            payload.value = boost::endian::native_to_little(domain_value);

            _peer->send_data(
                entry->domain_signo,
                boost::asio::const_buffer{&payload, sizeof(payload)});
        }
    }

    _peer->send_data(
        entry->signo,
        boost::asio::const_buffer{data, size});

    entry->value_index += sample_count;

    if (entry->is_explicit && domain_table)
        domain_table->drive_to(entry->value_index);
}

void wss::connection::on_signal_subscribe_requested(
    const std::string& signal_id)
{
    boost::asio::dispatch(
        _executor,
        [self_weak = weak_from_this(), signal_id]()
        {
            auto self = self_weak.lock();
            if (!self || !self->_command_interface_client)
                return; // @todo XXX TODO

            self->_command_interface_client->async_request(
                self->_remote_stream_id + ".subscribe", { signal_id },
                [](const boost::system::error_code& /*ec*/, const nlohmann::json& /*response*/)
                {
                });
        });
}

void wss::connection::on_signal_unsubscribe_requested(
    const std::string& signal_id)
{
    boost::asio::dispatch(
        _executor,
        [self_weak = weak_from_this(), signal_id]()
        {
            auto self = self_weak.lock();
            if (!self || !self->_command_interface_client)
                return; // @todo XXX TODO

            self->_command_interface_client->async_request(
                self->_remote_stream_id + ".unsubscribe", { signal_id },
                [](const boost::system::error_code& /*ec*/, const nlohmann::json& /*response*/)
                {
                });
        });
}

std::shared_ptr<wss::detail::remote_signal_impl>
wss::connection::on_table_sought(
    const std::string& table_id)
{
    auto *entry = detail::remote_signal_container::find_table(table_id);
    if (entry)
        return entry->signal;

    return nullptr;
}

void wss::connection::dispatch_metadata(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    auto *entry = detail::remote_signal_container::find_remote_signal(signo);

    if (entry)
        entry->signal->handle_metadata(method, params);
}

void wss::connection::handle_api_version(
    const nlohmann::json& params)
{
    if (params.is_object()
            && params.contains("version")
            && params["version"].is_string())
        _api_version = detail::semver::try_parse(params["version"]).value_or(detail::semver());
}

void wss::connection::handle_init(
    const nlohmann::json& params)
{
    if (!params.is_object())
        return;

    if (params.contains("streamId") && params["streamId"].is_string())
        _remote_stream_id = params["streamId"];

    if (params.contains("commandInterfaces"))
        _command_interface_client = detail::command_interface_client_factory::create_client(
            params["commandInterfaces"],
            _peer);

    if (_is_client && _api_version >= detail::semver(2, 0, 0))
        do_hello();
}

void wss::connection::handle_available(
    const nlohmann::json& params)
{
    nlohmann::json array = nullptr;

    if (params.is_object() && params.contains("signalIds") && params["signalIds"].is_array())
        array = params["signalIds"];

    else if (params.is_array())
        array = params;

    if (!array.is_array())
        return;

    for (const auto& id : array)
    {
        if (!id.is_string())
            continue;

        auto [added, signal] = add_remote_signal(id, false);

        // The signal can already be known if the peer sent a subscribe for it
        // before advertising it (see handle_subscribe()), in which case it was recorded as
        // hidden and never announced to the application. Now that the peer advertises it,
        // it is an ordinary signal and must be announced. Its slots are already connected.
        if (!added && !signal.hidden)
            continue;

        if (added)
        {
            signal.on_subscribe_requested = signal.signal->on_subscribe_requested.connect(
                std::bind(&connection::on_signal_subscribe_requested, this, id));

            signal.on_unsubscribe_requested = signal.signal->on_unsubscribe_requested.connect(
                std::bind(&connection::on_signal_unsubscribe_requested, this, id));

            signal.on_table_sought = signal.signal->on_table_sought.connect(
                std::bind(&connection::on_table_sought, this, _1));
        }

        signal.hidden = false;

        on_available(signal.signal);
    }
}

void wss::connection::handle_subscribe(
    unsigned signo,
    const nlohmann::json& params)
{
    std::string signal_id;

    if (params.is_object() && params.contains("signalId") && params["signalId"].is_string())
        signal_id = params["signalId"];
    else if (params.is_array() && params.size() > 0 && params[0].is_string())
        signal_id = params[0];

    if (signal_id.empty())
        return;

    auto [added, signal] = add_remote_signal(signal_id, true);

    if (added)
    {
        signal.on_subscribe_requested = signal.signal->on_subscribe_requested.connect(
            std::bind(&connection::on_signal_subscribe_requested, this, signal_id));

        signal.on_unsubscribe_requested = signal.signal->on_unsubscribe_requested.connect(
            std::bind(&connection::on_signal_unsubscribe_requested, this, signal_id));

        signal.on_table_sought = signal.signal->on_table_sought.connect(
            std::bind(&connection::on_table_sought, this, _1));
    }

    set_remote_signal_signo(&signal, signo);
    signal.signal->handle_metadata("subscribe", params);
}

void wss::connection::handle_unsubscribe(
    unsigned signo,
    const nlohmann::json& params)
{
    dispatch_metadata(signo, "unsubscribe", params);
    forget_remote_signo(signo);
}

void wss::connection::handle_unavailable(
    const nlohmann::json& params)
{
    nlohmann::json array = nullptr;

    if (params.is_object() && params.contains("signalIds") && params["signalIds"].is_array())
        array = params["signalIds"];

    else if (params.is_array())
        array = params;

    if (!array.is_array())
        return;

    for (const auto& id : array)
    {
        if (!id.is_string())
            continue;

        // Hidden signals were never announced to the application
        // they must not be withdrawn from it
        const auto *entry = detail::remote_signal_container::find_remote_signal(
            static_cast<std::string>(id));
        bool hidden = entry && entry->hidden;

        auto signal = remove_remote_signal(id);
        if (signal)
        {
            signal->detach();
            if (!hidden)
                on_unavailable(signal);
        }
    }
}

void wss::connection::handle_command_interface_request(const nlohmann::json& params)
{
    nlohmann::json result;
    nlohmann::json error;

    try
    {
        if (!params.is_object()
                || !params.contains("method")
                || !params["method"].is_string())
            throw json_rpc_exception(
                json_rpc_exception::invalid_request,
                "invalid request object");

        std::string method = params["method"];
        result = do_command_interface(method, params.value<nlohmann::json>("params", nullptr));
    }

    catch (const json_rpc_exception& ex)
    {
        error = ex.json();
    }

    nlohmann::json response_params
    {
        { "jsonrpc", "2.0" },
        { "id", params.value<nlohmann::json>("id", nullptr) }
    };

    if (!error.is_null())
        response_params["error"] = error;
    else response_params["result"] = result;

    _peer->send_metadata(
        0,
        "response",
        response_params);
}

nlohmann::json wss::connection::do_command_interface_subscribe(const nlohmann::json& params)
{
    if (params.is_string())
    {
        if (!subscribe(params, true))
            throw json_rpc_exception(
                json_rpc_exception::server_error,
                "failed to subscribe signal");

        return true;
    }

    else if (params.is_array())
    {
        auto results = nlohmann::json::array();

        for (const auto& signal_id : params)
            results.push_back(signal_id.is_string() && subscribe(signal_id, true));

        return results;
    }

    else
        throw json_rpc_exception(
            json_rpc_exception::invalid_params,
            "params must be a signal ID or an array of signal IDs");
}

nlohmann::json wss::connection::do_command_interface_unsubscribe(const nlohmann::json& params)
{
    if (params.is_string())
    {
        if (!unsubscribe(params, true))
            throw json_rpc_exception(
                json_rpc_exception::server_error,
                "failed to unsubscribe signal");

        return true;
    }

    else if (params.is_array())
    {
        auto results = nlohmann::json::array();

        for (const auto& signal_id : params)
            results.push_back(signal_id.is_string() && unsubscribe(signal_id, true));

        return results;
    }

    else
        throw json_rpc_exception(
            json_rpc_exception::invalid_params,
            "params must be a signal ID or an array of signal IDs");
}

void wss::connection::handle_command_interface_response(const nlohmann::json& params)
{
    if (auto *client = dynamic_cast<detail::in_band_command_interface_client *>(_command_interface_client.get()); client)
        client->handle_response(params);
}

bool wss::connection::subscribe(
    const std::string& signal_id,
    bool is_explicit)
{
    auto signal = find_local_signal(signal_id);
    if (!signal)
        return false;

    bool was_subscribed = signal->is_explicitly_subscribed || signal->implicit_subscribe_count > 0;

    if (is_explicit)
        signal->is_explicitly_subscribed = true;
    else ++signal->implicit_subscribe_count;

    if (was_subscribed)
        return false;

    signal->holder = signal->signal.increment_subscribe_count();

    if (is_explicit)
    {
        auto table_id = signal->signal.metadata().table_id();
        if (!table_id.empty() && table_id != signal_id)
            subscribe(table_id, false);
    }

    _peer->send_metadata(
        signal->signo,
        "subscribe",
        {
            { "signalId", signal->signal.id() }
        });

    // The signal's metadata can have changed while nobody was subscribed, in which
    // case no metadata-changed notification arrived to update the derived state.
    refresh_local_signal_bindings(*signal);

    nlohmann::json metadata = signal->signal.metadata().json();
    metadata["valueIndex"] = signal->value_index;

    _peer->send_metadata(
        signal->signo,
        "signal",
        metadata);

    auto self_weak = weak_from_this();
    auto signal_weak = std::weak_ptr<detail::registered_local_signal>(signal);

    signal->on_data_published = signal->signal.on_data_published.connect(
        [self_weak, signal_weak](std::int64_t domain_value,
                                 std::size_t sample_count,
                                 const void* data,
                                 std::size_t size) {
            if (auto self = self_weak.lock())
                if (auto s = signal_weak.lock())
                    self->on_local_signal_data_published(s, domain_value, sample_count, data, size);
        });

    signal->on_metadata_changed = signal->signal.on_metadata_changed.connect(
        [self_weak, signal_weak]() {
            if (auto self = self_weak.lock())
                if (auto s = signal_weak.lock())
                    self->on_local_signal_metadata_changed(*s);
        });

    return true;
}

bool wss::connection::unsubscribe(
    const std::string& signal_id,
    bool is_explicit)
{
    auto signal = find_local_signal(signal_id);
    if (!signal)
        return false;

    if (is_explicit)
        signal->is_explicitly_subscribed = false;
    else --signal->implicit_subscribe_count;

    if (signal->is_explicitly_subscribed || signal->implicit_subscribe_count > 0)
        return false;

    signal->on_data_published.disconnect();
    signal->on_metadata_changed.disconnect();
    signal->holder.close();

    _peer->send_metadata(
        signal->signo,
        "unsubscribe",
        {
            { "signalId", signal->signal.id() }
        });

    auto table_id = signal->signal.metadata().table_id();
    if (!table_id.empty() && table_id != signal_id)
        unsubscribe(table_id, false);

    return true;
}
