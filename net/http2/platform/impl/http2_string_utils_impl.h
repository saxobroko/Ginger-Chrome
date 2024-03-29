// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_STRING_UTILS_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_STRING_UTILS_IMPL_H_

#include <sstream>
#include <utility>

#include "base/strings/abseil_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "net/base/hex_utils.h"
#include "net/third_party/quiche/src/common/quiche_text_utils.h"

namespace http2 {

inline std::string Http2HexDumpImpl(absl::string_view data) {
  return quiche::QuicheTextUtils::HexDump(data);
}

}  // namespace http2

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_STRING_UTILS_IMPL_H_
