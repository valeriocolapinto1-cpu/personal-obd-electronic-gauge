// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/util/json.h"

#include <cstdlib>
#include <cstring>

namespace core::json {

namespace {
const std::string kEmpty;

void append_utf8(std::string &s, uint32_t cp)
{
    if (cp < 0x80) {
        s.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

int hex_val(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}
} // namespace

// ---------------------------------------------------------------------------
// Value
// ---------------------------------------------------------------------------
Type Value::type() const
{
    return doc_ ? doc_->nodes_[index_].type : Type::Null;
}

bool Value::as_bool(bool fallback) const
{
    return is_bool() ? doc_->nodes_[index_].boolean : fallback;
}

double Value::as_number(double fallback) const
{
    return is_number() ? doc_->nodes_[index_].number : fallback;
}

const std::string &Value::as_string() const
{
    return is_string() ? doc_->strings_[doc_->nodes_[index_].string] : kEmpty;
}

size_t Value::size() const
{
    return (is_array() || is_object()) ? doc_->nodes_[index_].children.size() : 0;
}

Value Value::at(size_t i) const
{
    if (!is_array() || i >= size()) {
        return Value();
    }
    return Value(doc_, doc_->nodes_[index_].children[i]);
}

Value Value::operator[](const char *key) const
{
    if (!is_object()) {
        return Value();
    }
    const auto &n = doc_->nodes_[index_];
    for (size_t i = 0; i < n.member_keys.size(); ++i) {
        if (doc_->strings_[n.member_keys[i]] == key) {
            return Value(doc_, n.children[i]);
        }
    }
    return Value();
}

const std::string &Value::key_at(size_t i) const
{
    if (!is_object() || i >= size()) {
        return kEmpty;
    }
    return doc_->strings_[doc_->nodes_[index_].member_keys[i]];
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------
bool Document::fail(const char *msg)
{
    error_ = msg;
    error_offset_ = static_cast<size_t>(p_ - begin_);
    return false;
}

uint32_t Document::new_node(Type t)
{
    nodes_.emplace_back();
    nodes_.back().type = t;
    return static_cast<uint32_t>(nodes_.size() - 1);
}

void Document::skip_ws()
{
    while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) {
        ++p_;
    }
}

bool Document::parse(const char *text, size_t len)
{
    nodes_.clear();
    strings_.clear();
    error_ = "";
    error_offset_ = 0;
    begin_ = p_ = text;
    end_ = text + len;
    if (text == nullptr) {
        return fail("null input");
    }
    uint32_t root = 0;
    skip_ws();
    if (!parse_value(root, 0)) {
        nodes_.clear();
        return false;
    }
    skip_ws();
    if (p_ != end_) {
        nodes_.clear();
        return fail("trailing characters");
    }
    return true;
}

bool Document::parse_literal(const char *lit)
{
    const size_t n = std::strlen(lit);
    if (static_cast<size_t>(end_ - p_) < n || std::strncmp(p_, lit, n) != 0) {
        return fail("invalid literal");
    }
    p_ += n;
    return true;
}

bool Document::parse_value(uint32_t &out, int depth)
{
    if (depth > kMaxDepth) {
        return fail("nesting too deep");
    }
    skip_ws();
    if (p_ >= end_) {
        return fail("unexpected end");
    }
    switch (*p_) {
        case '{':
            out = new_node(Type::Object);
            return parse_object(out, depth + 1);
        case '[':
            out = new_node(Type::Array);
            return parse_array(out, depth + 1);
        case '"': {
            std::string s;
            if (!parse_string(s)) {
                return false;
            }
            out = new_node(Type::String);
            strings_.push_back(std::move(s));
            nodes_[out].string = static_cast<uint32_t>(strings_.size() - 1);
            return true;
        }
        case 't':
            out = new_node(Type::Bool);
            nodes_[out].boolean = true;
            return parse_literal("true");
        case 'f':
            out = new_node(Type::Bool);
            return parse_literal("false");
        case 'n':
            out = new_node(Type::Null);
            return parse_literal("null");
        default: {
            double d = 0;
            if (!parse_number(d)) {
                return false;
            }
            out = new_node(Type::Number);
            nodes_[out].number = d;
            return true;
        }
    }
}

bool Document::parse_object(uint32_t node, int depth)
{
    ++p_; // '{'
    skip_ws();
    if (p_ < end_ && *p_ == '}') {
        ++p_;
        return true;
    }
    for (;;) {
        skip_ws();
        if (p_ >= end_ || *p_ != '"') {
            return fail("expected key");
        }
        std::string key;
        if (!parse_string(key)) {
            return false;
        }
        skip_ws();
        if (p_ >= end_ || *p_ != ':') {
            return fail("expected ':'");
        }
        ++p_;
        uint32_t child = 0;
        if (!parse_value(child, depth)) {
            return false;
        }
        strings_.push_back(std::move(key));
        nodes_[node].member_keys.push_back(static_cast<uint32_t>(strings_.size() - 1));
        nodes_[node].children.push_back(child);
        skip_ws();
        if (p_ < end_ && *p_ == ',') {
            ++p_;
            continue;
        }
        if (p_ < end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        return fail("expected ',' or '}'");
    }
}

bool Document::parse_array(uint32_t node, int depth)
{
    ++p_; // '['
    skip_ws();
    if (p_ < end_ && *p_ == ']') {
        ++p_;
        return true;
    }
    for (;;) {
        uint32_t child = 0;
        if (!parse_value(child, depth)) {
            return false;
        }
        nodes_[node].children.push_back(child);
        skip_ws();
        if (p_ < end_ && *p_ == ',') {
            ++p_;
            continue;
        }
        if (p_ < end_ && *p_ == ']') {
            ++p_;
            return true;
        }
        return fail("expected ',' or ']'");
    }
}

bool Document::parse_string(std::string &out)
{
    ++p_; // opening quote
    while (p_ < end_) {
        const char c = *p_++;
        if (c == '"') {
            return true;
        }
        if (static_cast<unsigned char>(c) < 0x20) {
            return fail("control character in string");
        }
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (p_ >= end_) {
            break;
        }
        const char e = *p_++;
        switch (e) {
            case '"':
            case '\\':
            case '/':
                out.push_back(e);
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u': {
                if (end_ - p_ < 4) {
                    return fail("bad \\u escape");
                }
                uint32_t cp = 0;
                for (int i = 0; i < 4; ++i) {
                    const int h = hex_val(p_[i]);
                    if (h < 0) {
                        return fail("bad \\u escape");
                    }
                    cp = (cp << 4) | static_cast<uint32_t>(h);
                }
                p_ += 4;
                append_utf8(out, cp); // surrogate pairs are not combined (not needed for profiles)
                break;
            }
            default:
                return fail("bad escape");
        }
    }
    return fail("unterminated string");
}

bool Document::parse_number(double &out)
{
    const char *start = p_;
    if (p_ < end_ && *p_ == '-') {
        ++p_;
    }
    if (p_ >= end_ || !(*p_ >= '0' && *p_ <= '9')) {
        return fail("invalid value");
    }
    if (*p_ == '0') {
        ++p_;
    } else {
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') {
            ++p_;
        }
    }
    if (p_ < end_ && *p_ == '.') {
        ++p_;
        if (p_ >= end_ || !(*p_ >= '0' && *p_ <= '9')) {
            return fail("invalid number");
        }
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') {
            ++p_;
        }
    }
    if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
        ++p_;
        if (p_ < end_ && (*p_ == '+' || *p_ == '-')) {
            ++p_;
        }
        if (p_ >= end_ || !(*p_ >= '0' && *p_ <= '9')) {
            return fail("invalid number");
        }
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') {
            ++p_;
        }
    }
    // strtod needs a terminated buffer: copy the (short) token.
    char buf[64];
    const size_t n = static_cast<size_t>(p_ - start);
    if (n >= sizeof(buf)) {
        return fail("number too long");
    }
    std::memcpy(buf, start, n);
    buf[n] = '\0';
    out = std::strtod(buf, nullptr);
    return true;
}

} // namespace core::json
