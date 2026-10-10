// A small JSON value with a reader and a pretty printer, for level.json in save folders.
#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : type_(Type::Bool), bool_(b) {}
    Json(int n) : type_(Type::Number), number_(n) {}
    Json(long long n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(double n) : type_(Type::Number), number_(n) {}
    Json(const char* s) : type_(Type::String), string_(s) {}
    Json(std::string s) : type_(Type::String), string_(std::move(s)) {}
    static Json array() { Json j; j.type_ = Type::Array; return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    Type type() const { return type_; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    double as_number(double fallback = 0) const { return type_ == Type::Number ? number_ : fallback; }
    bool as_bool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    const std::string& as_string() const { return string_; }

    // Arrays.
    const std::vector<Json>& items() const { return items_; }
    void push(Json value) { items_.push_back(std::move(value)); }

    // Objects keep their keys in insertion order. operator[] adds a missing key.
    Json& operator[](const std::string& key);
    const Json* get(const std::string& key) const;  // nullptr if missing (or not an object)
    double number(const std::string& key, double fallback) const;
    bool boolean(const std::string& key, bool fallback) const;
    std::string string(const std::string& key, const std::string& fallback = "") const;
    const std::vector<std::pair<std::string, Json>>& members() const { return members_; }

    // Two-space indented text.
    std::string dump() const;
    // Nothing if the text isn't valid JSON.
    static std::optional<Json> parse(const std::string& text);

private:
    void dump_to(std::string& out, int indent) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0;
    std::string string_;
    std::vector<Json> items_;
    std::vector<std::pair<std::string, Json>> members_;
};
