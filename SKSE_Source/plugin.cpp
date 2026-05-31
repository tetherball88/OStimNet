#include <Windows.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "PCH.h"
#include "src/Config.h"
#include "src/SkyrimNetIntegration.h"
#include "src/api/OstimNG-API-Thread.h"
#include "src/api/OStimNavigator_PublicAPI.h"
#include "src/Papyrus/PapyrusFunctions.h"
#include "src/OStimEventListener.h"
#include "src/LocationScanService.h"

using namespace SKSE;

namespace {
    void SetupLogging() {
        auto logDir = SKSE::log::log_directory();
        if (!logDir) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("OstimNet: log directory unavailable");
            }
            return;
        }

        std::filesystem::path logPath = *logDir;
        if (!std::filesystem::is_directory(logPath)) {
            logPath = logPath.parent_path();
        }
        logPath /= "OstimNet.log";

        std::error_code ec;
        std::filesystem::create_directories(logPath.parent_path(), ec);
        if (ec) {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("OstimNet: failed to create log folder (%s)", ec.message().c_str());
            }
            return;
        }

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto logger = std::make_shared<spdlog::logger>("OstimNet", std::move(sink));
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);
        logger->set_pattern("[%H:%M:%S] [%l] %v");

        spdlog::set_default_logger(std::move(logger));
    }

    void PrintToConsole(std::string_view message) {
        SKSE::log::info("{}", message);
        if (auto* console = RE::ConsoleLog::GetSingleton()) {
            console->Print("%s", message.data());
        }
    }
}

class HotkeyInputSink : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static HotkeyInputSink* GetSingleton() {
        static HotkeyInputSink s;
        return &s;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events,
                                          RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!a_events) return RE::BSEventNotifyControl::kContinue;

        for (auto* event = *a_events; event; event = event->next) {
            auto* btn = event->AsButtonEvent();
            if (!btn || !btn->IsDown()) continue;

            // SKSE::log::info("OStimNet: toggleMuteHotkey={}, locationScanHotkey={}",
            //                 OStimNet::Config::GetSingleton().ToggleMuteHotkey(),
            //                 OStimNet::Config::GetSingleton().LocationScanHotkey());

            int hotkey = OStimNet::Config::GetSingleton().ToggleMuteHotkey();
            // SKSE::log::debug("HotkeyInputSink: button event detected (device={}, code={}, isDown={}), toggleMuteHotkey={}",
            //                 event->GetDevice(), btn->GetIDCode(), btn->IsDown(), hotkey);
            if (hotkey <= 0) continue;

            uint32_t diCode = MapVirtualKeyA(static_cast<uint32_t>(hotkey), MAPVK_VK_TO_VSC);
            if (btn->GetIDCode() == diCode) {
                SKSE::log::info("OStimNet: toggleMuteHotkey fired (key={})", hotkey);
                OStimNet::Config::GetSingleton().ToggleMuteSession();
            }

            int scanHotkey = OStimNet::Config::GetSingleton().LocationScanHotkey();
            if (scanHotkey > 0) {
                uint32_t scanDiCode = MapVirtualKeyA(static_cast<uint32_t>(scanHotkey), MAPVK_VK_TO_VSC);
                if (btn->GetIDCode() == scanDiCode) {
                    SKSE::log::info("OStimNet: locationScanHotkey fired (key={})", scanHotkey);
                    OStimNet::LocationScanService::GetSingleton().TriggerManualScan();
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

SKSEPluginLoad(const LoadInterface* skse) {
    SKSE::Init(skse);

    SetupLogging();
    SKSE::log::info("OstimNet plugin loading...");

    // Check if OStim is loaded (it should load before us alphabetically)
    if (const auto* ostimInfo = skse->GetPluginInfo("OStim")) {
        SKSE::log::info("Found OStim plugin version {}.{}.{}",
            ostimInfo->version >> 24,
            (ostimInfo->version >> 16) & 0xFF,
            (ostimInfo->version >> 8) & 0xFF);
    }

    if (const auto* papyrus = SKSE::GetPapyrusInterface()) {
        if (!papyrus->Register(OStimNet::Papyrus::PapyrusFunctions::Register)) {
            SKSE::log::critical("Failed to register Papyrus Decorators.");
            return false;
        }
    } else {
        SKSE::log::critical("Papyrus interface unavailable.");
        return false;
    }

    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        if (!messaging->RegisterListener([](SKSE::MessagingInterface::Message* message) {
                switch (message->type) {
                    case SKSE::MessagingInterface::kPreLoadGame:
                        SKSE::log::info("PreLoadGame...");
                        OStimNet::Config::GetSingleton().ResetMuteOverride();
                        if (auto* listener = OStimNet::OStimEventListener::GetInstance())
                            listener->Reset();
                        OStimNet::LocationScanService::GetSingleton().Reset();
                        break;

                    case SKSE::MessagingInterface::kPostLoadGame:
                    case SKSE::MessagingInterface::kNewGame:
                        SKSE::log::info("New game/Load...");
                        OStimNet::Config::GetSingleton().InitFromConfig();
                        if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
                            idm->AddEventSink(HotkeyInputSink::GetSingleton());
                            SKSE::log::info("OStimNet: hotkey input sink re-registered after load.");
                        }
                        OStimNet::LocationScanService::GetSingleton().OnGameReady();
                        break;

                    case SKSE::MessagingInterface::kDataLoaded: {
                        SKSE::log::info("Data loaded successfully.");

                        // Connect to OStim, OStimNavigator, and SkyrimNet APIs,
                        // then register all SkyrimNet decorators.
                        g_ostimThreadInterface = OstimNG_API::Thread::GetAPI(
                            "OStimNet",
                            SKSE::PluginDeclaration::GetSingleton()->GetVersion());
                        if (g_ostimThreadInterface) {
                            SKSE::log::info("OStim API connected successfully.");
                        } else {
                            SKSE::log::warn("OStim API unavailable — scene description decorator will return empty.");
                        }

                        if (ONavFindFunctions()) {
                            SKSE::log::info("OStimNavigator API connected successfully.");
                        } else {
                            SKSE::log::warn("OStimNavigator not found — scene descriptions will fall back to node names.");
                        }

                        if (OStimNet::SkyrimNetIntegration::InitSkyrimNetAPI()) {
                            SKSE::log::info("SkyrimNet API v{} connected.", OStimNet::SkyrimNetIntegration::GetSkyrimNetAPIVersion());
                        } else {
                            SKSE::log::warn("SkyrimNet not found — decorator registration skipped.");
                        }

                        OStimNet::SkyrimNetIntegration::Register();
                        OStimNet::Config::GetSingleton().ApplyLogLevel();
                        OStimNet::OStimEventListener::Register();
                        OStimNet::LocationScanService::GetSingleton().Register();

                        break;
                    }

                    default:
                        break;
                }
            })) {
            SKSE::log::critical("Failed to register messaging listener.");
            return false;
        }
    } else {
        SKSE::log::critical("Messaging interface unavailable.");
        return false;
    }

    return true;
}
