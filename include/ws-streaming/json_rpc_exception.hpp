#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace wss
{
    class json_rpc_exception : public std::runtime_error
    {
        public:

            static constexpr int parse_error = -32700;
            static constexpr int invalid_request = -32600;
            static constexpr int method_not_found = -32601;
            static constexpr int invalid_params = -32602;
            static constexpr int internal_error = -32603;
            static constexpr int server_error = -32000;

        public:

            json_rpc_exception(
                    int code,
                    const std::string& message,
                    const nlohmann::json& data = nullptr)
                : std::runtime_error("JSON-RPC error " + std::to_string(code) + ": " + message)
                , _code(code)
                , _message(message)
                , _data(data)
            {
            }

            int code() const noexcept
            {
                return _code;
            }

            const std::string& message() const noexcept
            {
                return _message;
            }

            const nlohmann::json& data() const noexcept
            {
                return _data;
            }

            nlohmann::json json() const
            {
                nlohmann::json result =
                {
                    { "code", code() },
                    { "message", message() },
                };

                if (!data().is_null())
                    result["data"] = data();

                return result;
            }

        private:

            int _code;
            std::string _message;
            nlohmann::json _data;
    };
}