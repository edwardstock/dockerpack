#include "builder.h"
#include "config.h"
#include "execmd.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <csignal>
#include <iostream>
#include <string>
#include <termcolor/termcolor.hpp>
#include <toolbox/io.h>
#include <toolbox/strings.hpp>
#include <unordered_map>
#include <vector>

namespace style = termcolor;
namespace po = boost::program_options;

std::string state_path;

std::string usage() {
    std::stringstream ss;
    ss << "    ____             __             ____             __       \n"
          "   / __ \\____  _____/ /_____  _____/ __ \\____ ______/ /__   \n"
          "  / / / / __ \\/ ___/ //_/ _ \\/ ___/ /_/ / __ `/ ___/ //_/   \n"
          " / /_/ / /_/ / /__/ ,< /  __/ /  / ____/ /_/ / /__/ ,<        \n"
          "/_____/\\____/\\___/_/|_|\\___/_/  /_/    \\__,_/\\___/_/|_|  \n"
          "\n";
    ss << DOCKERPACK_VERSION << "\n";
    ss << "DockerPack is a small simple local stateful docker-based CI to test and deploy projects";
    ss << std::endl;
    ss << R"(
Commands:
  build                 Build all jobs and images
  build-images          Build images only. Works almost like docker-compose,
                        but you can create a special container for you project based on other.
                        It helps to speedup development and test time
  print-jobs            Print all existent jobs
  cleanup               Remove all running dockerpack images

  command -h [ --help ] Prints help for selected command
)";

    return ss.str();
}

enum app_command {
    _unknown,
    build,
    build_images,
    cleanup,
    print_jobs
};

std::unordered_map<std::string, app_command> command_map = {
    {"build", app_command::build},
    {"build-images", app_command::build_images},
    {"cleanup", app_command::cleanup},
    {"print-jobs", app_command::print_jobs},
};

inline bool validate_command(const std::string& command) {
    return command_map.count(toolbox::strings::to_lower_case(command));
}

int main(int argc, char** argv) {
    po::options_description desc(usage());
    desc.add_options()("help,h", "Print this help");
    desc.add_options()("version,v", "Print version");
    desc.add_options()("config,c", po::value<std::string>(), "Path to config file (by default, it looking for dockerpack.yml in current directory)");

    if (argc == 1) {
        std::cout << desc << std::endl;
        return 0;
    }

    std::string command_arg = std::string(argv[1]);
    app_command cmd = _unknown;

    if (validate_command(command_arg)) {
        cmd = command_map[command_arg];
    }

    switch (cmd) {
    case build:
        desc.add_options()("name,n", po::value<std::string>(), "Filter job or image to build. For multijob input 'repo:tag'. Filter is based on find substring in job name or job image.");
        desc.add_options()("reset", "Reset dockerpack.lock file and start build from begin");
        desc.add_options()("stateless", "Build jobs and don't save build state.");
        desc.add_options()("no-cleanup", "Don't stop and don't remove running container after success build");
        desc.add_options()("copy-local", "Copy all files from $PWD to image workdir");
        desc.add_options()("env,e", po::value<std::vector<std::string>>(), "Pass build-time environment variables (-e A=1 -e B=2)");
        break;

    case build_images:
        desc.add_options()("name,n", po::value<std::string>(), "Filter job or image to build. For multijob input 'repo:tag'. Filter is based on find substring in job name or job image.");
        desc.add_options()("reset", "Reset dockerpack.lock file and start build from begin");
        desc.add_options()("stateless", "Build jobs and don't save build state.");
        desc.add_options()("env,e", po::value<std::vector<std::string>>(), "Pass build-time environment variables (-e A=1 -e B=2)");
        break;

    case cleanup:
        desc.add_options()("name,n", po::value<std::string>(), "Filter job or image to build. For multijob input 'repo:tag'. Filter is based on find substring in job name or job image.");
        break;

    case print_jobs:
        desc.add_options()("name,n", po::value<std::string>(), "Filter job or image to build. For multijob input 'repo:tag'. Filter is based on find substring in job name or job image.");
        break;

    case _unknown:

        break;
    }

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << std::endl
                  << style::red << style::bold << "Command \"" << command_arg << "\": " << e.what() << style::reset << std::endl;
        std::cout << std::endl
                  << usage() << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }
    if (vm.count("version")) {
        std::cout << "DockerPack: " << DOCKERPACK_VERSION << std::endl;
        return 0;
    }

    if (cmd == _unknown) {
        std::cerr << "Unknown command " << command_arg << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    char dir[255];
    getcwd(dir, 255);
    std::string cwd(dir);
    std::cout << "Working directory: " << cwd << std::endl;

    std::string cfg_path;
    if (vm.count("config")) {
        cfg_path = vm.at("config").as<std::string>();
    } else {
        cfg_path = cwd + "/dockerpack.yml";
    }
    if (!toolbox::io::file_exists(cfg_path)) {
        std::cerr << "Config file " << cfg_path << " not found" << std::endl;
        exit(1);
    }

    dockerpack::build_options opts;
    opts.reset_lock = vm.count("reset");
    opts.stateless = vm.count("stateless");
    opts.no_cleanup = vm.count("no-cleanup");
    opts.copy_local = vm.count("copy-local");
    if (vm.count("name")) {
        opts.filter_name = vm.at("name").as<std::string>();
    }
    if (vm.count("env")) {
        const std::vector<std::string> envs = vm.at("env").as<std::vector<std::string>>();
        for (const auto& var : envs) {
            auto pair = toolbox::strings::split_pair(var, "=");
            opts.envs[pair.first] = pair.second;
        }
    }

    state_path = cwd + "/" + dockerpack::STATE_FILE;

    signal(SIGINT, [](int signum) {
        if (boost::filesystem::exists(state_path)) {
            std::cout << "sigint caught; removing lock file." << std::endl;
            boost::filesystem::remove(state_path);
        }
        exit(signum);
    });

    dockerpack::builder b(cwd, cfg_path, state_path, std::move(opts));
    try {
        bool ret = true;
        switch (cmd) {
        case build:
            b.init();
            ret = b.build_all();
            break;
        case build_images:
            b.init();
            ret = b.build_images();
            break;
        case cleanup:
            b.init();
            ret = b.cleanup();
            break;
        case print_jobs:
            b.init();
            b.print_jobs();
        case _unknown:
            break;
        }

        if (!ret) {
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
