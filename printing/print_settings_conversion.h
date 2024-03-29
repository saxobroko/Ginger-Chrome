// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_CONVERSION_H_
#define PRINTING_PRINT_SETTINGS_CONVERSION_H_

#include <memory>

#include "base/component_export.h"
#include "printing/page_range.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace printing {

class PrintSettings;

COMPONENT_EXPORT(PRINTING)
PageRanges GetPageRangesFromJobSettings(const base::Value& job_settings);

// Returns nullptr on failure.
COMPONENT_EXPORT(PRINTING)
std::unique_ptr<PrintSettings> PrintSettingsFromJobSettings(
    const base::Value& job_settings);

// Use for debug only, because output is not completely consistent with format
// of `PrintSettingsFromJobSettings` input.
void PrintSettingsToJobSettingsDebug(const PrintSettings& settings,
                                     base::DictionaryValue* job_settings);

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_CONVERSION_H_
