#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace wss::detail
{
    /**
     * Accesses a sub-value inside a JSON value, returning a default value if the specified
     * sub-value does not exist or its own type is incompatible.
     *
     * @tparam Value The type of the value to return.
     *
     * @param json The JSON value to consider.
     * @param json_ptr The JSON Pointer path to the desired sub-value.
     * @param fallback The fallback value to return if the access is not successful.
     *
     * @return The converted value of the sub-value, or @p fallback if not successful.
     */
    template <typename Value>
    Value json_ptr(const nlohmann::json& json, const std::string& json_ptr, const Value& fallback)
    {
        try
        {
            return json.value<nlohmann::json>(
                    nlohmann::json::json_pointer(json_ptr),
                    nullptr)
                .get<Value>();
        }

        catch (const nlohmann::json::type_error&)
        {
            return fallback;
        }
    }
}
