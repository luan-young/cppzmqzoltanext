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
 * @file zpl_config.h
 * @brief ZPL (ZeroMQ Property Language) configuration API
 */

#pragma once

#include <cstddef>
#include <istream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

class CZZE_EXPORT zpl_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class CZZE_EXPORT zpl_parse_error : public zpl_error {
public:
    zpl_parse_error(std::string message, std::size_t line);

    std::size_t line() const noexcept;

private:
    std::size_t _line;
};

class CZZE_EXPORT zpl_property_not_found : public zpl_error {
public:
    using zpl_error::zpl_error;
};

class CZZE_EXPORT zpl_invalid_path : public zpl_error {
public:
    using zpl_error::zpl_error;
};

class CZZE_EXPORT zpl_config_t {
public:
    zpl_config_t() noexcept;
    explicit zpl_config_t(std::istream& input);

    static zpl_config_t from_stream(std::istream& input);
    static zpl_config_t from_file(const std::string& file_path);

    void load(std::istream& input);
    void load_from_file(const std::string& file_path);

    bool empty() const noexcept;
    const std::string& name() const noexcept;
    const std::string& value() const noexcept;

    bool contains(const std::string& path) const noexcept;

    const std::string& get(const std::string& path) const;
    std::optional<std::string> try_get(const std::string& path) const noexcept;
    std::string get_or(const std::string& path, std::string default_value) const noexcept;

    zpl_config_t child(const std::string& path) const;
    std::optional<zpl_config_t> try_child(const std::string& path) const noexcept;

    std::vector<zpl_config_t> children() const noexcept;

private:
    struct impl_t;
    std::shared_ptr<impl_t> _impl;
};

}  // namespace zmqzext
