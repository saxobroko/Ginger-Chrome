// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/method_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace payments {
namespace {

using ::testing::_;
using ::testing::Eq;

static constexpr char kNetworkDataBase64[] = "aaaa";
static constexpr char kCredentialIdBase64[] = "cccc";

class MockAuthenticator : public autofill::InternalAuthenticator {
 public:
  explicit MockAuthenticator(bool should_succeed)
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)),
        should_succeed_(should_succeed) {}
  ~MockAuthenticator() override = default;

  MOCK_METHOD1(SetEffectiveOrigin, void(const url::Origin&));
  MOCK_METHOD2(
      MakeCredential,
      void(blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
           blink::mojom::Authenticator::MakeCredentialCallback callback));
  MOCK_METHOD1(IsUserVerifyingPlatformAuthenticatorAvailable,
               void(blink::mojom::Authenticator::
                        IsUserVerifyingPlatformAuthenticatorAvailableCallback));
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD1(VerifyChallenge, void(const std::vector<uint8_t>&));

  content::RenderFrameHost* GetRenderFrameHost() override {
    return web_contents_->GetMainFrame();
  }

  // Implements an autofill::InternalAuthenticator method to delegate fields of
  // |options| to gmock methods for easier verification.
  void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) override {
    VerifyChallenge(options->challenge);
    std::move(callback).Run(
        should_succeed_ ? blink::mojom::AuthenticatorStatus::SUCCESS
                        : blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR,
        blink::mojom::GetAssertionAuthenticatorResponse::New());
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;  // Owned by `web_contents_factory_`.
  bool should_succeed_;
};

class SecurePaymentConfirmationAppTest : public testing::Test,
                                         public PaymentApp::Delegate {
 protected:
  SecurePaymentConfirmationAppTest() : label_(u"test instrument") {
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->total = mojom::PaymentItem::New();
    details->total->amount = mojom::PaymentCurrencyAmount::New();
    details->total->amount->currency = "USD";
    details->total->amount->value = "1.25";
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    spec_ = std::make_unique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), std::move(details),
        std::move(method_data), /*observer=*/nullptr, /*app_locale=*/"en-US");
  }

  void SetUp() override {
    ASSERT_TRUE(base::Base64Decode(kNetworkDataBase64, &network_data_bytes_));
    ASSERT_TRUE(base::Base64Decode(kCredentialIdBase64, &credential_id_bytes_));
  }

  mojom::SecurePaymentConfirmationRequestPtr MakeRequest() {
    auto request = mojom::SecurePaymentConfirmationRequest::New();
    request->network_data = std::vector<uint8_t>(network_data_bytes_.begin(),
                                                 network_data_bytes_.end());
    return request;
  }

  // PaymentApp::Delegate:
  void OnInstrumentDetailsReady(const std::string& method_name,
                                const std::string& stringified_details,
                                const PayerData& payer_data) override {
    EXPECT_EQ(method_name, methods::kSecurePaymentConfirmation);
    EXPECT_EQ(
        stringified_details,
        "{\"appid_extension\":false,\"challenge\":\"{\\\"merchantData\\\":{"
        "\\\"merchantOrigin\\\":\\\"https://"
        "merchant.example\\\",\\\"total\\\":{\\\"currency\\\":\\\"USD\\\","
        "\\\"value\\\":\\\"1.25\\\"}},\\\"networkData\\\":\\\"aaaa\\\"}\","
        "\"echo_appid_extension\":false,\"echo_prf\":false,\"info\":{},\"prf_"
        "not_evaluated\":false,\"prf_results\":{},\"signature\":\"\"}");
    EXPECT_EQ(payer_data.payer_name, "");
    EXPECT_EQ(payer_data.payer_email, "");
    EXPECT_EQ(payer_data.payer_phone, "");
    EXPECT_TRUE(payer_data.shipping_address.is_null());
    EXPECT_EQ(payer_data.selected_shipping_option_id, "");

    on_instrument_details_ready_called_ = true;
  }

  void OnInstrumentDetailsError(const std::string& error_message) override {
    EXPECT_EQ(error_message, "Authenticator returned NOT_ALLOWED_ERROR.");
    on_instrument_details_error_called_ = true;
  }

  std::u16string label_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  std::string network_data_bytes_;
  std::string credential_id_bytes_;
  bool on_instrument_details_ready_called_ = false;
  bool on_instrument_details_error_called_ = false;

  base::WeakPtrFactory<SecurePaymentConfirmationAppTest> weak_ptr_factory_{
      this};
};

TEST_F(SecurePaymentConfirmationAppTest, Smoke) {
  std::vector<uint8_t> credential_id(credential_id_bytes_.begin(),
                                     credential_id_bytes_.end());

  auto authenticator =
      std::make_unique<MockAuthenticator>(/*should_succeed=*/true);
  MockAuthenticator* mock_authenticator = authenticator.get();
  content::WebContents* web_contents = authenticator->web_contents();

  SecurePaymentConfirmationApp app(
      web_contents, "effective_rp.example",
      /*Icon=*/std::make_unique<SkBitmap>(), label_, std::move(credential_id),
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator));

  EXPECT_CALL(*mock_authenticator, SetEffectiveOrigin(Eq(url::Origin::Create(
                                       GURL("https://effective_rp.example")))));

  // This is the SHA-256 hash of the serialized JSON string:
  // {"merchantData":{"merchantOrigin":"https://merchant.example","total":
  // {"currency":"USD","value":"1.25"}},"networkData":"aaaa"}
  //
  // To update the test expectation, open
  // //components/test/data/payments/secure_payment_confirmation_debut.html in a
  // browser and follow the instructions.
  std::vector<uint8_t> expected_bytes = {
      240, 123, 37,  51,  16,  34,  244, 220, 166, 179, 139,
      85,  229, 152, 242, 133, 88,  44,  222, 133, 49,  97,
      146, 20,  207, 119, 43,  142, 171, 239, 125, 250};
  EXPECT_CALL(*mock_authenticator, VerifyChallenge(Eq(expected_bytes)));
  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
  EXPECT_TRUE(on_instrument_details_ready_called_);
  EXPECT_FALSE(on_instrument_details_error_called_);
}

// Test that OnInstrumentDetailsError is called when the authenticator returns
// an error.
TEST_F(SecurePaymentConfirmationAppTest, OnInstrumentDetailsError) {
  std::vector<uint8_t> credential_id(credential_id_bytes_.begin(),
                                     credential_id_bytes_.end());

  auto authenticator =
      std::make_unique<MockAuthenticator>(/*should_succeed=*/false);
  content::WebContents* web_contents = authenticator->web_contents();

  SecurePaymentConfirmationApp app(
      web_contents, "effective_rp.example",
      /*Icon=*/std::make_unique<SkBitmap>(), label_, std::move(credential_id),
      url::Origin::Create(GURL("https://merchant.example")), spec_->AsWeakPtr(),
      MakeRequest(), std::move(authenticator));

  app.InvokePaymentApp(/*delegate=*/weak_ptr_factory_.GetWeakPtr());
  EXPECT_FALSE(on_instrument_details_ready_called_);
  EXPECT_TRUE(on_instrument_details_error_called_);
}

}  // namespace
}  // namespace payments
