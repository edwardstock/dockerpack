/*!
 * dockerpack.
 * docker.h
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_DOCKER_H
#define DOCKERPACK_DOCKER_H

#include "config.h"
#include "data.h"
#include "execmd.h"

#include <unordered_map>

namespace dockerpack {

class docker {
public:
    static bool check_docker_exists();

    explicit docker(std::shared_ptr<dockerpack::config> config);

    void copy(const job_ptr_t& job, const std::string& path);
    void restore_from_ps();
    void run(const job_ptr_t& runner);
    void exec(const job_ptr_t& job, const step_ptr_t& step);
    void stop(const job_ptr_t& job);
    void stop(const std::string& job_name);
    void rm(const job_ptr_t& job);
    void rm(const std::string& job);
    std::vector<docker_image> images() const;
    bool has_image(const std::string& repo, const std::string& tag) const;
    void commit(const imb_ptr_t& image);
    bool has_running_job(const job_ptr_t& job);
    bool has_running_job(const std::string& job_name);
    std::vector<std::string> filter_running_job(const std::string& name_filter);

private:
    std::shared_ptr<dockerpack::config> m_config;
    std::unordered_map<std::string, std::string> m_run_jobs;
};

} // namespace dockerpack

#endif //DOCKERPACK_DOCKER_H
