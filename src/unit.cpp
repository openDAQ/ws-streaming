#include <string>

#include <ws-streaming/quantities.hpp>
#include <ws-streaming/unit.hpp>

wss::unit wss::unit::seconds    {-1,    "seconds",      quantities::time,       "s"};
wss::unit wss::unit::volts      {-1,    "volts",        quantities::voltage,    "V"};

wss::unit::unit(
        int id,
        const std::string& name,
        const std::string& quantity,
        const std::string& symbol)
    : _id(id)
    , _name(name)
    , _quantity(quantity)
    , _symbol(symbol)
{
}

int wss::unit::id() const noexcept
{
    return _id;
}

const std::string& wss::unit::name() const noexcept
{
    return _name;
}

const std::string& wss::unit::quantity() const noexcept
{
    return _quantity;
}

const std::string& wss::unit::symbol() const noexcept
{
    return _symbol;
}
