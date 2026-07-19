#pragma once

#include "RE/B/BGSKeyword.h"
#include "RE/B/BGSKeywordForm.h"

namespace OStimNet::KeywordUtils {

// VR-safe keyword presence check for any BGSKeywordForm subtype (BGSLocation, TESRace, etc.)
//
// Do NOT call BGSKeywordForm::HasKeyword(), HasKeywordString(), or ForEachKeyword() in SkyrimVR.
// All three compile to an MSVC AVX2 vpcmpeqq loop (32-byte reads) over keywords[].
// When the keyword array ends within 32 bytes of an unmapped page boundary, the SIMD read
// faults with EXCEPTION_ACCESS_VIOLATION.
//
// #pragma optimize("", off) disables all MSVC optimizations for this function, guaranteeing
// individual scalar 8-byte QWORD loads only — no vectorization of any kind.
// The pragma is narrow: it covers only this function and does not affect callers.
//
// Crash history: three identical vpcmpeqq faults (Jul-17, Jul-18, Jul-19) before this fix.
// See ctd_history.md for full analysis.
#pragma optimize("", off)
inline bool FormHasKeyword(const RE::BGSKeywordForm* form, const RE::BGSKeyword* kw) {
    if (!form || !kw) return false;
    RE::BGSKeyword* const* kwds = form->keywords;
    const std::uint32_t count  = form->numKeywords;
    if (!kwds) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (kwds[i] == kw) return true;
    }
    return false;
}
#pragma optimize("", on)

}  // namespace OStimNet::KeywordUtils
