// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/safety_tip_message_delegate.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

SafetyTipMessageDelegate::SafetyTipMessageDelegate() = default;

SafetyTipMessageDelegate::~SafetyTipMessageDelegate() {
  DismissInternal();
}

void SafetyTipMessageDelegate::DisplaySafetyTipPrompt(
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    content::WebContents* web_contents,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  if (message_)
    return;
  web_contents_ = web_contents;
  safety_tip_status_ = safety_tip_status;
  suggested_url_ = suggested_url;
  close_callback_ = std::move(close_callback);
  message_ = std::make_unique<messages::MessageWrapper>(
      base::BindOnce(&SafetyTipMessageDelegate::HandleLeaveSiteClick,
                     base::Unretained(this)),
      base::BindOnce(&SafetyTipMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));
  message_->SetTitle(GetSafetyTipTitle(safety_tip_status, suggested_url));

  message_->SetDescription(
      GetSafetyTipDescription(safety_tip_status, suggested_url));

  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(GetSafetyTipLeaveButtonId(safety_tip_status)));
  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_INFOBAR_SAFETYTIP_SHIELD));
  message_->DisableIconTint();
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_SETTINGS));
  message_->SetSecondaryButtonMenuText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_MORE_INFO_LINK));

  message_->SetSecondaryActionCallback(base::BindOnce(
      &SafetyTipMessageDelegate::HandleLearnMoreClick, base::Unretained(this)));

  // 60s
  message_->SetDuration(60000);

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION);
}

void SafetyTipMessageDelegate::HandleLeaveSiteClick() {
  action_taken_ = SafetyTipInteraction::kLeaveSite;
  auto url = safety_tip_status_ == security_state::SafetyTipStatus::kLookalike
                 ? suggested_url_
                 : GURL();
  LeaveSiteFromSafetyTip(web_contents_, url);
}

void SafetyTipMessageDelegate::HandleLearnMoreClick() {
  OpenHelpCenterFromSafetyTip(web_contents_);
}

void SafetyTipMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  if (dismiss_reason == messages::DismissReason::GESTURE) {
    action_taken_ = SafetyTipInteraction::kDismissWithClose;
  } else if (dismiss_reason == messages::DismissReason::SCOPE_DESTROYED) {
    action_taken_ = SafetyTipInteraction::kCloseTab;
  }
  std::move(close_callback_).Run(action_taken_);
  action_taken_ = SafetyTipInteraction::kNoAction;
  message_.reset();
  web_contents_ = nullptr;
}

void SafetyTipMessageDelegate::DismissInternal() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, messages::DismissReason::UNKNOWN);
  }
}
