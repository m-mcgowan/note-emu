// cJSON-based JsonBackend for note-cpp.
//
// Uses ESP-IDF's built-in cJSON library. Suitable for any platform where
// cJSON is available (ESP-IDF, note-c, standalone cJSON).
#pragma once

#include <note/json.hpp>

#include <cJSON.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace noteemu {

class CjsonBuilder : public note::JsonBuilder {
public:
    CjsonBuilder() {
        stack_.push_back(cJSON_CreateObject());
    }
    ~CjsonBuilder() override {
        // Root is owned by stack_[0]; only delete if to_string() wasn't called
        if (!moved_ && !stack_.empty())
            cJSON_Delete(stack_.front());
    }

    CjsonBuilder& add(note::string_view k, bool v) override {
        cJSON_AddBoolToObject(top(), key(k), v);
        return *this;
    }
    CjsonBuilder& add(note::string_view k, int32_t v) override {
        cJSON_AddNumberToObject(top(), key(k), v);
        return *this;
    }
    CjsonBuilder& add(note::string_view k, double v) override {
        cJSON_AddNumberToObject(top(), key(k), v);
        return *this;
    }
    CjsonBuilder& add(note::string_view k, note::string_view v) override {
        // cJSON needs null-terminated strings
        auto str = std::string(v);
        cJSON_AddStringToObject(top(), key(k), str.c_str());
        return *this;
    }
    CjsonBuilder& begin_object(note::string_view k) override {
        auto* obj = cJSON_CreateObject();
        cJSON_AddItemToObject(top(), key(k), obj);
        stack_.push_back(obj);
        return *this;
    }
    CjsonBuilder& end_object() override {
        if (stack_.size() > 1) stack_.pop_back();
        return *this;
    }
    CjsonBuilder& begin_array(note::string_view k) override {
        auto* arr = cJSON_CreateArray();
        cJSON_AddItemToObject(top(), key(k), arr);
        stack_.push_back(arr);
        return *this;
    }
    CjsonBuilder& end_array() override {
        if (stack_.size() > 1) stack_.pop_back();
        return *this;
    }
    std::string to_string() override {
        moved_ = true;
        char* raw = cJSON_PrintUnformatted(stack_.front());
        std::string result(raw);
        cJSON_free(raw);
        cJSON_Delete(stack_.front());
        stack_.clear();
        return result;
    }

private:
    std::vector<cJSON*> stack_;  // [0] = root, [n] = current container
    std::string key_buf_;
    bool moved_ = false;

    cJSON* top() { return stack_.back(); }

    const char* key(note::string_view k) {
        key_buf_.assign(k.data(), k.size());
        return key_buf_.c_str();
    }
};

class CjsonReader : public note::JsonReader {
public:
    // Takes ownership of the cJSON tree.
    explicit CjsonReader(cJSON* root, bool owns = true)
        : root_(root), owns_(owns) {}

    ~CjsonReader() override {
        if (owns_ && root_) cJSON_Delete(root_);
    }

    CjsonReader(const CjsonReader&) = delete;
    CjsonReader& operator=(const CjsonReader&) = delete;

    bool has(note::string_view k) const override {
        return cJSON_HasObjectItem(root_, key(k));
    }

    bool get_bool(note::string_view k, bool def) const override {
        auto* item = cJSON_GetObjectItem(root_, key(k));
        if (!item) return def;
        if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
        return def;
    }

    int32_t get_int(note::string_view k, int32_t def) const override {
        auto* item = cJSON_GetObjectItem(root_, key(k));
        if (!item || !cJSON_IsNumber(item)) return def;
        return static_cast<int32_t>(item->valuedouble);
    }

    double get_double(note::string_view k, double def) const override {
        auto* item = cJSON_GetObjectItem(root_, key(k));
        if (!item || !cJSON_IsNumber(item)) return def;
        return item->valuedouble;
    }

    note::string_view get_string(note::string_view k, note::string_view def) const override {
        auto* item = cJSON_GetObjectItem(root_, key(k));
        if (!item || !cJSON_IsString(item)) return def;
        return note::string_view(item->valuestring);
    }

    std::unique_ptr<note::JsonReader> get_object(note::string_view k) const override {
        auto* item = cJSON_GetObjectItem(root_, key(k));
        if (!item || !cJSON_IsObject(item)) return nullptr;
        // Non-owning — the parent reader owns the tree
        return std::make_unique<CjsonReader>(item, false);
    }

    bool has_error() const override {
        return cJSON_HasObjectItem(root_, "err");
    }

    note::string_view get_error() const override {
        auto* item = cJSON_GetObjectItem(root_, "err");
        if (!item || !cJSON_IsString(item)) return {};
        return note::string_view(item->valuestring);
    }

private:
    cJSON* root_;
    bool owns_;
    mutable std::string key_buf_;

    const char* key(note::string_view k) const {
        key_buf_.assign(k.data(), k.size());
        return key_buf_.c_str();
    }
};

class CjsonBackend : public note::JsonBackend {
public:
    std::unique_ptr<note::JsonBuilder> create_builder() override {
        return std::make_unique<CjsonBuilder>();
    }

    std::unique_ptr<note::JsonReader> parse_response(note::string_view json) override {
        std::string str(json.data(), json.size());
        cJSON* root = cJSON_Parse(str.c_str());
        if (!root) {
            // Return a reader that reports an error
            root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "err", "JSON parse error");
        }
        return std::make_unique<CjsonReader>(root);
    }
};

}  // namespace noteemu
