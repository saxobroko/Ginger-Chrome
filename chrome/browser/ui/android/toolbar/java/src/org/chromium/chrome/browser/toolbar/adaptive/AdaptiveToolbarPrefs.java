// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;

/**
 * A utility class for handling adaptive toolbar cuistomization user settings used by {@link
 * AdaptiveToolbarButtonController}.
 */
public class AdaptiveToolbarPrefs {
    /** Returns whether customization is enabled. */
    public static boolean isCustomizationEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED, false);
    }

    /**
     * Sets customization setting enabled or not.
     * @param enabled Whether the customization should be enabled.
     */
    public static void saveToolbarSettingsToggleState(boolean enabled) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_ENABLED, enabled);
    }

    /**
     * The current customization setting, reflecting either the user setting or the default if the
     * user has not explicitly set a preference.
     * @return The current customization setting. See {@link AdaptiveToolbarButtonVariant}.
     */
    public static @AdaptiveToolbarButtonVariant int getCustomizationSetting() {
        return SharedPreferencesManager.getInstance().readInt(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS, AdaptiveToolbarButtonVariant.AUTO);
    }

    /**
     * Set customization setting.
     * @param setting The {@link AdaptiveToolbarButtonVariant} for this Preference.
     */
    public static void saveToolbarButtonManualOverride(@AdaptiveToolbarButtonVariant int settings) {
        SharedPreferencesManager.getInstance().writeInt(
                ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS, settings);
    }
}
