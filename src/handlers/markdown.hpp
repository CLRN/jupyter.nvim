#include "nvim.hpp"
#include "nvim_graphics.hpp"

#include <boost/cobalt/promise.hpp>

namespace jupyter {

auto handle_markdown(nvim::Api& api, nvim::Graphics& remote, int augroup) -> boost::cobalt::promise<void>;

} // namespace jupyter
