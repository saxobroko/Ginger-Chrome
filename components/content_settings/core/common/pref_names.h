// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_PREF_NAMES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_PREF_NAMES_H_

#include "build/build_config.h"

namespace prefs {

// NOTE: This file does not contain all content settings related pref names as
// some of these are generated by WebsiteSettingsInfo from content settings
// names.

extern const char kCookieControlsMode[];

extern const char kContentSettingsVersion[];
extern const char kContentSettingsWindowLastTabIndex[];

extern const char kManagedDefaultAdsSetting[];
extern const char kManagedDefaultCookiesSetting[];
extern const char kManagedDefaultImagesSetting[];
extern const char kManagedDefaultInsecureContentSetting[];
extern const char kManagedDefaultJavaScriptSetting[];
extern const char kManagedDefaultPopupsSetting[];
extern const char kManagedDefaultGeolocationSetting[];
extern const char kManagedDefaultNotificationsSetting[];
extern const char kManagedDefaultMediaStreamSetting[];
extern const char kManagedDefaultSensorsSetting[];
extern const char kManagedDefaultWebBluetoothGuardSetting[];
extern const char kManagedDefaultWebUsbGuardSetting[];
extern const char kManagedDefaultFileHandlingGuardSetting[];
extern const char kManagedDefaultFileSystemReadGuardSetting[];
extern const char kManagedDefaultFileSystemWriteGuardSetting[];
extern const char kManagedDefaultLegacyCookieAccessSetting[];
extern const char kManagedDefaultSerialGuardSetting[];
extern const char kManagedDefaultInsecurePrivateNetworkSetting[];

extern const char kManagedCookiesAllowedForUrls[];
extern const char kManagedCookiesBlockedForUrls[];
extern const char kManagedCookiesSessionOnlyForUrls[];
extern const char kManagedImagesAllowedForUrls[];
extern const char kManagedImagesBlockedForUrls[];
extern const char kManagedInsecureContentAllowedForUrls[];
extern const char kManagedInsecureContentBlockedForUrls[];
extern const char kManagedJavaScriptAllowedForUrls[];
extern const char kManagedJavaScriptBlockedForUrls[];
extern const char kManagedPopupsAllowedForUrls[];
extern const char kManagedPopupsBlockedForUrls[];
extern const char kManagedNotificationsAllowedForUrls[];
extern const char kManagedNotificationsBlockedForUrls[];
extern const char kManagedSensorsAllowedForUrls[];
extern const char kManagedSensorsBlockedForUrls[];
extern const char kManagedAutoSelectCertificateForUrls[];
extern const char kManagedWebUsbAllowDevicesForUrls[];
extern const char kManagedWebUsbAskForUrls[];
extern const char kManagedWebUsbBlockedForUrls[];
extern const char kManagedFileHandlingAllowedForUrls[];
extern const char kManagedFileHandlingBlockedForUrls[];
extern const char kManagedFileSystemReadAskForUrls[];
extern const char kManagedFileSystemReadBlockedForUrls[];
extern const char kManagedFileSystemWriteAskForUrls[];
extern const char kManagedFileSystemWriteBlockedForUrls[];
extern const char kManagedLegacyCookieAccessAllowedForDomains[];
extern const char kManagedSerialAskForUrls[];
extern const char kManagedSerialBlockedForUrls[];
extern const char kManagedInsecurePrivateNetworkAllowedForUrls[];

extern const char kEnableQuietNotificationPermissionUi[];
extern const char kQuietNotificationPermissionUiEnablingMethod[];
extern const char kQuietNotificationPermissionUiDisabledTime[];

#if defined(OS_ANDROID)
extern const char kNotificationsVibrateEnabled[];
#endif

}  // namespace prefs

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_PREF_NAMES_H_
