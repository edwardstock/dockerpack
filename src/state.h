/*!
 * dockerpack.
 * state.h
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_STATE_H
#define DOCKERPACK_STATE_H

#include "config.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dockerpack {

using sstep_t = std::unordered_map<std::string, std::vector<std::string>>;
using sjob_t = std::vector<std::string>;

class state {
public:
    explicit state(std::string save_path);

    void load();
    void save();
    bool exists();
    void remove();
    void enable(bool enable);

    bool has_success_step(const std::shared_ptr<dockerpack::job>& job, const std::shared_ptr<dockerpack::step>& step);
    bool has_success_build_step(const imb_ptr_t& job, const std::shared_ptr<dockerpack::step>& step);
    bool has_success_job(const std::shared_ptr<dockerpack::job>& job);

    void add_success_job(const std::shared_ptr<dockerpack::job>& job);
    void add_success_step(const std::shared_ptr<dockerpack::job>& job, const std::shared_ptr<dockerpack::step>& step);
    void add_success_build_step(const std::shared_ptr<dockerpack::image_to_build>& job, const std::shared_ptr<dockerpack::step>& step);

private:
    std::string save_path;
    uint64_t last_build_time;
    sjob_t success_jobs;
    sstep_t success_steps;
    sstep_t success_build_steps;
    bool m_enable = true;
};

} // namespace dockerpack

#endif //DOCKERPACK_STATE_H
