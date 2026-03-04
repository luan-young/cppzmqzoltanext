# ZPL Config Requirements

The `zpl_config_t` class provides functionality to load and access configuration data from ZPL (ZeroMQ Property Language) files.

## ZPL Files Specification

Here we summarize the RFC 4/ZeroMQ Protocol Specification (https://rfc.zeromq.org/spec/4/):

- ZPL is an ASCII text format that uses whitespace - line endings and indentation - for framing and hierarchy. ZPL data consists of a series of properties encoded as name/value pairs, one per line, where the name may be structured, and where the value is an untyped string.
- Implementations should treat any of the following sequences as a line-ending: newline (%x0A), carriage-return (%x0D), or carriage-return followed by newline (%x0D %x0A).
- Whitespace is significant only before property names and inside values.
- Text starting with `#` is discarded as a comment.
- Each non-empty line defines a property consisting of a name and an optional value.
- Values are untyped strings which the application may interpret in any way it wishes.
- An entire value can be enclosed with single or double quotes, which do not form part of the value.
- Any printable character except the closing quote is valid in a quoted string.
- A value that starts with a quote and does not end in a matching quote is treated as unquoted.
- There is no mechanism for escaping quotes or other characters in a quoted string.
- The only special characters in ZPL are: whitespace, `#`, `=`, and single and double quotes.
- Hierarchy is signaled by indentation, where a child is indented 4 spaces more than its parent.
- The first non-whitespace character in a ZPL file is always either `#` or an alphanumeric character.
- Whitespace after a value is discarded unless within valid quotes.

Names SHALL match this grammar:

```text
name = *name-char
name-char = ALPHA | DIGIT | "$" | "-" | "_" | "@" | "." | "&" | "+" | "/"
```

## Class Requirements

### Construction and Loading

- The class must provide a default constructor that initializes an empty configuration tree.
- The class must provide a constructor that accepts an input stream and loads ZPL data from it.
- The class must provide methods to load configuration data from file path and from input stream.
- Load methods must replace the current configuration only after a successful parse (strong exception guarantee).
- The class should provide static factory helpers for loading from file and from string.

### Core Data Model

- Parsed configuration data must be stored in a hierarchical structure preserving source order of sibling properties.
- Each node must keep:
  - property name
  - property value (possibly empty)
  - ordered child nodes
- A `zpl_config_t` object must represent a handle to a node in this hierarchy.
- Child handles returned by lookup must remain valid while any handle to the same parsed tree exists.

### Lookup and Navigation

- Path lookup is relative to the current node.
- A leading `/` in path strings must be accepted and ignored.
- The class must provide:
  - `contains(path)` to check property existence.
  - `child(path)` to retrieve a child node and throw `zpl_property_not_found` if missing.
  - `try_child(path)` returning an empty optional when missing.
  - `value()` to retrieve the current node value.
  - `get(path)` to retrieve the value at a path and throw on missing property.
  - `get_or(path, default_value)` to retrieve with default fallback.
  - `try_get(path)` returning optional value.
  - `children()` to enumerate child nodes.

### Forward Slash in Names

- A forward slash `/` in ZPL names is valid and each slash is treated as a path segment separator in the tree structure.
- For example, a property named `foo/bar` at the root level is treated as a property named `foo` with a child property named `bar`.

### Duplicate Name Semantics

- Duplicate property names at the same level are not alloed. If encountered, the parser must throw a `zpl_parse_error`.

### Comments

- A line starting with `#` is a comment line and must be ignored entirely.
- Comments with `#` are allowed after a property value or after a name with no value, and must be ignored by the parser.
- For example, `foo=bar # this is a comment` defines a property named `foo` with value `bar`, and the comment is discarded.

### Error Model

- `zpl_parse_error` must be thrown for syntax/structure violations.
- standard exceptions must be thrown for file open/read failures.
- `zpl_property_not_found` must be thrown by throwing lookup methods when property is absent.
- `zpl_parse_error` must carry line of the violation information.

### Parsing Conformance

- Name validation must follow RFC 4 grammar.
- Indentation errors must be detected:
  - tabs used for indentation
  - indentation not divisible by 4 spaces
  - invalid indentation transitions in hierarchy
- Value parsing must follow RFC 4 semantics for comments, quoting, and trailing whitespace.
