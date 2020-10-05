/*!
 * dockerpack.
 * dockerpack.cpp
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#include "builder.h"

#include <termcolor/termcolor.hpp>
#include <toolbox/strings.hpp>

namespace style = termcolor;

const std::string dockerpack::STATE_FILE = "dockerpack.lock";

static inline void error(const std::string& message, const std::exception& e) {
    std::cerr << style::red << message << ":" << std::endl;
    std::cerr << e.what() << style::reset << std::endl;
}

dockerpack::builder::builder(std::string cwd, const std::string& config_path, const std::string& state_file_path, build_options&& opts)
    : m_config(std::make_shared<dockerpack::config>(std::move(cwd), config_path)),
      m_docker(m_config),
      m_state(state_file_path),
      m_options(std::move(opts)) {

    m_state.enable(!opts.stateless);
}

void dockerpack::builder::print_jobs() {
    std::vector<job_ptr_t> jobs;
    if (m_options.filter_name.empty()) {
        std::for_each(m_config->jobs.begin(), m_config->jobs.end(), [&jobs](job_ptr_t job) {
            jobs.push_back(job->shared_from_this());
        });
    } else {
        for (auto& job : m_config->jobs) {
            if (toolbox::strings::has_substring(m_options.filter_name, job->job_name())) {
                jobs.push_back(job->shared_from_this());
            } else if (toolbox::strings::has_substring(m_options.filter_name, job->image)) {
                jobs.push_back(job->shared_from_this());
            }
        }
    }

    if (!m_options.filter_name.empty() && jobs.empty()) {
        std::cout << "No one job found by filter \"" << m_options.filter_name << "\"" << std::endl;
        return;
    } else if (jobs.empty()) {
        std::cout << "No one job in config" << std::endl;
        return;
    }

    for (const auto& job : jobs) {
        std::cout << "Job: " << std::endl;
        std::cout << "   name: " << style::green << job->job_name() << style::reset << "\n";
        std::cout << "  image: " << style::green << job->image << style::reset << "\n";
        if (!job->envs.empty()) {
            std::cout << "    env: "
                      << "\n";
            for (const auto& env : job->envs) {
                std::cout << "         " << style::green << env.first << "=" << env.second << style::reset << "\n";
            }
        } else {
            std::cout << "    env: <none>"
                      << "\n";
        }
        std::cout << "  steps:"
                  << "\n";
        for (const auto& step : job->steps) {
            if (!step->name.empty()) {
                std::cout << "         " << style::green << step->name << ": " << step->command << style::reset << "\n";
            } else {
                std::cout << "         " << style::green << step->command << style::reset << "\n";
            }
        }
        std::cout << std::endl;
    }
}

bool dockerpack::builder::build_images() {
    std::vector<imb_ptr_t> images;
    if (!m_options.filter_name.empty()) {
        for (auto& image : m_config->build_images) {
            if (toolbox::strings::has_substring(m_options.filter_name, image->job_name())) {
                images.push_back(image->shared_from_this());
            } else if (toolbox::strings::has_substring(m_options.filter_name, image->image)) {
                images.push_back(image->shared_from_this());
            } else if (toolbox::strings::has_substring(m_options.filter_name, image->repo)) {
                images.push_back(image->shared_from_this());
            }
        }
    } else {
        std::for_each(m_config->build_images.begin(), m_config->build_images.end(), [&images](imb_ptr_t image) {
            images.push_back(image->shared_from_this());
        });
    }
    for (auto& image : images) {
        if (m_docker.has_image(image->full_name(), image->tag)) {
            std::cout << "Skipping build " << style::green << image->full_name() << ":" << image->tag << style::reset << std::endl;
            continue;
        }

        std::cout << "Starting building image: " << style::green << image->name << style::reset << std::endl;

        image->add_envs(m_options.envs);

        // run image
        try {
            m_docker.run(image);
        } catch (const std::exception& e) {
            error("Failed to start job " + image->name, e);
            return false;
        }

        // copy local sources to image
        if (!m_config->copy_paths.empty()) {
            for (const auto& copy_path : m_config->copy_paths) {
                try {
                    m_docker.copy(image, copy_path);
                } catch (const std::exception& e) {
                    error("Failed to copy " + copy_path, e);
                    return false;
                }
            }
        }

        // execute commands
        for (const auto& step : image->steps) {
            if (!step->name.empty()) {
                std::cout << " - " << style::green << step->name << style::reset << std::endl;
            } else {
                std::cout << " - exec: " << style::green << step->command << style::reset << std::endl;
            }
            if (m_state.has_success_build_step(image, step)) {
                std::cout << "   - skipping..." << std::endl;
                continue;
            }

            try {
                m_docker.exec(image, step);
                m_state.add_success_build_step(image, step);
                m_state.save();
            } catch (const std::exception& e) {
                std::stringstream ss;
                ss << "Failed to execute command: " << step->command << "\nIn image " << image->image << std::endl;
                error(ss.str(), e);
                return false;
            }
        }

        // finalize, stop and remove container
        try {
            m_docker.commit(image);
            m_docker.stop(image);
            m_docker.rm(image);
        } catch (const std::exception& e) {
            error("Failed stop and remove docker image", e);
            return false;
        }
    }

    std::cout << style::green << "All images are built!\n"
              << style::reset
              << std::endl;
    return true;
}
bool dockerpack::builder::build_jobs() {
    std::vector<job_ptr_t> jobs;
    if (!m_options.filter_name.empty()) {
        for (auto& job : m_config->jobs) {
            if (toolbox::strings::has_substring(m_options.filter_name, job->job_name())) {
                jobs.push_back(job->shared_from_this());
            } else if (toolbox::strings::has_substring(m_options.filter_name, job->image)) {
                jobs.push_back(job->shared_from_this());
            }
        }
    } else {
        std::for_each(m_config->jobs.begin(), m_config->jobs.end(), [&jobs](job_ptr_t job) {
            jobs.push_back(job->shared_from_this());
        });
    }
    if (jobs.empty()) {
        std::cout << "Nothing to run: ";
        if (!m_options.filter_name.empty()) {
            std::cout << "no one job has found by name \"" << style::green << m_options.filter_name << style::reset << std::endl;
        } else {
            std::cout << "job list is empty" << std::endl;
        }

        return true;
    }
    for (auto& job : jobs) {
        if (m_state.has_success_job(job)) {
            std::cout << "Skipping successful job " << style::green << job->name << style::reset << std::endl;
            continue;
        }

        job->add_envs(m_options.envs);

        // run image
        try {
            if (m_options.stateless && m_docker.has_running_job(job)) {
                m_docker.stop(job);
                m_docker.rm(job);
            }

            std::cout << "Starting job: " << style::green << job->name << style::reset << std::endl;
            m_docker.run(job);
        } catch (const std::exception& e) {
            error("Failed to start job " + job->name, e);
            return false;
        }

        // copy local sources to image
        if (!m_config->copy_paths.empty()) {
            for (const auto& copy_path : m_config->copy_paths) {
                try {
                    std::cout << " - copy: " << style::green << copy_path << style::reset << std::endl;
                    m_docker.copy(job, copy_path);
                } catch (const std::exception& e) {
                    error("Failed to copy " + copy_path, e);
                    return false;
                }
            }
        }

        // execute commands
        for (const auto& step : job->steps) {
            if (!step->name.empty()) {
                std::cout << " - " << style::green << step->name << style::reset << std::endl;
            } else {
                std::cout << " - exec: " << style::green << step->command << style::reset << std::endl;
            }
            if (m_state.has_success_step(job, step)) {
                std::cout << "   - skipping..." << std::endl;
                continue;
            }

            try {
                m_docker.exec(job, step);
                if (m_config->debug) {
                    std::cout << style::yellow << "[debug] add success step " << job->job_name() << " - " << step->to_string() << style::reset << std::endl;
                }
                m_state.add_success_step(job, step);
                m_state.save();
            } catch (const std::exception& e) {
                std::stringstream ss;
                ss << "Failed to execute command: " << style::green << step->command << style::reset << "\nIn job " << style::green << job->name << style::reset << std::endl;
                error(ss.str(), e);
                return false;
            }
        }

        // finalize, stop and remove container
        if (not m_options.no_cleanup) {
            try {
                m_docker.stop(job);
                m_docker.rm(job);
            } catch (const std::exception& e) {
                error("Failed stop and remove docker image", e);
                return false;
            }
        }

        m_state.add_success_job(job);
        m_state.save();
    }

    m_state.remove();
    std::cout << style::green << "\n\nAll jobs are done!" << style::reset << std::endl;
    return true;
}

void dockerpack::builder::init() {

    if (!dockerpack::docker::check_docker_exists()) {
        throw std::runtime_error("'docker' binary not found on your system. Please install it first or add to $PATH.");
    }

    if (m_options.reset_lock) {
        m_state.remove();
    }

    m_config->parse(m_options.copy_local);
    m_state.load();
}

bool dockerpack::builder::build_all() {
    if (!build_images()) {
        return false;
    }

    if (!build_jobs()) {
        return false;
    }

    return true;
}

bool dockerpack::builder::cleanup() {
    m_state.remove();
    auto jobs = m_docker.filter_running_job(m_options.filter_name);
    if (jobs.empty()) {
        std::cout << "Nothing to cleanup" << std::endl;
        return true;
    }

    size_t i = 0;
    for (const auto& j : jobs) {
        m_docker.stop(j);
        m_docker.rm(j);
        i++;
    }

    std::cout << "Stopped and removed " << i << " images" << std::endl;

    return true;
}
