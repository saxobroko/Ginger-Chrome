// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.view.View;
import android.webkit.ValueCallback;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.test_interfaces.ITestWebLayer;

import java.util.ArrayList;
import java.util.Set;

/**
 * TestWebLayer is responsible for passing messages over a test only AIDL to the
 * WebLayer implementation.
 */
public final class TestWebLayer {
    @Nullable
    private ITestWebLayer mITestWebLayer;

    @Nullable
    private static TestWebLayer sInstance;

    public static TestWebLayer getTestWebLayer(@NonNull Context appContext) {
        if (sInstance == null) sInstance = new TestWebLayer(appContext);
        return sInstance;
    }

    private TestWebLayer(@NonNull Context appContext) {
        try {
            Class TestWebLayerClass = WebLayer.loadRemoteClass(
                    appContext, "org.chromium.weblayer_private.test.TestWebLayerImpl");
            mITestWebLayer = ITestWebLayer.Stub.asInterface(
                    (IBinder) TestWebLayerClass.getMethod("create").invoke(null));
        } catch (PackageManager.NameNotFoundException | ReflectiveOperationException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    public boolean isNetworkChangeAutoDetectOn() throws RemoteException {
        return mITestWebLayer.isNetworkChangeAutoDetectOn();
    }

    /**
     * Gets the processed context which is returned by ContextUtils.getApplicationContext() on the
     * remote side.
     */
    public static Context getRemoteContext(@NonNull Context appContext) {
        return WebLayer.getApplicationContextForTesting(appContext);
    }

    /** Gets the context for the WebLayer implementation package. */
    public static Context getWebLayerContext(@NonNull Context appContext) {
        try {
            return WebLayer.getOrCreateRemoteContext(appContext);
        } catch (PackageManager.NameNotFoundException | ReflectiveOperationException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    public void setMockLocationProvider(boolean enabled) throws RemoteException {
        mITestWebLayer.setMockLocationProvider(enabled);
    }

    public boolean isMockLocationProviderRunning() throws RemoteException {
        return mITestWebLayer.isMockLocationProviderRunning();
    }

    public boolean isPermissionDialogShown() throws RemoteException {
        return mITestWebLayer.isPermissionDialogShown();
    }

    public void clickPermissionDialogButton(boolean allow) throws RemoteException {
        mITestWebLayer.clickPermissionDialogButton(allow);
    }

    public void setSystemLocationSettingEnabled(boolean enabled) throws RemoteException {
        mITestWebLayer.setSystemLocationSettingEnabled(enabled);
    }

    // Runs |runnable| when cc::RenderFrameMetadata's |top_controls_height| and
    // |bottom_controls_height| matches the supplied values. |runnable| may be run synchronously.
    public void waitForBrowserControlsMetadataState(Tab tab, int top, int bottom, Runnable runnable)
            throws RemoteException {
        mITestWebLayer.waitForBrowserControlsMetadataState(
                tab.getITab(), top, bottom, ObjectWrapper.wrap(runnable));
    }

    public void setAccessibilityEnabled(boolean enabled) throws RemoteException {
        mITestWebLayer.setAccessibilityEnabled(enabled);
    }

    public boolean canBrowserControlsScroll(Tab tab) throws RemoteException {
        return mITestWebLayer.canBrowserControlsScroll(tab.getITab());
    }

    public void addInfoBar(Tab tab, Runnable runnable) throws RemoteException {
        mITestWebLayer.addInfoBar(tab.getITab(), ObjectWrapper.wrap(runnable));
    }

    public View getInfoBarContainerView(Tab tab) throws RemoteException {
        return (View) ObjectWrapper.unwrap(
                mITestWebLayer.getInfoBarContainerView(tab.getITab()), View.class);
    }

    public void setIgnoreMissingKeyForTranslateManager(boolean ignore) throws RemoteException {
        mITestWebLayer.setIgnoreMissingKeyForTranslateManager(ignore);
    }

    public void forceNetworkConnectivityState(boolean networkAvailable) throws RemoteException {
        mITestWebLayer.forceNetworkConnectivityState(networkAvailable);
    }

    public boolean canInfoBarContainerScroll(Tab tab) throws RemoteException {
        return mITestWebLayer.canInfoBarContainerScroll(tab.getITab());
    }

    public String getDisplayedUrl(View urlBarView) throws RemoteException {
        return mITestWebLayer.getDisplayedUrl(ObjectWrapper.wrap(urlBarView));
    }

    public String getTranslateInfoBarTargetLanguage(Tab tab) throws RemoteException {
        return mITestWebLayer.getTranslateInfoBarTargetLanguage(tab.getITab());
    }

    public static void disableWebViewCompatibilityMode() {
        WebLayer.disableWebViewCompatibilityMode();
    }

    public boolean didShowFullscreenToast(Tab tab) throws RemoteException {
        return mITestWebLayer.didShowFullscreenToast(tab.getITab());
    }

    public void initializeMockMediaRouteProvider(boolean closeRouteWithErrorOnSend,
            boolean disableIsSupportsSource, @Nullable String createRouteErrorMessage,
            @Nullable String joinRouteErrorMessage) throws RemoteException {
        mITestWebLayer.initializeMockMediaRouteProvider(closeRouteWithErrorOnSend,
                disableIsSupportsSource, createRouteErrorMessage, joinRouteErrorMessage);
    }

    public View getMediaRouteButton(String name) throws RemoteException {
        return (View) ObjectWrapper.unwrap(mITestWebLayer.getMediaRouteButton(name), View.class);
    }

    public void crashTab(Tab tab) throws RemoteException {
        mITestWebLayer.crashTab(tab.getITab());
    }

    public boolean isWindowOnSmallDevice(Browser browser) throws RemoteException {
        return mITestWebLayer.isWindowOnSmallDevice(browser.getIBrowser());
    }

    public ImageView getSecurityButton(View urlBarView) throws RemoteException {
        return (ImageView) ObjectWrapper.unwrap(
                mITestWebLayer.getSecurityButton(ObjectWrapper.wrap(urlBarView)), ImageView.class);
    }

    public void fetchAccessToken(Profile profile, Set<String> scopes,
            Callback<String> onTokenFetched) throws RemoteException {
        ValueCallback<String> valueCallback = (String token) -> {
            onTokenFetched.onResult(token);
        };
        mITestWebLayer.fetchAccessToken(profile.getIProfile(), ObjectWrapper.wrap(scopes),
                ObjectWrapper.wrap(valueCallback));
    }

    public void addContentCaptureConsumer(Browser browser, Runnable runnable,
            ArrayList<Integer> callbacks) throws RemoteException {
        mITestWebLayer.addContentCaptureConsumer(
                browser.getIBrowser(), ObjectWrapper.wrap(runnable), ObjectWrapper.wrap(callbacks));
    }

    public void notifyOfAutofillEvents(Browser browser, Runnable onNewEvent,
            ArrayList<Integer> eventsObserved) throws RemoteException {
        mITestWebLayer.notifyOfAutofillEvents(browser.getIBrowser(), ObjectWrapper.wrap(onNewEvent),
                ObjectWrapper.wrap(eventsObserved));
    }

    public void activateBackgroundFetchNotification(int id) throws RemoteException {
        mITestWebLayer.activateBackgroundFetchNotification(id);
    }

    public void expediteDownloadService() throws RemoteException {
        mITestWebLayer.expediteDownloadService();
    }

    public void setMockWebAuthnEnabled(boolean enabled) throws RemoteException {
        mITestWebLayer.setMockWebAuthnEnabled(enabled);
    }
}
