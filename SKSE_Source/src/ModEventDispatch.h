#pragma once
#include <string>

namespace OStimNet {

// Dispatches a mod callback event via AddTask so it never fires while the
// BSTEventSource spin lock is still held (e.g. inside a ProcessEvent callback).
// Calling SendEvent re-entrantly on the same source deadlocks because
// BSTEventSource::SendEvent holds its BSSpinLock for the full duration.
inline void FireModEvent(const char* eventName, const char* strArg, float numArg,
                         RE::TESForm* sender = nullptr) {
    std::string evName(eventName);
    std::string strArgStr(strArg ? strArg : "");
    RE::FormID senderID = sender ? sender->GetFormID() : 0;

    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask([evName, strArgStr, numArg, senderID]() {
            RE::TESForm* s = senderID ? RE::TESForm::LookupByID(senderID) : nullptr;
            if (auto* source = SKSE::GetModCallbackEventSource()) {
                SKSE::ModCallbackEvent e{evName.c_str(), strArgStr.c_str(), numArg, s};
                source->SendEvent(&e);
            }
        });
    }
}

}  // namespace OStimNet
