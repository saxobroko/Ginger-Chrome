// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link SyncConsentActivity} in modularized code. */
public interface SyncConsentActivityLauncher {
    @IntDef({SigninAccessPoint.SETTINGS, SigninAccessPoint.BOOKMARK_MANAGER,
            SigninAccessPoint.RECENT_TABS, SigninAccessPoint.SIGNIN_PROMO,
            SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, SigninAccessPoint.AUTOFILL_DROPDOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}

    /**
     * Launches the SigninActivity with default sign-in flow from personalized sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    void launchActivityForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName);

    /**
     * Launches the SigninActivity with "Choose account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    void launchActivityForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName);

    /**
     * Launches the SigninActivity with "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    void launchActivityForPromoAddAccountFlow(Context context, @SigninAccessPoint int accessPoint);

    /**
     * Launches the {@link SigninActivity} if signin is allowed.
     * @param context A {@link Context} object.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @return a boolean indicating if the {@link SigninActivity} is launched.
     */
    boolean launchActivityIfAllowed(Context context, @SigninAccessPoint int accessPoint);
}
