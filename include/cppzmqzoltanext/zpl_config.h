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
 * @brief ZPL (ZeroMQ Property Language) parser
 *
 * This header provides the zpl_config_t class for parsing and navigating ZPL
 * configuration files (https://rfc.zeromq.org/spec/4/).
 * The parser builds a hierarchical tree of properties,
 * preserving the original order of siblings for stable iteration.
 *
 * @details
 * ZPL file structure:
 * - Each non-empty line defines a property name with an optional value separated by a equals sign ('=').
 * - Whitespace before a name defines indentation level (4 spaces per level).
 * - Child properties are indented exactly one level deeper than their parent.
 * - A line starting with '#' is a comment and is ignored.
 * - Inline comments are allowed after a name or value and start with '#'.
 * - Values are untyped strings; quoted values may use single or double quotes.
 * - Quotes are not escaped; a missing closing quote is a parse error.
 * - Property names may include '/', which is treated as a path separator.
 *
 * Class rules and behavior:
 * - Parsing builds a tree of nodes with name, value, and ordered children.
 * - Lookup paths are relative to the current node; a leading '/' is ignored.
 * - Throwing lookup functions signal missing properties or invalid paths.
 * - try_* functions return empty optionals instead of throwing.
 * - children() returns direct children in source order.
 *
 * Error model:
 * - zpl_parse_error for syntax/structure violations (with line number).
 * - zpl_property_not_found for missing properties on throwing lookups.
 * - std::ios_base::failure for stream or file read failures.
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

#include <cstddef>
#include <istream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Base exception for ZPL configuration errors
 *
 * Used as the common base for all zpl_config_t related exceptions.
 */
class CZZE_EXPORT zpl_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Exception thrown when a badly formatted ZPL input is encountered during parsing
 */
class CZZE_EXPORT zpl_parse_error : public zpl_error {
public:
    /**
     * @brief Construct a parse error with message and line
     *
     * @param message Error message describing the failure
     * @param line Line number where the parse error occurred (1-based)
     */
    zpl_parse_error(std::string message, std::size_t line);

    /**
     * @brief Get the line number where the parse error occurred
     *
     * @return 1-based line number
     */
    std::size_t line() const noexcept;

private:
    std::size_t _line;  ///< 1-based line number for the parse error
};

/**
 * @brief Exception thrown when a requested property is missing
 */
class CZZE_EXPORT zpl_property_not_found : public zpl_error {
public:
    using zpl_error::zpl_error;
};

/**
 * @brief ZPL configuration tree loaded from text or file
 *
 * The zpl_config_t class provides a read-only view over a parsed ZPL
 * (ZeroMQ Property Language) configuration. Each instance represents a node
 * in the configuration tree. Nodes expose their name and value, and can be
 * queried for children by path.
 *
 * Paths are expressed in ZPL relative/path/to/property notation (e.g. "child/key")
 * relative to the current configuration node. A property in the tree can be accessed
 * by its full path from the root or by navigating through all path segments.
 * Query methods throw when a path is invalid or missing, while try_* variants
 * return empty optionals or defaults.
 *
 * @note This class is not thread-safe.
 */
class CZZE_EXPORT zpl_config_t {
public:
    /**
     * @brief Construct an empty configuration
     */
    zpl_config_t() noexcept;

    /**
     * @brief Construct and parse a configuration from a stream
     *
     * @param input Input stream containing ZPL text
     * @throws zpl_parse_error on parse errors
     * @throws std::ios_base::failure if any error during input reading occurs
     */
    explicit zpl_config_t(std::istream& input);

    /**
     * @brief Parse and return a configuration from a stream
     *
     * @param input Input stream containing ZPL text
     * @return Parsed configuration
     * @throws zpl_parse_error on parse errors
     * @throws std::ios_base::failure if any error during input reading occurs
     */
    static zpl_config_t from_stream(std::istream& input);

    /**
     * @brief Parse and return a configuration from a file
     *
     * @param file_path Path to the ZPL file
     * @return Parsed configuration
     * @throws zpl_parse_error on parse errors
     * @throws std::ios_base::failure if the path is invalid or any error during file reading occurs
     */
    static zpl_config_t from_file(const std::string& file_path);

    /**
     * @brief Load configuration data from a stream
     *
     * Replaces the current configuration with the parsed content.
     *
     * @param input Input stream containing ZPL text
     * @throws zpl_parse_error on parse errors
     * @throws std::ios_base::failure if any error during input reading occurs
     */
    void load(std::istream& input);

    /**
     * @brief Load configuration data from a file
     *
     * Replaces the current configuration with the parsed content.
     *
     * @param file_path Path to the ZPL file
     * @throws zpl_parse_error on parse errors
     * @throws std::ios_base::failure if the path is invalid or any error during file reading occurs
     */
    void load_from_file(const std::string& file_path);

    /**
     * @brief Check whether the configuration is empty
     *
     * @return true if no root node is present, false otherwise
     */
    bool empty() const noexcept;

    /**
     * @brief Get the node name
     *
     * @return Node name
     */
    const std::string& name() const noexcept;

    /**
     * @brief Get the node value
     *
     * @return Node value
     */
    const std::string& value() const noexcept;

    /**
     * @brief Check if a path exists
     *
     * @param path ZPL path to a property relative to this node
     * @return true if the path exists, false otherwise
     */
    bool contains(const std::string& path) const noexcept;

    /**
     * @brief Get a property value by path
     *
     * @param path ZPL path to a property relative to this node
     * @return Property value
     * @throws zpl_property_not_found if the path does not exist
     */
    const std::string& get(const std::string& path) const;

    /**
     * @brief Try to get a property value by path
     *
     * @param path ZPL path to a property relative to this node
     * @return Property value if found, std::nullopt otherwise
     */
    std::optional<std::string> try_get(const std::string& path) const noexcept;

    /**
     * @brief Get a property value or return a default
     *
     * @param path ZPL path to a property relative to this node
     * @param default_value Value returned when the property is not found
     * @return Property value if found, otherwise default_value
     */
    std::string get_or(const std::string& path, std::string default_value) const noexcept;

    /**
     * @brief Get a child node by path
     *
     * @param path ZPL path to a property relative to this node
     * @return Child configuration node
     * @throws zpl_property_not_found if the path does not exist
     */
    zpl_config_t child(const std::string& path) const;

    /**
     * @brief Try to get a child node by path
     *
     * @param path ZPL path to a property relative to this node
     * @return Child configuration node if found, std::nullopt otherwise
     */
    std::optional<zpl_config_t> try_child(const std::string& path) const noexcept;

    /**
     * @brief Get direct children of this node
     *
     * @return Vector of child configuration nodes
     */
    std::vector<zpl_config_t> children() const noexcept;

private:
    /**
     * @brief Pimpl for configuration storage
     */
    struct impl_t;
    std::shared_ptr<impl_t> _impl;  ///< Shared parsed-tree state
};

}  // namespace zmqzext
