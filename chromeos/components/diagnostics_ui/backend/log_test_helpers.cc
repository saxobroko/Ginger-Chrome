// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/log_test_helpers.h"

#include <string>
#include <vector>

#include "base/strings/string_split.h"

namespace chromeos {
namespace diagnostics {

const char kSeparator[] = "-";
const char kNewline[] = "\n";

std::vector<std::string> GetLogLines(const std::string& log) {
  return base::SplitString(log, kNewline,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string> GetLogLineContents(const std::string& log_line) {
  const std::vector<std::string> result = base::SplitString(
      log_line, kSeparator, base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return result;
}

}  // namespace diagnostics
}  // namespace chromeos
