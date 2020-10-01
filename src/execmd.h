/*!
 * dockerpack.
 * execmd.h
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_EXECMD_H
#define DOCKERPACK_EXECMD_H

#include <boost/process.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace bp = boost::process;

namespace dockerpack {
class execmd {
public:
    explicit execmd(std::string cmd);
    std::string run(int* exit_code) const;

private:
    std::string cmd;
};

class exec_stream {
public:
    explicit exec_stream(std::string cmd);
    void run(bool output = true);
    int exit_code() const;
    std::error_code error_code() const;
    int wait();

private:
    int m_exit_code = 0;
    std::string cmd;
    std::thread* m_stdout_printer = nullptr;
    std::thread* m_stderr_printer = nullptr;
    bp::child m_child;
    bp::ipstream m_stdout_stream;
    bp::ipstream m_stderr_stream;
    std::error_code m_err_code;
};

} // namespace dockerpack

#endif //DOCKERPACK_EXECMD_H
