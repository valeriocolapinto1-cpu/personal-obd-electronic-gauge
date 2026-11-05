// SPDX-License-Identifier: GPL-3.0-or-later
//
// Small strict JSON (RFC 8259) parser producing a read-only DOM.
// Used for car profiles and themes. Bounded nesting depth, no exceptions.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace core::json {

enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };

class Document;

/// Lightweight handle to a node of a Document. Invalid handles behave like Null.
class Value {
public:
    Value() = default;

    bool valid() const { return doc_ != nullptr; }
    Type type() const;
    bool is_null() const { return type() == Type::Null; }
    bool is_bool() const { return valid() && type() == Type::Bool; }
    bool is_number() const { return valid() && type() == Type::Number; }
    bool is_string() const { return valid() && type() == Type::String; }
    bool is_array() const { return valid() && type() == Type::Array; }
    bool is_object() const { return valid() && type() == Type::Object; }

    bool as_bool(bool fallback = false) const;
    double as_number(double fallback = 0.0) const;
    /// Empty string if not a string.
    const std::string &as_string() const;

    /// Array/object element count.
    size_t size() const;
    /// Array element (invalid handle if out of range).
    Value at(size_t index) const;
    /// Object member (invalid handle if missing).
    Value operator[](const char *key) const;
    /// Object member key by index.
    const std::string &key_at(size_t index) const;

private:
    friend class Document;
    Value(const Document *doc, uint32_t index) : doc_(doc), index_(index) {}
    const Document *doc_ = nullptr;
    uint32_t index_ = 0;
};

class Document {
public:
    static constexpr int kMaxDepth = 16;

    /// Parses `text`. On failure returns false and error_offset() points at the problem.
    bool parse(const char *text, size_t len);
    Value root() const { return nodes_.empty() ? Value() : Value(this, 0); }
    size_t error_offset() const { return error_offset_; }
    const char *error() const { return error_; }

private:
    friend class Value;
    struct Node {
        Type type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        uint32_t string = 0;               // index into strings_ (String, or member key)
        std::vector<uint32_t> children;    // node indices (Array, Object values)
        std::vector<uint32_t> member_keys; // string indices (Object)
    };

    bool parse_value(uint32_t &out, int depth);
    bool parse_object(uint32_t node, int depth);
    bool parse_array(uint32_t node, int depth);
    bool parse_string(std::string &out);
    bool parse_number(double &out);
    bool parse_literal(const char *lit);
    void skip_ws();
    bool fail(const char *msg);
    uint32_t new_node(Type t);

    const char *p_ = nullptr;
    const char *begin_ = nullptr;
    const char *end_ = nullptr;
    std::vector<Node> nodes_;
    std::vector<std::string> strings_;
    size_t error_offset_ = 0;
    const char *error_ = "";
};

} // namespace core::json
