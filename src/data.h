/*!
 * dockerpack.
 * data.h
 *
 * \date 09/26/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_DATA_H
#define DOCKERPACK_DATA_H

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace dockerpack {

using env_map = std::unordered_map<std::string, std::string>;

struct docker_image {
    std::string repo;
    std::string tag;
};

class step : public std::enable_shared_from_this<dockerpack::step> {
public:
    std::string name;
    std::string command;
    bool skip_on_error = false;
    std::string workdir;
    env_map envs;

    std::string to_string() {
        std::stringstream ss;
        if (!name.empty()) {
            ss << name << ": ";
        }
        ss << command << "; ";
        ss << "skip_on_error: " << skip_on_error << "; ";
        ss << "workdir: " << (workdir.empty() ? "<empty>" : workdir);
        return ss.str();
    }

    std::string hash() const;
};

struct virtual_enable_shared_from_this_base : std::enable_shared_from_this<virtual_enable_shared_from_this_base> {
    virtual ~virtual_enable_shared_from_this_base() {
    }
};
template<typename T>
struct virtual_enable_shared_from_this : virtual virtual_enable_shared_from_this_base {
    std::shared_ptr<T> shared_from_this() {
        return std::dynamic_pointer_cast<T>(
            virtual_enable_shared_from_this_base::shared_from_this());
    }
};

class job : public virtual_enable_shared_from_this<dockerpack::job> {
public:
    std::string name;
    std::string image;
    env_map envs;
    std::vector<std::shared_ptr<step>> steps;

    std::string job_name() const;
    void add_envs(const dockerpack::env_map& ext_envs);
};

class image_to_build : public job, public virtual_enable_shared_from_this<dockerpack::image_to_build> {
public:
    std::string repo;
    std::string tag;

    std::string full_name() const;

    std::shared_ptr<image_to_build> shared_from_this() {
        return virtual_enable_shared_from_this<dockerpack::image_to_build>::shared_from_this();
    }
};

using job_ptr_t = std::shared_ptr<dockerpack::job>;
using step_ptr_t = std::shared_ptr<dockerpack::step>;
using imb_ptr_t = std::shared_ptr<dockerpack::image_to_build>;

} // namespace dockerpack

#endif //DOCKERPACK_DATA_H
