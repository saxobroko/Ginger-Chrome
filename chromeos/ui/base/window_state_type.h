// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_STATE_TYPE_H_
#define CHROMEOS_UI_BASE_WINDOW_STATE_TYPE_H_

#include <cstdint>
#include <ostream>

#include "base/component_export.h"
#include "ui/base/ui_base_types.h"

namespace chromeos {

// A superset of ui::WindowShowState. Ash has more states than the general
// ui::WindowShowState enum. These need to be communicated back to Chrome.
// The separate enum is defined here because we don't want to leak these type to
// ui/base until they're stable and we know for sure that they'll persist over
// time.
enum class WindowStateType {
  // States which correspond to ui.mojom.ShowState.
  kDefault,
  kNormal,
  kMinimized,
  kMaximized,
  kInactive,
  kFullscreen,

  // Additional ash states.
  kLeftSnapped,
  kRightSnapped,

  // A window is in this state when it is automatically placed and
  // sized by the window manager. (it's newly opened, or pushed to the side
  // due to new window, for example).
  kAutoPositioned,

  // A window is pinned on top of other windows with fullscreenized.
  // Corresponding shelf should be hidden, also most of windows other than the
  // pinned one should be hidden.
  kPinned,
  kTrustedPinned,

  // A window in Picture-in-Picture mode (PIP).
  kPip,
};

COMPONENT_EXPORT(CHROMEOS_UI_BASE)
std::ostream& operator<<(std::ostream& stream, WindowStateType state);

// Utility functions to convert WindowStateType <-> ui::WindowShowState.
// Note: LEFT/RIGHT MAXIMIZED, AUTO_POSITIONED types will be lost when
// converting to ui::WindowShowState.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
WindowStateType ToWindowStateType(ui::WindowShowState state);
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
ui::WindowShowState ToWindowShowState(WindowStateType type);

// Returns true if |type| is FULLSCREEN, PINNED, or TRUSTED_PINNED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsFullscreenOrPinnedWindowStateType(WindowStateType type);

// Returns true if |type| is MAXIMIZED, FULLSCREEN, PINNED, or TRUSTED_PINNED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsMaximizedOrFullscreenOrPinnedWindowStateType(WindowStateType type);

// Returns true if |type| is MINIMIZED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsMinimizedWindowStateType(WindowStateType type);

// Returns true if |type| is either NORMAL or DEFAULT.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsNormalWindowStateType(WindowStateType type);

COMPONENT_EXPORT(CHROMEOS_UI_BASE) bool IsValidWindowStateType(int64_t value);

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_STATE_TYPE_H_
