/*!
 * dockerpack.
 * utils.h
 *
 * \date 09/28/2020
 * \author Eduard Maximovich (edward.vstock@gmail.com)
 * \link   https://github.com/edwardstock
 */
#ifndef DOCKERPACK_UTILS_H
#define DOCKERPACK_UTILS_H

#include <string>

namespace dockerpack {
namespace utils {

void normalize_path(std::string& path);

} // namespace utils
} // namespace dockerpack

#endif //DOCKERPACK_UTILS_H
