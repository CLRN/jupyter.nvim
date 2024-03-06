#include "nvim.hpp"
#include "nvim_graphics.hpp"

#include <boost/cobalt/promise.hpp>

namespace jupyter {

auto handle_images(nvim::Api& api, nvim::RemoteGraphics& remote, int augroup) -> boost::cobalt::promise<void>;

} // namespace jupyter
