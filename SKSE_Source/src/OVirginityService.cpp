#include "PCH.h"

#include "OVirginityService.h"

#include <atomic>
#include <thread>

#include "Config.h"
#include "EventPayloadBuilder.h"
#include "OStimEventListener.h"
#include "PulloutService.h"
#include "SexualPhase.h"
#include "SpeakerSelector.h"
#include "ThreadRegistry.h"
#include "api/OstimNG-API-Thread.h"
#include "api/SkyrimNet_PublicAPI.h"
#include "ui/SessionEpoch.h"

namespace OStimNet {

namespace {

constexpr auto kCacheTTL = std::chrono::seconds(20);
constexpr auto kResolveTimeout = std::chrono::milliseconds(1500);

struct BatchState {
    std::atomic<int> remaining{0};
    std::atomic<bool> finished{false};
    UI::SessionEpoch expectedEpoch{0};
    std::function<void()> onReady;

    void FinishOnce() {
        if (!finished.exchange(true) && onReady) {
            UI::AddEpochTask(expectedEpoch, onReady);
        }
    }
};

class IsVirginCallback : public RE::BSScript::IStackCallbackFunctor {
public:
    IsVirginCallback(RE::FormID formID, std::shared_ptr<BatchState> state)
        : m_formID(formID), m_state(std::move(state)) {}

    void operator()(RE::BSScript::Variable a_result) override {
        bool val = false;
        if (a_result.IsBool()) {
            val = a_result.GetBool();
        } else if (a_result.IsInt()) {
            val = (a_result.GetSInt() != 0);
        }

        OVirginityService::GetSingleton().OnVirginityResolved(m_formID, val);

        if (m_state && m_state->remaining.fetch_sub(1) == 1) {
            m_state->FinishOnce();
        }
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    RE::FormID m_formID;
    std::shared_ptr<BatchState> m_state;
};

void DispatchRegisterEvent(const std::string& jsonStr, RE::Actor* speaker, RE::Actor* target) {
    if (!speaker) return;
    RE::FormID speakerID = speaker->GetFormID();
    RE::FormID targetID = target ? target->GetFormID() : 0;
    const auto expectedEpoch = UI::GetSessionEpoch();

    UI::AddEpochTask(expectedEpoch, [jsonStr, speakerID, targetID]() {
        auto* spk = RE::TESForm::LookupByID<RE::Actor>(speakerID);
        if (!spk) return;
        auto* tgt = targetID ? RE::TESForm::LookupByID<RE::Actor>(targetID) : nullptr;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (vm) {
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
            auto eventType = RE::BSFixedString("tton_event");
            auto msg = RE::BSFixedString(jsonStr);
            vm->DispatchStaticCall("SkyrimNetApi", "RegisterEvent",
                RE::MakeFunctionArguments(std::move(eventType), std::move(msg), std::move(spk), std::move(tgt)),
                nullCallback);
        }
    });
}

std::string ResolveActorLocationName(RE::Actor* primary, RE::Actor* secondary) {
    RE::Actor* ref = primary ? primary : secondary;
    if (ref) {
        if (auto* loc = ref->GetCurrentLocation()) {
            if (const char* name = loc->GetFullName(); name && name[0] != '\0') {
                return name;
            }
        }
        if (auto* cell = ref->GetParentCell()) {
            if (const char* name = cell->GetFullName(); name && name[0] != '\0') {
                return name;
            }
        }
    }
    return "Skyrim";
}

std::vector<RE::FormID> CollectActorFormIDs(const std::vector<RE::Actor*>& actors) {
    std::vector<RE::FormID> out;
    out.reserve(actors.size());
    for (RE::Actor* a : actors) {
        if (a) out.push_back(a->GetFormID());
    }
    return out;
}

bool ResolveVirginIsMain(const std::vector<RE::FormID>& mainFIDs,
                         const std::vector<RE::FormID>& secFIDs,
                         RE::FormID virginFID) {
    if (std::find(secFIDs.begin(), secFIDs.end(), virginFID) != secFIDs.end()) return false;
    if (std::find(mainFIDs.begin(), mainFIDs.end(), virginFID) != mainFIDs.end()) return true;
    return false;
}

struct MemorySpec {
    std::string text;
    float importance{0.90f};
    const char* memType{"RELATIONSHIP"};
    const char* emotion{"content"};
    const char* tags{"[\"virginity\",\"first_time\",\"intimacy\"]"};
};

void DispatchVirginityLossMemory(RE::Actor* virginActor, RE::Actor* partnerActor, Intent intent, bool virginIsMain) {
    if (!virginActor) return;

    RE::FormID virginFID = virginActor->GetFormID();
    RE::FormID partnerFID = partnerActor ? partnerActor->GetFormID() : 0;
    auto* player = RE::PlayerCharacter::GetSingleton();
    RE::FormID playerFormID = player ? player->GetFormID() : 0;

    bool virginIsNpc = (virginFID != playerFormID);
    bool partnerIsNpc = (partnerActor != nullptr && partnerFID != playerFormID);
    if (!virginIsNpc && !partnerIsNpc) return;

    std::string virginName = ThreadDataStore::GetActorDisplayName(virginActor, "someone");
    std::string partnerName = partnerActor ? ThreadDataStore::GetActorDisplayName(partnerActor, "") : "";
    std::string locationName = ResolveActorLocationName(virginActor, partnerActor);

    MemorySpec virginMem;
    MemorySpec partnerMem;

    switch (intent) {
        case Intent::Aggressive:
            if (!virginIsMain) {
                virginMem.text = std::string("Had my virginity forcibly taken against my will") +
                                 (!partnerName.empty() ? (" by " + partnerName) : "") +
                                 " during a non-consensual sexual assault at " + locationName + ".";
                virginMem.importance = 0.95f;
                virginMem.memType = "TRAUMA";
                virginMem.emotion = "traumatized";
                virginMem.tags = "[\"virginity\",\"first_time\",\"assault\",\"trauma\"]";

                partnerMem.text = "Forcibly took " + virginName +
                                  "'s virginity against her will during a non-consensual sexual assault at " +
                                  locationName + ".";
                partnerMem.importance = 0.85f;
                partnerMem.memType = "EXPERIENCE";
                partnerMem.emotion = "neutral";
                partnerMem.tags = "[\"virginity\",\"first_time\",\"assault\"]";
            } else {
                virginMem.text = !partnerName.empty()
                    ? ("Lost my virginity by forcing myself on " + partnerName +
                       " against their will during a non-consensual sexual assault at " + locationName + ".")
                    : ("Lost my virginity as the aggressor during a non-consensual sexual assault at " +
                       locationName + ".");
                virginMem.importance = 0.90f;
                virginMem.memType = "EXPERIENCE";
                virginMem.emotion = "neutral";
                virginMem.tags = "[\"virginity\",\"first_time\",\"assault\"]";

                partnerMem.text = "Was sexually assaulted against my will by " + virginName + " at " +
                                  locationName + ", where she forced herself on me for her first time.";
                partnerMem.importance = 0.95f;
                partnerMem.memType = "TRAUMA";
                partnerMem.emotion = "traumatized";
                partnerMem.tags = "[\"virginity\",\"first_time\",\"assault\",\"trauma\"]";
            }
            break;

        case Intent::Dom:
            if (!virginIsMain) {
                virginMem.text = std::string("Willingly surrendered my virginity as the submissive partner") +
                                 (!partnerName.empty() ? (" to " + partnerName) : "") +
                                 " during a consensual dominant-submissive encounter at " + locationName + ".";
                virginMem.importance = 0.95f;
                virginMem.memType = "RELATIONSHIP";
                virginMem.emotion = "content";
                virginMem.tags = "[\"virginity\",\"first_time\",\"intimacy\",\"submission\"]";

                partnerMem.text = "Took " + virginName +
                                  "'s virginity as the dominant partner while she willingly submitted to me at " +
                                  locationName + ".";
                partnerMem.importance = 0.85f;
                partnerMem.memType = "RELATIONSHIP";
                partnerMem.emotion = "content";
                partnerMem.tags = "[\"virginity\",\"first_time\",\"intimacy\",\"dominance\"]";
            } else {
                virginMem.text = std::string("Lost my virginity while taking command as the dominant partner") +
                                 (!partnerName.empty() ? (" over " + partnerName) : "") +
                                 " during a consensual dominant-submissive encounter at " + locationName + ".";
                virginMem.importance = 0.95f;
                virginMem.memType = "RELATIONSHIP";
                virginMem.emotion = "content";
                virginMem.tags = "[\"virginity\",\"first_time\",\"intimacy\",\"dominance\"]";

                partnerMem.text = "Willingly submitted to " + virginName + " at " + locationName +
                                  " as her very first sexual partner while she took the dominant role.";
                partnerMem.importance = 0.85f;
                partnerMem.memType = "RELATIONSHIP";
                partnerMem.emotion = "content";
                partnerMem.tags = "[\"virginity\",\"first_time\",\"intimacy\",\"submission\"]";
            }
            break;

        case Intent::Romantic:
            virginMem.text = std::string("Shared my very first time and lost my virginity") +
                             (!partnerName.empty() ? (" to " + partnerName) : "") +
                             " during a loving, romantic encounter at " + locationName + ".";
            virginMem.importance = 0.95f;
            virginMem.memType = "RELATIONSHIP";
            virginMem.emotion = "joyful";
            virginMem.tags = "[\"virginity\",\"first_time\",\"intimacy\",\"romance\"]";

            partnerMem.text = "Was " + virginName + "'s very first sexual partner during a loving, romantic encounter at " +
                              locationName + ".";
            partnerMem.importance = 0.85f;
            partnerMem.memType = "RELATIONSHIP";
            partnerMem.emotion = "joyful";
            partnerMem.tags = "[\"virginity\",\"first_time\",\"intimacy\",\"romance\"]";
            break;

        case Intent::Transactional:
            if (!virginIsMain) {
                virginMem.text = std::string("Lost my virginity while providing sexual services") +
                                 (!partnerName.empty() ? (" to " + partnerName) : "") +
                                 " in a transactional arrangement at " + locationName + ".";
                virginMem.importance = 0.90f;
                virginMem.memType = "EXPERIENCE";
                virginMem.emotion = "neutral";
                virginMem.tags = "[\"virginity\",\"first_time\",\"transactional\"]";

                partnerMem.text = "Was " + virginName +
                                  "'s very first sexual partner as the client in a transactional arrangement at " +
                                  locationName + ".";
                partnerMem.importance = 0.80f;
                partnerMem.memType = "EXPERIENCE";
                partnerMem.emotion = "neutral";
                partnerMem.tags = "[\"virginity\",\"first_time\",\"transactional\"]";
            } else {
                virginMem.text = std::string("Lost my virginity") +
                                 (!partnerName.empty() ? (" with " + partnerName) : "") +
                                 " as the client in a transactional sexual arrangement at " + locationName + ".";
                virginMem.importance = 0.90f;
                virginMem.memType = "EXPERIENCE";
                virginMem.emotion = "neutral";
                virginMem.tags = "[\"virginity\",\"first_time\",\"transactional\"]";

                partnerMem.text = "Provided " + virginName +
                                  " with her very first sexual experience in a transactional arrangement at " +
                                  locationName + ".";
                partnerMem.importance = 0.80f;
                partnerMem.memType = "EXPERIENCE";
                partnerMem.emotion = "neutral";
                partnerMem.tags = "[\"virginity\",\"first_time\",\"transactional\"]";
            }
            break;

        default: {
            const char* encounterPhrase = (intent == Intent::Lustful)
                ? "a passionate, lustful sexual encounter"
                : "an intimate sexual encounter";
            virginMem.text = std::string("Lost my virginity") +
                             (!partnerName.empty() ? (" to " + partnerName) : "") +
                             " during " + encounterPhrase + " at " + locationName + ".";
            virginMem.importance = 0.92f;
            virginMem.memType = "RELATIONSHIP";
            virginMem.emotion = "content";
            virginMem.tags = "[\"virginity\",\"first_time\",\"intimacy\"]";

            partnerMem.text = "Was " + virginName + "'s very first sexual partner during " +
                              encounterPhrase + " at " + locationName + ".";
            partnerMem.importance = 0.85f;
            partnerMem.memType = "RELATIONSHIP";
            partnerMem.emotion = "content";
            partnerMem.tags = "[\"virginity\",\"first_time\",\"intimacy\"]";
            break;
        }
    }

    std::thread([virginFID, partnerFID, virginIsNpc, partnerIsNpc, partnerName, locationName,
                 virginMem = std::move(virginMem), partnerMem = std::move(partnerMem)]() {
        if (!PublicAddMemory || !PublicIsMemorySystemReady || !PublicIsMemorySystemReady()) {
            return;
        }

        if (virginIsNpc) {
            std::string relatedVirgin = (partnerFID != 0)
                ? ("[" + std::to_string(partnerFID) + "]")
                : "[]";
            PublicAddMemory(
                virginFID,
                virginMem.text.c_str(),
                virginMem.importance,
                virginMem.memType,
                virginMem.emotion,
                locationName.c_str(),
                virginMem.tags,
                relatedVirgin.c_str());
        }

        if (partnerIsNpc && partnerFID != 0 && !partnerName.empty()) {
            std::string relatedPartner = "[" + std::to_string(virginFID) + "]";
            PublicAddMemory(
                partnerFID,
                partnerMem.text.c_str(),
                partnerMem.importance,
                partnerMem.memType,
                partnerMem.emotion,
                locationName.c_str(),
                partnerMem.tags,
                relatedPartner.c_str());
        }
    }).detach();
}

}  // namespace

void OVirginityService::Initialize() {
    RE::TESQuest* quest = nullptr;
    RE::BGSKeyword* npcKw = nullptr;
    if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
        quest = dataHandler->LookupForm<RE::TESQuest>(0x800, "OVirginity.esp");
        npcKw = dataHandler->LookupForm<RE::BGSKeyword>(0x013794, "Skyrim.esm");
    }
    if (!quest) {
        quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("OVirginityQuest");
    }
    if (!npcKw) {
        npcKw = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeNPC");
    }
    _npcKeyword.store(npcKw, std::memory_order_release);
    _quest.store(quest, std::memory_order_release);
    if (quest) {
        SKSE::log::info("OVirginityService: OVirginityQuest resolved (0x{:08X}).", quest->GetFormID());
    }
}

bool OVirginityService::IsActorEligible(RE::Actor* actor) const {
    if (!actor || actor->IsChild()) return false;
    const auto* base = actor->GetActorBase();
    if (!base || base->GetSex() != RE::SEX::kFemale) return false;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player && actor == static_cast<RE::Actor*>(player)) return true;
    if (auto* kw = _npcKeyword.load(std::memory_order_acquire)) {
        return actor->HasKeyword(kw);
    }
    return actor->HasKeywordString("ActorTypeNPC");
}

bool OVirginityService::IsActorInTrackedThread(RE::FormID formID) const {
    std::lock_guard<std::mutex> lock(_threadMutex);
    for (const auto& [tid, virgins] : _threadVirgins) {
        if (virgins.contains(formID)) return true;
    }
    for (const auto& [tid, records] : _threadDeflowered) {
        for (const auto& rec : records) {
            if (rec.virginFormID == formID || rec.partnerFormID == formID) return true;
        }
    }
    return false;
}

void OVirginityService::OnVirginityResolved(RE::FormID formID, bool isVirgin) {
    std::unique_lock<std::shared_mutex> lock(_cacheMutex);
    _cache[formID] = CacheEntry{ isVirgin, std::chrono::steady_clock::now() };
    _inFlightQueries.erase(formID);
}

std::optional<bool> OVirginityService::GetCachedVirginity(RE::Actor* actor) {
    if (!IsLoaded() || !IsActorEligible(actor)) return std::nullopt;

    RE::FormID fid = actor->GetFormID();
    auto now = std::chrono::steady_clock::now();

    std::optional<bool> cached;
    bool needsRefresh = false;

    {
        std::shared_lock<std::shared_mutex> lock(_cacheMutex);
        auto it = _cache.find(fid);
        if (it != _cache.end()) {
            cached = it->second.isVirgin;
            if ((now - it->second.timestamp) >= kCacheTTL) {
                needsRefresh = true;
            }
        } else {
            needsRefresh = true;
        }
    }

    if (needsRefresh && !IsActorInTrackedThread(fid)) {
        bool shouldQueue = false;
        {
            std::unique_lock<std::shared_mutex> lock(_cacheMutex);
            if (_inFlightQueries.insert(fid).second) {
                shouldQueue = true;
            }
        }
        if (shouldQueue) {
            if (!UI::AddCurrentEpochTask([this, fid]() {
                    EnsureActorsResolved({ fid }, []() {}, true);
                })) {
                std::unique_lock<std::shared_mutex> lock(_cacheMutex);
                _inFlightQueries.erase(fid);
            }
        }
    }

    return cached;
}

void OVirginityService::EnsureActorsResolved(const std::vector<RE::FormID>& formIDs,
                                             std::function<void()> onReady,
                                             bool forceRefresh) {
    if (!IsLoaded() || formIDs.empty()) {
        if (onReady) onReady();
        return;
    }

    auto now = std::chrono::steady_clock::now();
    std::unordered_set<RE::FormID> seen;
    std::vector<RE::Actor*> toQuery;

    for (RE::FormID fid : formIDs) {
        if (fid == 0 || !seen.insert(fid).second) continue;
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
        if (!IsActorEligible(actor)) continue;

        bool queryNeeded = forceRefresh;
        if (!queryNeeded) {
            std::shared_lock<std::shared_mutex> lock(_cacheMutex);
            auto it = _cache.find(fid);
            if (it == _cache.end() || (now - it->second.timestamp) >= kCacheTTL) {
                queryNeeded = true;
            }
        }
        if (queryNeeded) {
            toQuery.push_back(actor);
        }
    }

    if (toQuery.empty()) {
        if (onReady) onReady();
        return;
    }

    RE::TESQuest* quest = _quest.load(std::memory_order_acquire);
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
    RE::VMHandle handle = (policy && quest) ? policy->GetHandleForObject(quest->GetFormType(), quest) : 0;

    if (!vm || !policy || !quest || !handle || handle == policy->EmptyHandle()) {
        SKSE::log::warn("OVirginityService: OVirginityQuest VM handle unavailable, skipping resolution");
        {
            std::unique_lock<std::shared_mutex> lock(_cacheMutex);
            for (auto* a : toQuery) _inFlightQueries.erase(a->GetFormID());
        }
        if (onReady) onReady();
        return;
    }

    auto state = std::make_shared<BatchState>();
    state->remaining.store(static_cast<int>(toQuery.size()));
    state->expectedEpoch = UI::GetSessionEpoch();
    state->onReady = std::move(onReady);

    for (RE::Actor* actor : toQuery) {
        RE::FormID fid = actor->GetFormID();
        {
            std::unique_lock<std::shared_mutex> lock(_cacheMutex);
            _inFlightQueries.insert(fid);
        }
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback =
            RE::make_smart<IsVirginCallback>(fid, state);
        RE::Actor* argActor = actor;
        bool ok = vm->DispatchMethodCall(
            handle,
            "OVirginityScript",
            "isVirgin",
            RE::MakeFunctionArguments(std::move(argActor)),
            callback);
        if (!ok) {
            SKSE::log::warn("OVirginityService: DispatchMethodCall(isVirgin) failed for 0x{:08X}", fid);
            {
                std::unique_lock<std::shared_mutex> lock(_cacheMutex);
                _inFlightQueries.erase(fid);
            }
            if (state->remaining.fetch_sub(1) == 1) {
                state->FinishOnce();
            }
        }
    }

    if (!state->finished.load()) {
        std::thread([state]() {
            std::this_thread::sleep_for(kResolveTimeout);
            if (!state->finished.load()) {
                SKSE::log::warn("OVirginityService: EnsureActorsResolved timed out ({} pending), continuing",
                                state->remaining.load());
                state->FinishOnce();
            }
        }).detach();
    }
}

void OVirginityService::OnThreadStart(int threadID, const std::vector<uint32_t>& actorFormIDs) {
    if (!IsLoaded() || actorFormIDs.empty()) return;

    auto now = std::chrono::steady_clock::now();
    std::unordered_set<RE::FormID> seededVirgins;
    std::vector<RE::FormID> toResolve;
    toResolve.reserve(actorFormIDs.size());

    for (uint32_t rawFid : actorFormIDs) {
        RE::FormID fid = static_cast<RE::FormID>(rawFid);
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
        if (!IsActorEligible(actor)) continue;

        bool seededFromFreshTrue = false;
        {
            std::shared_lock<std::shared_mutex> cacheLock(_cacheMutex);
            auto it = _cache.find(fid);
            if (it != _cache.end() && it->second.isVirgin && (now - it->second.timestamp) < kCacheTTL) {
                seededVirgins.insert(fid);
                seededFromFreshTrue = true;
            }
        }
        if (!seededFromFreshTrue) {
            toResolve.push_back(fid);
        }
    }

    if (!seededVirgins.empty()) {
        std::lock_guard<std::mutex> lock(_threadMutex);
        auto& dst = _threadVirgins[threadID];
        dst.insert(seededVirgins.begin(), seededVirgins.end());
        SKSE::log::info("OVirginityService: thread {} seeded with {} cached virgin(s)",
                        threadID, dst.size());
    }

    if (toResolve.empty()) return;

    EnsureActorsResolved(toResolve, [this, threadID, toResolve]() {
        if (ThreadDataStore::GetSingleton().GetPhase(threadID) == ThreadPhase::Ended) return;
        if (g_ostimThreadInterface && !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) return;

        std::unordered_set<RE::FormID> resolvedVirgins;
        {
            std::shared_lock<std::shared_mutex> cacheLock(_cacheMutex);
            for (RE::FormID fid : toResolve) {
                auto it = _cache.find(fid);
                if (it != _cache.end() && it->second.isVirgin) {
                    resolvedVirgins.insert(fid);
                }
            }
        }

        if (!resolvedVirgins.empty()) {
            {
                std::lock_guard<std::mutex> lock(_threadMutex);
                auto& dst = _threadVirgins[threadID];
                dst.insert(resolvedVirgins.begin(), resolvedVirgins.end());
                SKSE::log::info("OVirginityService: thread {} resolved {} additional virgin(s)",
                                threadID, resolvedVirgins.size());
            }
            if (g_ostimThreadInterface &&
                g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) {
                if (const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
                    curScene && curScene[0] != '\0') {
                    OnSceneChanged(threadID, curScene);
                }
            }
        }
    }, true);
}

void OVirginityService::TransferForContinuation(int oldThreadID,
                                                int newThreadID,
                                                const std::vector<uint32_t>& newActorFormIDs) {
    if (!IsLoaded()) return;

    std::unordered_set<RE::FormID> existingKnown;
    {
        std::lock_guard<std::mutex> lock(_threadMutex);
        if (oldThreadID != newThreadID) {
            if (auto it = _threadVirgins.find(oldThreadID); it != _threadVirgins.end()) {
                _threadVirgins[newThreadID] = std::move(it->second);
                _threadVirgins.erase(it);
            }
            if (auto it = _threadDeflowered.find(oldThreadID); it != _threadDeflowered.end()) {
                _threadDeflowered[newThreadID] = std::move(it->second);
                _threadDeflowered.erase(it);
            }
            if (_threadHadVaginalScene.erase(oldThreadID) > 0) {
                _threadHadVaginalScene.insert(newThreadID);
            }
        }
        if (auto it = _threadVirgins.find(newThreadID); it != _threadVirgins.end()) {
            for (RE::FormID fid : it->second) existingKnown.insert(fid);
        }
        if (auto it = _threadDeflowered.find(newThreadID); it != _threadDeflowered.end()) {
            for (const auto& rec : it->second) existingKnown.insert(rec.virginFormID);
        }
    }

    std::vector<RE::FormID> newFids;
    for (uint32_t fid : newActorFormIDs) {
        RE::FormID rfid = static_cast<RE::FormID>(fid);
        if (!existingKnown.contains(rfid)) {
            newFids.push_back(rfid);
        }
    }
    if (newFids.empty()) return;

    EnsureActorsResolved(newFids, [this, newThreadID, newFids]() {
        if (ThreadDataStore::GetSingleton().GetPhase(newThreadID) == ThreadPhase::Ended) return;
        std::unordered_set<RE::FormID> newVirgins;
        {
            std::shared_lock<std::shared_mutex> cacheLock(_cacheMutex);
            for (RE::FormID fid : newFids) {
                auto it = _cache.find(fid);
                if (it != _cache.end() && it->second.isVirgin) {
                    newVirgins.insert(fid);
                }
            }
        }
        if (!newVirgins.empty()) {
            std::lock_guard<std::mutex> lock(_threadMutex);
            auto& dst = _threadVirgins[newThreadID];
            for (RE::FormID fid : newVirgins) {
                dst.insert(fid);
            }
        }
    }, true);
}

bool OVirginityService::ThreadHasActiveVirgin(int threadID) const {
    std::lock_guard<std::mutex> lock(_threadMutex);
    auto it = _threadVirgins.find(threadID);
    return it != _threadVirgins.end() && !it->second.empty();
}

void OVirginityService::ClampInitialVirginSpeed(int threadID, const std::string& sceneID) {
    if (!IsLoaded() || sceneID.empty() || !ThreadHasActiveVirgin(threadID)) return;
    if (PulloutService::GetVaginalPairs(sceneID.c_str()).empty()) return;

    Intent intent = ThreadDataStore::GetSingleton().GetIntent(threadID);
    if ((intent == Intent::Romantic || intent == Intent::Lustful) && g_ostimThreadInterface) {
        uint32_t uid = static_cast<uint32_t>(threadID);
        if (g_ostimThreadInterface->IsThreadValid(uid) &&
            g_ostimThreadInterface->GetMaxSpeed(uid) > 0 &&
            g_ostimThreadInterface->GetCurrentSpeed(uid) > 0) {
            g_ostimThreadInterface->SetSpeed(uid, 0);
            SKSE::log::info("OVirginityService: clamped initial intercourse speed to 0 for gentle first-time pacing on thread {}",
                            threadID);
        }
    }
}

void OVirginityService::OnSceneChanged(int threadID, const std::string& sceneID) {
    if (!IsLoaded() || sceneID.empty() || !ThreadHasActiveVirgin(threadID)) return;
    if (PulloutService::GetVaginalPairs(sceneID.c_str()).empty()) return;

    {
        std::lock_guard<std::mutex> lock(_threadMutex);
        _threadHadVaginalScene.insert(threadID);
    }

    ClampInitialVirginSpeed(threadID, sceneID);

    const auto expectedEpoch = UI::GetSessionEpoch();
    std::thread([this, threadID, sceneID, expectedEpoch]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        UI::AddEpochTask(expectedEpoch, [this, threadID, sceneID]() {
            CheckThreadVirginityLoss(threadID, sceneID);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(1300));
        UI::AddEpochTask(expectedEpoch, [this, threadID, sceneID]() {
            if (ThreadDataStore::GetSingleton().GetPhase(threadID) != ThreadPhase::Ended &&
                ThreadHasActiveVirgin(threadID)) {
                CheckThreadVirginityLoss(threadID, sceneID);
            }
        });
    }).detach();
}

void OVirginityService::CheckThreadVirginityLoss(int threadID, const std::string& sceneID) {
    if (!IsLoaded()) return;

    std::vector<RE::FormID> candidates;
    {
        std::lock_guard<std::mutex> lock(_threadMutex);
        auto it = _threadVirgins.find(threadID);
        if (it == _threadVirgins.end() || it->second.empty()) return;
        candidates.assign(it->second.begin(), it->second.end());
    }

    EnsureActorsResolved(candidates, [this, threadID, sceneID, candidates]() {
        std::vector<RE::FormID> newlyLost;
        {
            std::lock_guard<std::mutex> threadLock(_threadMutex);
            auto tvIt = _threadVirgins.find(threadID);
            if (tvIt == _threadVirgins.end() || tvIt->second.empty()) return;

            std::shared_lock<std::shared_mutex> cacheLock(_cacheMutex);
            for (RE::FormID fid : candidates) {
                if (!tvIt->second.contains(fid)) continue;
                auto cIt = _cache.find(fid);
                if (cIt != _cache.end() && !cIt->second.isVirgin) {
                    tvIt->second.erase(fid);
                    newlyLost.push_back(fid);
                }
            }
        }

        if (newlyLost.empty()) return;

        auto& store = ThreadDataStore::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        RE::FormID playerFormID = player ? player->GetFormID() : 0;

        std::string effectiveScene = sceneID;
        if (effectiveScene.empty() && g_ostimThreadInterface &&
            g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) {
            if (const char* s = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID))) {
                effectiveScene = s;
            }
        }

        auto pairs = PulloutService::GetVaginalPairs(effectiveScene.c_str());
        constexpr uint32_t kMaxActors = 8;
        OstimNG_API::Thread::ActorData buf[kMaxActors]{};
        uint32_t count = (g_ostimThreadInterface &&
                          g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)))
            ? g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buf, kMaxActors)
            : 0;

        auto resolvePartner = [&](RE::FormID virginFID) -> RE::Actor* {
            int virginSlot = -1;
            for (uint32_t i = 0; i < count; ++i) {
                if (buf[i].formID == virginFID) {
                    virginSlot = static_cast<int>(i);
                    break;
                }
            }
            if (virginSlot >= 0) {
                for (const auto& pair : pairs) {
                    int partnerSlot = -1;
                    if (pair.receiverSlot == virginSlot) {
                        partnerSlot = pair.giverSlot;
                    } else if (pair.giverSlot == virginSlot) {
                        partnerSlot = pair.receiverSlot;
                    }
                    if (partnerSlot >= 0 && static_cast<uint32_t>(partnerSlot) < count) {
                        if (auto* p = RE::TESForm::LookupByID<RE::Actor>(buf[partnerSlot].formID)) {
                            return p;
                        }
                    }
                }
            }
            for (RE::Actor* a : store.GetActorPtrs(threadID)) {
                if (a && a->GetFormID() != virginFID) {
                    return a;
                }
            }
            return nullptr;
        };

        Intent intent = store.GetIntent(threadID);
        std::vector<RE::FormID> mainFIDs = CollectActorFormIDs(store.GetMainActors(threadID));
        std::vector<RE::FormID> secFIDs = CollectActorFormIDs(store.GetSecondaryActors(threadID));

        if (auto* listener = OStimEventListener::GetInstance()) {
            listener->MarkSkipNextSceneTrigger(threadID);
        }

        for (RE::FormID virginFID : newlyLost) {
            RE::Actor* virginActor = RE::TESForm::LookupByID<RE::Actor>(virginFID);
            if (!virginActor) continue;

            RE::Actor* partnerActor = resolvePartner(virginFID);
            RE::FormID partnerFID = partnerActor ? partnerActor->GetFormID() : 0;
            bool virginIsMain = ResolveVirginIsMain(mainFIDs, secFIDs, virginFID);

            {
                std::lock_guard<std::mutex> lock(_threadMutex);
                _threadDeflowered[threadID].push_back(
                    DefloweredRecord{ virginFID, partnerFID, effectiveScene, static_cast<int>(intent), virginIsMain });
            }

            RE::Actor* speaker = nullptr;
            RE::Actor* target = nullptr;
            if (virginFID != playerFormID) {
                speaker = virginActor;
                target = partnerActor ? partnerActor : virginActor;
            } else {
                speaker = (partnerActor && partnerFID != playerFormID)
                    ? partnerActor
                    : SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
                target = virginActor;
            }

            if (speaker) {
                std::string jsonStr = EventPayloadBuilder::BuildVirginityLost(
                    threadID, virginActor, partnerActor, speaker, intent, virginIsMain);
                DispatchRegisterEvent(jsonStr, speaker, target);
            }

            std::string virginName = ThreadDataStore::GetActorDisplayName(virginActor, "someone");
            std::string partnerName = partnerActor ? ThreadDataStore::GetActorDisplayName(partnerActor, "") : "";
            SKSE::log::info("OVirginityService: thread {} virginity lost ({} -> partner='{}', role={}, intent={}, scene='{}')",
                            threadID, virginName, partnerName, virginIsMain ? "main" : "secondary",
                            IntentToString(intent), effectiveScene);

            DispatchVirginityLossMemory(virginActor, partnerActor, intent, virginIsMain);
        }
    }, true);
}

void OVirginityService::OnThreadEnd(int threadID) {
    std::vector<DefloweredRecord> deflowered;
    std::unordered_set<RE::FormID> remainingVirgins;
    bool hadVaginal = false;
    {
        std::lock_guard<std::mutex> lock(_threadMutex);
        if (auto tvIt = _threadVirgins.find(threadID); tvIt != _threadVirgins.end()) {
            remainingVirgins = std::move(tvIt->second);
            _threadVirgins.erase(tvIt);
        }
        if (auto tdIt = _threadDeflowered.find(threadID); tdIt != _threadDeflowered.end()) {
            deflowered = std::move(tdIt->second);
            _threadDeflowered.erase(tdIt);
        }
        hadVaginal = (_threadHadVaginalScene.erase(threadID) > 0);
    }

    auto dispatchDiaries = [threadID](const std::vector<DefloweredRecord>& records) {
        if (records.empty()) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        RE::FormID playerFormID = player ? player->GetFormID() : 0;

        for (const auto& rec : records) {
            RE::Actor* virginActor = RE::TESForm::LookupByID<RE::Actor>(rec.virginFormID);
            RE::Actor* partnerActor = rec.partnerFormID ? RE::TESForm::LookupByID<RE::Actor>(rec.partnerFormID) : nullptr;
            if (!virginActor) continue;

            RE::Actor* diaryActor = nullptr;
            if (rec.virginFormID != playerFormID) {
                diaryActor = virginActor;
            } else if (partnerActor && rec.partnerFormID != playerFormID) {
                diaryActor = partnerActor;
            }
            if (!diaryActor) continue;

            RE::Actor* target = (diaryActor == virginActor && partnerActor) ? partnerActor : virginActor;
            std::string jsonStr = EventPayloadBuilder::BuildVirginityLostDiary(
                threadID, virginActor, partnerActor, diaryActor,
                static_cast<Intent>(rec.intentRaw), rec.virginIsMain);
            DispatchRegisterEvent(jsonStr, diaryActor, target);
        }
    };

    if (remainingVirgins.empty() || !hadVaginal || !IsLoaded()) {
        dispatchDiaries(deflowered);
        return;
    }

    // Fallback check: catch deflowering if the thread ended before the 500ms/1800ms poll saw IsVirgin == 0.
    // Snapshot thread metadata synchronously before ThreadDataStore::ClearThread(threadID) runs.
    auto& store = ThreadDataStore::GetSingleton();
    Intent intent = store.GetIntent(threadID);
    std::vector<RE::FormID> threadActorFIDs = CollectActorFormIDs(store.GetActorPtrs(threadID));
    std::vector<RE::FormID> mainFIDs = CollectActorFormIDs(store.GetMainActors(threadID));
    std::vector<RE::FormID> secFIDs = CollectActorFormIDs(store.GetSecondaryActors(threadID));

    std::vector<RE::FormID> remainingVec(remainingVirgins.begin(), remainingVirgins.end());
    EnsureActorsResolved(
        remainingVec,
        [this, dispatchDiaries, deflowered = std::move(deflowered), remainingVec,
         threadActorFIDs, mainFIDs, secFIDs, intent]() mutable {
            std::vector<RE::FormID> newlyLost;
            {
                std::shared_lock<std::shared_mutex> cacheLock(_cacheMutex);
                for (RE::FormID fid : remainingVec) {
                    auto cIt = _cache.find(fid);
                    if (cIt != _cache.end() && !cIt->second.isVirgin) {
                        newlyLost.push_back(fid);
                    }
                }
            }

            for (RE::FormID virginFID : newlyLost) {
                RE::Actor* virginActor = RE::TESForm::LookupByID<RE::Actor>(virginFID);
                if (!virginActor) continue;

                RE::Actor* partnerActor = nullptr;
                RE::FormID partnerFID = 0;
                for (RE::FormID cand : threadActorFIDs) {
                    if (cand != virginFID) {
                        if (auto* p = RE::TESForm::LookupByID<RE::Actor>(cand)) {
                            partnerActor = p;
                            partnerFID = cand;
                            break;
                        }
                    }
                }

                bool virginIsMain = ResolveVirginIsMain(mainFIDs, secFIDs, virginFID);
                deflowered.push_back(DefloweredRecord{
                    virginFID, partnerFID, "", static_cast<int>(intent), virginIsMain });
                DispatchVirginityLossMemory(virginActor, partnerActor, intent, virginIsMain);
            }

            dispatchDiaries(deflowered);
        },
        true);
}

void OVirginityService::ClearThread(int threadID) {
    std::lock_guard<std::mutex> lock(_threadMutex);
    _threadVirgins.erase(threadID);
    _threadDeflowered.erase(threadID);
    _threadHadVaginalScene.erase(threadID);
}

void OVirginityService::Reset() {
    {
        std::unique_lock<std::shared_mutex> cacheLock(_cacheMutex);
        _cache.clear();
        _inFlightQueries.clear();
    }
    {
        std::lock_guard<std::mutex> threadLock(_threadMutex);
        _threadVirgins.clear();
        _threadDeflowered.clear();
        _threadHadVaginalScene.clear();
    }
    SKSE::log::info("OVirginityService: reset on game load.");
}

}  // namespace OStimNet
