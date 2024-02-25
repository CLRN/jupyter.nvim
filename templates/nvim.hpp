// Auto generated

#pragma once

#include "rpc.hpp"

namespace nvim {

class Nvim {
public:
    Nvim(std::string host, std::uint16_t port) : client_{rpc::Socket{std::move(host), port}} {}
{% for func in functions %}
    auto {{func.name}} ({% for arg in func.args %}{{arg.type}} {{arg.name}}{% if not loop.last %}, {% endif %}{% endfor %}) -> boost::cobalt::promise<{{func.return}}> { 
        {% if func.return != "void" %}
        co_return (co_await client_.call("{{func.name}}"{% for arg in func.args %}, {{arg.name}}{% endfor %})).{{func.convert}}();
        {% else %}
        co_await client_.call("{{func.name}}"{% for arg in func.args %}, {{arg.name}}{% endfor %}); 
        {% endif %}
    }
{% endfor %}

private:
    rpc::Client client_;

};

} //namespace nvim

