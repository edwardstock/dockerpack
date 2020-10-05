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
#include <toolbox/strings/regex.h>

namespace style = termcolor;

static std::string exec_internal(const std::string& job_name, const std::string& bash_command) {
    std::stringstream cmd_builder;
    cmd_builder << "docker exec ";
    cmd_builder << job_name << " ";
    cmd_builder << "bash -c \"";
    cmd_builder << bash_command;
    cmd_builder << "\"";

    int status = 0;
    dockerpack::execmd cmd(cmd_builder.str());
    std::string res = cmd.run(&status);
    if (status) {
        throw std::runtime_error(res);
    }

    return res;
}

void dockerpack::docker::ensure_workdir(const dockerpack::job_ptr_t& job, const std::string& workdir) {
    if (workdir.empty()) {
        std::cerr << "[debug] can't make cwd: workdir is empty" << std::endl;
        return;
    } else if (workdir == "/") {
        return;
    }
    std::stringstream cmd_builder;
    cmd_builder << "docker exec ";
    cmd_builder << job->job_name() << " ";

    if (!job->envs.empty()) {
        for (const auto& kv : job->envs) {
            cmd_builder << "-e " << kv.first << "=" << kv.second << " ";
        }
    }

    cmd_builder << "bash -c \"";
    cmd_builder << "mkdir -p " << workdir;
    cmd_builder << "\"";

    if (m_config->debug) {
        std::cout << "[debug] make cwd: " << style::green << cmd_builder.str() << style::reset << std::endl;
    }

    dockerpack::execmd cmd(cmd_builder.str());
    int status = 0;
    const std::string result = cmd.run(&status);
}

bool dockerpack::docker::check_docker_exists() {
    namespace bp = boost::process;
    auto path = bp::search_path("docker");
    return !path.empty();
}

dockerpack::docker::docker(std::shared_ptr<dockerpack::config> config)
    : m_config(std::move(config)) {
}

void dockerpack::docker::normalize_remote_path(std::string& path) const {
    for (const auto& env_value : image_envs) {
        if (toolbox::strings::has_substring(env_value.first, path)) {
            toolbox::strings::replace(env_value.first, env_value.second, path);
        }
    }

    if (toolbox::strings::has_substring("~", path) && image_envs.count("HOME")) {
        toolbox::strings::replace("~", image_envs.at("HOME"), path);
    }

    if (m_config->debug) {
        std::cout << "[debug] Normalize path: " << path << std::endl;
    }
}

void dockerpack::docker::normalize_local_path(std::string& path) const {
    const std::string env_pattern = R"(\$[A-Z0-9_]+)";
    if (!toolbox::strings::matches_pattern(env_pattern, path)) {
        return;
    }

    auto all_find = toolbox::strings::find_all_pattern(env_pattern, path);
    assert(all_find.size() != 0);

    for (const auto& res : all_find) {
        if (!res.empty()) {
            auto name = res[0];
            toolbox::strings::replace("$", "", name);
            const char* value = getenv(name.c_str());
            toolbox::strings::replace(res[0], value == nullptr ? "" : std::string(value), path);
        }
    }

    if (toolbox::strings::has_substring("~", path)) {
        const char* hp = getenv("HOME");
        const std::string homepath = std::string(hp);
        toolbox::strings::replace("~", homepath, path);
    }

    if (m_config->debug) {
        std::cout << "[debug] normalize path: " << path << std::endl;
    }
}

void dockerpack::docker::copy(const dockerpack::job_ptr_t& job, const std::string& path) {
    if (!has_running_job(job)) {
        throw std::runtime_error("Job " + job->job_name() + " is not run yet");
    }
    std::string p = path;
    auto path_segments = toolbox::strings::split_pair(p, " ");
    toolbox::strings::trim_ref(path_segments.first);
    if (path_segments.second.empty()) {
        path_segments.second = path_segments.first;
    }
    toolbox::strings::trim_ref(path_segments.second);
    normalize_local_path(path_segments.first);
    normalize_remote_path(path_segments.second);

    if (!toolbox::strings::has_substring("$image", path_segments.second)) {
        path_segments.second = job->job_name() + ":" + path_segments.second;
    } else {
        toolbox::strings::replace("$image", job->job_name(), path_segments.second);
    }

    std::stringstream res_path;
    res_path << path_segments.first << " " << path_segments.second;

    if (m_config->debug) {
        std::cout << "[debug] copy: " << res_path.str() << std::endl;
    }
    dockerpack::execmd cmd("docker cp " + res_path.str());
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
        if (toolbox::strings::has_substring("_dockerpack", items[1])) {
            m_run_jobs[items[1]] = items[0];
        }
    }
}

void dockerpack::docker::load_remote_envs(const dockerpack::job_ptr_t& job) {
    std::string env_result = exec_internal(job->job_name(), "env");
    if (!env_result.empty()) {
        std::vector<std::string> env_lines = toolbox::strings::split(env_result, "\n");
        for (const auto& env_line : env_lines) {
            auto pair = toolbox::strings::split_pair(env_line, "=");
            image_envs[pair.first] = pair.second;
        }
    }
}

void dockerpack::docker::run(const dockerpack::job_ptr_t& job) {
    if (has_running_job(job)) {
        load_remote_envs(job->shared_from_this());
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

    load_remote_envs(job->shared_from_this());
}

void dockerpack::docker::exec(const dockerpack::job_ptr_t& job, const dockerpack::step_ptr_t& step) {
    if (!has_running_job(job)) {
        throw std::runtime_error("Image " + job->job_name() + " is not run");
    }

    std::stringstream cmd_builder;
    cmd_builder << "docker exec ";
    std::string workdir;
    if (!step->workdir.empty()) {
        workdir = step->workdir;
    } else if (!m_config->workdir.empty()) {
        workdir = m_config->workdir;
    }

    if (!workdir.empty()) {
        normalize_remote_path(workdir);
        cmd_builder << "-w " << workdir << " ";
        ensure_workdir(job, workdir);
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
