#include "PCH.h"

#include "ui/ModalTypes.h"

#include <charconv>
#include <utility>

namespace OStimNet::UI {

    namespace {

        std::string SafeGetJsonString(const nlohmann::json& j, std::string_view key, std::string defaultVal = "") {
            if (!j.is_object()) return defaultVal;
            auto it = j.find(key);
            if (it == j.end() || it->is_null()) return defaultVal;
            if (it->is_string()) return it->get<std::string>();
            if (it->is_number_integer()) return std::to_string(it->get<std::int64_t>());
            if (it->is_number_unsigned()) return std::to_string(it->get<std::uint64_t>());
            if (it->is_boolean()) return it->get<bool>() ? "true" : "false";
            return defaultVal;
        }

        bool SafeGetJsonBool(const nlohmann::json& j, std::string_view key, bool defaultVal = false) {
            if (!j.is_object()) return defaultVal;
            auto it = j.find(key);
            if (it == j.end() || it->is_null()) return defaultVal;
            if (it->is_boolean()) return it->get<bool>();
            if (it->is_number()) return it->get<double>() != 0.0;
            if (it->is_string()) {
                const auto& s = it->get_ref<const std::string&>();
                if (s == "true" || s == "1") return true;
                if (s == "false" || s == "0") return false;
            }
            return defaultVal;
        }

        std::int32_t SafeGetJsonInt(const nlohmann::json& j, std::string_view key, std::int32_t defaultVal = 0) {
            if (!j.is_object()) return defaultVal;
            auto it = j.find(key);
            if (it == j.end() || it->is_null()) return defaultVal;
            if (it->is_number_integer() || it->is_number_unsigned()) {
                return static_cast<std::int32_t>(it->get<std::int64_t>());
            }
            if (it->is_number_float()) return static_cast<std::int32_t>(it->get<double>());
            if (it->is_boolean()) return it->get<bool>() ? 1 : 0;
            if (it->is_string()) {
                const auto& s = it->get_ref<const std::string&>();
                std::int32_t parsed = defaultVal;
                const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), parsed);
                if (ec == std::errc{}) return parsed;
            }
            return defaultVal;
        }

    }  // namespace

    std::string_view ModalToneToString(ModalTone tone) {
        switch (tone) {
            case ModalTone::Warning: return "warning";
            case ModalTone::Danger:  return "danger";
            case ModalTone::Success: return "success";
            case ModalTone::Ai:      return "ai";
            case ModalTone::Default:
            default:                 return "default";
        }
    }

    std::string_view ModalSizeToString(ModalSize size) {
        switch (size) {
            case ModalSize::Sm: return "sm";
            case ModalSize::Lg: return "lg";
            case ModalSize::Xl: return "xl";
            case ModalSize::Md:
            default:            return "md";
        }
    }

    std::string_view ModalButtonVariantToString(ModalButtonVariant variant) {
        switch (variant) {
            case ModalButtonVariant::Primary:   return "default";
            case ModalButtonVariant::Outline:   return "outline";
            case ModalButtonVariant::Ghost:     return "ghost";
            case ModalButtonVariant::Danger:    return "danger";
            case ModalButtonVariant::Secondary:
            default:                            return "secondary";
        }
    }

    std::string_view ModalControlTypeToString(ModalControlType type) {
        switch (type) {
            case ModalControlType::Checkbox:  return "checkbox";
            case ModalControlType::Select:    return "select";
            case ModalControlType::TextInput: return "text";
            case ModalControlType::Checklist: return "checklist";
            case ModalControlType::InfoBox:   return "infobox";
            default:                          return "checkbox";
        }
    }

    nlohmann::json ModalSelectOption::ToJson() const {
        nlohmann::json j;
        j["value"] = value;
        j["label"] = label;
        if (disabled) j["disabled"] = true;
        if (!tooltip.empty()) j["tooltip"] = tooltip;
        return j;
    }

    nlohmann::json ModalChecklistItem::ToJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["label"] = label;
        if (!subtitle.empty()) j["subtitle"] = subtitle;
        if (!description.empty()) j["description"] = description;
        if (!badgeText.empty()) {
            j["badgeText"] = badgeText;
            j["badgeTone"] = badgeTone.empty() ? "default" : badgeTone;
        }
        j["checked"] = checked;
        if (checkboxDisabled) j["checkboxDisabled"] = true;

        if (!selectOptions.empty()) {
            if (!selectKey.empty()) j["selectKey"] = selectKey;
            if (!selectLabel.empty()) j["selectLabel"] = selectLabel;
            j["selectValue"] = selectValue;
            nlohmann::json opts = nlohmann::json::array();
            for (const auto& opt : selectOptions) {
                opts.push_back(opt.ToJson());
            }
            j["selectOptions"] = std::move(opts);
        }

        if (meta.is_object() && !meta.empty()) {
            j["meta"] = meta;
        }
        return j;
    }

    ModalControl ModalControl::MakeCheckbox(std::string id,
                                            std::string label,
                                            bool defaultChecked,
                                            std::string description,
                                            bool disabled) {
        ModalControl ctrl;
        ctrl.type = ModalControlType::Checkbox;
        ctrl.id = std::move(id);
        ctrl.label = std::move(label);
        ctrl.description = std::move(description);
        ctrl.defaultValue = defaultChecked;
        ctrl.disabled = disabled;
        return ctrl;
    }

    ModalControl ModalControl::MakeSelect(std::string id,
                                          std::string label,
                                          std::vector<ModalSelectOption> options,
                                          std::string defaultValue,
                                          std::string description,
                                          bool disabled) {
        ModalControl ctrl;
        ctrl.type = ModalControlType::Select;
        ctrl.id = std::move(id);
        ctrl.label = std::move(label);
        ctrl.description = std::move(description);
        ctrl.options = std::move(options);
        if (!defaultValue.empty()) {
            ctrl.defaultValue = std::move(defaultValue);
        } else if (!ctrl.options.empty()) {
            ctrl.defaultValue = ctrl.options.front().value;
        } else {
            ctrl.defaultValue = "";
        }
        ctrl.disabled = disabled;
        return ctrl;
    }

    ModalControl ModalControl::MakeTextInput(std::string id,
                                             std::string label,
                                             std::string defaultValue,
                                             std::string placeholder,
                                             std::string description,
                                             bool disabled) {
        ModalControl ctrl;
        ctrl.type = ModalControlType::TextInput;
        ctrl.id = std::move(id);
        ctrl.label = std::move(label);
        ctrl.defaultValue = std::move(defaultValue);
        ctrl.placeholder = std::move(placeholder);
        ctrl.description = std::move(description);
        ctrl.disabled = disabled;
        return ctrl;
    }

    ModalControl ModalControl::MakeChecklist(std::string id,
                                             std::string label,
                                             std::vector<ModalChecklistItem> items,
                                             bool showSelectAllToggle,
                                             std::string description,
                                             std::string emptyMessage) {
        ModalControl ctrl;
        ctrl.type = ModalControlType::Checklist;
        ctrl.id = std::move(id);
        ctrl.label = std::move(label);
        ctrl.items = std::move(items);
        ctrl.showSelectAllToggle = showSelectAllToggle;
        ctrl.description = std::move(description);
        ctrl.emptyMessage = std::move(emptyMessage);
        return ctrl;
    }

    ModalControl ModalControl::MakeInfoBox(std::string id,
                                           std::string label,
                                           std::string description,
                                           ModalTone tone) {
        ModalControl ctrl;
        ctrl.type = ModalControlType::InfoBox;
        ctrl.id = std::move(id);
        ctrl.label = std::move(label);
        ctrl.description = std::move(description);
        ctrl.tone = tone;
        return ctrl;
    }

    nlohmann::json ModalControl::ToJson() const {
        nlohmann::json j;
        j["type"] = ModalControlTypeToString(type);
        j["id"] = id;
        if (!label.empty()) j["label"] = label;
        if (!description.empty()) j["description"] = description;
        if (required) j["required"] = true;
        if (disabled) j["disabled"] = true;
        if (!defaultValue.is_null()) j["defaultValue"] = defaultValue;
        if (!placeholder.empty()) j["placeholder"] = placeholder;

        if (type == ModalControlType::Select) {
            nlohmann::json opts = nlohmann::json::array();
            for (const auto& opt : options) {
                opts.push_back(opt.ToJson());
            }
            j["options"] = std::move(opts);
        } else if (type == ModalControlType::Checklist) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& item : items) {
                arr.push_back(item.ToJson());
            }
            j["items"] = std::move(arr);
            j["showSelectAllToggle"] = showSelectAllToggle;
            if (!emptyMessage.empty()) j["emptyMessage"] = emptyMessage;
        } else if (type == ModalControlType::InfoBox) {
            j["tone"] = ModalToneToString(tone);
        }

        return j;
    }

    ModalButton ModalButton::Make(std::string id,
                                  std::string label,
                                  ModalButtonVariant variant,
                                  bool disabled,
                                  std::string tooltip) {
        ModalButton btn;
        btn.id = std::move(id);
        btn.label = std::move(label);
        btn.variant = variant;
        btn.disabled = disabled;
        btn.tooltip = std::move(tooltip);
        return btn;
    }

    nlohmann::json ModalButton::ToJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["label"] = label;
        j["variant"] = ModalButtonVariantToString(variant);
        if (disabled) j["disabled"] = true;
        if (!tooltip.empty()) j["tooltip"] = tooltip;
        if (!requireChecklistId.empty()) {
            j["requireChecklistId"] = requireChecklistId;
            j["minCheckedCount"] = minCheckedCount;
            if (maxCheckedCount > 0) {
                j["maxCheckedCount"] = maxCheckedCount;
            }
        }
        return j;
    }

    nlohmann::json ModalRequest::ToJson(const std::string& assignedModalId) const {
        nlohmann::json j;
        j["modalId"] = assignedModalId;
        j["modalType"] = modalType.empty() ? "generic" : modalType;
        j["title"] = title;
        if (!subtitle.empty()) j["subtitle"] = subtitle;
        if (!message.empty()) j["message"] = message;
        j["tone"] = ModalToneToString(tone);
        j["size"] = ModalSizeToString(size);

        if (!controls.empty()) {
            nlohmann::json ctrlArr = nlohmann::json::array();
            for (const auto& ctrl : controls) {
                ctrlArr.push_back(ctrl.ToJson());
            }
            j["controls"] = std::move(ctrlArr);
        }

        nlohmann::json btnArr = nlohmann::json::array();
        for (const auto& btn : buttons) {
            btnArr.push_back(btn.ToJson());
        }
        j["buttons"] = std::move(btnArr);

        if (customData.is_object() && !customData.empty()) {
            j["customData"] = customData;
        }

        return j;
    }

    bool ModalResponse::GetBool(std::string_view controlId, bool defaultVal) const {
        return SafeGetJsonBool(values, controlId, defaultVal);
    }

    std::string ModalResponse::GetString(std::string_view controlId, std::string defaultVal) const {
        return SafeGetJsonString(values, controlId, std::move(defaultVal));
    }

    std::int32_t ModalResponse::GetInt(std::string_view controlId, std::int32_t defaultVal) const {
        return SafeGetJsonInt(values, controlId, defaultVal);
    }

    std::vector<std::string> ModalResponse::GetCheckedItemIds(std::string_view checklistId) const {
        std::vector<std::string> out;
        if (!values.is_object()) return out;
        auto it = values.find(checklistId);
        if (it == values.end() || !it->is_object()) return out;

        auto checkedIt = it->find("checkedIds");
        if (checkedIt != it->end() && checkedIt->is_array()) {
            out.reserve(checkedIt->size());
            for (const auto& elem : *checkedIt) {
                if (elem.is_string()) {
                    out.push_back(elem.get<std::string>());
                }
            }
        }
        return out;
    }

    std::vector<std::string> ModalResponse::GetUncheckedItemIds(std::string_view checklistId) const {
        std::vector<std::string> out;
        if (!values.is_object()) return out;
        auto it = values.find(checklistId);
        if (it == values.end() || !it->is_object()) return out;

        auto uncheckedIt = it->find("uncheckedIds");
        if (uncheckedIt != it->end() && uncheckedIt->is_array()) {
            out.reserve(uncheckedIt->size());
            for (const auto& elem : *uncheckedIt) {
                if (elem.is_string()) {
                    out.push_back(elem.get<std::string>());
                }
            }
        }
        return out;
    }

    bool ModalResponse::IsItemChecked(std::string_view checklistId, std::string_view itemId, bool defaultVal) const {
        if (!values.is_object()) return defaultVal;
        auto it = values.find(checklistId);
        if (it == values.end() || !it->is_object()) return defaultVal;

        auto itemsIt = it->find("items");
        if (itemsIt == it->end() || !itemsIt->is_object()) return defaultVal;

        auto itemIt = itemsIt->find(itemId);
        if (itemIt == itemsIt->end() || !itemIt->is_object()) return defaultVal;

        return SafeGetJsonBool(*itemIt, "checked", defaultVal);
    }

    std::string ModalResponse::GetItemSelectValue(std::string_view checklistId,
                                                  std::string_view itemId,
                                                  std::string defaultVal) const {
        if (!values.is_object()) return defaultVal;
        auto it = values.find(checklistId);
        if (it == values.end() || !it->is_object()) return defaultVal;

        auto itemsIt = it->find("items");
        if (itemsIt == it->end() || !itemsIt->is_object()) return defaultVal;

        auto itemIt = itemsIt->find(itemId);
        if (itemIt == itemsIt->end() || !itemIt->is_object()) return defaultVal;

        return SafeGetJsonString(*itemIt, "selectValue", std::move(defaultVal));
    }

}  // namespace OStimNet::UI
