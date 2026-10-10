#include "json.h"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>

Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object) *this = object();
    for (auto& [k, v] : members_)
        if (k == key) return v;
    members_.emplace_back(key, Json());
    return members_.back().second;
}

const Json* Json::get(const std::string& key) const {
    for (const auto& [k, v] : members_)
        if (k == key) return &v;
    return nullptr;
}

double Json::number(const std::string& key, double fallback) const {
    const Json* j = get(key);
    return j ? j->as_number(fallback) : fallback;
}

bool Json::boolean(const std::string& key, bool fallback) const {
    const Json* j = get(key);
    return j ? j->as_bool(fallback) : fallback;
}

std::string Json::string(const std::string& key, const std::string& fallback) const {
    const Json* j = get(key);
    return j && j->is_string() ? j->as_string() : fallback;
}

// --- Writing ----------------------------------------------------------------------------

namespace {

void write_string(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void newline(std::string& out, int indent) {
    out += '\n';
    out.append(static_cast<size_t>(indent) * 2, ' ');
}

}  // namespace

std::string Json::dump() const {
    std::string out;
    dump_to(out, 0);
    out += '\n';
    return out;
}

void Json::dump_to(std::string& out, int indent) const {
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: {
            if (!std::isfinite(number_)) {
                out += "null";
                break;
            }
            char buf[32];
            auto res = std::to_chars(buf, buf + sizeof buf, number_);  // shortest text that reads back exactly
            out.append(buf, res.ptr);
            break;
        }
        case Type::String: write_string(out, string_); break;
        case Type::Array:
            if (items_.empty()) {
                out += "[]";
                break;
            }
            out += '[';
            for (size_t i = 0; i < items_.size(); ++i) {
                newline(out, indent + 1);
                items_[i].dump_to(out, indent + 1);
                if (i + 1 < items_.size()) out += ',';
            }
            newline(out, indent);
            out += ']';
            break;
        case Type::Object:
            if (members_.empty()) {
                out += "{}";
                break;
            }
            out += '{';
            for (size_t i = 0; i < members_.size(); ++i) {
                newline(out, indent + 1);
                write_string(out, members_[i].first);
                out += ": ";
                members_[i].second.dump_to(out, indent + 1);
                if (i + 1 < members_.size()) out += ',';
            }
            newline(out, indent);
            out += '}';
            break;
    }
}

// --- Reading ----------------------------------------------------------------------------

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    std::optional<Json> document() {
        Json v;
        if (!value(v, 0)) return std::nullopt;
        skip_space();
        if (i_ != s_.size()) return std::nullopt;
        return v;
    }

private:
    void skip_space() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }
    bool literal(const char* word) {
        size_t n = std::char_traits<char>::length(word);
        if (s_.compare(i_, n, word) != 0) return false;
        i_ += n;
        return true;
    }

    bool value(Json& out, int depth) {
        if (depth > 64) return false;
        skip_space();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') {
            std::string str;
            if (!string(str)) return false;
            out = Json(std::move(str));
            return true;
        }
        if (literal("true")) return out = Json(true), true;
        if (literal("false")) return out = Json(false), true;
        if (literal("null")) return out = Json(), true;
        return number(out);
    }

    bool number(Json& out) {
        size_t start = i_;
        if (i_ < s_.size() && s_[i_] == '-') ++i_;
        while (i_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[i_])) || s_[i_] == '.' || s_[i_] == 'e' ||
                                  s_[i_] == 'E' || s_[i_] == '+' || s_[i_] == '-'))
            ++i_;
        double d = 0;
        auto res = std::from_chars(s_.data() + start, s_.data() + i_, d);
        if (res.ec != std::errc() || res.ptr != s_.data() + i_ || i_ == start) return false;
        out = Json(d);
        return true;
    }

    static void put_utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(uint32_t& v) {
        if (s_.size() - i_ < 4) return false;
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char h = s_[i_++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= h - '0';
            else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10;
            else return false;
        }
        return true;
    }

    bool string(std::string& out) {
        ++i_;  // opening quote
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i_ >= s_.size()) return false;
            char e = s_[i_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00) {  // surrogate pair
                        uint32_t lo;
                        if (!literal("\\u") || !hex4(lo) || lo < 0xDC00 || lo >= 0xE000) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    put_utf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool array(Json& out, int depth) {
        ++i_;
        out = Json::array();
        skip_space();
        if (i_ < s_.size() && s_[i_] == ']') return ++i_, true;
        for (;;) {
            Json item;
            if (!value(item, depth + 1)) return false;
            out.push(std::move(item));
            skip_space();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') return ++i_, true;
            return false;
        }
    }

    bool object(Json& out, int depth) {
        ++i_;
        out = Json::object();
        skip_space();
        if (i_ < s_.size() && s_[i_] == '}') return ++i_, true;
        for (;;) {
            skip_space();
            std::string key;
            if (i_ >= s_.size() || s_[i_] != '"' || !string(key)) return false;
            skip_space();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;
            Json v;
            if (!value(v, depth + 1)) return false;
            out[key] = std::move(v);
            skip_space();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') return ++i_, true;
            return false;
        }
    }

    const std::string& s_;
    size_t i_ = 0;
};

}  // namespace

std::optional<Json> Json::parse(const std::string& text) { return Parser(text).document(); }
