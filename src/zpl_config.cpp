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
 * @file zpl_config.cpp
 * @brief ZPL (ZeroMQ Property Language) configuration API
 */

#include "cppzmqzoltanext/zpl_config.h"

#include <utility>

namespace zmqzext {

namespace {
std::string const k_empty_string;
}

struct zpl_config_t::impl_t {
    std::string node_name;
    std::string node_value;
};

zpl_parse_error::zpl_parse_error(std::string message, std::size_t line)
    : zpl_error(std::move(message)), _line(line) {}

std::size_t zpl_parse_error::line() const noexcept { return _line; }

zpl_config_t::zpl_config_t() noexcept : _impl(std::make_shared<impl_t>()) {}

zpl_config_t::zpl_config_t(std::istream& input) : _impl(std::make_shared<impl_t>()) { load(input); }

zpl_config_t zpl_config_t::from_stream(std::istream& input) {
    zpl_config_t config;
    config.load(input);
    return config;
}

zpl_config_t zpl_config_t::from_file(const std::string& file_path) {
    zpl_config_t config;
    config.load_from_file(file_path);
    return config;
}

void zpl_config_t::load(std::istream& input) {
    (void)input;
}

void zpl_config_t::load_from_file(const std::string& file_path) { (void)file_path; }

bool zpl_config_t::empty() const noexcept { return true; }

const std::string& zpl_config_t::name() const noexcept { return _impl ? _impl->node_name : k_empty_string; }

const std::string& zpl_config_t::value() const noexcept { return _impl ? _impl->node_value : k_empty_string; }

bool zpl_config_t::contains(const std::string& path) const noexcept {
    (void)path;
    return false;
}

const std::string& zpl_config_t::get(const std::string& path) const {
    (void)path;
    throw zpl_property_not_found("zpl_config_t::get(path) is not implemented yet");
}

std::optional<std::string> zpl_config_t::try_get(const std::string& path) const noexcept {
    (void)path;
    return std::nullopt;
}

std::string zpl_config_t::get_or(const std::string& path, std::string default_value) const noexcept {
    (void)path;
    return default_value;
}

zpl_config_t zpl_config_t::child(const std::string& path) const {
    (void)path;
    throw zpl_property_not_found("zpl_config_t::child(path) is not implemented yet");
}

std::optional<zpl_config_t> zpl_config_t::try_child(const std::string& path) const noexcept {
    (void)path;
    return std::nullopt;
}

std::vector<zpl_config_t> zpl_config_t::children() const noexcept { return {}; }

}  // namespace zmqzext
