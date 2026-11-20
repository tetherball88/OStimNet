#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace OStimNet::UI {

    enum class ModalTone : std::uint8_t {
        Default = 0,
        Warning,
        Danger,
        Success,
        Ai
    };

    enum class ModalSize : std::uint8_t {
        Sm = 0,
        Md,
        Lg,
        Xl
    };

    enum class ModalButtonVariant : std::uint8_t {
        Primary = 0,
        Secondary,
        Outline,
        Ghost,
        Danger
    };

    enum class ModalControlType : std::uint8_t {
        Checkbox = 0,
        Select,
        TextInput,
        Checklist,
        InfoBox
    };

    std::string_view ModalToneToString(ModalTone tone);
    std::string_view ModalSizeToString(ModalSize size);
    std::string_view ModalButtonVariantToString(ModalButtonVariant variant);
    std::string_view ModalControlTypeToString(ModalControlType type);

    struct ModalSelectOption {
        std::string value;
        std::string label;
        bool disabled{ false };
        std::string tooltip;

        nlohmann::json ToJson() const;
    };

    struct ModalChecklistItem {
        std::string id;
        std::string label;
        std::string subtitle;
        std::string description;
        std::string badgeText;
        std::string badgeTone{ "default" };
        bool checked{ true };
        bool checkboxDisabled{ false };

        std::string selectKey;
        std::string selectLabel;
        std::string selectValue;
        std::vector<ModalSelectOption> selectOptions;

        nlohmann::json meta;

        nlohmann::json ToJson() const;
    };

    struct ModalControl {
        ModalControlType type{ ModalControlType::Checkbox };
        std::string id;
        std::string label;
        std::string description;
        bool required{ false };
        bool disabled{ false };
        nlohmann::json defaultValue;
        std::string placeholder;

        std::vector<ModalSelectOption> options;
        std::vector<ModalChecklistItem> items;
        bool showSelectAllToggle{ false };
        std::string emptyMessage;
        ModalTone tone{ ModalTone::Default };

        nlohmann::json ToJson() const;

        static ModalControl MakeCheckbox(std::string id,
                                         std::string label,
                                         bool defaultChecked = false,
                                         std::string description = "",
                                         bool disabled = false);

        static ModalControl MakeSelect(std::string id,
                                       std::string label,
                                       std::vector<ModalSelectOption> options,
                                       std::string defaultValue = "",
                                       std::string description = "",
                                       bool disabled = false);

        static ModalControl MakeTextInput(std::string id,
                                          std::string label,
                                          std::string defaultValue = "",
                                          std::string placeholder = "",
                                          std::string description = "",
                                          bool disabled = false);

        static ModalControl MakeChecklist(std::string id,
                                          std::string label,
                                          std::vector<ModalChecklistItem> items,
                                          bool showSelectAllToggle = false,
                                          std::string description = "",
                                          std::string emptyMessage = "");

        static ModalControl MakeInfoBox(std::string id,
                                        std::string label,
                                        std::string description,
                                        ModalTone tone = ModalTone::Default);
    };

    struct ModalButton {
        std::string id;
        std::string label;
        ModalButtonVariant variant{ ModalButtonVariant::Secondary };
        bool disabled{ false };
        std::string tooltip;
        std::string requireChecklistId;
        std::int32_t minCheckedCount{ 1 };
        std::int32_t maxCheckedCount{ 0 };

        nlohmann::json ToJson() const;

        static ModalButton Make(std::string id,
                                std::string label,
                                ModalButtonVariant variant = ModalButtonVariant::Secondary,
                                bool disabled = false,
                                std::string tooltip = "");
    };

    struct ModalRequest {
        std::string modalId;
        std::string modalType{ "generic" };
        std::string title;
        std::string subtitle;
        std::string message;
        ModalTone tone{ ModalTone::Default };
        ModalSize size{ ModalSize::Md };
        std::int32_t defaultCancelButtonIndex{ 1 };
        std::int32_t associatedThreadId{ -1 };

        std::vector<ModalControl> controls;
        std::vector<ModalButton> buttons;
        nlohmann::json customData;

        nlohmann::json ToJson(const std::string& assignedModalId) const;
    };

    struct ModalResponse {
        std::string modalId;
        std::string buttonId;
        std::int32_t buttonIndex{ -1 };
        bool aborted{ false };
        nlohmann::json values;
        nlohmann::json rawPayload;

        bool GetBool(std::string_view controlId, bool defaultVal = false) const;
        std::string GetString(std::string_view controlId, std::string defaultVal = "") const;
        std::int32_t GetInt(std::string_view controlId, std::int32_t defaultVal = 0) const;

        std::vector<std::string> GetCheckedItemIds(std::string_view checklistId) const;
        std::vector<std::string> GetUncheckedItemIds(std::string_view checklistId) const;
        bool IsItemChecked(std::string_view checklistId, std::string_view itemId, bool defaultVal = false) const;
        std::string GetItemSelectValue(std::string_view checklistId,
                                       std::string_view itemId,
                                       std::string defaultVal = "") const;
    };

    using ModalCallback = std::function<void(const ModalResponse&)>;

}  // namespace OStimNet::UI
