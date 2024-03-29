// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {
namespace {

bool IsSimulatingKeyPresses(KeyboardValueFillStrategy fill_strategy) {
  return fill_strategy == SIMULATE_KEY_PRESSES ||
         fill_strategy == SIMULATE_KEY_PRESSES_SELECT_VALUE ||
         fill_strategy == SIMULATE_KEY_PRESSES_FOCUS;
}

}  // namespace

SetFormFieldValueAction::FieldInput::FieldInput(
    std::unique_ptr<std::vector<UChar32>> _keyboard_input)
    : keyboard_input(std::move(_keyboard_input)) {}

SetFormFieldValueAction::FieldInput::FieldInput(std::string _value)
    : value(_value) {}

SetFormFieldValueAction::FieldInput::FieldInput(
    PasswordManagerValue _password_manager_value)
    : password_manager_value(_password_manager_value) {}

SetFormFieldValueAction::FieldInput::FieldInput(FieldInput&& other) = default;

SetFormFieldValueAction::FieldInput::~FieldInput() {}

SetFormFieldValueAction::SetFormFieldValueAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_set_form_value());
  DCHECK_GT(proto_.set_form_value().value_size(), 0);
}

SetFormFieldValueAction::~SetFormFieldValueAction() {}

void SetFormFieldValueAction::InternalProcessAction(
    ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);
  selector_ = Selector(proto_.set_form_value().element());
  if (selector_.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  // Check proto fields.
  int keypress_index = 0;
  for (const auto& keypress : proto_.set_form_value().value()) {
    switch (keypress.keypress_case()) {
      case SetFormFieldValueProto_KeyPress::kKeycode:
        // DEPRECATED: the field `keycode' used to contain a single character to
        // input as text. Since there is no easy way to convert keycodes to
        // text, this field is now deprecated and only works for US-ASCII
        // characters. You should use the `keyboard_input' field instead.
        if (keypress.keycode() >= 128) {
          VLOG(1) << "SetFormFieldValueAction: field `keycode' is deprecated "
                  << "and only supports US-ASCII values (encountered value > "
                     "127). Use field `key' instead.";
          FailAction(ClientStatus(INVALID_ACTION), keypress_index);
          return;
        }
        field_inputs_.emplace_back(
            /* keyboard_input = */ std::make_unique<std::vector<UChar32>>(
                1, keypress.keycode()));
        break;
      case SetFormFieldValueProto_KeyPress::kKeyboardInput:
        if (keypress.keyboard_input().empty()) {
          VLOG(1) << "SetFormFieldValueAction: field 'keyboard_input' must be "
                     "non-empty if set.";
          FailAction(ClientStatus(INVALID_ACTION), keypress_index);
          return;
        }
        field_inputs_.emplace_back(
            /* keyboard_input = */ std::make_unique<std::vector<UChar32>>(
                UTF8ToUnicode(keypress.keyboard_input())));
        break;
      case SetFormFieldValueProto_KeyPress::kUseUsername: {
        PasswordManagerValue password_manager_value;
        password_manager_value.set_credential_type(
            PasswordManagerValue::USERNAME);
        field_inputs_.emplace_back(password_manager_value);
        break;
      }
      case SetFormFieldValueProto_KeyPress::kUsePassword: {
        PasswordManagerValue password_manager_value;
        password_manager_value.set_credential_type(
            PasswordManagerValue::PASSWORD);
        field_inputs_.emplace_back(password_manager_value);
        break;
      }
      case SetFormFieldValueProto_KeyPress::kText:
        // Currently no check required.
        field_inputs_.emplace_back(/* value = */ keypress.text());
        break;
      case SetFormFieldValueProto_KeyPress::kClientMemoryKey: {
        std::string value;
        ClientStatus client_memory_status = GetClientMemoryStringValue(
            keypress.client_memory_key(), delegate_->GetUserData(), &value);
        if (!client_memory_status.ok()) {
          VLOG(1) << "SetFormFieldValueAction: bad |client_memory_key|";
          FailAction(client_memory_status, keypress_index);
          return;
        }
        field_inputs_.emplace_back(/* value = */ value);
        break;
      }
      case SetFormFieldValueProto_KeyPress::kAutofillValue: {
        std::string value;
        ClientStatus autofill_status = GetFormattedAutofillValue(
            keypress.autofill_value(), delegate_->GetUserData(), &value);
        if (!autofill_status.ok()) {
          FailAction(autofill_status, keypress_index);
          return;
        }

        field_inputs_.emplace_back(value);
        break;
      }
      default:
        VLOG(1) << "Unrecognized field for SetFormFieldValueProto_KeyPress";
        FailAction(ClientStatus(INVALID_ACTION), keypress_index);
        return;
    }
    ++keypress_index;
  }

  delegate_->ShortWaitForElementWithSlowWarning(
      selector_,
      base::BindOnce(&SetFormFieldValueAction::OnWaitForElementTimed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&SetFormFieldValueAction::OnWaitForElement,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void SetFormFieldValueAction::OnWaitForElement(
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }
  delegate_->FindElement(selector_,
                         base::BindOnce(&SetFormFieldValueAction::OnFindElement,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void SetFormFieldValueAction::OnFindElement(
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  element_ = std::move(element_result);
  SetFieldValueSequentially(/* field_index = */ 0, OkClientStatus());
}

void SetFormFieldValueAction::SetFieldValueSequentially(
    int field_index,
    const ClientStatus& status) {
  // If something went wrong or we are out of values: finish.
  if (!status.ok() || field_index >= proto_.set_form_value().value_size()) {
    EndAction(status);
    return;
  }

  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  auto fill_strategy = proto_.set_form_value().fill_strategy();
  auto next_field_callback =
      base::BindOnce(&SetFormFieldValueAction::SetFieldValueSequentially,
                     weak_ptr_factory_.GetWeakPtr(), field_index + 1);
  const auto& field_input = field_inputs_[field_index];
  if (field_input.keyboard_input) {
    action_delegate_util::PerformSendKeyboardInput(
        delegate_, *field_input.keyboard_input, delay_in_millisecond,
        fill_strategy == SIMULATE_KEY_PRESSES_FOCUS, *element_,
        std::move(next_field_callback));
  } else if (field_input.password_manager_value.credential_type() !=
             PasswordManagerValue::NOT_SET) {
    GetPasswordManagerValue(
        field_input.password_manager_value, *element_, delegate_->GetUserData(),
        delegate_->GetWebsiteLoginManager(),
        base::BindOnce(&SetFormFieldValueAction::OnGetPasswordManagerValue,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(next_field_callback)));
  } else {
    SetFieldValueAndCheckFallback(field_input.value,
                                  std::move(next_field_callback));
  }
}

void SetFormFieldValueAction::OnGetPasswordManagerValue(
    base::OnceCallback<void(const ClientStatus&)> next_field_callback,
    const ClientStatus& status,
    const std::string& value) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  SetFieldValueAndCheckFallback(value, std::move(next_field_callback));
}

void SetFormFieldValueAction::SetFieldValueAndCheckFallback(
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> next_field_callback) {
  auto fill_strategy = proto_.set_form_value().fill_strategy();
  action_delegate_util::PerformSetFieldValue(
      delegate_, value, fill_strategy,
      proto_.set_form_value().delay_in_millisecond(), *element_,
      IsSimulatingKeyPresses(fill_strategy)
          ? std::move(next_field_callback)
          : base::BindOnce(
                &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
                weak_ptr_factory_.GetWeakPtr(), std::move(next_field_callback),
                /* requested_value = */ value));
}

void SetFormFieldValueAction::OnSetFieldValueAndCheckFallback(
    base::OnceCallback<void(const ClientStatus&)> next_field_callback,
    const std::string& requested_value,
    const ClientStatus& status) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  delegate_->GetWebController()->GetFieldValue(
      *element_,
      base::BindOnce(&SetFormFieldValueAction::OnGetFieldValue,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_field_callback), requested_value));
}

void SetFormFieldValueAction::OnGetFieldValue(
    base::OnceCallback<void(const ClientStatus&)> next_field_callback,
    const std::string& requested_value,
    const ClientStatus& element_status,
    const std::string& actual_value) {
  // Move to next value if |GetFieldValue| failed.
  if (!element_status.ok()) {
    std::move(next_field_callback).Run(OkClientStatus());
    return;
  }

  // If value is still empty while it is not supposed to be, trigger keyboard
  // simulation fallback.
  if (!requested_value.empty() && actual_value.empty()) {
    // Report a key press simulation fallback has happened.
    processed_action_proto_->mutable_set_form_field_value_result()
        ->set_fallback_to_simulate_key_presses(true);

    // Run |SetFieldValue| with keyboard simulation on and move on to next value
    // afterwards.
    action_delegate_util::PerformSetFieldValue(
        delegate_, requested_value, SIMULATE_KEY_PRESSES,
        proto_.set_form_value().delay_in_millisecond(), *element_,
        std::move(next_field_callback));
    return;
  }

  // Move to next value in all other cases.
  std::move(next_field_callback).Run(OkClientStatus());
}

void SetFormFieldValueAction::FailAction(const ClientStatus& status,
                                         int keypress_index) {
  processed_action_proto_->mutable_status_details()
      ->mutable_form_field_error_info()
      ->set_invalid_keypress_index(keypress_index);
  EndAction(status);
}

void SetFormFieldValueAction::EndAction(const ClientStatus& status) {
  // Clear immediately, to prevent sensitive information from staying in memory.
  field_inputs_.clear();
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
