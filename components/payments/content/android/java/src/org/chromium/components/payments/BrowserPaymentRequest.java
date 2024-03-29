// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentRequest;
import org.chromium.payments.mojom.PaymentResponse;
import org.chromium.payments.mojom.PaymentValidationErrors;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Map;

/**
 * The browser part of the PaymentRequest implementation. The browser here can be either the
 * Android Chrome browser or the WebLayer "browser".
 */
public interface BrowserPaymentRequest {
    /**
     * The client of the interface calls this when it has received the payment details update
     * from the merchant and has updated the PaymentRequestSpec.
     * @param details The details that the merchant provides to update the payment request.
     * @param hasNotifiedInvokedPaymentApp Whether the client has notified the invoked
     *      payment app of the updated details.
     */
    void onPaymentDetailsUpdated(PaymentDetails details, boolean hasNotifiedInvokedPaymentApp);

    /**
     * The browser part of the {@link PaymentRequest#onPaymentDetailsNotUpdated} implementation.
     * @param selectedShippingOptionError The selected shipping option error, can be null.
     */
    void onPaymentDetailsNotUpdated(@Nullable String selectedShippingOptionError);

    /**
     * Handles the payment request completion.
     * @param result The status of the transaction, defined in {@link PaymentComplete}.
     * @param onCompleteHandled Called when the complete has been handled.
     */
    void complete(int result, Runnable onCompleteHandled);

    /**
     * Called when {@link PaymentRequest#retry} is invoked.
     * @param errors The merchant-defined error message strings, which are used to indicate to the
     *         end-user that something is wrong with the data of the payment response.
     */
    void onRetry(PaymentValidationErrors errors);

    /**
     * Close this instance. The callers of this method should stop referencing this instance upon
     * calling it. This method can be called within itself without causing infinite loop.
     */
    void close();

    /**
     * Modifies the given method data if needed, called when methodData is created.
     * @param methodData A map of method names to PaymentMethodData, could be null. This parameter
     * could be modified in place.
     */
    default void modifyMethodDataIfNeeded(@Nullable Map<String, PaymentMethodData> methodData) {}

    /**
     * Modifies queryForQuota if needed, called when queryForQuota is created.
     * @param queryForQuota The created queryForQuota, which could be modified in place.
     * @param paymentOptions The payment options specified by the merchant.
     */
    default void modifyQueryForQuotaCreatedIfNeeded(
            Map<String, PaymentMethodData> queryForQuota, PaymentOptions paymentOptions) {}

    /**
     * Performs extra validation for the given input and disconnects the mojo pipe if failed.
     * @param webContents The WebContents that represents the merchant page.
     * @param methodData A map of the method data specified for the request.
     * @param details The payment details specified for the request.
     * @param paymentOptions The payment options specified for the request.
     * @return Whether this method has disconnected the mojo pipe.
     */
    default boolean disconnectIfExtraValidationFails(WebContents webContents,
            Map<String, PaymentMethodData> methodData, PaymentDetails details,
            PaymentOptions paymentOptions) {
        return false;
    }

    /**
     * Called when the PaymentRequestSpec is validated.
     * @param spec The validated PaymentRequestSpec.
     */
    void onSpecValidated(PaymentRequestSpec spec);

    /**
     * Adds the PaymentAppFactory(s) specified by the implementers to the given PaymentAppService.
     * @param service The PaymentAppService to be added with the factories.
     * @param delegate The delegate of payment app factory.
     */
    void addPaymentAppFactories(PaymentAppService service, PaymentAppFactoryDelegate delegate);

    default void onWhetherGooglePayBridgeEligible(boolean googlePayBridgeEligible,
            WebContents webContents, PaymentMethodData[] rawMethodData) {}

    /**
     * @return Whether at least one payment app (including basic-card payment app) is available
     *         (excluding the pending apps).
     */
    boolean hasAvailableApps();

    /**
     * Shows the payment apps selector or skip it to invoke the payment app directly.
     * @param isShowWaitingForUpdatedDetails Whether {@link PaymentRequest#show} is waiting for the
     *        updated details.
     * @param total The total amount specified in the payment request.
     * @param shouldSkipAppSelector True if the app selector should be skipped. Note that the
     *        implementer may consider other factors before deciding whether to show or skip.
     * @return The error of the showing if any; null if success.
     */
    @Nullable
    String showOrSkipAppSelector(boolean isShowWaitingForUpdatedDetails, PaymentItem total,
            boolean shouldSkipAppSelector);

    /**
     * Notifies the payment UI service of the payment apps pending to be handled
     * @param pendingApps The payment apps that are pending to be handled.
     */
    void notifyPaymentUiOfPendingApps(List<PaymentApp> pendingApps);

    /**
     * Called when these conditions are satisfied: (1) show() has been called, (2) payment apps
     * are all queried, and (3) PaymentDetails is finalized.
     * @return The error if it fails; null otherwise.
     * @param isUserGestureShow Whether PaymentRequest.show() was invoked with a user gesture.
     */
    @Nullable
    default String onShowCalledAndAppsQueriedAndDetailsFinalized(boolean isUserGestureShow) {
        return null;
    }

    /**
     * Called when a new payment app is created.
     * @param paymentApp The new payment app.
     */
    default void onPaymentAppCreated(PaymentApp paymentApp) {}

    /**
     * @return Whether payment sheet based payment app is supported, e.g., user entering credit
     *      cards on payment sheet.
     */
    default boolean isPaymentSheetBasedPaymentAppSupported() {
        return false;
    }

    /**
     * Patches the given payment response if needed.
     * @param response The payment response to be patched in place.
     * @return Whether the patching is successful.
     */
    default boolean patchPaymentResponseIfNeeded(PaymentResponse response) {
        return true;
    }

    /**
     * Called by the payment app to let Chrome know that the payment app's UI is now hidden, but
     * the payment details have not been returned yet. This is a good time to show a "loading"
     * progress indicator UI.
     */
    default void onInstrumentDetailsLoading() {}

    /**
     * Called after retrieving payment details.
     */
    default void onInstrumentDetailsReady() {}

    /**
     * Called if unable to retrieve payment details.
     * @param errorMessage Developer-facing error message to be used when rejecting the promise
     *                     returned from PaymentRequest.show().
     */
    void onInstrumentDetailsError(String errorMessage);

    /**
     * Opens a payment handler window and creates a WebContents with the given url to display in it.
     * @param url The url of the page to be opened in the window.
     * @param isOffTheRecord Whether the profile is off the record.
     * @param ukmSourceId The ukm source id assigned to the payment app.
     * @return The created WebContents.
     */
    default WebContents openPaymentHandlerWindow(
            GURL url, boolean isOffTheRecord, long ukmSourceId) {
        return null;
    }

    /**
     * Continues the unfinished part of show() that was blocked for the payment details that was
     * pending to be updated.
     * @param details The updated payment details.
     * @param isFinishedQueryingPaymentApps Whether all payment app factories have been queried for
     *         their payment apps.
     * @return The error if it fails; null otherwise.
     */
    @Nullable
    default String continueShowWithUpdatedDetails(
            PaymentDetails details, boolean isFinishedQueryingPaymentApps) {
        return null;
    }

    /**
     * If needed, do extra parsing and validation for details.
     * @param details The details specified by the merchant.
     * @return True if the validation pass.
     */
    default boolean parseAndValidateDetailsFurtherIfNeeded(PaymentDetails details) {
        return true;
    }

    /** @return The selected payment app. */
    PaymentApp getSelectedPaymentApp();

    /** @return All of the available payment apps. */
    List<PaymentApp> getPaymentApps();

    /**
     * @return Whether the payment apps includes at least one that is "complete" which is defined
     *         by {@link PaymentApp#isComplete()}.
     */
    boolean hasAnyCompleteApp();
}
