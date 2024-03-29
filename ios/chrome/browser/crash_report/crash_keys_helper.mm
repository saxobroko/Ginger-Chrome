// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_keys_helper.h"

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/crash_report_user_application_state.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace crash_keys {

const char kBreadcrumbsProductDataKey[] = "breadcrumbs";

namespace {

const char kCrashedInBackground[] = "crashed_in_background";
const char kFreeDiskInKB[] = "free_disk_in_kb";
const char kFreeMemoryInKB[] = "free_memory_in_kb";
const char kMemoryWarningInProgress[] = "memory_warning_in_progress";
const char kMemoryWarningCount[] = "memory_warning_count";
const char kGridToVisibleTabAnimation[] = "grid_to_visible_tab_animation";
static crash_reporter::CrashKeyString<1028> kRemoveGridToVisibleTabAnimationKey(
    kGridToVisibleTabAnimation);

// Multiple state information are combined into one CrashReportMultiParameter
// to save limited and finite number of ReportParameters.
// These are the values grouped in the user_application_state parameter.
NSString* const kOrientationState = @"orient";
NSString* const kHorizontalSizeClass = @"sizeclass";
NSString* const kUserInterfaceStyle = @"user_interface_style";
NSString* const kSignedIn = @"signIn";
NSString* const kIsShowingPDF = @"pdf";
NSString* const kVideoPlaying = @"avplay";
NSString* const kIncognitoTabCount = @"OTRTabs";
NSString* const kRegularTabCount = @"regTabs";
NSString* const kDestroyingAndRebuildingIncognitoBrowserState =
    @"destroyingAndRebuildingOTR";

}  // namespace

void SetCurrentlyInBackground(bool background) {
  static crash_reporter::CrashKeyString<4> key(kCrashedInBackground);
  if (background) {
    key.Set("yes");
    [[MainThreadFreezeDetector sharedInstance] stop];
  } else {
    key.Clear();
    [[MainThreadFreezeDetector sharedInstance] start];
  }
}

void SetMemoryWarningCount(int count) {
  static crash_reporter::CrashKeyString<16> key(kMemoryWarningCount);
  if (count) {
    key.Set(base::NumberToString(count));
  } else {
    key.Clear();
  }
}

void SetMemoryWarningInProgress(bool value) {
  static crash_reporter::CrashKeyString<4> key(kMemoryWarningInProgress);
  if (value)
    key.Set("yes");
  else
    key.Clear();
}

void SetCurrentFreeMemoryInKB(int value) {
  static crash_reporter::CrashKeyString<16> key(kFreeMemoryInKB);
  key.Set(base::NumberToString(value));
}

void SetCurrentFreeDiskInKB(int value) {
  static crash_reporter::CrashKeyString<16> key(kFreeDiskInKB);
  key.Set(base::NumberToString(value));
}

void SetCurrentTabIsPDF(bool value) {
  if (value) {
    [[CrashReportUserApplicationState sharedInstance]
        incrementValue:kIsShowingPDF];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        decrementValue:kIsShowingPDF];
  }
}

void SetCurrentOrientation(int statusBarOrientation, int deviceOrientation) {
  DCHECK((statusBarOrientation < 10) && (deviceOrientation < 10));
  int deviceAndUIOrientation = 10 * statusBarOrientation + deviceOrientation;
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kOrientationState
      withValue:deviceAndUIOrientation];
}

void SetCurrentHorizontalSizeClass(int horizontalSizeClass) {
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kHorizontalSizeClass
      withValue:horizontalSizeClass];
}

void SetCurrentUserInterfaceStyle(int userInterfaceStyle) {
  [[CrashReportUserApplicationState sharedInstance]
       setValue:kUserInterfaceStyle
      withValue:userInterfaceStyle];
}

void SetCurrentlySignedIn(bool signedIn) {
  if (signedIn) {
    [[CrashReportUserApplicationState sharedInstance] setValue:kSignedIn
                                                     withValue:1];
  } else {
    [[CrashReportUserApplicationState sharedInstance] removeValue:kSignedIn];
  }
}

void SetRegularTabCount(int tabCount) {
  [[CrashReportUserApplicationState sharedInstance] setValue:kRegularTabCount
                                                   withValue:tabCount];
  [[PreviousSessionInfo sharedInstance] updateCurrentSessionTabCount:tabCount];
}

void SetIncognitoTabCount(int tabCount) {
  [[CrashReportUserApplicationState sharedInstance] setValue:kIncognitoTabCount
                                                   withValue:tabCount];
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionOTRTabCount:tabCount];
}

void SetDestroyingAndRebuildingIncognitoBrowserState(bool in_progress) {
  if (in_progress) {
    [[CrashReportUserApplicationState sharedInstance]
         setValue:kDestroyingAndRebuildingIncognitoBrowserState
        withValue:1];
  } else {
    [[CrashReportUserApplicationState sharedInstance]
        removeValue:kDestroyingAndRebuildingIncognitoBrowserState];
  }
}

void SetGridToVisibleTabAnimation(NSString* to_view_controller,
                                  NSString* presenting_view_controller,
                                  NSString* presented_view_controller,
                                  NSString* parent_view_controller) {
  NSString* formatted_value =
      [NSString stringWithFormat:
                    @"{toVC:%@, presentingVC:%@, presentedVC:%@, parentVC:%@}",
                    to_view_controller, presenting_view_controller,
                    presented_view_controller, parent_view_controller];
  kRemoveGridToVisibleTabAnimationKey.Set(
      base::SysNSStringToUTF8(formatted_value));
}

void RemoveGridToVisibleTabAnimation() {
  kRemoveGridToVisibleTabAnimationKey.Clear();
}

void SetBreadcrumbEvents(NSString* breadcrumbs) {
  static crash_reporter::CrashKeyString<2550> key(kBreadcrumbsProductDataKey);
  key.Set(base::SysNSStringToUTF8(breadcrumbs));
}

void MediaStreamPlaybackDidStart() {
  [[CrashReportUserApplicationState sharedInstance]
      incrementValue:kVideoPlaying];
}

void MediaStreamPlaybackDidStop() {
  [[CrashReportUserApplicationState sharedInstance]
      decrementValue:kVideoPlaying];
}

}  // namespace crash_keys
