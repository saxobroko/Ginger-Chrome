// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/actionable_view.h"

#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/painter.h"

namespace ash {

// static
const char ActionableView::kViewClassName[] = "tray/ActionableView";

ActionableView::ActionableView(TrayPopupInkDropStyle ink_drop_style)
    : views::Button(base::BindRepeating(&ActionableView::ButtonPressed,
                                        base::Unretained(this))),
      ink_drop_style_(ink_drop_style) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetHasInkDropActionOnClick(false);
  SetNotifyEnterExitOnChild(true);
  // TODO(pbos): Replace the use of FocusPainter with the FocusRing (using the
  // below HighlightPathGenerator).
  SetInstallFocusRingOnFocus(false);
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  TrayPopupUtils::InstallHighlightPathGenerator(this, ink_drop_style_);
  ink_drop()->SetCreateInkDropCallback(base::BindRepeating(
      [](Button* host) { return TrayPopupUtils::CreateInkDrop(host); }, this));
  ink_drop()->SetCreateHighlightCallback(base::BindRepeating(
      &TrayPopupUtils::CreateInkDropHighlight, base::Unretained(this)));
  ink_drop()->SetCreateRippleCallback(base::BindRepeating(
      [](ActionableView* host) {
        return TrayPopupUtils::CreateInkDropRipple(host->ink_drop_style_, host);
      },
      this));
}

ActionableView::~ActionableView() {
  if (destroyed_)
    *destroyed_ = true;
}

void ActionableView::HandlePerformActionResult(bool action_performed,
                                               const ui::Event& event) {
  ink_drop()->AnimateToState(action_performed
                                 ? views::InkDropState::ACTION_TRIGGERED
                                 : views::InkDropState::HIDDEN,
                             ui::LocatedEvent::FromIfValid(&event));
}

const char* ActionableView::GetClassName() const {
  return kViewClassName;
}

bool ActionableView::OnKeyPressed(const ui::KeyEvent& event) {
  if (GetState() != STATE_DISABLED && event.key_code() == ui::VKEY_SPACE) {
    NotifyClick(event);
    return true;
  }
  return Button::OnKeyPressed(event);
}

void ActionableView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetName(GetAccessibleName());
}


void ActionableView::ButtonPressed(const ui::Event& event) {
  bool destroyed = false;
  destroyed_ = &destroyed;
  const bool action_performed = PerformAction(event);
  if (destroyed)
    return;
  destroyed_ = nullptr;

  HandlePerformActionResult(action_performed, event);
}

}  // namespace ash
