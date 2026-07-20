#pragma once

#include "RE/B/BGSKeyword.h"
#include "RE/B/BGSKeywordForm.h"

namespace OStimNet::KeywordUtils {

// Helper wrapper to check keyword presence on any BGSKeywordForm subtype (BGSLocation, TESRace, etc.)
inline bool FormHasKeyword(const RE::BGSKeywordForm* form, const RE::BGSKeyword* kw) {
    if (!form || !kw) return false;
    return form->HasKeyword(kw);
}

}  // namespace OStimNet::KeywordUtils
