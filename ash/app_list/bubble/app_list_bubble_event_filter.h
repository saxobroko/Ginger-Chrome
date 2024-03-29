// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_EVENT_FILTER_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

// Observes mouse and touch events. Invokes a callback when a press event
// happens outside the bubble widget's bounds and also outside the button that
// spawned the bubble. Tests the button bounds because otherwise a click on the
// button will result in the bubble being closed then immediately reopened.
// Similar to TrayEventFilter, but only deals with a single widget, and is not
// coupled to system tray details.
class ASH_EXPORT AppListBubbleEventFilter : public ui::EventHandler {
 public:
  // See class comment. Runs `on_click_outside` when a click or tap occurs
  // outside the bounds of `widget` and `button`.
  AppListBubbleEventFilter(views::Widget* widget,
                           views::View* button,
                           base::RepeatingClosure on_click_outside);
  AppListBubbleEventFilter(const AppListBubbleEventFilter&) = delete;
  AppListBubbleEventFilter& operator=(const AppListBubbleEventFilter&) = delete;
  ~AppListBubbleEventFilter() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  views::Widget* const widget_;
  views::View* const button_;
  base::RepeatingClosure on_click_outside_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_EVENT_FILTER_H_
