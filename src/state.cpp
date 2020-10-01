/*!
 * dockerpack.
 * state.cpp
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */

#include "state.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <toolbox/io.h>
#include <toolbox/strings.hpp>

namespace fs = boost::filesystem;

dockerpack::state::state(std::string save_path)
    : save_path(std::move(save_path)),
      last_build_time((uint64_t) time(nullptr)) {
}
void dockerpack::state::load() {
    if (!exists() || !m_enable) {
        return;
    }
    nlohmann::json j;
    std::ifstream is(save_path, std::ios::in);
    if (!is.is_open()) {
        std::cerr << "Can't open file " << save_path << "\n";
        return;
    }

    is >> j;

    success_jobs = j.at("success_jobs").get<sjob_t>();
    success_steps = j.at("success_steps").get<sstep_t>();
    success_build_steps = j.at("success_build_steps").get<sstep_t>();
}
bool dockerpack::state::exists() {
    return fs::exists(save_path);
}
void dockerpack::state::remove() {
    if (exists()) {
        fs::remove(save_path);
    }
}
void dockerpack::state::enable(bool enable) {
    m_enable = enable;
}
void dockerpack::state::save() {
    if (!m_enable)
        return;
    nlohmann::json j;
    j["last_build_time"] = last_build_time;
    j["success_jobs"] = success_jobs;
    j["success_steps"] = success_steps;
    j["success_build_steps"] = success_build_steps;

    const auto res = j.dump();
    toolbox::io::file_write_string(save_path, res);
}
bool dockerpack::state::has_success_step(const std::shared_ptr<dockerpack::job>& job, const std::shared_ptr<dockerpack::step>& step) {
    if (!m_enable)
        return false;
    if (!success_steps.count(job->job_name())) {
        return false;
    }

    return std::any_of(success_steps[job->job_name()].begin(), success_steps[job->job_name()].end(), [step](const std::string& j) {
        return toolbox::strings::equals_icase(step->hash(), j);
    });
}
bool dockerpack::state::has_success_build_step(const dockerpack::imb_ptr_t& job, const std::shared_ptr<dockerpack::step>& step) {
    if (!m_enable)
        return false;
    if (!success_build_steps.count(job->job_name())) {
        return false;
    }

    return std::any_of(success_build_steps[job->job_name()].begin(), success_build_steps[job->job_name()].end(), [step](const std::string& j) {
        return toolbox::strings::equals_icase(step->hash(), j);
    });
}
bool dockerpack::state::has_success_job(const std::shared_ptr<dockerpack::job>& job) {
    if (!m_enable)
        return false;
    return std::any_of(success_jobs.begin(), success_jobs.end(), [job](const std::string& j) {
        return toolbox::strings::equals_icase(job->job_name(), j);
    });
}
void dockerpack::state::add_success_job(const std::shared_ptr<dockerpack::job>& job) {
    if (!m_enable)
        return;
    if (!has_success_job(job)) {
        success_jobs.push_back(job->job_name());
    }
}
void dockerpack::state::add_success_step(const std::shared_ptr<dockerpack::job>& job, const std::shared_ptr<dockerpack::step>& step) {
    if (!m_enable)
        return;
    if (!success_steps.count(job->job_name())) {
        success_steps[job->job_name()] = std::vector<std::string>();
    }

    success_steps[job->job_name()].push_back(step->hash());
}
void dockerpack::state::add_success_build_step(const std::shared_ptr<dockerpack::image_to_build>& job, const std::shared_ptr<dockerpack::step>& step) {
    if (!m_enable)
        return;
    if (!success_build_steps.count(job->job_name())) {
        success_build_steps[job->job_name()] = std::vector<std::string>();
    }

    success_build_steps[job->job_name()].push_back(step->hash());
}
