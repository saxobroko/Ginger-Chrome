// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/system_token_cert_db_storage_test_util.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/nss_cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class SystemTokenCertDbInitializerTest : public testing::Test {
 public:
  SystemTokenCertDbInitializerTest() {
    TPMTokenLoader::InitializeForTest();
    UserDataAuthClient::InitializeFake();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    TpmManagerClient::InitializeFake();

    system_token_cert_db_initializer_ =
        std::make_unique<SystemTokenCertDBInitializer>();
  }

  SystemTokenCertDbInitializerTest(
      const SystemTokenCertDbInitializerTest& other) = delete;
  SystemTokenCertDbInitializerTest& operator=(
      const SystemTokenCertDbInitializerTest& other) = delete;

  ~SystemTokenCertDbInitializerTest() override {
    TpmManagerClient::Shutdown();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    UserDataAuthClient::Shutdown();
    TPMTokenLoader::Shutdown();
  }

 protected:
  bool InitializeTestSystemSlot() {
    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
    return test_system_slot_->ConstructedSuccessfully();
  }

  SystemTokenCertDBInitializer* system_token_cert_db_initializer() {
    return system_token_cert_db_initializer_.get();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<SystemTokenCertDBInitializer>
      system_token_cert_db_initializer_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
};

// Tests that the system token certificate database will be returned
// successfully by SystemTokenCertDbInitializer if it was available in less than
// 5 minutes after being requested, and the system slot uses software fallback
// when it's allowed and TPM is disabled.
TEST_F(SystemTokenCertDbInitializerTest, GetDatabaseSuccessSoftwareFallback) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);
  system_token_cert_db_initializer()
      ->set_is_system_slot_software_fallback_allowed(true);

  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Check that after 1 minute, SystemTokenCertDBInitializer is still waiting
  // for the system token slot to be initialized and the DB retrieval hasn't
  // timed out yet.
  const auto kOneMinuteDelay = base::TimeDelta::FromMinutes(1);
  EXPECT_LT(kOneMinuteDelay,
            SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);

  task_environment()->FastForwardBy(kOneMinuteDelay);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  EXPECT_TRUE(InitializeTestSystemSlot());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_TRUE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database will be not returned
// successfully by SystemTokenCertDbInitializer if TPM is disabled and system
// slot software fallback is not allowed.
TEST_F(SystemTokenCertDbInitializerTest, GetDatabaseFailureDisabledTPM) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);
  system_token_cert_db_initializer()
      ->set_is_system_slot_software_fallback_allowed(false);

  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  SystemTokenCertDbStorage::Get()->GetDatabase(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Check that after 1 minute, SystemTokenCertDBInitializer is still waiting
  // for the system token slot to be initialized and the DB retrieval hasn't
  // timed out yet.
  const auto kOneMinuteDelay = base::TimeDelta::FromMinutes(1);
  EXPECT_LT(kOneMinuteDelay,
            SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);

  task_environment()->FastForwardBy(kOneMinuteDelay);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  EXPECT_TRUE(InitializeTestSystemSlot());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

}  // namespace chromeos
