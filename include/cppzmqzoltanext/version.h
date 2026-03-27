/*
MIT License

Copyright (c) 2025 Luan Young

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/**
 * @file version.h
 * @brief Library version accessors
 *
 * This header exposes runtime functions to query the library version.
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2026 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */

#pragma once

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Get the major version component
 * @return Major version number
 */
CZZE_EXPORT int version_major() noexcept;

/**
 * @brief Get the minor version component
 * @return Minor version number
 */
CZZE_EXPORT int version_minor() noexcept;

/**
 * @brief Get the patch version component
 * @return Patch version number
 */
CZZE_EXPORT int version_patch() noexcept;

/**
 * @brief Get full version string
 * @return Null-terminated semantic version string (for example, "0.0.1")
 */
CZZE_EXPORT const char* version_string() noexcept;

}  // namespace zmqzext
