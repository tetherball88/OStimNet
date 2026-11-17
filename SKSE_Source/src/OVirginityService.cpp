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

    std::vector<RE::FormID> fids;
    fids.reserve(actorFormIDs.size());
    for (uint32_t fid : actorFormIDs) {
        fids.push_back(static_cast<RE::FormID>(fid));
    }

    EnsureActorsResolved(fids, [this, threadID, fids]() {
        if (ThreadDataStore::GetSingleton().GetPhase(threadID) == ThreadPhase::Ended) return;
        if (g_ostimThreadInterface && !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) return;

        std::unordered_set<RE::FormID> activeVirgins;
        {
            std::shared_lock<std::shared_mutex> cacheLock(_cacheMutex);
            for (RE::FormID fid : fids) {
                auto it = _cache.find(fid);
                if (it != _cache.end() && it->second.isVirgin) {
                    activeVirgins.insert(fid);
                }
            }
        }

        if (!activeVirgins.empty()) {
            std::lock_guard<std::mutex> lock(_threadMutex);
            _threadVirgins[threadID] = std::move(activeVirgins);
            SKSE::log::info("OVirginityService: thread {} started with {} virgin(s)",
                            threadID, _threadVirgins[threadID].size());
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

void OVirginityService::OnSceneChanged(int threadID, const std::string& sceneID) {
    if (!IsLoaded() || !ThreadHasActiveVirgin(threadID)) return;

    Intent intent = ThreadDataStore::GetSingleton().GetIntent(threadID);
    if (intent == Intent::Romantic || intent == Intent::Lustful) {
        auto phase = DetectPhaseFromSceneID(sceneID.c_str());
        if (phase == SexualPhase::Sex && g_ostimThreadInterface) {
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

    const auto expectedEpoch = UI::GetSessionEpoch();
    std::thread([this, threadID, sceneID, expectedEpoch]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        UI::AddEpochTask(expectedEpoch, [this, threadID, sceneID]() {
            CheckThreadVirginityLoss(threadID, sceneID);
        });
    }).detach();
}

void OVirginityService::OnOVirginityLostModEvent() {
    if (!IsLoaded()) return;

    std::vector<int> candidateThreads;
    {
        std::lock_guard<std::mutex> lock(_threadMutex);
        for (const auto& [tid, virgins] : _threadVirgins) {
            if (!virgins.empty()) {
                candidateThreads.push_back(tid);
            }
        }
    }

    for (int tid : candidateThreads) {
        std::string curScene;
        if (g_ostimThreadInterface && g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(tid))) {
            const char* s = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(tid));
            if (s) curScene = s;
        }
        CheckThreadVirginityLoss(tid, curScene);
    }
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
        std::string intentStr = IntentToString(intent);
        if (intentStr.empty() || intentStr == "unset") {
            intentStr = "intimate";
        }
        const bool isAggressive = (intent == Intent::Aggressive);
        const bool isRomantic = (intent == Intent::Romantic);

        if (auto* listener = OStimEventListener::GetInstance()) {
            listener->MarkSkipNextSceneTrigger(threadID);
        }

        for (RE::FormID virginFID : newlyLost) {
            RE::Actor* virginActor = RE::TESForm::LookupByID<RE::Actor>(virginFID);
            if (!virginActor) continue;

            RE::Actor* partnerActor = resolvePartner(virginFID);
            RE::FormID partnerFID = partnerActor ? partnerActor->GetFormID() : 0;

            {
                std::lock_guard<std::mutex> lock(_threadMutex);
                _threadDeflowered[threadID].push_back(DefloweredRecord{ virginFID, partnerFID, effectiveScene });
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
                std::string jsonStr = EventPayloadBuilder::BuildVirginityLost(threadID, virginActor, partnerActor, speaker);
                DispatchRegisterEvent(jsonStr, speaker, target);
            }

            std::string virginName = ThreadDataStore::GetActorDisplayName(virginActor, "someone");
            std::string partnerName = partnerActor ? ThreadDataStore::GetActorDisplayName(partnerActor, "") : "";
            std::string locationName = ResolveActorLocationName(virginActor, partnerActor);
            bool isFemaleVirgin = false;
            if (auto* base = virginActor->GetActorBase()) {
                isFemaleVirgin = (base->GetSex() == RE::SEX::kFemale);
            }
            bool partnerIsNpc = (partnerActor != nullptr && partnerFID != playerFormID);

            SKSE::log::info("OVirginityService: thread {} virginity lost ({} -> partner='{}', scene='{}')",
                            threadID, virginName, partnerName, effectiveScene);

            std::thread([virginFID, partnerFID, virginName, partnerName, locationName,
                         intentStr, isAggressive, isRomantic, isFemaleVirgin, partnerIsNpc]() {
                if (!PublicAddMemory || !PublicIsMemorySystemReady || !PublicIsMemorySystemReady()) {
                    return;
                }

                const char* memType = isAggressive ? "TRAUMA" : "RELATIONSHIP";
                const char* emotion = isAggressive ? "traumatized" : (isRomantic ? "tender" : "overwhelmed");
                std::string relatedVirgin = (partnerFID != 0)
                    ? ("[" + std::to_string(partnerFID) + "]")
                    : "[]";
                std::string virginText;
                if (!partnerName.empty()) {
                    virginText = virginName + " lost " + (isFemaleVirgin ? "her" : "their") +
                                 " virginity to " + partnerName + " at " + locationName +
                                 " during a " + intentStr + " intimate encounter.";
                } else {
                    virginText = virginName + " lost " + (isFemaleVirgin ? "her" : "their") +
                                 " virginity at " + locationName + ".";
                }

                PublicAddMemory(
                    virginFID,
                    virginText.c_str(),
                    0.95f,
                    memType,
                    emotion,
                    locationName.c_str(),
                    "[\"virginity\",\"first_time\",\"intimacy\"]",
                    relatedVirgin.c_str());

                if (partnerIsNpc && partnerFID != 0 && !partnerName.empty()) {
                    std::string relatedPartner = "[" + std::to_string(virginFID) + "]";
                    std::string partnerText = partnerName + " was " + virginName +
                                              "'s very first sexual partner at " + locationName +
                                              " during a " + intentStr + " intimate encounter.";
                    PublicAddMemory(
                        partnerFID,
                        partnerText.c_str(),
                        0.85f,
                        "RELATIONSHIP",
                        isAggressive ? "dominant" : (isRomantic ? "affectionate" : "intrigued"),
                        locationName.c_str(),
                        "[\"virginity\",\"first_time\",\"intimacy\"]",
                        relatedPartner.c_str());
                }
            }).detach();
        }
    }, true);
}

void OVirginityService::OnThreadEnd(int threadID) {
    std::vector<DefloweredRecord> deflowered;
    {
        std::lock_guard<std::mutex> lock(_threadMutex);
        _threadVirgins.erase(threadID);
        auto it = _threadDeflowered.find(threadID);
        if (it != _threadDeflowered.end()) {
            deflowered = std::move(it->second);
            _threadDeflowered.erase(it);
        }
    }

    if (deflowered.empty()) return;

    auto* player = RE::PlayerCharacter::GetSingleton();
    RE::FormID playerFormID = player ? player->GetFormID() : 0;

    for (const auto& rec : deflowered) {
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
        std::string jsonStr = EventPayloadBuilder::BuildVirginityLostDiary(threadID, virginActor, partnerActor, diaryActor);
        DispatchRegisterEvent(jsonStr, diaryActor, target);
    }
}

void OVirginityService::ClearThread(int threadID) {
    std::lock_guard<std::mutex> lock(_threadMutex);
    _threadVirgins.erase(threadID);
    _threadDeflowered.erase(threadID);
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
    }
    SKSE::log::info("OVirginityService: reset on game load.");
}

}  // namespace OStimNet
