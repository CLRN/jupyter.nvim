#include "api.hpp"
#include "graphics.hpp"

#include <boost/cobalt/promise.hpp>

namespace jupyter {

auto handle_images(nvim::Api& api, nvim::Graphics& remote, int augroup) -> boost::cobalt::promise<void>;

} // namespace jupyter
