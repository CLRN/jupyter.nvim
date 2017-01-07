#ifndef NEOVIM_CPP__NVIM_RPC_HPP_
#define NEOVIM_CPP__NVIM_RPC_HPP__

#include "msgpack.hpp"
#include <string>
#include <iostream>
#include <sstream>

#include "socket.hpp"

namespace nvim {

using Integer = int64_t;
using Window = Integer;
using Buffer = Integer;
using Tabpage = Integer;
using Object = msgpack::type::variant;

class NvimRPC {
        
    enum {
        REQUEST  = 0,
        RESPONSE = 1,
        NOTIFY   = 2
    };
    
public:
    NvimRPC() : 
        msgid_(0) {
        socket_.connect("127.0.0.1", "6666", 1.0);
    }
    
    template<typename T, typename...U>
    void send(const std::string &method, T& res, const U&...u);
    
    template<typename...U>
    void send(const std::string &method, Integer& res, const U&...u);

    template<typename...U>
    void send(const std::string &method, Object& res, const U&...u);
    
    template<typename...U>
    void send(const std::string &method, nullptr_t res, const U&...u);
    
private:
    template<typename...U>
    Object do_send(const std::string &method, const U&...u);
    
    uint64_t msgid_;
    Socket socket_;

};

} //namespace nvim

#include "impl/nvim_rpc.hpp"

#endif //NEOVIM_CPP__NVIM_RPC_HPP_

