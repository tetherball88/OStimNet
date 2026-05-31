#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace OStimNet::JsonService {

// Navigate a JSON tree by a dot-separated path (e.g. "actors.0.name").
// Returns nullptr if any segment is missing or type mismatches.
inline const nlohmann::json* Traverse(const nlohmann::json& root, const char* path) {
    const nlohmann::json* cur = &root;
    if (!path || path[0] == '\0') return cur;
    std::string_view sv(path);
    while (!sv.empty() && cur) {
        auto dot = sv.find('.');
        auto key = (dot == std::string_view::npos) ? sv : sv.substr(0, dot);
        sv       = (dot == std::string_view::npos) ? std::string_view{} : sv.substr(dot + 1);
        if (cur->is_object()) {
            std::string keyLower(key);
            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            // Case-insensitive search: lowercase both sides so "SexualPosition" matches "sexualPosition"
            const nlohmann::json* found = nullptr;
            for (auto it = cur->begin(); it != cur->end(); ++it) {
                std::string jsonKeyLower = it.key();
                std::transform(jsonKeyLower.begin(), jsonKeyLower.end(), jsonKeyLower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (jsonKeyLower == keyLower) { found = &it.value(); break; }
            }
            cur = found;
        } else if (cur->is_array()) {
            std::size_t idx = 0;
            for (char c : key) {
                if (c < '0' || c > '9') return nullptr;
                idx = idx * 10 + static_cast<std::size_t>(c - '0');
            }
            cur = (idx < cur->size()) ? &(*cur)[idx] : nullptr;
        } else {
            cur = nullptr;
        }
    }
    return cur;
}

inline std::string GetString(const char* json, const char* path, const char* def = "") {
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val) return def;
        if (val->is_string()) return val->get<std::string>();
        return val->dump();
    } catch (...) { return def; }
}

inline int32_t GetInt(const char* json, const char* path, int32_t def = 0) {
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val || !val->is_number()) return def;
        return val->get<int32_t>();
    } catch (...) { return def; }
}

inline float GetFloat(const char* json, const char* path, float def = 0.0f) {
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val || !val->is_number()) return def;
        return val->get<float>();
    } catch (...) { return def; }
}

inline bool GetBool(const char* json, const char* path, bool def = false) {
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val || !val->is_boolean()) return def;
        return val->get<bool>();
    } catch (...) { return def; }
}

inline int32_t GetArrayLength(const char* json, const char* path) {
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val || !val->is_array()) return -1;
        return static_cast<int32_t>(val->size());
    } catch (...) { return -1; }
}

inline std::vector<RE::Actor*> GetActorArray(const char* json, const char* path) {
    std::vector<RE::Actor*> result;
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val || !val->is_array()) return result;
        for (const auto& item : *val) {
            if (!item.is_number_unsigned()) continue;
            auto fid = item.get<uint32_t>();
            if (!fid) continue;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(fid));
            if (actor) result.push_back(actor);
        }
    } catch (...) {}
    return result;
}

// ---------------------------------------------------------------------------
// Resolve a JSON array of display-name strings from a source object into a
// JSON array of uint32_t FormIDs, using a caller-supplied name→FormID map.
//
// @param src          JSON object that contains the array at `key`.
// @param key          Key name inside `src` whose value is the name array.
// @param nameToFormID Map built on the game thread before the LLM call.
// @param caller       Short label used in log messages (e.g. function name).
// ---------------------------------------------------------------------------
inline nlohmann::json ResolveNamesToFormIDs(
    const nlohmann::json& src,
    const char* key,
    const std::map<std::string, RE::FormID>& nameToFormID,
    const char* caller = "") {
    auto arr = nlohmann::json::array();
    if (!src.contains(key) || !src[key].is_array()) {
        SKSE::log::warn("{} ResolveNamesToFormIDs: key '{}' missing or not an array", caller, key);
        return arr;
    }
    for (const auto& item : src[key]) {
        if (!item.is_string()) {
            SKSE::log::warn("{} ResolveNamesToFormIDs[{}]: skipping non-string item", caller, key);
            continue;
        }
        std::string name = item.get<std::string>();
        auto it = nameToFormID.find(name);
        if (it != nameToFormID.end()) {
            SKSE::log::info("{} ResolveNamesToFormIDs[{}]: '{}' -> FormID 0x{:08X}",
                            caller, key, name, static_cast<uint32_t>(it->second));
            arr.push_back(static_cast<uint32_t>(it->second));
        } else {
            std::string known;
            for (const auto& p : nameToFormID) { if (!known.empty()) known += ','; known += p.first; }
            SKSE::log::warn("{} ResolveNamesToFormIDs[{}]: '{}' not found in map (known: {})",
                            caller, key, name, known);
        }
    }
    SKSE::log::info("{} ResolveNamesToFormIDs[{}]: resolved {} FormID(s)", caller, key, arr.size());
    return arr;
}

// ---------------------------------------------------------------------------
// Sanitize a raw LLM response string into a bare JSON object string.
// Extracts the substring between the first '{' and the last '}', which
// handles markdown code fences, leading/trailing prose, BOM, and any other
// wrapping the model may add.
// Returns an empty string if no JSON object delimiters are found.
// ---------------------------------------------------------------------------
inline std::string SanitizeLLMJson(const char* raw) {
    if (!raw || raw[0] == '\0') return {};

    std::string_view s(raw);

    // Strip UTF-8 BOM if present.
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF)
        s.remove_prefix(3);

    auto first = s.find('{');
    auto last  = s.rfind('}');
    if (first == std::string_view::npos || last == std::string_view::npos || last < first)
        return {};

    return std::string(s.substr(first, last - first + 1));
}

inline RE::Actor* GetActor(const char* json, const char* path) {
    try {
        auto j    = nlohmann::json::parse(json ? json : "null");
        auto* val = Traverse(j, path);
        if (!val || !val->is_number_unsigned()) return nullptr;
        auto fid = val->get<uint32_t>();
        if (!fid) return nullptr;
        return RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(fid));
    } catch (...) {}
    return nullptr;
}

}  // namespace OStimNet::JsonService
