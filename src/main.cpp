// #include "nvim.hpp"
#include <string>
import socket;


int main() {
    std::string test = "aaa";
    test.clear();
    nvim::Socket s{};

    s.connect_tcp("localhost", "666", 999);
    char buf[] = "test";
    s.write(buf, sizeof(buf) - 1, 999);
    s.read(buf, 4, 999);
    
    // nvim::Nvim nvim;
    // nvim.connect_tcp("localhost", "6666");
    // nvim.nvim_eval("( 3 + 2 ) * 4");
    // std::cout << "get_current_line = " << nvim.nvim_get_current_line() << std::endl;
    // nvim.vim_set_current_line("testhogefuga");


    return 0;
}
