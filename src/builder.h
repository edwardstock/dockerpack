/*!
 * dockerpack.
 * dockerpack.h
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_BUILDER_H
#define DOCKERPACK_BUILDER_H

#include "config.h"
#include "docker.h"
#include "state.h"

#include <memory>
#include <string>
#include <vector>

namespace dockerpack {

const extern std::string STATE_FILE;

struct build_options {
    std::string filter_name;
    bool reset_lock = false;
    bool stateless = false;
    bool no_cleanup = false;
};

class builder {
public:
    builder(const std::string& config_path, const std::string& pwd, build_options&& opts);

    void init();
    bool build_all();
    bool build_images();
    bool build_jobs();
    void print_jobs();
    bool cleanup();

private:
    config_ptr_t m_config;
    dockerpack::docker m_docker;
    dockerpack::state m_state;
    dockerpack::build_options m_options;
};
} // namespace dockerpack

#endif //DOCKERPACK_BUILDER_H
