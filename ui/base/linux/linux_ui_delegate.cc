// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/linux_ui_delegate.h"

#include "base/callback.h"
#include "base/notreached.h"

namespace ui {

// static
LinuxUiDelegate* LinuxUiDelegate::instance_ = nullptr;

// static
LinuxUiDelegate* LinuxUiDelegate::GetInstance() {
  return instance_;
}

LinuxUiDelegate::LinuxUiDelegate() {
  DCHECK(!instance_);
  instance_ = this;
}

LinuxUiDelegate::~LinuxUiDelegate() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

bool LinuxUiDelegate::SetWidgetTransientFor(
    uint32_t parent_widget,
    base::OnceCallback<void(const std::string&)> callback) {
  // This function should not be called when using a platform that doesn't
  // implement it.
  NOTREACHED();
  return false;
}

int LinuxUiDelegate::GetKeyState() {
  // This function should not be called when using a platform that doesn't
  // implement it.
  NOTREACHED();
  return 0;
}

}  // namespace ui
