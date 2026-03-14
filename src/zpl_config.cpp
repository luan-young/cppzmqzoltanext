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
 * @brief ZPL (ZeroMQ Property Language) parser
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2026 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "cppzmqzoltanext/zpl_config.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zmqzext {

namespace {
/// Empty string used for safe return references when no node is available.
std::string const k_empty_string;

/// Internal node representation for the parsed ZPL tree.
struct zpl_node_t {
    std::string name;                ///< Node name segment
    std::string value;               ///< Raw string value
    bool explicitly_defined{false};  ///< True when this node appears explicitly in the input.
                                     ///< Nodes created only as path containers (e.g., from "a/b")
                                     ///< keep this false unless their own property line is present.
    std::vector<std::shared_ptr<zpl_node_t>> ordered_children;                      ///< Children in source order
    std::unordered_map<std::string, std::shared_ptr<zpl_node_t>> children_by_name;  ///< Fast lookup
};

/// Parsed line information after trimming, indentation checks and value parsing.
struct parsed_line_t {
    std::size_t indent_level;  ///< Indentation level (4-space units)
    std::string name;          ///< Property name (may include '/')
    std::string value;         ///< Property value (possibly empty)
};

/// Trim leading whitespace.
std::string ltrim_copy(const std::string& text) {
    std::size_t start = 0;
    while (start < text.size() && std::isblank(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    return text.substr(start);
}

/// Trim trailing whitespace.
std::string rtrim_copy(const std::string& text) {
    std::size_t end = text.size();
    while (end > 0 && std::isblank(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(0, end);
}

/// Trim both leading and trailing whitespace.
std::string trim_copy(const std::string& text) { return rtrim_copy(ltrim_copy(text)); }

/// Validate a single character against the ZPL name grammar.
bool is_valid_name_char(char ch) noexcept {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '$' || ch == '-' || ch == '_' || ch == '@' ||
           ch == '.' || ch == '&' || ch == '+' || ch == '/';
}

/// Split a path string into segments, honoring leading '/' and rejecting empty segments.
std::vector<std::string> split_segments(const std::string& path, bool& valid) {
    valid = true;
    std::vector<std::string> segments;

    std::size_t pos = 0;
    while (pos < path.size() && path[pos] == '/') {
        ++pos;
    }

    if (pos == path.size()) {
        return segments;
    }

    while (pos <= path.size()) {
        const std::size_t slash = path.find('/', pos);
        const std::size_t end = (slash == std::string::npos) ? path.size() : slash;
        if (end == pos) {
            valid = false;
            return {};
        }

        segments.emplace_back(path.substr(pos, end - pos));
        if (slash == std::string::npos) {
            break;
        }
        pos = slash + 1;
        if (pos == path.size()) {
            valid = false;
            return {};
        }
    }

    return segments;
}

/// Split content into lines, supporting LF, CR, or CRLF endings.
std::vector<std::string> split_lines(const std::string& content) {
    std::vector<std::string> lines;

    std::size_t start = 0;
    std::size_t i = 0;
    while (i < content.size()) {
        if (content[i] == '\n') {
            lines.emplace_back(content.substr(start, i - start));
            ++i;
            start = i;
            continue;
        }
        if (content[i] == '\r') {
            lines.emplace_back(content.substr(start, i - start));
            ++i;
            if (i < content.size() && content[i] == '\n') {
                ++i;
            }
            start = i;
            continue;
        }
        ++i;
    }

    if (start < content.size()) {
        lines.emplace_back(content.substr(start));
    } else if (!content.empty() && (content.back() == '\n' || content.back() == '\r')) {
        lines.emplace_back("");
    }

    return lines;
}

/// Parse a value fragment, honoring quoted values and inline comments.
std::string parse_value_fragment(const std::string& fragment, std::size_t line_number) {
    const std::string without_leading = ltrim_copy(fragment);
    if (without_leading.empty()) {
        return "";
    }

    const char first = without_leading.front();
    if (first == '\'' || first == '"') {
        const std::size_t closing_quote = without_leading.find(first, 1);
        if (closing_quote == std::string::npos) {
            throw zpl_parse_error("unterminated quoted value", line_number);
        }

        const std::string value = without_leading.substr(1, closing_quote - 1);
        const std::string trailing = without_leading.substr(closing_quote + 1);

        std::size_t pos = 0;
        while (pos < trailing.size() && std::isblank(static_cast<unsigned char>(trailing[pos]))) {
            ++pos;
        }

        if (pos == trailing.size()) {
            return value;
        }

        if (trailing[pos] == '#') {
            return value;
        }

        throw zpl_parse_error("invalid characters after quoted value", line_number);
    }

    const std::size_t comment_pos = without_leading.find('#');
    const std::string raw_value =
        (comment_pos == std::string::npos) ? without_leading : without_leading.substr(0, comment_pos);
    return rtrim_copy(raw_value);
}

/// Parse a single ZPL line into indentation, name, and value.
parsed_line_t parse_line(const std::string& line, std::size_t line_number) {
    std::size_t pos = 0;
    std::size_t leading_spaces = 0;

    while (pos < line.size()) {
        if (line[pos] == ' ') {
            ++leading_spaces;
            ++pos;
            continue;
        }
        if (line[pos] == '\t') {
            throw zpl_parse_error("tab indentation is not allowed", line_number);
        }
        break;
    }

    if ((leading_spaces % 4U) != 0U) {
        throw zpl_parse_error("indentation must be divisible by 4 spaces", line_number);
    }

    const std::string content = line.substr(pos);
    if (content.empty() || content.front() == '#') {
        return {leading_spaces / 4U, "", ""};
    }

    const std::size_t equal_pos = content.find('=');
    const std::size_t hash_pos = content.find('#');

    std::string name;
    std::string value;
    if (equal_pos == std::string::npos) {
        const std::size_t comment_pos = content.find('#');
        const std::string active = (comment_pos == std::string::npos) ? content : content.substr(0, comment_pos);
        name = trim_copy(active);
    } else if (hash_pos != std::string::npos && hash_pos < equal_pos) {
        const std::string active = content.substr(0, hash_pos);
        name = trim_copy(active);
    } else {
        name = trim_copy(content.substr(0, equal_pos));
        value = parse_value_fragment(content.substr(equal_pos + 1), line_number);
    }

    if (name.empty()) {
        throw zpl_parse_error("property name is empty", line_number);
    }

    if (name.front() == '/' || name.back() == '/') {
        throw zpl_parse_error("property name must not start or end with '/'", line_number);
    }

    for (char ch : name) {
        if (!is_valid_name_char(ch)) {
            throw zpl_parse_error("invalid character in property name", line_number);
        }
    }

    return {leading_spaces / 4U, std::move(name), std::move(value)};
}

/// Parse the entire input stream into a ZPL tree root node.
std::shared_ptr<zpl_node_t> parse_stream(std::istream& input) {
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        throw std::ios_base::failure("failed while reading stream");
    }
    if (input.fail() && !input.eof()) {
        throw std::ios_base::failure("failed while reading stream");
    }

    auto root = std::make_shared<zpl_node_t>();
    std::vector<std::shared_ptr<zpl_node_t>> stack;
    stack.push_back(root);

    const auto lines = split_lines(buffer.str());

    std::size_t previous_indent = 0;
    bool has_previous_property = false;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t line_number = i + 1;
        const parsed_line_t parsed = parse_line(lines[i], line_number);
        if (parsed.name.empty()) {
            continue;
        }

        if (has_previous_property && parsed.indent_level > previous_indent + 1) {
            throw zpl_parse_error("invalid indentation transition", line_number);
        }
        if (parsed.indent_level + 1 > stack.size()) {
            throw zpl_parse_error("invalid indentation transition", line_number);
        }

        stack.resize(parsed.indent_level + 1);
        auto current_parent = stack.back();

        bool path_valid = false;
        const auto segments = split_segments(parsed.name, path_valid);
        if (!path_valid || segments.empty()) {
            throw zpl_parse_error("invalid property name", line_number);
        }

        auto current = current_parent;
        for (std::size_t segment_idx = 0; segment_idx < segments.size(); ++segment_idx) {
            const bool is_last = (segment_idx + 1 == segments.size());
            const auto found = current->children_by_name.find(segments[segment_idx]);
            if (found == current->children_by_name.end()) {
                auto created = std::make_shared<zpl_node_t>();
                created->name = segments[segment_idx];
                current->ordered_children.push_back(created);
                current->children_by_name.emplace(segments[segment_idx], created);
                current = created;
            } else {
                current = found->second;
            }

            if (is_last) {
                if (current->explicitly_defined) {
                    throw zpl_parse_error("duplicate property name in same level", line_number);
                }
                current->explicitly_defined = true;
                current->value = parsed.value;
            }
        }

        stack.push_back(current);
        previous_indent = parsed.indent_level;
        has_previous_property = true;
    }

    return root;
}

/// Find a node by path segments starting from a given node.
std::shared_ptr<zpl_node_t> find_node(const std::shared_ptr<zpl_node_t>& start, const std::string& path) {
    if (!start) {
        return nullptr;
    }

    bool valid = false;
    const auto segments = split_segments(path, valid);
    if (!valid) {
        return nullptr;
    }

    auto current = start;
    for (const auto& segment : segments) {
        const auto found = current->children_by_name.find(segment);
        if (found == current->children_by_name.end()) {
            return nullptr;
        }
        current = found->second;
    }

    return current;
}

}  // namespace

struct zpl_config_t::impl_t {
    std::shared_ptr<zpl_node_t> node;  ///< Root node for the parsed tree
};

zpl_parse_error::zpl_parse_error(std::string message, std::size_t line) : zpl_error(std::move(message)), _line(line) {}

std::size_t zpl_parse_error::line() const noexcept { return _line; }

zpl_config_t::zpl_config_t() noexcept : _impl(std::make_shared<impl_t>()) {
    _impl->node = std::make_shared<zpl_node_t>();
}

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

void zpl_config_t::load(std::istream& input) { _impl->node = parse_stream(input); }

void zpl_config_t::load_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::ios_base::failure("unable to open file: " + file_path);
    }
    load(file);
}

bool zpl_config_t::empty() const noexcept { return !_impl || !_impl->node || _impl->node->ordered_children.empty(); }

const std::string& zpl_config_t::name() const noexcept {
    if (!_impl || !_impl->node) {
        return k_empty_string;
    }
    return _impl->node->name;
}

const std::string& zpl_config_t::value() const noexcept {
    if (!_impl || !_impl->node) {
        return k_empty_string;
    }
    return _impl->node->value;
}

bool zpl_config_t::contains(const std::string& path) const noexcept {
    return static_cast<bool>(find_node(_impl ? _impl->node : nullptr, path));
}

const std::string& zpl_config_t::get(const std::string& path) const {
    auto node = find_node(_impl ? _impl->node : nullptr, path);
    if (!node) {
        throw zpl_property_not_found("property not found: " + path);
    }
    return node->value;
}

std::optional<std::string> zpl_config_t::try_get(const std::string& path) const noexcept {
    auto node = find_node(_impl ? _impl->node : nullptr, path);
    if (!node) {
        return std::nullopt;
    }
    return node->value;
}

std::string zpl_config_t::get_or(const std::string& path, std::string default_value) const noexcept {
    auto node = find_node(_impl ? _impl->node : nullptr, path);
    if (!node) {
        return default_value;
    }
    return node->value;
}

zpl_config_t zpl_config_t::child(const std::string& path) const {
    auto node = find_node(_impl ? _impl->node : nullptr, path);
    if (!node) {
        throw zpl_property_not_found("property not found: " + path);
    }

    zpl_config_t result;
    result._impl->node = std::move(node);
    return result;
}

std::optional<zpl_config_t> zpl_config_t::try_child(const std::string& path) const noexcept {
    auto node = find_node(_impl ? _impl->node : nullptr, path);
    if (!node) {
        return std::nullopt;
    }

    zpl_config_t result;
    result._impl->node = std::move(node);
    return result;
}

std::vector<zpl_config_t> zpl_config_t::children() const noexcept {
    if (!_impl || !_impl->node) {
        return {};
    }

    std::vector<zpl_config_t> result;
    result.reserve(_impl->node->ordered_children.size());
    for (const auto& node : _impl->node->ordered_children) {
        zpl_config_t child;
        child._impl->node = node;
        result.push_back(std::move(child));
    }
    return result;
}

}  // namespace zmqzext
