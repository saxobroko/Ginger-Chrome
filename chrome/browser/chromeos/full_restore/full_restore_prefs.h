// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace full_restore {

// Enum that specifies restore options on startup. The values must not be
// changed as they are persisted on disk.
//
// This is used to record histograms, so do not remove or reorder existing
// entries.
enum class RestoreOption {
  kAlways = 1,
  kAskEveryTime = 2,
  kDoNotRestore = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kDoNotRestore,
};

extern const char kRestoreAppsAndPagesPrefName[];
extern const char kRestoreSelectedCountPrefName[];

// Registers the restore pref |kRestoreAppsAndPagesPrefName| and
// |kRestoreSelectedCountPrefName|.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the pref has |kRestoreAppsAndPagesPrefName|. Otherwise,
// return false.
bool HasRestorePref(PrefService* prefs);

// Returns true if the restore pref doesn't exist or the pref is 'Always' or
// 'Ask every time'. Otherwise, return false for 'Do not restore'.
bool CanPerformRestore(PrefService* prefs);

// Sets the default restore pref |kRestoreAppsAndPagesPrefName| based on the
// current browser restore settings. If it is the first time to run Chrome OS,
// or the browser restore settings doesn't exist, set the restore pref setting
// as |kAskEveryTime|.
//
// This function should be called only when |kRestoreAppsAndPagesPrefName|
// doesn't exist.
void SetDefaultRestorePrefIfNecessary(PrefService* prefs);

// Updates the restore pref |kRestoreAppsAndPagesPrefName| when the browser
// restore settings is synced.
void UpdateRestorePrefIfNecessary(PrefService* prefs);

// Returns the value of the pref |kRestoreSelectedCountPrefName|.
int GetRestoreSelectedCountPref(PrefService* prefs);

// Sets the pref |kRestoreSelectedCountPrefName| as |count|.
void SetRestoreSelectedCountPref(PrefService* prefs, int count);

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_PREFS_H_
