#include <gtest/gtest.h>
#include "cppzmqzoltanext/zpl_config.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace zmqzext {

class ZplConfigTest : public ::testing::Test {
protected:
    std::string temp_file_path = "/tmp/test_zpl_config.zpl";

    void SetUp() override {
        // Clean up any temporary files
        if (std::filesystem::exists(temp_file_path)) {
            std::filesystem::remove(temp_file_path);
        }
    }

    void TearDown() override {
        // Clean up any temporary files
        if (std::filesystem::exists(temp_file_path)) {
            std::filesystem::remove(temp_file_path);
        }
    }

    std::string create_temp_zpl_file(const std::string& content) {
        // Create a temporary file with the given content
        std::ofstream temp_file(temp_file_path);
        if (!temp_file) {
            throw std::runtime_error("Failed to create temporary file");
        }
        temp_file << content;
        temp_file.close();
        return temp_file_path;
    }
};

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST_F(ZplConfigTest, DefaultConstructorCreatesEmptyConfig) {
    zpl_config_t config;
    EXPECT_FALSE(config.contains("any_property"));
}

TEST_F(ZplConfigTest, ParsesSinglePropertyFromStream) {
    std::istringstream input("key = value");
    zpl_config_t config(input);
}

TEST_F(ZplConfigTest, ParsesSimpleKeyValuePair) {
    std::istringstream input("name = John");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("name"));
    EXPECT_EQ(config.get("name"), "John");
}

TEST_F(ZplConfigTest, ParsesMultipleProperties) {
    std::istringstream input(
        "key1 = value1\n"
        "key2 = value2\n"
        "key3 = value3"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("key1"));
    ASSERT_TRUE(config.contains("key2"));
    ASSERT_TRUE(config.contains("key3"));
    EXPECT_EQ(config.get("key1"), "value1");
    EXPECT_EQ(config.get("key2"), "value2");
    EXPECT_EQ(config.get("key3"), "value3");
}

// ============================================================================
// Value Retrieval Tests
// ============================================================================

TEST_F(ZplConfigTest, GetThrowsWhenPropertyNotFound) {
    std::istringstream input("key = value");
    zpl_config_t config(input);

    EXPECT_THROW(config.get("nonexistent"), zpl_property_not_found);
}

TEST_F(ZplConfigTest, GetReturnsEmptyValueWhenPropertyWithoutValue) {
    std::istringstream input("key");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("key"));
    EXPECT_EQ(config.get("key"), "");
}

TEST_F(ZplConfigTest, GetOrReturnsDefaultValueWhenNotFound) {
    std::istringstream input("key = value");
    zpl_config_t config(input);

    EXPECT_EQ(config.get_or("nonexistent", "default"), "default");
}

TEST_F(ZplConfigTest, TryGetReturnsValueWhenFound) {
    std::istringstream input("name = John");
    zpl_config_t config(input);

    auto result = config.try_get("name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "John");
}

TEST_F(ZplConfigTest, TryGetReturnsEmptyOptionalWhenNotFound) {
    std::istringstream input("name = John");
    zpl_config_t config(input);

    auto result = config.try_get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Comments and Empty Lines Tests
// ============================================================================

TEST_F(ZplConfigTest, IgnoresCommentLines) {
    std::istringstream input(
        "# This is a comment\n"
        "key = value"
    );
    zpl_config_t config(input);

    EXPECT_FALSE(config.contains("#"));
    ASSERT_TRUE(config.contains("key"));
    EXPECT_EQ(config.get("key"), "value");
}

TEST_F(ZplConfigTest, IgnoresEmptyLines) {
    std::istringstream input(
        "key1 = value1\n"
        "\n"
        "key2 = value2"
    );
    zpl_config_t config(input);

    EXPECT_TRUE(config.contains("key1"));
    EXPECT_TRUE(config.contains("key2"));
}

TEST_F(ZplConfigTest, IgnoresCommentAtEndOfLine) {
    std::istringstream input(
        "key = value # comment"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("key"));
    EXPECT_EQ(config.get("key"), "value");
}

TEST_F(ZplConfigTest, CommentAfterNameWithoutValueIsIgnored) {
    std::istringstream input(
        "flag # feature toggle\n"
        "key = value"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("flag"));
    EXPECT_EQ(config.get("flag"), "");
    EXPECT_EQ(config.get("key"), "value");
}

// ============================================================================
// Quoted Values Tests
// ============================================================================

TEST_F(ZplConfigTest, ParsesSingleQuotedValue) {
    std::istringstream input("message = 'hello world'");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello world");
}

TEST_F(ZplConfigTest, ParsesDoubleQuotedValue) {
    std::istringstream input("message = \"hello world\"");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello world");
}

TEST_F(ZplConfigTest, QuotedValueWithSpaces) {
    std::istringstream input("message = \"hello  world  \"");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello  world  ");
}

TEST_F(ZplConfigTest, QuotedValueWithCommentsInside) {
    std::istringstream input("message = \"hello world # this is not a comment\"");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello world # this is not a comment");
}

TEST_F(ZplConfigTest, UnmatchedQuoteArePartOfValue) {
    // When quote doesn't match, it should be treated as unquoted
    std::istringstream input("message = 'unmatched");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "'unmatched");
}

TEST_F(ZplConfigTest, SingleQuoteInsideDoubleQuotedValue) {
    std::istringstream input("message = \"hello 'world'\"");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello 'world'");
}

TEST_F(ZplConfigTest, DoubleQuoteInsideSingleQuotedValue) {
    std::istringstream input("message = 'hello \"world\"'");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello \"world\"");
}

// ============================================================================
// Whitespace Handling Tests
// ============================================================================

TEST_F(ZplConfigTest, TrimsWhitespaceAroundEquals) {
    std::istringstream input("key   =   value");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("key"));
    EXPECT_EQ(config.get("key"), "value");
}

TEST_F(ZplConfigTest, PreservesWhitespaceInsideValue) {
    std::istringstream input("message = hello   world");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello   world");
}

TEST_F(ZplConfigTest, StripsWhitespaceAfterUnquotedValue) {
    std::istringstream input("message = hello world   ");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("message"));
    EXPECT_EQ(config.get("message"), "hello world");
}

// ============================================================================
// Property Names Tests
// ============================================================================

TEST_F(ZplConfigTest, PropertyNamesWithValidCharacters) {
    std::istringstream input(
        "simple = v1\n"
        "with$dollar = v2\n"
        "with-dash = v3\n"
        "with_underscore = v4\n"
        "with@at = v5\n"
        "with.dot = v6\n"
        "with&amper = v7\n"
        "with+plus = v8\n"
        "path/to/file = v9"
    );
    zpl_config_t config(input);

    EXPECT_TRUE(config.contains("simple"));
    EXPECT_TRUE(config.contains("with$dollar"));
    EXPECT_TRUE(config.contains("with-dash"));
    EXPECT_TRUE(config.contains("with_underscore"));
    EXPECT_TRUE(config.contains("with.dot"));
    EXPECT_TRUE(config.contains("with@at"));
    EXPECT_TRUE(config.contains("with&amper"));
    EXPECT_TRUE(config.contains("with+plus"));
    EXPECT_TRUE(config.contains("path/to/file"));
}

TEST_F(ZplConfigTest, InvalidNameCharacterThrowsParseError) {
    std::istringstream input("bad*name = value");

    EXPECT_THROW((void)zpl_config_t(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, PropertyWithoutValue) {
    std::istringstream input("flag");
    zpl_config_t config(input);

    EXPECT_TRUE(config.contains("flag"));
}

// ============================================================================
// Hierarchy and Indentation Tests
// ============================================================================

TEST_F(ZplConfigTest, ParsesHierarchicalStructure) {
    std::istringstream input(
        "server\n"
        "    address = tcp://127.0.0.1\n"
        "    port = 5555"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server"));
    ASSERT_TRUE(config.contains("server/address"));
    ASSERT_TRUE(config.contains("server/port"));
    EXPECT_EQ(config.get("server/address"), "tcp://127.0.0.1");
    EXPECT_EQ(config.get("server/port"), "5555");
}

TEST_F(ZplConfigTest, ParsesNestedHierarchy) {
    std::istringstream input(
        "server\n"
        "    options\n"
        "        reuse_addr = true\n"
        "        linger = 1000"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server/options/reuse_addr"));
    ASSERT_TRUE(config.contains("server/options/linger"));
    EXPECT_EQ(config.get("server/options/reuse_addr"), "true");
    EXPECT_EQ(config.get("server/options/linger"), "1000");
}

TEST_F(ZplConfigTest, ForwardSlashInNameBuildsHierarchy) {
    std::istringstream input("foo/bar = value");
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("foo"));
    ASSERT_TRUE(config.contains("foo/bar"));
    EXPECT_EQ(config.get("foo/bar"), "value");
}

TEST_F(ZplConfigTest, PathWithLeadingSlash) {
    std::istringstream input(
        "database\n"
        "    host = localhost"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("/database/host"));
    EXPECT_EQ(config.get("/database/host"), "localhost");
}

TEST_F(ZplConfigTest, GetChildConfiguration) {
    std::istringstream input(
        "server\n"
        "    address = tcp://127.0.0.1\n"
        "    port = 5555"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server"));
    const zpl_config_t server_config = config.child("server");
    EXPECT_TRUE(server_config.contains("address"));
    EXPECT_TRUE(server_config.contains("port"));
}

TEST_F(ZplConfigTest, GetChildConfigurationWithPath) {
    std::istringstream input(
        "server\n"
        "    options\n"
        "        reuse_addr = true"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server/options"));
    const zpl_config_t options = config.child("server/options");
    EXPECT_TRUE(options.contains("reuse_addr"));
}

TEST_F(ZplConfigTest, GetChildOfChildConfiguration) {
    std::istringstream input(
        "server\n"
        "    options\n"
        "        reuse_addr = true"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server"));
    const zpl_config_t server = config.child("server");
    ASSERT_TRUE(server.contains("options"));
    const zpl_config_t options = server.child("options");
    EXPECT_TRUE(options.contains("reuse_addr"));
}

// ============================================================================
// Children Tests
// ============================================================================

TEST_F(ZplConfigTest, ChildConfigurationNameAndValue) {
    std::istringstream input(
        "server = main\n"
        "    address = localhost"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server"));
    const zpl_config_t server = config.child("server");
    EXPECT_EQ(server.name(), "server");
    EXPECT_EQ(server.value(), "main");
}

TEST_F(ZplConfigTest, ConfigurationWithoutValueReturnsEmptyString) {
    std::istringstream input(
        "server\n"
        "    address = localhost"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server"));
    const zpl_config_t server = config.child("server");
    EXPECT_EQ(server.value(), "");
}

TEST_F(ZplConfigTest, ChildThrowsWhenNotFound) {
    std::istringstream input(
        "server\n"
        "    address = value"
    );
    zpl_config_t config(input);

    EXPECT_THROW(config.child("nonexistent"), zpl_property_not_found);
}

TEST_F(ZplConfigTest, TryChildReturnsChildWhenFound) {
    std::istringstream input(
        "server\n"
        "    address = value"
    );
    zpl_config_t config(input);

    auto result = config.try_child("server");
    EXPECT_TRUE(result.has_value());
}

TEST_F(ZplConfigTest, TryChildReturnsEmptyOptionalWhenNotFound) {
    std::istringstream input(
        "server\n"
        "    address = value"
    );
    zpl_config_t config(input);

    auto result = config.try_child("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ZplConfigTest, ChildrenPreserveRootSiblingOrder) {
    std::istringstream input(
        "first = one\n"
        "second = two\n"
        "third = three"
    );
    zpl_config_t config(input);

    const auto children = config.children();
    ASSERT_EQ(children.size(), 3U);
    EXPECT_EQ(children[0].name(), "first");
    EXPECT_EQ(children[1].name(), "second");
    EXPECT_EQ(children[2].name(), "third");
}

TEST_F(ZplConfigTest, ChildrenPreserveNestedSiblingOrder) {
    std::istringstream input(
        "server\n"
        "    first = one\n"
        "    second = two\n"
        "    third = three"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("server"));
    const auto children = config.child("server").children();
    ASSERT_EQ(children.size(), 3U);
    EXPECT_EQ(children[0].name(), "first");
    EXPECT_EQ(children[1].name(), "second");
    EXPECT_EQ(children[2].name(), "third");
}

TEST_F(ZplConfigTest, LeadingSlashIsIgnoredByChildLookup) {
    std::istringstream input(
        "database\n"
        "    host = localhost"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("/database/host"));
    const auto host = config.child("/database/host");
    EXPECT_EQ(host.name(), "host");
    EXPECT_EQ(host.value(), "localhost");
}

// ============================================================================
// Loading Tests
// ============================================================================

TEST_F(ZplConfigTest, LoadFromStream) {
    std::istringstream input(
        "server\n"
        "    address = tcp://127.0.0.1\n"
        "    port = 5555"
    );

    zpl_config_t config;
    config.load(input);

    ASSERT_TRUE(config.contains("server/address"));
    EXPECT_EQ(config.get("server/address"), "tcp://127.0.0.1");
}

TEST_F(ZplConfigTest, LoadFromFile) {
    std::string content =
        "server\n"
        "    address = tcp://127.0.0.1\n"
        "    port = 5555";

    std::string file_path = create_temp_zpl_file(content);
    zpl_config_t config;
    config.load_from_file(file_path);

    ASSERT_TRUE(config.contains("server/address"));
    EXPECT_EQ(config.get("server/address"), "tcp://127.0.0.1");
}

TEST_F(ZplConfigTest, LoadFromFileThrowsOnInvalidPath) {
    zpl_config_t config;
    EXPECT_THROW(config.load_from_file("/nonexistent/path/file.zpl"),
                 std::ios_base::failure);
}

TEST_F(ZplConfigTest, LoadClearsExistingConfiguration) {
    zpl_config_t config;

    std::istringstream input1("key1 = value1");
    config.load(input1);
    ASSERT_TRUE(config.contains("key1"));

    std::istringstream input2("key2 = value2");
    config.load(input2);
    EXPECT_FALSE(config.contains("key1"));
    EXPECT_TRUE(config.contains("key2"));
}

TEST_F(ZplConfigTest, LoadFromFileClearsExistingConfiguration) {
    zpl_config_t config;

    std::istringstream input("key1 = value1");
    config.load(input);
    ASSERT_TRUE(config.contains("key1"));

    std::string content = "key2 = value2";
    std::string file_path = create_temp_zpl_file(content);
    config.load_from_file(file_path);

    EXPECT_FALSE(config.contains("key1"));
    EXPECT_TRUE(config.contains("key2"));
}

TEST_F(ZplConfigTest, ChildHandleRemainsValidAfterParentReload) {
    std::istringstream input(
        "server\n"
        "    host = localhost\n"
        "        timeout = 1000"
    );
    zpl_config_t config(input);
    ASSERT_TRUE(config.contains("server"));
    zpl_config_t server = config.child("server");

    std::istringstream replacement(
        "other\n"
        "    value = 1"
    );
    config.load(replacement);

    ASSERT_TRUE(server.contains("host"));
    zpl_config_t host = server.child("host");
    EXPECT_TRUE(host.contains("timeout"));
    EXPECT_TRUE(config.contains("other/value"));
}

TEST_F(ZplConfigTest, LoadProvidesStrongExceptionGuaranteeOnParseError) {
    zpl_config_t config;
    std::istringstream valid_input("server\n    host = localhost");
    config.load(valid_input);
    ASSERT_TRUE(config.contains("server/host"));

    std::istringstream invalid_input(
        "dup = one\n"
        "dup = two"
    );
    EXPECT_THROW(config.load(invalid_input), zpl_parse_error);

    ASSERT_TRUE(config.contains("server/host"));
    EXPECT_EQ(config.get("server/host"), "localhost");
    EXPECT_FALSE(config.contains("dup"));
}

TEST_F(ZplConfigTest, LoadFromFileProvidesStrongExceptionGuaranteeOnParseError) {
    zpl_config_t config;
    std::istringstream valid_input("server\n    host = localhost");
    config.load(valid_input);
    ASSERT_TRUE(config.contains("server/host"));

    std::string invalid_file = create_temp_zpl_file(
        "dup = one\n"
        "dup = two"
    );
    EXPECT_THROW(config.load_from_file(invalid_file), zpl_parse_error);

    ASSERT_TRUE(config.contains("server/host"));
    EXPECT_EQ(config.get("server/host"), "localhost");
    EXPECT_FALSE(config.contains("dup"));
}

// ============================================================================
// Static Factory Methods Tests
// ============================================================================

TEST_F(ZplConfigTest, StaticFactoryFromStreamLoadsConfiguration) {
    std::istringstream input("server\n    address = tcp://127.0.0.1");
    zpl_config_t config = zpl_config_t::from_stream(input);

    ASSERT_TRUE(config.contains("server/address"));
    EXPECT_EQ(config.get("server/address"), "tcp://127.0.0.1");
}

TEST_F(ZplConfigTest, StaticFactoryFromFileLoadsConfiguration) {
    std::string file_path = create_temp_zpl_file("database\n    host = localhost");

    zpl_config_t config = zpl_config_t::from_file(file_path);
    ASSERT_TRUE(config.contains("database/host"));
    EXPECT_EQ(config.get("database/host"), "localhost");
}

// ============================================================================
// Copy and Move Semantics Tests
// ============================================================================

TEST_F(ZplConfigTest, CopyConstructor) {
    std::istringstream input("key = value");
    zpl_config_t config1(input);

    zpl_config_t config2 = config1;
    EXPECT_TRUE(config2.contains("key"));
    EXPECT_EQ(config2.get("key"), "value");
}

TEST_F(ZplConfigTest, CopyAssignment) {
    std::istringstream input1("key1 = value1");
    zpl_config_t config1(input1);

    std::istringstream input2("key2 = value2");
    zpl_config_t config2(input2);

    config2 = config1;
    EXPECT_TRUE(config2.contains("key1"));
    EXPECT_FALSE(config2.contains("key2"));
}

// TEST_F(ZplConfigTest, MoveConstructor) {
//     std::istringstream input("key = value");
//     zpl_config_t config1(input);

//     zpl_config_t config2 = std::move(config1);
//     EXPECT_TRUE(config2.contains("key"));
//     EXPECT_EQ(config2.get("key"), "value");

//     EXPECT_FALSE(config1.contains("key"));
// }

// TEST_F(ZplConfigTest, MoveAssignment) {
//     std::istringstream input1("key1 = value1");
//     zpl_config_t config1(input1);

//     std::istringstream input2("key2 = value2");
//     zpl_config_t config2(input2);

//     config2 = std::move(config1);
//     EXPECT_TRUE(config2.contains("key1"));
//     EXPECT_FALSE(config2.contains("key2"));

//     EXPECT_FALSE(config1.contains("key"));
// }

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(ZplConfigTest, EmptyConfiguration) {
    std::istringstream input("");
    zpl_config_t config(input);

    EXPECT_FALSE(config.contains("anything"));
}

TEST_F(ZplConfigTest, OnlyComments) {
    std::istringstream input(
        "# Comment 1\n"
        "# Comment 2\n"
        "# Comment 3"
    );
    zpl_config_t config(input);

    EXPECT_FALSE(config.contains("anything"));
}

TEST_F(ZplConfigTest, OnlyWhitespace) {
    std::istringstream input("   \n  \n   ");
    zpl_config_t config(input);

    EXPECT_FALSE(config.contains("anything"));
}

TEST_F(ZplConfigTest, DuplicatePropertyNameAtSameLevelThrowsParseError) {
    std::istringstream input(
        "key = v1\n"
        "key = v2"
    );

    zpl_config_t config;
    EXPECT_THROW(config.load(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, DuplicatePropertyNameInNestedLevelThrowsParseError) {
    std::istringstream input(
        "server\n"
        "    port = 5555\n"
        "    port = 5556"
    );

    zpl_config_t config;
    EXPECT_THROW(config.load(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, MultilineNotSupported) {
    // ZPL doesn't support multiline values
    // This test documents expected behavior
    std::istringstream input(
        "line1 = 'value\n"
        "continuation'\n" // this is not a real value continuation, so it has an invalid ' character in name property
        "line2 = value2"
    );

    zpl_config_t config;
    EXPECT_THROW(config.load(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, CaseSensitiveProperties) {
    // This test documents whether property names are case-sensitive
    std::istringstream input(
        "Key = value1\n"
        "key = value2"
    );
    zpl_config_t config(input);

    ASSERT_TRUE(config.contains("Key"));
    ASSERT_TRUE(config.contains("key"));
    EXPECT_EQ(config.get("Key"), "value1");
    EXPECT_EQ(config.get("key"), "value2");
}

TEST_F(ZplConfigTest, IndentationWithTabsThrowsParseError) {
    std::istringstream input(
        "server\n"
        "\tport = 5555"
    );
    zpl_config_t config;

    EXPECT_THROW(config.load(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, IndentationNotDivisibleByFourThrowsParseError) {
    std::istringstream input(
        "server\n"
        "   port = 5555"
    );
    zpl_config_t config;

    EXPECT_THROW(config.load(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, InvalidIndentationTransitionThrowsParseError) {
    std::istringstream input(
        "server\n"
        "        port = 5555"
    );
    zpl_config_t config;

    EXPECT_THROW(config.load(input), zpl_parse_error);
}

TEST_F(ZplConfigTest, ParseErrorIncludesViolationLine) {
    std::istringstream input(
        "key = value\n"
        "   invalid_indent = true"
    );
    zpl_config_t config;

    try {
        config.load(input);
        FAIL() << "Expected zpl_parse_error";
    } catch (const zpl_parse_error& error) {
        EXPECT_EQ(error.line(), 2U);
    } catch (...) {
        FAIL() << "Expected zpl_parse_error";
    }
}

TEST_F(ZplConfigTest, DifferentLineEndingsLF) {
    // Test support for LF line endings (\n)
    std::istringstream input_lf("key1 = value1\nkey2 = value2\nkey3 = value3");
    zpl_config_t config_lf(input_lf);
    EXPECT_TRUE(config_lf.contains("key1"));
    EXPECT_TRUE(config_lf.contains("key2"));
    EXPECT_TRUE(config_lf.contains("key3"));
}

TEST_F(ZplConfigTest, DifferentLineEndingsCR) {
    // Test support for CR line endings (\r)
    std::istringstream input_cr("key1 = value1\rkey2 = value2\rkey3 = value3");
    zpl_config_t config_cr(input_cr);
    EXPECT_TRUE(config_cr.contains("key1"));
    EXPECT_TRUE(config_cr.contains("key2"));
    EXPECT_TRUE(config_cr.contains("key3"));
}

TEST_F(ZplConfigTest, DifferentLineEndingsCRLF) {
    // Test support for CRLF line endings (\r\n)
    std::istringstream input_crlf("key1 = value1\r\nkey2 = value2\r\nkey3 = value3");
    zpl_config_t config_crlf(input_crlf);
    EXPECT_TRUE(config_crlf.contains("key1"));
    EXPECT_TRUE(config_crlf.contains("key2"));
    EXPECT_TRUE(config_crlf.contains("key3"));
}

TEST_F(ZplConfigTest, MixedLineEndings) {
    // Test support for mixed line endings
    std::istringstream input_mixed("key1 = value1\nkey2 = value2\r\nkey3 = value3\rkey4 = value4");
    zpl_config_t config_mixed(input_mixed);
    EXPECT_TRUE(config_mixed.contains("key1"));
    EXPECT_TRUE(config_mixed.contains("key2"));
    EXPECT_TRUE(config_mixed.contains("key3"));
    EXPECT_TRUE(config_mixed.contains("key4"));
}

// ============================================================================
// Complex Real-World Scenarios
// ============================================================================

TEST_F(ZplConfigTest, ComplexZmlConfiguration) {
    std::istringstream input(
        "# Server configuration\n"
        "server\n"
        "    address = tcp://127.0.0.1:5555\n"
        "    thread_pool = 4\n"
        "    options\n"
        "        reuse_addr = true\n"
        "        linger = 1000\n"
        "    verbose = true\n"
        "\n"
        "# Database configuration\n"
        "database\n"
        "    host = localhost\n"
        "    port = 5432\n"
        "    credentials\n"
        "        user = admin\n"
        "        password = secret"
    );
    zpl_config_t config(input);

    EXPECT_EQ(config.get("server/address"), "tcp://127.0.0.1:5555");
    EXPECT_EQ(config.get("server/options/linger"), "1000");
    EXPECT_EQ(config.get("database/credentials/user"), "admin");
}

TEST_F(ZplConfigTest, ConfigurationWithMixedQuoting) {
    std::istringstream input(
        "name = 'John Doe'\n"
        "email = \"user@example.com\"\n"
        "city = San Francisco"
    );
    zpl_config_t config(input);

    EXPECT_EQ(config.get("name"), "John Doe");
    EXPECT_EQ(config.get("email"), "user@example.com");
    EXPECT_EQ(config.get("city"), "San Francisco");
}

} // namespace zmqzext
