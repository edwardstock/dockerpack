/*!
 * dockerpack.
 * config.cpp
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#include "config.h"

#include "utils.h"

#include <stdexcept>
#include <toolbox/strings.hpp>

inline dockerpack::step_ptr_t create_step(std::string command, std::string name = "", bool skip_on_error = false, std::string workdir = "") {
    dockerpack::step_ptr_t step = std::make_shared<dockerpack::step>();
    step->command = std::move(command);
    step->name = std::move(name);
    step->skip_on_error = skip_on_error;
    step->workdir = std::move(workdir);
    return step;
}

dockerpack::config::config(std::string cwd, std::string cfg_path)
    : cfg_path(std::move(cfg_path)),
      workdir("~/project"),
      m_cwd(std::move(cwd)) {
}

void dockerpack::config::parse(bool copy_local) {
    const YAML::Node config = YAML::LoadFile(cfg_path);

#if !defined(DOCKERPACK_NODEBUG)
    if (config["debug"]) {
        debug = config["debug"].as<bool>();
    }
#endif
    if (config["commands_verbose"]) {
        commands_verbose = config["commands_verbose"].as<bool>();
    }
    if (config["sudo"]) {
        sudo = config["sudo"].as<bool>();
    }
    if (config["workdir"]) {
        workdir = config["workdir"].as<std::string>();
    } else {
        workdir = "~/project";
    }

    if (config["checkout"] && !copy_local) {
        if (!config["checkout"].IsScalar()) {
            throw config_parse_error("checkout section must be a string", "checkout");
        }
        checkout_command = config["checkout"].as<std::string>();
    }

    if (config["copy"]) {
        if (config["copy"].IsSequence()) {
            copy_paths = config["copy"].as<std::vector<std::string>>();
        } else if (config["copy"]) {
            copy_paths.push_back(config["copy"].as<std::string>());
        }
    }
    if (copy_local) {
        copy_paths.push_back(m_cwd + "/. " + workdir);
    }

    if (config["include"]) {
        parse_includes(config["include"]);
    }

    if (!config["commands"]) {
        throw config_parse_error("Config files does not include \"commands\" section", ".", "commands");
    }

    if (!config["jobs"] && !config["multijob"]) {
        throw config_parse_error("Config files does not include \"jobs\" or \"multijob\" section", "jobs|multijob");
    }

    parse_commands(config["commands"]);

    if (config["build_images"]) {
        parse_build_images(config["build_images"]);
    }

    if (config["jobs"]) {
        parse_jobs(config["jobs"]);
    } else if (config["multijob"]) {
        parse_multijob(config["multijob"]);
    }
}

void dockerpack::config::parse_includes(const YAML::Node& include_list_node) {
    std::vector<std::string> include_paths;
    if (include_list_node.IsSequence()) {
        for (const auto& item : include_list_node) {
            std::string p = item.as<std::string>();
            dockerpack::utils::normalize_path(p);
            include_paths.push_back(std::move(p));
        }
    } else if (include_list_node.IsScalar()) {
        std::string p = include_list_node.as<std::string>();
        dockerpack::utils::normalize_path(p);
        include_paths.push_back(std::move(p));
    }

    for (const auto& include_path : include_paths) {
        YAML::Node config;
        try {
            config = YAML::LoadFile(include_path);
        } catch (const std::exception& e) {
            throw std::runtime_error("Unable to load include " + include_path + ": " + e.what());
        }

        // recursive include
        if (config["include"]) {
            parse_includes(config["include"]);
        }

        if (config["commands"]) {
            parse_commands(config["commands"]);
        }
        if (config["build_images"]) {
            parse_build_images(config["build_images"]);
        }
    }
}

dockerpack::env_map dockerpack::config::parse_envs(const YAML::Node& node, bool redacted) const {
    env_map out;
    for (const auto& env : node) {
        const std::string key = env.first.as<std::string>();
        std::string value = env.second.as<std::string>();
        if (toolbox::strings::equals_icase(value, "$ENV")) {
            //todo: print for debug only redacted values, now we just disable debug option for release build
            if (redacted) {
                value = "**REDACTED**";
            } else {
                const char* v = std::getenv(key.c_str());
                if (v == nullptr) {
                    value = "";
                } else {
                    value = std::string(v);
                }
            }
        }
        out[key] = std::move(value);
    }
    return out;
}

static std::string clean_job_name(const std::string& name) {
    return toolbox::strings::substr_replace_all_ret(std::vector<std::string>{"/", ".", ":"}, "_", name);
}

void dockerpack::config::parse_multijob(const YAML::Node& multijob_node) {
    if (!multijob_node.IsMap()) {
        throw config_parse_error("multijob node must be a map", "multijob");
    }

    if (!multijob_node["steps"]) {
        throw config_parse_error("multijob must have a step list", "multijob", "steps");
    }
    if (!multijob_node["steps"].IsSequence()) {
        throw config_parse_error("multijob steps must be a list", "multijob", "steps");
    }

    if (!multijob_node["images"]) {
        throw config_parse_error("multijob must have a images list", "multijob", "images");
    }
    if (!multijob_node["images"].IsSequence()) {
        throw config_parse_error("multijob images must be a list", "multijob", "images");
    }

    std::vector<job_ptr_t> local_jobs;
    local_jobs.reserve(multijob_node["images"].size());

    std::unordered_map<std::string, std::string> local_envs;

    if (multijob_node["env"]) {
        local_envs = parse_envs(multijob_node["env"]);
    }

    size_t i = 0;
    for (const auto& image : multijob_node["images"]) {
        if (image.IsScalar()) {
            job_ptr_t job = std::make_shared<dockerpack::job>();
            job->image = image.as<std::string>();
            job->name = clean_job_name(job->image);
            job->envs = local_envs;

            local_jobs.push_back(std::move(job));
        } else if (image.IsMap()) {
            std::vector<job_ptr_t> jobs_tmp;
            if (image["image"]) {
                job_ptr_t job = std::make_shared<dockerpack::job>();
                job->image = image["image"].as<std::string>();
                job->name = clean_job_name(job->image);
                jobs_tmp.push_back(std::move(job));
            } else if (image["images"] && image["images"].IsSequence()) {
                for (const auto& item : image["images"]) {
                    job_ptr_t job = std::make_shared<dockerpack::job>();
                    job->image = item.as<std::string>();
                    job->name = clean_job_name(job->image);
                    jobs_tmp.push_back(std::move(job));
                }
            } else {
                throw config_parse_error("multijob image does not have a docker image or image list name with tag", "multijob", "images[" + std::to_string(i) + "]");
            }

            if (image["env"]) {
                const auto image_envs = parse_envs(image["env"]);
                for (auto& job : jobs_tmp) {
                    job->envs.insert(local_envs.begin(), local_envs.end());
                    // if something repeats, priority to local env
                    job->envs.insert(image_envs.begin(), image_envs.end());
                }
            }

            for (auto&& job : jobs_tmp) {
                local_jobs.push_back(std::move(job));
            }
        }

        i++;
    }

    std::vector<step_ptr_t> local_steps;
    i = 0;
    for (const auto& step : multijob_node["steps"]) {
        if (step.IsScalar()) {
            std::string step_name = step.as<std::string>();
            if (steps.count(step_name)) {
                local_steps.insert(
                    local_steps.end(),
                    steps[step_name].begin(),
                    steps[step_name].begin() + steps[step_name].size());
            } else {
                throw config_parse_error("multijob step cannot have unnamed type. It must reference to defined command in 'commands' section", "multijob", "steps[" + std::to_string(i) + "]");
            }
        }

        i++;
    }

    for (auto& job : local_jobs) {
        if (!checkout_command.empty()) {
            job->steps.push_back(
                create_step(
                    checkout_command,
                    "checkout",
                    true,
                    workdir));
        }
        job->steps.insert(
            job->steps.end(),
            local_steps.begin(),
            local_steps.end());
    }

    jobs = std::move(local_jobs);
}

void dockerpack::config::parse_build_images(const YAML::Node& build_images_node) {
    for (const auto& image_node : build_images_node) {
        imb_ptr_t image = std::make_shared<dockerpack::image_to_build>();
        image->name = image_node.first.as<std::string>();
        if (toolbox::strings::has_substring("/", image->name)) {
            auto split = toolbox::strings::split(image->name, "/");
            image->repo = split[0];
            image->name = split[1];
        }

        if (!image_node.second.IsMap()) {
            throw config_parse_error("Image has invalid format. Must be a map.", "build_images", image->name);
        }
        if (!image_node.second["image"]) {
            throw config_parse_error("Image to build does not have a source image.", "build_images", image->name);
        }
        if (!image_node.second["steps"]) {
            throw config_parse_error("Image does not have a steps list.", "build_images", image->name);
        }
        if (!image_node.second["tag"]) {
            throw config_parse_error("Image does not have a tag name", "build_images", image->name);
        }
        if (image_node.second["env"]) {
            image->envs = parse_envs(image_node.second["env"]);
        }

        image->image = image_node.second["image"].as<std::string>();
        image->tag = image_node.second["tag"].as<std::string>();

        if (image_node.second["steps"].IsSequence()) {
            for (const auto& step_item : image_node.second["steps"]) {
                const std::string step_name = step_item.as<std::string>();
                // find reference in commands section
                if (steps.count(step_name)) {
                    image->steps.insert(
                        image->steps.end(),
                        steps[step_name].begin(),
                        steps[step_name].begin() + steps[step_name].size());
                } else {
                    // user added undefined command, add it too to local job scope
                    step_ptr_t step = std::make_shared<dockerpack::step>();
                    step->command = step_name;
                    image->steps.push_back(std::move(step));
                }
            }
        }

        build_images.push_back(std::move(image));
    }
}
void dockerpack::config::parse_jobs(const YAML::Node& jobs_node) {
    for (const auto& job_node : jobs_node) {
        job_ptr_t job = std::make_shared<dockerpack::job>();
        job->name = job_node.first.as<std::string>();

        if (!checkout_command.empty()) {
            job->steps.push_back(
                create_step(
                    checkout_command,
                    "checkout",
                    true,
                    "/"));
        }

        if (!job_node.second.IsMap()) {
            throw config_parse_error("Job has invalid format. Must be a map.", "jobs", job->name);
        }
        if (!job_node.second["image"]) {
            throw config_parse_error("Job does not have an image.", "jobs", job->name);
        }
        if (!job_node.second["steps"]) {
            throw config_parse_error("Job does not have a steps list.", "jobs", job->name);
        }
        if (job_node.second["env"] && job_node.second["env"].IsMap()) {
            std::unordered_map<std::string, std::string> envs;
            for (const auto& env : job_node.second["env"]) {
                const std::string key = env.first.as<std::string>();
                std::string value = env.second.as<std::string>();
                if (toolbox::strings::equals_icase(value, "$ENV")) {
                    value = std::string(std::getenv(key.c_str()));
                }
                envs[key] = std::move(value);
            }
            job->envs = std::move(envs);
        }

        job->image = job_node.second["image"].as<std::string>();

        if (job_node.second["steps"].IsSequence()) {
            for (const auto& step_item : job_node.second["steps"]) {
                const std::string step_name = step_item.as<std::string>();
                // find reference in commands section
                if (steps.count(step_name)) {
                    job->steps.insert(
                        job->steps.end(),
                        steps[step_name].begin(),
                        steps[step_name].begin() + steps[step_name].size());
                } else {
                    // user added undefined command, add it too to local job scope
                    job->steps.push_back(create_step(step_name));
                }
            }
        }

        jobs.push_back(std::move(job));
    }
}

std::vector<dockerpack::step_ptr_t> dockerpack::config::parse_steps(const YAML::Node& steps_node) const {
    std::vector<step_ptr_t> out;
    out.reserve(steps_node.size());
    for (const auto& config_step : steps_node) {
        if (config_step.IsMap()) {
            if (config_step["run"]) {
                if (config_step["run"].IsScalar()) {
                    step_ptr_t step = create_step(config_step["run"].as<std::string>());
                    if (steps.count(step->command)) {
                        for (auto& s : steps.at(step->command)) {
                            out.push_back(s->shared_from_this());
                        }
                    } else {
                        out.push_back(std::move(step));
                    }

                } else if (config_step["run"].IsMap() && config_step["run"]["command"]) {

                    auto step = std::make_shared<dockerpack::step>();
                    step->command = config_step["run"]["command"].as<std::string>();
                    if (config_step["run"]["name"]) {
                        step->name = config_step["run"]["name"].as<std::string>();
                    }
                    if (config_step["run"]["workdir"]) {
                        step->workdir = config_step["run"]["workdir"].as<std::string>();
                    }
                    step->skip_on_error = config_step["run"]["skip_on_error"] != nullptr && config_step["run"]["skip_on_error"].as<bool>();
                    if (config_step["run"]["env"]) {
                        step->envs = parse_envs(config_step["run"]["env"]);
                    }

                    // check command step is a reference to another command
                    if (steps.count(step->command)) {
                        for (auto& s : steps.at(step->command)) {
                            out.push_back(s->shared_from_this());
                        }
                    } else {
                        out.push_back(std::move(step));
                    }
                }
            }
        } else if (config_step.IsScalar()) {
            step_ptr_t step = create_step(config_step.as<std::string>());
            if (steps.count(step->command)) {
                for (auto& s : steps.at(step->command)) {
                    out.push_back(s->shared_from_this());
                }
            } else {
                out.push_back(std::move(step));
            }
        }
    }
    return out;
}

void dockerpack::config::parse_commands(const YAML::Node& commands) {
    for (const auto& command : commands) {
        const std::string command_name = command.first.as<std::string>();
        if (command.second["steps"]) {

            if (!steps.count(command_name)) {
                steps[command_name] = std::vector<step_ptr_t>();
            }

            YAML::Node config_steps = command.second["steps"];
            std::vector<step_ptr_t> parsed_steps = parse_steps(config_steps);

            steps[command_name].reserve(parsed_steps.size());

            // validate recursive link
            size_t i = 0;
            for (const auto& step : parsed_steps) {
                if (step->command == command_name) {
                    throw config_parse_error("Command can't reference to itself", "commands", command_name + ".steps[" + std::to_string(i) + "]");
                }
                i++;
            }
            for (auto&& item : parsed_steps) {
                steps[command_name].push_back(std::move(item));
            }
            //            std::move(parsed_steps.begin(), parsed_steps.begin() + parsed_steps.size(), steps[command_name].end());
        }
    }
}
void dockerpack::config::insert_step(const std::string& print_name, std::string&& command, std::string&& name, bool skip_on_error) {
    step_ptr_t step = std::make_shared<dockerpack::step>();
    step->command = std::move(command);
    step->name = std::move(name);
    step->skip_on_error = skip_on_error;

    // nested steps: any step can include another step simply by it's name
    if (steps.count(step->command)) {
        if (toolbox::strings::equals_icase(step->command, print_name)) {
            throw config_parse_error("Command can't links to itself", "commands", print_name);
        }
        steps[print_name].insert(
            steps[print_name].end(),
            steps[step->command].begin(),
            steps[step->command].begin() + steps[step->command].size());
    } else {
        // insert
        steps[print_name].push_back(std::move(step));
    }
}
