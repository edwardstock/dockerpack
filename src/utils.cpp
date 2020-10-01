/*!
 * dockerpack.
 * utils.cpp
 *
 * \date 09/28/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#include "utils.h"

#include <boost/filesystem.hpp>
#include <toolbox/strings.hpp>

void dockerpack::utils::normalize_path(std::string& path) {
    toolbox::strings::trim_ref(path);
    if (toolbox::strings::has_substring("~", path)) {
        const char* homepath = getenv("HOME");
        const std::string h(homepath);
        toolbox::strings::replace("~", h, path);
    }
}
