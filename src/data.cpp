/*!
 * dockerpack.
 * data.cpp
 *
 * \date 09/26/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */

#include "data.h"

#include <sodium/crypto_hash_sha256.h>
#include <toolbox/data/bytes_data.h>
#include <toolbox/strings.hpp>
std::string dockerpack::step::hash() const {
    toolbox::data::bytes_data out(crypto_hash_sha256_BYTES);
    const toolbox::data::bytes_data tmp = toolbox::data::bytes_data::from_string_raw(name + command);
    crypto_hash_sha256(&out[0], &tmp[0], tmp.size());
    return out.to_hex();
}

std::string dockerpack::job::job_name() const {
    return name + "_dockerpack";
}

void dockerpack::job::add_envs(const dockerpack::env_map& ext_envs) {
    for (const auto& kv : ext_envs) {
        envs[kv.first] = kv.second;
    }
}

std::string dockerpack::image_to_build::full_name() const {
    if (!repo.empty()) {
        return repo + "/" + name;
    }
    return name;
}
