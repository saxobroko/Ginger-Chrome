// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/tablet_state.h"

#include "base/check_op.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/screen.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/base/pointer/touch_ui_controller.h"
#endif

namespace chromeos {

namespace {
TabletState* g_instance = nullptr;
}

TabletState* TabletState::Get() {
  return g_instance;
}

TabletState::TabletState() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
  display::Screen::GetScreen()->AddObserver(this);
}

TabletState::~TabletState() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
  display::Screen::GetScreen()->RemoveObserver(this);
}

bool TabletState::InTabletMode() const {
  return state_ == display::TabletState::kInTabletMode ||
         state_ == display::TabletState::kEnteringTabletMode;
}

void TabletState::OnDisplayTabletStateChanged(display::TabletState state) {
  state_ = state;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TouchUIController is used by Chrome and other apps to determine whether
  // the device is in either a primarily touch-input or primarily keyboard
  // input mode, and to show different UI depending on which mode it's in.
  //
  // On ChromeOS this was previously hooked up through Ash code. On Lacros,
  // however, TabletState is one of the few classes which receives the relevant
  // events *and* can communicate safely with TouchUiController. The ozone/
  // wayland code can't see ui/base, and TouchController can't listen for
  // events on display::Screen because of order of instatiation (there is no
  // Screen object when TouchUiController is created).
  //
  // TODO(crbug.com/1170013): consolidate all of the tablet/touch state logic
  // into a single place on all platforms (likely display::Screen).
  ui::TouchUiController::Get()->OnTabletModeToggled(InTabletMode());
#endif
}

}  // namespace chromeos
