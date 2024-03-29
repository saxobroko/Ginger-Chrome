// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.NotificationManager;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.provider.Settings;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsService;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManagerImpl;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;

/**
 * Tests for  {@link PriceDropNotificationManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_price_notification/true"})
@Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
public class PriceDropNotificationManagerTest {
    // clang-format on
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";
    private static final String EXTRA_APP_PACKAGE = "app_package";
    private static final String EXTRA_APP_UID = "app_uid";
    private static final String ACTION_ID_VISIT_SITE = "visit_site";
    private static final String ACTION_ID_TURN_OFF_ALERT = "turn_off_alert";
    private static final String TEST_URL = "www.test.com";

    private MockNotificationManagerProxy mMockNotificationManager;
    private PriceDropNotificationManager mPriceDropNotificationManager;

    @Rule
    public IntentsTestRule<ChromeActivity> mIntentTestRule =
            new IntentsTestRule<>(ChromeActivity.class, false, false);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private CommerceSubscriptionsService mMockSubscriptionsService;

    @Mock
    private SubscriptionsManagerImpl mMockSubscriptionsManager;

    @Before
    public void setUp() {
        mMockNotificationManager = new MockNotificationManagerProxy();
        PriceDropNotificationManager.setNotificationManagerForTesting(mMockNotificationManager);
        mPriceDropNotificationManager = new PriceDropNotificationManager();
    }

    @After
    public void tearDown() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mPriceDropNotificationManager.deleteChannelForTesting();
        }
        PriceDropNotificationManager.setNotificationManagerForTesting(null);
    }

    @Test
    @MediumTest
    public void testCanPostNotification_FeatureDisabled() {
        mMockNotificationManager.setNotificationsEnabled(true);
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(false);
        assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
        assertFalse(mPriceDropNotificationManager.canPostNotification());
    }

    @Test
    @MediumTest
    public void testCanPostNotification_NotificationDisabled() {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        mMockNotificationManager.setNotificationsEnabled(false);
        assertFalse(mPriceDropNotificationManager.areAppNotificationsEnabled());
        assertFalse(mPriceDropNotificationManager.canPostNotification());
    }

    @Test
    @MediumTest
    public void testCanPostNotificaton() {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        assertTrue(PriceTrackingUtilities.isPriceTrackingEligible());
        mMockNotificationManager.setNotificationsEnabled(true);
        assertTrue(mPriceDropNotificationManager.areAppNotificationsEnabled());

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertTrue(mPriceDropNotificationManager.canPostNotification());
        } else {
            assertNull(mPriceDropNotificationManager.getNotificationChannel());
            assertFalse(mPriceDropNotificationManager.canPostNotification());

            mPriceDropNotificationManager.createNotificationChannel();
            assertNotNull(mPriceDropNotificationManager.getNotificationChannel());
            assertEquals(NotificationManager.IMPORTANCE_LOW,
                    mPriceDropNotificationManager.getNotificationChannel().getImportance());

            assertTrue(mPriceDropNotificationManager.canPostNotification());
        }
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationDisabled() {
        mMockNotificationManager.setNotificationsEnabled(false);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertEquals(ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(EXTRA_APP_PACKAGE));
            assertEquals(ContextUtils.getApplicationContext().getApplicationInfo().uid,
                    intent.getIntExtra(EXTRA_APP_UID, 0));
        } else {
            assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
        }
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationEnabled() {
        mMockNotificationManager.setNotificationsEnabled(true);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertEquals(ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(EXTRA_APP_PACKAGE));
            assertEquals(ContextUtils.getApplicationContext().getApplicationInfo().uid,
                    intent.getIntExtra(EXTRA_APP_UID, 0));
        } else {
            assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
            assertEquals(ChromeChannelDefinitions.ChannelId.PRICE_DROP,
                    intent.getStringExtra(Settings.EXTRA_CHANNEL_ID));
        }
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }

    @Test
    @MediumTest
    public void testGetNotificationClickIntent() {
        Intent intent = mPriceDropNotificationManager.getNotificationClickIntent(TEST_URL);
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(Uri.parse(TEST_URL), intent.getData());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT,
                intent.getFlags());
        assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                intent.getStringExtra(Browser.EXTRA_APPLICATION_ID));
        assertEquals(true,
                intent.getBooleanExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
    }

    @Test
    @MediumTest
    public void testOnNotificationClicked() {
        Intents.init();
        mPriceDropNotificationManager.onNotificationClicked(TEST_URL);
        intended(allOf(hasAction(Intent.ACTION_VIEW), hasData(TEST_URL),
                hasComponent(ChromeLauncherActivity.class.getName())));
        Intents.release();
    }

    @Test
    @MediumTest
    public void testOnNotificationActionClicked_VisitSite() {
        Intents.init();
        mPriceDropNotificationManager.onNotificationActionClicked(
                ACTION_ID_VISIT_SITE, TEST_URL, null);
        intended(allOf(hasAction(Intent.ACTION_VIEW), hasData(TEST_URL),
                hasComponent(ChromeLauncherActivity.class.getName())));
        Intents.release();
    }

    @Test
    @MediumTest
    public void testOnNotificationActionClicked_TurnOffAlert() {
        doReturn(mMockSubscriptionsManager)
                .when(mMockSubscriptionsService)
                .getSubscriptionsManager();
        CommerceSubscriptionsServiceFactory.setSubscriptionsServiceForTesting(
                mMockSubscriptionsService);

        String offerId = "offer_id";
        CommerceSubscription commerceSubscription =
                new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK, offerId,
                        SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);

        mPriceDropNotificationManager.onNotificationActionClicked(
                ACTION_ID_TURN_OFF_ALERT, TEST_URL, null);
        verify(mMockSubscriptionsManager, times(0))
                .unsubscribe(eq(commerceSubscription), any(Callback.class));

        mPriceDropNotificationManager.onNotificationActionClicked(
                ACTION_ID_TURN_OFF_ALERT, TEST_URL, offerId);
        verify(mMockSubscriptionsManager, times(1))
                .unsubscribe(eq(commerceSubscription), any(Callback.class));
    }
}
