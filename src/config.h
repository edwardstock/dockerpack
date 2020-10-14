/*!
 * dockerpack.
 * config.h
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_CONFIG_H
#define DOCKERPACK_CONFIG_H

#include "data.h"
#include "execmd.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace dockerpack {

class config_parse_error : public virtual std::exception {
private:
    std::string m_err;

public:
    config_parse_error(const std::string& message, const std::string& section, const std::string& print_name = "") noexcept
        : std::exception() {
        std::stringstream ss;
        ss << message << ": in " << section;
        if (!print_name.empty()) {
            ss << "." << print_name;
        }
        m_err = ss.str();
    }

    const char* what() const noexcept override {
        return m_err.c_str();
    }
};

class config : public std::enable_shared_from_this<dockerpack::config> {
public:
    std::string cfg_path;
    std::string checkout_command;
    bool debug = false;
    bool sudo = true;
    bool commands_verbose = true;
    std::string docker_repository;
    std::string workdir;
    std::vector<std::string> copy_paths;
    std::unordered_map<std::string, std::vector<step_ptr_t>> steps;
    std::vector<job_ptr_t> jobs;
    std::vector<imb_ptr_t> build_images;
    std::string m_cwd;
    env_map global_envs;

    config(std::string cwd, std::string cfg_path);

    void parse(bool copy_local = false);

private:
    void parse_includes(const YAML::Node& include_list_node);
    void parse_build_images(const YAML::Node& build_images_node);
    void parse_jobs(const YAML::Node& jobs_node);
    void parse_multijob(const YAML::Node& multijob_node);
    void parse_commands(const YAML::Node& commands);
    std::vector<step_ptr_t> parse_steps(const YAML::Node& steps_node) const;
    void insert_step(const std::string& print_name, std::string&& command, std::string&& name, bool skip_on_error = false);
    env_map parse_envs(const YAML::Node& node, bool redacted = false) const;
};

using config_ptr_t = std::shared_ptr<dockerpack::config>;

} // namespace dockerpack

#endif //DOCKERPACK_CONFIG_H
