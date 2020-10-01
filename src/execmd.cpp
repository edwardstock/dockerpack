/*!
 * dockerpack.
 * execmd.cpp
 *
 * \date 09/25/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#include "execmd.h"

dockerpack::execmd::execmd(std::string cmd)
    : cmd(std::move(cmd)) {
}

std::string dockerpack::execmd::run(int* exit_code) const {
    try {
        bp::ipstream s;
        bp::child runner(cmd, bp::std_out > s);

        std::stringstream ss;
        std::string line;
        while (std::getline(s, line)) {
            ss << line << std::endl;
        }

        runner.wait();
        if (exit_code) {
            *exit_code = runner.exit_code();
        }
        return ss.str();
    } catch (const bp::process_error& e) {
        if (exit_code) {
            *exit_code = 127;
        }
        return e.code().message();
    }
}

dockerpack::exec_stream::exec_stream(std::string cmd)
    : cmd(std::move(cmd)) {
}
void dockerpack::exec_stream::run(bool output) {
    if (output) {
        m_child = bp::child(cmd, bp::std_out > stdout, bp::std_err > stderr);
    } else {
        m_child = bp::child(cmd, bp::std_out > bp::null);
    }
}
int dockerpack::exec_stream::exit_code() const {
    return 0;
}
std::error_code dockerpack::exec_stream::error_code() const {
    return m_err_code;
}
int dockerpack::exec_stream::wait() {
    if (m_stdout_printer != nullptr) {
        if (m_stdout_printer->joinable()) {
            m_stdout_printer->join();
        }
    }
    if (m_stderr_printer != nullptr) {
        if (m_stderr_printer->joinable()) {
            m_stderr_printer->join();
        }
    }
    if (m_child.running()) {
        m_child.wait(m_err_code);
    }
    m_exit_code = m_child.exit_code();

    return m_exit_code;
}
