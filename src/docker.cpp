/*!
 * dockerpack.
 * docker.cpp
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */

#include "docker.h"

#include "utils.h"

#include <boost/process.hpp>
#include <termcolor/termcolor.hpp>
#include <toolbox/strings.hpp>

namespace style = termcolor;

bool dockerpack::docker::check_docker_exists() {
    namespace bp = boost::process;
    auto path = bp::search_path("docker");
    return !path.empty();
}

dockerpack::docker::docker(std::shared_ptr<dockerpack::config> config)
    : m_config(std::move(config)) {
}

void dockerpack::docker::copy(const dockerpack::job_ptr_t& job, const std::string& path) {
    if (!has_running_job(job)) {
        throw std::runtime_error("Job " + job->job_name() + " is not run yet");
    }

    std::string p = path;
    toolbox::strings::replace("$image", job->job_name(), p);
    dockerpack::utils::normalize_path(p);

    dockerpack::execmd cmd("docker cp " + p);
    int status = 0;
    const auto res = cmd.run(&status);
    if (status) {
        throw std::runtime_error(res);
    }
}
void dockerpack::docker::restore_from_ps() {
    dockerpack::execmd cmd("docker ps -a --format \"{{.ID}}|{{.Names}}\"");
    int status = 0;
    std::string res = cmd.run(&status);
    if (status) {
        throw std::runtime_error(res);
    }
    if (res.empty()) {
        return;
    }
    std::vector<std::string> lines = toolbox::strings::split(res, "\n");
    for (const auto& line : lines) {
        std::vector<std::string> items = toolbox::strings::split(line, "|");
        if (items.size() != 2) {
            throw std::runtime_error("Undefined \"docker ps\" result: " + res);
        }
        m_run_jobs[items[1]] = items[0];
    }
}
void dockerpack::docker::run(const dockerpack::job_ptr_t& job) {
    if (has_running_job(job)) {
        return;
    }

    std::stringstream env_builder;
    for (const auto& entry : job->envs) {
        env_builder << "-e " << entry.first << "=" << entry.second << " ";
    }

    std::stringstream cmd_builder;
    cmd_builder << "docker run " << env_builder.str();
    cmd_builder << "-d -it --name ";
    cmd_builder << job->job_name() << " " << job->image << " ";
    cmd_builder << "/bin/bash";

    if (m_config->debug) {
        std::cout << "[debug] run: " << style::green << cmd_builder.str() << style::reset << std::endl;
    }

    int status = 0;
    dockerpack::execmd cmd(cmd_builder.str());
    const std::string res = cmd.run(&status);
    if (status) {
        throw std::runtime_error(res);
    }
    const std::string image_id = toolbox::strings::substr_replace_all_ret({"\n", "\t", "\r"}, {"", "", ""}, res);
    m_run_jobs[job->job_name()] = image_id;
}

void dockerpack::docker::exec(const dockerpack::job_ptr_t& job, const dockerpack::step_ptr_t& step) {
    if (!has_running_job(job)) {
        throw std::runtime_error("Image " + job->job_name() + " is not run");
    }

    std::stringstream cmd_builder;
    cmd_builder << "docker exec ";
    if (!step->workdir.empty()) {
        cmd_builder << "-w " << step->workdir << " ";
    } else if (!m_config->workdir.empty()) {
        cmd_builder << "-w " << m_config->workdir << " ";
    }

    if (!step->envs.empty()) {
        for (const auto& kv : step->envs) {
            cmd_builder << "-e " << kv.first << "=" << kv.second << " ";
        }
    }
    if (!job->envs.empty()) {
        for (const auto& kv : job->envs) {
            cmd_builder << "-e " << kv.first << "=" << kv.second << " ";
        }
    }
    cmd_builder << job->job_name() << " ";
    cmd_builder << "bash -c \"";
    cmd_builder << step->command;
    cmd_builder << "\"";

    if (m_config->debug) {
        std::cout << "[debug] exec: " << style::green << cmd_builder.str() << style::reset << std::endl;
    }

    dockerpack::exec_stream cmd(cmd_builder.str());
    cmd.run(m_config->commands_verbose);
    int status = cmd.wait();

    if (step->skip_on_error) {
        // ignore status checking
        return;
    }

    if (status) {
        const auto ec = cmd.error_code();
        if (ec) {
            throw std::runtime_error(ec.message());
        } else {
            throw std::runtime_error("Unknown error. Exit code: " + std::to_string(status));
        }
    }
}
void dockerpack::docker::stop(const dockerpack::job_ptr_t& job) {
    stop(job->job_name());
}

void dockerpack::docker::stop(const std::string& job_name) {
    if (!has_running_job(job_name)) {
        return;
    }

    int status = 0;

    if (m_config->debug) {
        std::cout << "[debug] stop: " << style::green << "docker stop " << job_name << style::reset << std::endl;
    }

    dockerpack::execmd cmd("docker stop " + job_name);
    const std::string res = cmd.run(&status);
    if (status) {
        throw std::runtime_error(res);
    }
}

void dockerpack::docker::rm(const dockerpack::job_ptr_t& job) {
    rm(job->job_name());
}

void dockerpack::docker::rm(const std::string& job_name) {
    if (!has_running_job(job_name)) {
        return;
    }
    if (m_config->debug) {
        std::cout << "[debug] rm: " << style::green << "docker rm " << job_name << style::reset << std::endl;
    }
    dockerpack::execmd cmd("docker rm " + job_name);
    int status = 0;
    cmd.run(&status);

    m_run_jobs.erase(job_name);
}

bool dockerpack::docker::has_image(const std::string& repo, const std::string& tag) const {
    const auto list = images();
    return std::any_of(list.begin(), list.end(), [repo, tag](const docker_image& image) {
        return image.repo == repo && image.tag == tag;
    });
}

std::vector<dockerpack::docker_image> dockerpack::docker::images() const {
    dockerpack::execmd cmd("docker images --format {{.Repository}}:{{.Tag}}");
    int status = 0;
    const std::string result = cmd.run(&status);
    if (status || result.empty()) {
        return std::vector<dockerpack::docker_image>(0);
    }

    std::vector<std::string> lines = toolbox::strings::split(result, "\n");
    std::vector<dockerpack::docker_image> out;
    out.reserve(lines.size());
    for (const std::string& line : lines) {
        std::vector<std::string> parts = toolbox::strings::split(line, ":");
        out.push_back(dockerpack::docker_image{
            parts[0],
            parts[1]});
    }
    return out;
}

void dockerpack::docker::commit(const dockerpack::imb_ptr_t& image) {
    std::stringstream ss;
    ss << "docker commit ";
    ss << image->job_name() << " ";
    ss << image->full_name() << ":" << image->tag;
    dockerpack::execmd cmd(ss.str());
    int status = 0;
    const std::string result = cmd.run(&status);
}

std::vector<std::string> dockerpack::docker::filter_running_job(const std::string& name_filter) {
    restore_from_ps();
    std::vector<std::string> out;
    for (const auto& kv : m_run_jobs) {
        if (toolbox::strings::has_substring(name_filter, kv.first) || toolbox::strings::has_substring(name_filter, kv.second)) {
            out.push_back(kv.first);
        }
    }
    return out;
}

bool dockerpack::docker::has_running_job(const std::string& job_name) {
    restore_from_ps();
    return m_run_jobs.count(job_name);
}

bool dockerpack::docker::has_running_job(const dockerpack::job_ptr_t& job) {
    restore_from_ps();
    return m_run_jobs.count(job->job_name());
}
