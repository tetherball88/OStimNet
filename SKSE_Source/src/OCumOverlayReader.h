#pragma once
#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "RE/B/BSGeometry.h"
#include "RE/B/BSLightingShaderMaterialBase.h"
#include "RE/B/BSShaderTextureSet.h"
#include "RE/N/NiRTTI.h"
#include "ThreadRegistry.h"

namespace OStimNet {

// A single active OCum overlay slot found on an actor's live 3D geometry.
struct OCumOverlay {
    std::string area;        // "Body", "Face", "Hands", or "Feet"
    int         slot;        // 0-based NiOverride overlay slot index
    std::string texturePath; // as stored in the BSShaderTextureSet, e.g. "CumOverlays\Facial1.dds"
};

// Scans all NiOverride overlay nodes on the actor's live 3D and returns every slot
// whose diffuse texture path contains "CumOverlays" (the marker common to all OCum
// Ascended textures: CumOverlays\*, CumOverlaysUBE\*).
//
// Reads directly from BSShaderTextureSet on the live geometry — no NiOverride API
// required. Reflects exactly what is currently rendered on the actor.
//
// Returns an empty vector when:
//   - actor is null
//   - actor has no 3D loaded (e.g. off-screen cell)
//   - no OCum overlays are active on the actor
inline std::vector<OCumOverlay> GetOCumTextures(RE::Actor* actor) {
    std::vector<OCumOverlay> results;
    if (!actor) {
        SKSE::log::warn("GetOCumTextures: called with null actor");
        return results;
    }

    RE::NiAVObject* root = actor->Get3D(false);
    if (!root) {
        SKSE::log::debug("GetOCumTextures: actor {:08X} has no 3D loaded, skipping",
                         actor->GetFormID());
        return results;
    }

    SKSE::log::debug("GetOCumTextures: scanning actor {:08X} ({})",
                     actor->GetFormID(),
                     actor->GetName() ? actor->GetName() : "unknown");

    static constexpr int         kMaxOverlaySlots = 32;
    static constexpr const char* kCumMarker       = "cumoverlays";
    static constexpr const char* kDefaultTex      = "actors\\character\\overlays\\default.dds";

    static const char* kAreas[] = { "Body", "Face", "Hands", "Feet" };

    for (const char* area : kAreas) {
        for (int slot = 0; slot < kMaxOverlaySlots; ++slot) {
            // NiOverride names overlay nodes "Area [ovlN]" (e.g. "Body [ovl0]")
            std::string nodeName = std::string(area) + " [ovl" + std::to_string(slot) + "]";

            RE::NiAVObject* avObj = root->GetObjectByName(RE::BSFixedString(nodeName.c_str()));
            // NiOverride allocates slots contiguously; a null node means no further
            // slots exist for this area, so we can stop scanning here.
            if (!avObj) {
                // SKSE::log::debug("GetOCumTextures: node '{}' not found, stopping {} scan at slot {}",
                //                  nodeName, area, slot);
                break;
            }

            RE::BSGeometry* geom = avObj->AsGeometry();
            if (!geom) {
                // SKSE::log::debug("GetOCumTextures: node '{}' is not a BSGeometry, skipping", nodeName);
                continue;
            }

            // lightingShaderProp_cast() checks RTTI — returns null for non-lighting shaders
            RE::BSLightingShaderProperty* shader = geom->lightingShaderProp_cast();
            if (!shader) {
                // SKSE::log::debug("GetOCumTextures: node '{}' has no BSLightingShaderProperty, skipping", nodeName);
                continue;
            }

            RE::BSShaderMaterial* mat = shader->GetBaseMaterial();
            if (!mat || mat->GetType() != RE::BSShaderMaterial::Type::kLighting) {
                // SKSE::log::debug("GetOCumTextures: node '{}' material missing or not kLighting, skipping", nodeName);
                continue;
            }

            auto* litMat = static_cast<RE::BSLightingShaderMaterialBase*>(mat);

            // netimmerse_cast validates the concrete subtype via NiRTTI at runtime
            RE::BSShaderTextureSet* texSet =
                netimmerse_cast<RE::BSShaderTextureSet*>(litMat->textureSet.get());
            if (!texSet) {
                // SKSE::log::debug("GetOCumTextures: node '{}' texture set is not BSShaderTextureSet, skipping", nodeName);
                continue;
            }

            const char* path = texSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse);
            if (!path || path[0] == '\0') {
                // SKSE::log::debug("GetOCumTextures: node '{}' has empty diffuse path, skipping", nodeName);
                continue;
            }
            if (_stricmp(path, kDefaultTex) == 0) {
                // SKSE::log::debug("GetOCumTextures: node '{}' has default texture, skipping", nodeName);
                continue;
            }

            // Case-insensitive search for the OCum marker — covers both
            // "CumOverlays\..." and "CumOverlaysUBE\..." paths
            std::string lower(path);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lower.find(kCumMarker) != std::string::npos) {
                SKSE::log::debug("GetOCumTextures: found OCum overlay on actor {:08X} — area={} slot={} tex={}",
                                 actor->GetFormID(), area, slot, path);
                results.push_back({ area, slot, path });
            } else {
                // SKSE::log::debug("GetOCumTextures: node '{}' tex='{}' not an OCum texture, skipping", nodeName, path);
            }
        }
    }

    SKSE::log::debug("GetOCumTextures: actor {:08X} — {} OCum overlay(s) found",
                     actor->GetFormID(), results.size());
    return results;
}

// ---------------------------------------------------------------------------
// Intensity grouping helpers
// ---------------------------------------------------------------------------

// Extracts the trailing integer level from a texture filename.
// "Mouth3.dds" → 3,  "Breast12.dds" → 12,  "Hands.dds" → -1 (no number)
inline int ExtractTextureLevel(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos || dot == 0) return -1;
    int end   = static_cast<int>(dot);
    int start = end - 1;
    while (start >= 0 && std::isdigit(static_cast<unsigned char>(filename[start])))
        --start;
    ++start;
    if (start >= end) return -1;
    return std::stoi(filename.substr(static_cast<size_t>(start),
                                     static_cast<size_t>(end - start)));
}

// Returns true if the filename (without directory) belongs to a given area prefix
// (case-insensitive, e.g. area="Mouth" matches "mouth3.dds").
inline bool FilenameMatchesArea(const std::string& filename, const char* area) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string areaLower(area);
    std::transform(areaLower.begin(), areaLower.end(), areaLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.rfind(areaLower, 0) == 0;
}

// Extracts only the filename from a full texture path.
inline std::string TextureFilename(const std::string& path) {
    auto sep = path.find_last_of("/\\");
    return (sep != std::string::npos) ? path.substr(sep + 1) : path;
}

enum class OCumIntensity { None, Small, Medium, Big, Huge };

// ---------------------------------------------------------------------------
// Mouth intensity
//
// Texture files: Mouth1.dds – Mouth7.dds  (7 levels)
//                BreastFace1.dds, BreastFace2.dds  (count as small-group)
//   Small-group levels : Mouth{1,2,3,4,5,7}, BreastFace{1,2}
//   Level 6            : counts as an escalator (immediately medium alone)
//
//   Group rules (checked highest → lowest):
//     Big    : smallCount >= 3  OR  (sixCount >= 1 AND smallCount >= 1)
//     Medium : smallCount >= 2  OR  sixCount >= 1
//     Small  : smallCount >= 1
//     None   : no mouth/breastface textures active
// ---------------------------------------------------------------------------
inline OCumIntensity ComputeMouthIntensity(const std::vector<OCumOverlay>& overlays) {
    int smallCount = 0;
    int sixCount   = 0;

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);

        if (FilenameMatchesArea(fname, "Mouth")) {
            int level = ExtractTextureLevel(fname);
            if (level <= 0) continue;

            if (level == 6)
                ++sixCount;
            else if ((level >= 1 && level <= 5) || level == 7)
                ++smallCount;
        } else if (FilenameMatchesArea(fname, "BreastFace")) {
            // BreastFace1 and BreastFace2 count as small-group mouth entries
            int level = ExtractTextureLevel(fname);
            if (level == 1 || level == 2)
                ++smallCount;
        }
    }

    if (smallCount >= 3 || (sixCount >= 1 && smallCount >= 1)) return OCumIntensity::Big;
    if (smallCount >= 2 || sixCount >= 1)                       return OCumIntensity::Medium;
    if (smallCount >= 1)                                        return OCumIntensity::Small;
    return OCumIntensity::None;
}

// ---------------------------------------------------------------------------
// Butt intensity
//
// Texture files: Butt1.dds – Butt7.dds  (7 levels)
//
//   Group rules (checked highest → lowest):
//     Big    : any level >= 5
//     Medium : level 3 or 4 present  OR  (level1Count >= 1 AND level2Count >= 1)
//     Small  : level 1 or 2 present
//     None   : no butt textures active
// ---------------------------------------------------------------------------
inline OCumIntensity ComputeButtIntensity(const std::vector<OCumOverlay>& overlays) {
    int level1Count = 0;
    int level2Count = 0;
    int medCount    = 0;  // levels 3–4
    int bigCount    = 0;  // levels 5–7

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (!FilenameMatchesArea(fname, "Butt")) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 1)           ++level1Count;
        else if (level == 2)      ++level2Count;
        else if (level >= 3 && level <= 4) ++medCount;
        else if (level >= 5)      ++bigCount;
    }

    if (bigCount >= 1)                                            return OCumIntensity::Big;
    if (medCount >= 1 || (level1Count >= 1 && level2Count >= 1)) return OCumIntensity::Medium;
    if (level1Count >= 1 || level2Count >= 1)                    return OCumIntensity::Small;
    return OCumIntensity::None;
}

// Returns a descriptive sentence for the butt area, or "" if no butt cum is active.
inline std::string GetOCumButtDescription(const std::vector<OCumOverlay>& overlays,
                                           const std::string& targetName) {
    OCumIntensity group = ComputeButtIntensity(overlays);
    if (group == OCumIntensity::None) return "";

    const std::string& target = targetName.empty() ? "someone" : targetName;

    switch (group) {
        case OCumIntensity::Small:
            return "A few streaks of cum glisten on " + target + "'s ass cheeks.";
        case OCumIntensity::Medium:
            return "Cum coats " + target + "'s ass cheeks and lower back in thick streaks.";
        case OCumIntensity::Big:
            return "Thick streaks of cum cover " + target + "'s ass and lower back, dripping down onto " + target + "'s upper thighs.";
        default:
            return "";
    }
}


//
// Texture files: Anal1.dds – Anal4.dds  (4 levels)
//
//   Group rules (checked highest → lowest):
//     Big    : level >= 3  OR  (level1Count >= 1 AND level2Count >= 1)
//     Medium : level2Count >= 1
//     Small  : level1Count >= 1
//     None   : no anal textures active
// ---------------------------------------------------------------------------
inline OCumIntensity ComputeAnalIntensity(const std::vector<OCumOverlay>& overlays) {
    int level1Count = 0;
    int level2Count = 0;
    int bigCount    = 0;  // levels 3 or 4

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (!FilenameMatchesArea(fname, "Anal")) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 1)      ++level1Count;
        else if (level == 2) ++level2Count;
        else if (level >= 3) ++bigCount;
    }

    if (bigCount >= 1 || (level1Count >= 1 && level2Count >= 1)) return OCumIntensity::Big;
    if (level2Count >= 1)                                         return OCumIntensity::Medium;
    if (level1Count >= 1)                                         return OCumIntensity::Small;
    return OCumIntensity::None;
}

// Returns a descriptive sentence for the anal area, or "" if no anal cum is active.
inline std::string GetOCumAnalDescription(const std::vector<OCumOverlay>& overlays,
                                           const std::string& targetName) {
    OCumIntensity group = ComputeAnalIntensity(overlays);
    if (group == OCumIntensity::None) return "";

    const std::string& target = targetName.empty() ? "someone" : targetName;

    switch (group) {
        case OCumIntensity::Small:
            return "A thin trickle of cum sits at " + target + "'s ass opening.";
        case OCumIntensity::Medium:
            return "Cum runs down " + target + "'s crack in a slow drip.";
        case OCumIntensity::Big:
            return "Cum streaks heavily down " + target + "'s crack, pooling at the base.";
        default:
            return "";
    }
}


// Takes a pre-fetched overlay list so GetOCumTextures is only called once per actor.
inline std::string GetOCumMouthDescription(const std::vector<OCumOverlay>& overlays,
                                            const std::string& targetName) {
    OCumIntensity group = ComputeMouthIntensity(overlays);
    if (group == OCumIntensity::None) return "";

    const std::string& target = targetName.empty() ? "someone" : targetName;

    switch (group) {
        case OCumIntensity::Small:
            return "A thin streak of cum sits at the corner of " + target + "'s mouth.";
        case OCumIntensity::Medium:
            return "Cum glistens across " + target + "'s lips.";
        case OCumIntensity::Big:
            return "Cum smears across " + target + "'s lips and around " + target + "'s mouth.";
        default:
            return "";
    }
}

// ---------------------------------------------------------------------------
// Breast intensity
//
// Texture files: Breast1.dds – Breast7.dds  (7 levels)
// Note: BreastFace1/2 are excluded by requiring the prefix to be exactly "Breast"
//       followed immediately by a digit (not "BreastFace").
//
//   Small levels  : 1, 2
//   Medium levels : 3, 4, 7
//   Big levels    : 5, 6
//
//   Group rules (checked highest → lowest):
//     Big    : bigCount >= 1  OR  mediumCount >= 2
//     Medium : mediumCount >= 1
//     Small  : smallCount >= 1
//     None   : no breast textures active
// ---------------------------------------------------------------------------
inline OCumIntensity ComputeBreastIntensity(const std::vector<OCumOverlay>& overlays) {
    int smallCount  = 0;  // levels 1–2
    int mediumCount = 0;  // levels 3, 4, 7
    int bigCount    = 0;  // levels 5–6

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        // Must match "Breast" followed by a digit — excludes "BreastFace"
        if (!FilenameMatchesArea(fname, "Breast")) continue;
        if (fname.size() > 6 && !std::isdigit(static_cast<unsigned char>(fname[6]))) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 1 || level == 2)      ++smallCount;
        else if (level == 3 || level == 4 || level == 7) ++mediumCount;
        else if (level == 5 || level == 6) ++bigCount;
    }

    if (bigCount >= 1 || mediumCount >= 2) return OCumIntensity::Big;
    if (mediumCount >= 1)                  return OCumIntensity::Medium;
    if (smallCount >= 1)                   return OCumIntensity::Small;
    return OCumIntensity::None;
}

// Returns a descriptive sentence for the breast/chest area, or "" if none active.
// Uses "chest" instead of "breasts" for male actors.
inline std::string GetOCumBreastDescription(const std::vector<OCumOverlay>& overlays,
                                             const std::string& targetName,
                                             bool targetIsMale) {
    OCumIntensity group = ComputeBreastIntensity(overlays);
    if (group == OCumIntensity::None) return "";

    const std::string& target = targetName.empty() ? "someone" : targetName;
    const std::string  chest  = targetIsMale ? "chest" : "breasts";

    switch (group) {
        case OCumIntensity::Small:
            return "A few streaks of cum glisten on " + target + "'s " + chest + ".";
        case OCumIntensity::Medium:
            return "Cum coats " + target + "'s " + chest + " and runs down " + target + "'s chest.";
        case OCumIntensity::Big:
            return "Thick streaks of cum cover " + target + "'s " + chest + ", chest, and trail over " + target + "'s shoulders.";
        default:
            return "";
    }
}

// ---------------------------------------------------------------------------
// Facial intensity
//
// Texture files: Facial1.dds – Facial11.dds  (11 levels)
//
//   Small levels  : {1, 2, 4, 5}
//   Medium levels : {3, 6, 7}
//   Big levels    : {8, 9, 10, 11}
//
//   Group rules (checked highest → lowest):
//     Huge   : mediumCount >= 3  OR  (bigCount >= 1 AND mediumCount >= 1)
//     Big    : bigCount >= 1  OR  smallCount >= 3  OR  mediumCount >= 2
//     Medium : mediumCount >= 1  OR  smallCount >= 2
//     Small  : smallCount >= 1
//     None   : no facial textures active
// ---------------------------------------------------------------------------
inline OCumIntensity ComputeFacialIntensity(const std::vector<OCumOverlay>& overlays) {
    int smallCount  = 0;  // levels 1, 2, 4, 5
    int mediumCount = 0;  // levels 3, 6, 7
    int bigCount    = 0;  // levels 8–11

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (!FilenameMatchesArea(fname, "Facial")) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 1 || level == 2 || level == 4 || level == 5) ++smallCount;
        else if (level == 3 || level == 6 || level == 7)           ++mediumCount;
        else if (level >= 8)                                        ++bigCount;
    }

    if (mediumCount >= 3 || bigCount >= 2 || (bigCount >= 1 && mediumCount >= 1)) return OCumIntensity::Huge;
    if (bigCount >= 1 || smallCount >= 3 || mediumCount >= 2)    return OCumIntensity::Big;
    if (mediumCount >= 1 || smallCount >= 2)                     return OCumIntensity::Medium;
    if (smallCount >= 1)                                         return OCumIntensity::Small;
    return OCumIntensity::None;
}

// Returns a descriptive sentence for the facial area, or "" if none active.
inline std::string GetOCumFacialDescription(const std::vector<OCumOverlay>& overlays,
                                             const std::string& targetName) {
    OCumIntensity group = ComputeFacialIntensity(overlays);
    if (group == OCumIntensity::None) return "";

    const std::string& target = targetName.empty() ? "someone" : targetName;

    switch (group) {
        case OCumIntensity::Small:
            return "A thin streak of cum sits across " + target + "'s cheek.";
        case OCumIntensity::Medium:
            return "Cum coats " + target + "'s face in thick streaks.";
        case OCumIntensity::Big:
            return "Thick streaks of cum cover " + target + "'s face, dripping from " + target + "'s chin.";
        case OCumIntensity::Huge:
            return "Cum covers every part of " + target + "'s face in thick streaks, dripping from " + target + "'s chin down " + target + "'s neck.";
        default:
            return "";
    }
}

// ---------------------------------------------------------------------------
// Combined Face description  (facial + mouth)
//
// Logic:
//   - No facial textures → fall back to mouth description only.
//   - Facial textures present → re-derive facial counts and add the mouth
//     intensity group as one extra tier contribution, then apply the normal
//     facial Huge/Big/Medium/Small rules.
//       mouth Small  → +1 facial smallCount
//       mouth Medium → +1 facial mediumCount
//       mouth Big    → +1 facial bigCount
// ---------------------------------------------------------------------------
inline std::string GetOCumFaceDescription(const std::vector<OCumOverlay>& overlays,
                                           const std::string& targetName) {
    // No facial — delegate entirely to mouth
    if (ComputeFacialIntensity(overlays) == OCumIntensity::None)
        return GetOCumMouthDescription(overlays, targetName);

    // Re-derive facial tier counts
    int smallCount  = 0;
    int mediumCount = 0;
    int bigCount    = 0;

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (!FilenameMatchesArea(fname, "Facial")) continue;
        int level = ExtractTextureLevel(fname);
        if (level == 1 || level == 2 || level == 4 || level == 5) ++smallCount;
        else if (level == 3 || level == 6 || level == 7)           ++mediumCount;
        else if (level >= 8)                                        ++bigCount;
    }

    // Add mouth group as one bonus tier count
    switch (ComputeMouthIntensity(overlays)) {
        case OCumIntensity::Small:  ++smallCount;  break;
        case OCumIntensity::Medium: ++mediumCount; break;
        case OCumIntensity::Big:    ++bigCount;    break;
        default: break;
    }

    // Apply standard facial grouping rules
    OCumIntensity combined;
    if (mediumCount >= 3 || (bigCount >= 1 && mediumCount >= 1)) combined = OCumIntensity::Huge;
    else if (bigCount >= 1 || smallCount >= 3 || mediumCount >= 2) combined = OCumIntensity::Big;
    else if (mediumCount >= 1 || smallCount >= 2)                  combined = OCumIntensity::Medium;
    else                                                            combined = OCumIntensity::Small;

    const std::string& target = targetName.empty() ? "someone" : targetName;

    switch (combined) {
        case OCumIntensity::Small:
            return "A thin streak of cum sits across " + target + "'s cheek.";
        case OCumIntensity::Medium:
            return "Cum coats " + target + "'s face in thick streaks.";
        case OCumIntensity::Big:
            return "Thick streaks of cum cover " + target + "'s face, dripping from " + target + "'s chin.";
        case OCumIntensity::Huge:
            return "Cum covers every part of " + target + "'s face in thick streaks, dripping from " + target + "'s chin down " + target + "'s neck.";
        default:
            return "";
    }
}

// ---------------------------------------------------------------------------
// Feet intensity
//
// Texture files: Feet1.dds, Feet2.dds  (2 levels)
//
//   Group rules:
//     Medium : level 2 present
//     Small  : level 1 present
//     None   : no feet textures active
// ---------------------------------------------------------------------------
inline OCumIntensity ComputeFeetIntensity(const std::vector<OCumOverlay>& overlays) {
    int level1Count = 0;
    int level2Count = 0;

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (!FilenameMatchesArea(fname, "Feet")) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 1)      ++level1Count;
        else if (level == 2) ++level2Count;
    }

    if (level2Count >= 1) return OCumIntensity::Medium;
    if (level1Count >= 1) return OCumIntensity::Small;
    return OCumIntensity::None;
}

// Returns a descriptive sentence for the feet area, or "" if none active.
inline std::string GetOCumFeetDescription(const std::vector<OCumOverlay>& overlays,
                                           const std::string& targetName) {
    OCumIntensity group = ComputeFeetIntensity(overlays);
    if (group == OCumIntensity::None) return "";

    const std::string& target = targetName.empty() ? "someone" : targetName;

    switch (group) {
        case OCumIntensity::Small:
            return "A thin streak of cum glistens across " + target + "'s toes.";
        case OCumIntensity::Medium:
            return "Cum coats " + target + "'s feet in thick streaks.";
        default:
            return "";
    }
}

// Returns "Cum coats {target}'s hands in thick streaks." if any Hands.dds overlay
// is active on the actor, otherwise "".
inline std::string GetOCumHandsDescription(const std::vector<OCumOverlay>& overlays,
                                            const std::string& targetName) {
    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (FilenameMatchesArea(fname, "Hands"))
            return "Cum coats " + (targetName.empty() ? std::string("someone") : targetName) + "'s hands in thick streaks.";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Legs / Thighs intensity
//
// Texture files: Legs1.dds, Legs2.dds, Thighs1.dds, Thighs2.dds
// Both variants are self-exclusive per level — Legs1 + Thighs1 still = Small.
//
//   Medium : any level-2 overlay present (Legs2 or Thighs2)
//   Small  : any level-1 overlay present (Legs1 or Thighs1)
//   None   : no legs/thighs textures active
// ---------------------------------------------------------------------------
inline std::string GetOCumLegsDescription(const std::vector<OCumOverlay>& overlays,
                                           const std::string& targetName) {
    bool hasLevel1 = false;
    bool hasLevel2 = false;

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        bool isLegs   = FilenameMatchesArea(fname, "Legs");
        bool isThighs = FilenameMatchesArea(fname, "Thighs");
        if (!isLegs && !isThighs) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 2)      hasLevel2 = true;
        else if (level == 1) hasLevel1 = true;
    }

    const std::string& target = targetName.empty() ? "someone" : targetName;

    if (hasLevel2) return "Cum runs down " + target + "'s thighs in thick streaks.";
    if (hasLevel1) return "A thin trickle of cum runs down " + target + "'s inner thigh.";
    return "";
}

// ---------------------------------------------------------------------------
// VaginalCreampie intensity
//
// Texture files: VaginalCreampie1.dds – VaginalCreampie6.dds  (6 levels)
//
//   Small levels  : {1, 2}
//   Medium levels : {3, 4, 6}
//   Big levels    : {5}
//
//   Group rules (highest → lowest):
//     Big    : bigCount >= 1  OR  mediumCount >= 2
//     Medium : mediumCount >= 1  OR  smallCount >= 2
//     Small  : smallCount >= 1
//     None   : no vaginal creampie textures active
// ---------------------------------------------------------------------------
inline std::string GetOCumVaginalDescription(const std::vector<OCumOverlay>& overlays,
                                              const std::string& targetName) {
    int smallCount  = 0;  // levels 1, 2
    int mediumCount = 0;  // levels 3, 4, 6
    int bigCount    = 0;  // level 5

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        if (!FilenameMatchesArea(fname, "VaginalCreampie")) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 1 || level == 2)                ++smallCount;
        else if (level == 3 || level == 4 || level == 6) ++mediumCount;
        else if (level == 5)                         ++bigCount;
    }

    const std::string& target = targetName.empty() ? "someone" : targetName;

    if (bigCount >= 1 || mediumCount >= 2)
        return "Cum covers " + target + "'s pussy and pubic area, trailing onto " + target + "'s lower belly.";
    if (mediumCount >= 1 || smallCount >= 2)
        return "Cum coats " + target + "'s pussy and spreads across " + target + "'s pubic area.";
    if (smallCount >= 1)
        return "A thin trickle of cum glistens along the sides of " + target + "'s pussy.";
    return "";
}

// ---------------------------------------------------------------------------
// Vagina intensity
//
// Texture files: Vagina1.dds – Vagina7.dds  (7 levels)
//
//   Small levels  : {1}
//   Medium levels : {2, 3, 4}
//   Big levels    : {5, 6}
//
//   Group rules (highest → lowest):
//     Huge   : level 7 present  OR  (bigCount >= 1 AND mediumCount >= 1)
//     Big    : bigCount >= 1  OR  mediumCount >= 2
//     Medium : mediumCount >= 1
//     Small  : smallCount >= 1
//     None   : no vagina textures active
// ---------------------------------------------------------------------------
inline std::string GetOCumVaginaDescription(const std::vector<OCumOverlay>& overlays,
                                             const std::string& targetName) {
    int smallCount  = 0;  // level 1
    int mediumCount = 0;  // levels 2, 3, 4
    int bigCount    = 0;  // levels 5, 6
    bool hasLevel7  = false;

    for (const auto& ov : overlays) {
        std::string fname = TextureFilename(ov.texturePath);
        // Exclude "VaginalCreampie*" — check char after "Vagina" is a digit
        if (!FilenameMatchesArea(fname, "Vagina")) continue;
        if (fname.size() <= 6 || !std::isdigit(static_cast<unsigned char>(fname[6]))) continue;

        int level = ExtractTextureLevel(fname);
        if (level == 7)                             hasLevel7 = true;
        else if (level == 5 || level == 6)          ++bigCount;
        else if (level == 2 || level == 3 || level == 4) ++mediumCount;
        else if (level == 1)                        ++smallCount;
    }

    const std::string& target = targetName.empty() ? "someone" : targetName;

    if (hasLevel7 || bigCount >= 2 || (bigCount >= 1 && mediumCount >= 1))
        return "Thick streaks of cum cover " + target + "'s entire belly from the pubic area up, running down onto " + target + "'s thighs.";
    if (bigCount >= 1 || mediumCount >= 2)
        return "Thick streaks of cum cover " + target + "'s pubic area and spread up across " + target + "'s belly.";
    if (mediumCount >= 1)
        return "Cum coats " + target + "'s lower belly and spreads across " + target + "'s pubic area.";
    if (smallCount >= 1)
        return "A thin streak of cum glistens on " + target + "'s lower belly, just above the pubic area.";
    return "";
}

// ---------------------------------------------------------------------------
// Combined full-body description
//
// Calls every area description function and joins non-empty results with a
// single space. Returns "" when no OCum overlays are active at all.
// ---------------------------------------------------------------------------
struct OCumCacheEntry {
    std::string description;
    std::chrono::steady_clock::time_point timestamp;
};

inline std::shared_mutex g_ocumCacheMutex;
inline std::unordered_map<RE::FormID, OCumCacheEntry> g_ocumCache;

inline std::string GetOCumDescription(RE::Actor* actor, bool forceRefresh = false) {
    if (!actor) return "";

    auto formID = actor->GetFormID();
    auto now = std::chrono::steady_clock::now();

    if (!forceRefresh) {
        std::shared_lock lock(g_ocumCacheMutex);
        auto it = g_ocumCache.find(formID);
        if (it != g_ocumCache.end() && (now - it->second.timestamp) < std::chrono::seconds(10)) {
            return it->second.description;
        }
    }

    auto overlays = GetOCumTextures(actor);
    if (overlays.empty()) {
        std::unique_lock lock(g_ocumCacheMutex);
        g_ocumCache[formID] = { "", now };
        return "";
    }

    std::string name;
    bool isMale = false;
    if (const char* n = actor->GetName(); n && n[0]) name = n;
    if (auto* base = actor->GetActorBase())
        isMale = base->GetSex() == RE::SEX::kMale;

    // Helpers to append a sentence if non-empty
    std::string result;
    auto append = [&](std::string s) {
        if (s.empty()) return;
        if (!result.empty()) result += ' ';
        result += std::move(s);
    };

    append(GetOCumFaceDescription(overlays, name));
    append(GetOCumBreastDescription(overlays, name, isMale));
    append(GetOCumButtDescription(overlays, name));
    append(GetOCumAnalDescription(overlays, name));
    append(GetOCumVaginaDescription(overlays, name));
    append(GetOCumVaginalDescription(overlays, name));
    append(GetOCumLegsDescription(overlays, name));
    append(GetOCumHandsDescription(overlays, name));
    append(GetOCumFeetDescription(overlays, name));

    {
        std::unique_lock lock(g_ocumCacheMutex);
        g_ocumCache[formID] = { result, now };
    }

    return result;
}

inline void RefreshOCumCacheForThread(int threadID) {
    const auto& actors = ThreadDataStore::GetSingleton().GetActorPtrs(threadID);
    std::unique_lock lock(g_ocumCacheMutex);
    for (auto* actor : actors) {
        if (actor) {
            g_ocumCache.erase(actor->GetFormID());
        }
    }
}

}  // namespace OStimNet
