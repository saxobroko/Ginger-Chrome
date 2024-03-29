// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/known_user.h"

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {
namespace {
absl::optional<std::string> GetStringPrefValue(KnownUser* known_user,
                                               const AccountId& account_id,
                                               const char* pref_name) {
  std::string value;
  if (!known_user->GetStringPref(account_id, pref_name, &value)) {
    return {};
  }
  return value;
}
}  // namespace

// Base class for tests of known_user.
// Sets up global objects necessary for known_user to be able to access
// local_state.
class KnownUserTest : public testing::Test {
 public:
  KnownUserTest() {
    auto fake_user_manager = std::make_unique<FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ =
        std::make_unique<ScopedUserManager>(std::move(fake_user_manager));

    KnownUser::RegisterPrefs(local_state_.registry());
  }
  ~KnownUserTest() override = default;

  KnownUserTest(const KnownUserTest& other) = delete;
  KnownUserTest& operator=(const KnownUserTest& other) = delete;

 protected:
  const AccountId kDefaultAccountId =
      AccountId::FromUserEmailGaiaId("default_account@gmail.com",
                                     "fake-gaia-id");

  FakeUserManager* fake_user_manager() { return fake_user_manager_; }

  PrefService* local_state() { return &local_state_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  // Owned by |scoped_user_manager_|.
  FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<ScopedUserManager> scoped_user_manager_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(KnownUserTest, FindPrefsNonExisting) {
  KnownUser known_user(local_state());
  const base::DictionaryValue* value = nullptr;
  bool read_success = known_user.FindPrefs(kDefaultAccountId, &value);
  EXPECT_FALSE(read_success);
  EXPECT_FALSE(value);
}

TEST_F(KnownUserTest, FindPrefsExisting) {
  KnownUser known_user(local_state());
  const std::string kCustomPrefName = "custom_pref";
  known_user.SetStringPref(kDefaultAccountId, kCustomPrefName, "value");

  const base::DictionaryValue* value = nullptr;
  bool read_success = known_user.FindPrefs(kDefaultAccountId, &value);
  EXPECT_TRUE(read_success);
  ASSERT_TRUE(value);

  const std::string* pref_value = value->FindStringKey(kCustomPrefName);
  ASSERT_TRUE(pref_value);
  EXPECT_EQ(*pref_value, "value");
}

TEST_F(KnownUserTest, FindPrefsIgnoresEphemeralGaiaUsers) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id_2");
  const AccountId kAccountIdEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account4@gmail.com", "guid_4");
  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(
      kAccountIdEphemeralGaia,
      /*is_ephemeral=*/true);
  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(kAccountIdEphemeralAd,
                                                         /*is_ephemeral=*/true);
  const std::string kCustomPrefName = "custom_pref";
  known_user.SetStringPref(kAccountIdEphemeralGaia, kCustomPrefName, "value");
  known_user.SetStringPref(kAccountIdEphemeralAd, kCustomPrefName, "value");

  {
    const base::DictionaryValue* value = nullptr;
    bool read_success = known_user.FindPrefs(kAccountIdEphemeralGaia, &value);
    EXPECT_FALSE(read_success);
    EXPECT_FALSE(value);
  }

  {
    const base::DictionaryValue* value = nullptr;
    bool read_success = known_user.FindPrefs(kAccountIdEphemeralAd, &value);
    EXPECT_TRUE(read_success);
    EXPECT_TRUE(value);
  }
}

TEST_F(KnownUserTest, FindPrefsMatchForUnknownAccountType) {
  KnownUser known_user(local_state());
  // All account ids have the same e-mail
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id_2");
  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account1@gmail.com", "guid");

  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");

  // Looking it up by AccountId always succeeds, no matter which AccountType is
  // used for the lookup.
  const base::DictionaryValue* value = nullptr;
  EXPECT_TRUE(known_user.FindPrefs(kAccountIdUnknown, &value));
  EXPECT_TRUE(known_user.FindPrefs(kAccountIdGaia, &value));
  EXPECT_TRUE(known_user.FindPrefs(kAccountIdAd, &value));
}

TEST_F(KnownUserTest, FindPrefsMatchForGaiaAccountWithEmail) {
  KnownUser known_user(local_state());
  const char* kEmailA = "a@gmail.com";
  const char* kEmailB = "b@gmail.com";
  const char* kGaiaIdA = "a";
  const char* kGaiaIdB = "b";

  known_user.SaveKnownUser(AccountId::FromUserEmailGaiaId(kEmailA, kGaiaIdA));

  const base::DictionaryValue* value = nullptr;

  // Finding by itself should work
  EXPECT_TRUE(known_user.FindPrefs(
      AccountId::FromUserEmailGaiaId(kEmailA, kGaiaIdA), &value));
  // Finding by gaia id should also work even if the e-mail doesn't match.
  EXPECT_TRUE(known_user.FindPrefs(
      AccountId::FromUserEmailGaiaId(kEmailB, kGaiaIdA), &value));
  // Finding by e-mail should also work even if the gaia id doesn't match.
  // TODO(https://crbug.com/1190902): This should likely be EXPECT_FALSE going
  // forward.
  EXPECT_TRUE(known_user.FindPrefs(
      AccountId::FromUserEmailGaiaId(kEmailA, kGaiaIdB), &value));
  // Finding by just gaia id without any e-mail doesn't work (because the
  // resulting AccountId is not considered valid).
  EXPECT_FALSE(known_user.FindPrefs(AccountId::FromGaiaId(kGaiaIdA), &value));

  // An unrelated gaia AccountId with the same Account Type doesn't find
  // anything.
  EXPECT_FALSE(known_user.FindPrefs(
      AccountId::FromUserEmailGaiaId(kEmailB, kGaiaIdB), &value));

  // Looking up an AccountId stored as gaia by an unknown-type AccountId with
  // the same e-mail address succeeds.
  EXPECT_TRUE(known_user.FindPrefs(AccountId::FromUserEmail(kEmailA), &value));

  // Looking up an AccountId stored as gaia by an AccountId with type Ad fails.
  EXPECT_FALSE(known_user.FindPrefs(
      AccountId::AdFromUserEmailObjGuid(kEmailA, "guid"), &value));
}

TEST_F(KnownUserTest, FindPrefsMatchForAdAccountWithEmail) {
  KnownUser known_user(local_state());
  const std::string kEmailA = "a@gmail.com";
  const std::string kEmailB = "b@gmail.com";

  known_user.SaveKnownUser(AccountId::AdFromUserEmailObjGuid(kEmailA, "a"));

  const base::DictionaryValue* value = nullptr;

  // Finding by itself should work
  EXPECT_TRUE(known_user.FindPrefs(
      AccountId::AdFromUserEmailObjGuid(kEmailA, "a"), &value));
  // Finding by guid should also work even if the e-mail doesn't match.
  EXPECT_TRUE(known_user.FindPrefs(
      AccountId::AdFromUserEmailObjGuid(kEmailB, "a"), &value));
  // Finding by e-mail should also work even if the guid doesn't match.
  EXPECT_TRUE(known_user.FindPrefs(
      AccountId::AdFromUserEmailObjGuid(kEmailA, "b"), &value));
  // Finding by just AD guid  without any e-mail doesn't work (because the
  // resulting AccountId is not considered valid).
  EXPECT_FALSE(known_user.FindPrefs(AccountId::AdFromObjGuid("a"), &value));

  // An unrelated AD AccountId with the same Account Type doesn't find
  // anything.
  EXPECT_FALSE(known_user.FindPrefs(
      AccountId::AdFromUserEmailObjGuid(kEmailB, "b"), &value));

  // Looking up an AccountId stored as AD by an unknown-type AccountId with
  // the same e-mail address succeeds.
  EXPECT_TRUE(known_user.FindPrefs(AccountId::FromUserEmail(kEmailA), &value));

  // Looking up an AccountId stored as AD by an AccountId with type gaia fails.
  EXPECT_FALSE(known_user.FindPrefs(
      AccountId::FromUserEmailGaiaId(kEmailA, "gaia_id"), &value));
}

TEST_F(KnownUserTest, UpdatePrefsWithoutClear) {
  KnownUser known_user(local_state());
  constexpr char kPrefName1[] = "pref1";
  constexpr char kPrefName2[] = "pref2";

  {
    base::DictionaryValue update;
    update.SetKey(kPrefName1, base::Value("pref1_value1"));
    known_user.UpdatePrefs(kDefaultAccountId, update, /*clear=*/false);
  }

  {
    base::DictionaryValue update;
    update.SetKey(kPrefName1, base::Value("pref1_value2"));
    known_user.UpdatePrefs(kDefaultAccountId, update, /*clear=*/false);
  }

  {
    base::DictionaryValue update;
    update.SetKey(kPrefName2, base::Value("pref2_value1"));
    known_user.UpdatePrefs(kDefaultAccountId, update, /*clear=*/false);
  }

  EXPECT_EQ(absl::make_optional(std::string("pref1_value2")),
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName1));
  EXPECT_EQ(absl::make_optional(std::string("pref2_value1")),
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName2));
}

TEST_F(KnownUserTest, UpdatePrefsWithClear) {
  KnownUser known_user(local_state());
  constexpr char kPrefName1[] = "pref1";
  constexpr char kPrefName2[] = "pref2";

  {
    base::DictionaryValue update;
    update.SetKey(kPrefName1, base::Value("pref1_value1"));
    known_user.UpdatePrefs(kDefaultAccountId, update, /*clear=*/false);
  }

  {
    base::DictionaryValue update;
    update.SetKey(kPrefName2, base::Value("pref2_value1"));
    known_user.UpdatePrefs(kDefaultAccountId, update, /*clear=*/true);
  }

  EXPECT_EQ(absl::nullopt,
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName1));
  EXPECT_EQ(absl::make_optional(std::string("pref2_value1")),
            GetStringPrefValue(&known_user, kDefaultAccountId, kPrefName2));
}

TEST_F(KnownUserTest, GetKnownAccountIdsNoAccounts) {
  KnownUser known_user(local_state());
  EXPECT_THAT(known_user.GetKnownAccountIds(), testing::IsEmpty());
}

TEST_F(KnownUserTest, GetKnownAccountIdsWithAccounts) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id");
  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account3@gmail.com", "obj_guid");

  known_user.SaveKnownUser(kAccountIdGaia);
  known_user.SaveKnownUser(kAccountIdAd);

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdGaia, kAccountIdAd));
}

TEST_F(KnownUserTest, SaveKnownUserIgnoresUnknownType) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account2@gmail.com");

  known_user.SaveKnownUser(kAccountIdUnknown);

  EXPECT_THAT(known_user.GetKnownAccountIds(), testing::IsEmpty());
}

TEST_F(KnownUserTest, SaveKnownUserIgnoresEphemeralGaiaUsers) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdNonEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id_1");
  const AccountId kAccountIdEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id_2");
  const AccountId kAccountIdNonEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account3@gmail.com", "guid_3");
  const AccountId kAccountIdEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account4@gmail.com", "guid_4");

  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(
      kAccountIdEphemeralGaia,
      /*is_ephemeral=*/true);
  fake_user_manager()->SetUserNonCryptohomeDataEphemeral(kAccountIdEphemeralAd,
                                                         /*is_ephemeral=*/true);

  known_user.SaveKnownUser(kAccountIdNonEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdNonEphemeralAd);
  known_user.SaveKnownUser(kAccountIdEphemeralAd);

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdNonEphemeralGaia,
                                            kAccountIdNonEphemeralAd,
                                            kAccountIdEphemeralAd));
}

TEST_F(KnownUserTest, UpdateGaiaID) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");

  {
    std::string gaia_id;
    EXPECT_FALSE(known_user.FindGaiaID(kAccountIdUnknown, &gaia_id));
  }

  known_user.UpdateGaiaID(kAccountIdUnknown, "gaia_id");

  {
    std::string gaia_id;
    EXPECT_TRUE(known_user.FindGaiaID(kAccountIdUnknown, &gaia_id));
    EXPECT_EQ(gaia_id, "gaia_id");
  }

  // UpdateGaiaID also sets account type to gaia account.
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id");
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdGaia));
}

TEST_F(KnownUserTest, UpdateIdForGaiaAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdUnknown));

  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id");
  known_user.UpdateId(kAccountIdGaia);
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdGaia));
}

TEST_F(KnownUserTest, UpdateIdForAdAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdUnknown =
      AccountId::FromUserEmail("account1@gmail.com");
  known_user.SetStringPref(kAccountIdUnknown, "some_pref", "some_value");
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdUnknown));

  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account1@gmail.com", "guid");
  known_user.UpdateId(kAccountIdAd);
  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdAd));
}

TEST_F(KnownUserTest, FindGaiaIdForGaiaAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id");
  known_user.SaveKnownUser(kAccountIdGaia);

  std::string gaia_id;
  EXPECT_TRUE(known_user.FindGaiaID(kAccountIdGaia, &gaia_id));
  EXPECT_EQ(gaia_id, "gaia_id");
}

TEST_F(KnownUserTest, FindGaiaIdForAdAccount) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdAd =
      AccountId::AdFromUserEmailObjGuid("account1@gmail.com", "guid");
  known_user.SaveKnownUser(kAccountIdAd);

  std::string gaia_id;
  EXPECT_FALSE(known_user.FindGaiaID(kAccountIdAd, &gaia_id));
}

// TODO(https://crbug.com/1148457): Add tests for GetAccountId.

TEST_F(KnownUserTest, RemovePrefOnCustomPref) {
  KnownUser known_user(local_state());
  const std::string kCustomPrefName = "custom_pref";

  known_user.SetStringPref(kDefaultAccountId, kCustomPrefName, "value");
  {
    std::string read_value;
    EXPECT_TRUE(known_user.GetStringPref(kDefaultAccountId, kCustomPrefName,
                                         &read_value));
  }

  known_user.RemovePref(kDefaultAccountId, kCustomPrefName);
  {
    std::string read_value;
    EXPECT_FALSE(known_user.GetStringPref(kDefaultAccountId, kCustomPrefName,
                                          &read_value));
  }
}

// Test failing on linux-chromeos-chrome (crbug.com/1198519)
TEST_F(KnownUserTest, DISABLED_RemovePrefOnReservedPref) {
  KnownUser known_user(local_state());
  const std::string kReservedPrefName = "device_id";

  known_user.SetStringPref(kDefaultAccountId, kReservedPrefName, "value");
  ASSERT_DEATH(known_user.RemovePref(kDefaultAccountId, kReservedPrefName),
               ".*Check failed.*");
}

TEST_F(KnownUserTest, GaiaIdMigrationStatus) {
  KnownUser known_user(local_state());
  const std::string kSubsystem1 = "subsystem1";
  const std::string kSubsystem2 = "subsystem2";

  EXPECT_FALSE(
      known_user.GetGaiaIdMigrationStatus(kDefaultAccountId, kSubsystem1));
  EXPECT_FALSE(
      known_user.GetGaiaIdMigrationStatus(kDefaultAccountId, kSubsystem2));

  known_user.SetGaiaIdMigrationStatusDone(kDefaultAccountId, kSubsystem1);

  EXPECT_TRUE(
      known_user.GetGaiaIdMigrationStatus(kDefaultAccountId, kSubsystem1));
  EXPECT_FALSE(
      known_user.GetGaiaIdMigrationStatus(kDefaultAccountId, kSubsystem2));
}

TEST_F(KnownUserTest, DeviceId) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetDeviceId(kDefaultAccountId), std::string());

  known_user.SetDeviceId(kDefaultAccountId, "test");

  EXPECT_EQ(known_user.GetDeviceId(kDefaultAccountId), "test");
}

TEST_F(KnownUserTest, GAPSCookie) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetGAPSCookie(kDefaultAccountId), std::string());

  known_user.SetGAPSCookie(kDefaultAccountId, "test");

  EXPECT_EQ(known_user.GetGAPSCookie(kDefaultAccountId), "test");
}

TEST_F(KnownUserTest, UsingSAML) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.IsUsingSAML(kDefaultAccountId));

  known_user.UpdateUsingSAML(kDefaultAccountId, /*using_saml=*/true);
  EXPECT_TRUE(known_user.IsUsingSAML(kDefaultAccountId));
}

TEST_F(KnownUserTest, UsingSAMLPrincipalsAPI) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetIsUsingSAMLPrincipalsAPI(kDefaultAccountId));

  known_user.UpdateIsUsingSAMLPrincipalsAPI(kDefaultAccountId,
                                            /*using_saml=*/true);
  EXPECT_TRUE(known_user.GetIsUsingSAMLPrincipalsAPI(kDefaultAccountId));
}

TEST_F(KnownUserTest, ProfileRequiresPolicy) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kUnknown);

  known_user.SetProfileRequiresPolicy(kDefaultAccountId,
                                      ProfileRequiresPolicy::kPolicyRequired);
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kPolicyRequired);

  known_user.SetProfileRequiresPolicy(kDefaultAccountId,
                                      ProfileRequiresPolicy::kNoPolicyRequired);
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kNoPolicyRequired);

  known_user.ClearProfileRequiresPolicy(kDefaultAccountId);
  EXPECT_EQ(known_user.GetProfileRequiresPolicy(kDefaultAccountId),
            ProfileRequiresPolicy::kUnknown);
}

TEST_F(KnownUserTest, ReauthReason) {
  KnownUser known_user(local_state());
  {
    int auth_reason;
    EXPECT_FALSE(known_user.FindReauthReason(kDefaultAccountId, &auth_reason));
  }

  known_user.UpdateReauthReason(kDefaultAccountId, 3);
  {
    int auth_reason;
    EXPECT_TRUE(known_user.FindReauthReason(kDefaultAccountId, &auth_reason));
    EXPECT_EQ(auth_reason, 3);
  }
}

TEST_F(KnownUserTest, ChallengeResponseKeys) {
  KnownUser known_user(local_state());
  EXPECT_TRUE(known_user.GetChallengeResponseKeys(kDefaultAccountId).is_none());

  base::Value challenge_response_keys(base::Value::Type::LIST);
  challenge_response_keys.Append(base::Value("key1"));
  known_user.SetChallengeResponseKeys(kDefaultAccountId,
                                      challenge_response_keys.Clone());

  EXPECT_EQ(known_user.GetChallengeResponseKeys(kDefaultAccountId),
            challenge_response_keys);
}

TEST_F(KnownUserTest, LastOnlineSignin) {
  KnownUser known_user(local_state());
  EXPECT_TRUE(known_user.GetLastOnlineSignin(kDefaultAccountId).is_null());

  base::Time last_online_signin = base::Time::Now();
  known_user.SetLastOnlineSignin(kDefaultAccountId, last_online_signin);

  EXPECT_EQ(known_user.GetLastOnlineSignin(kDefaultAccountId),
            last_online_signin);
}

TEST_F(KnownUserTest, OfflineSigninLimit) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetOfflineSigninLimit(kDefaultAccountId).has_value());

  base::TimeDelta offline_signin_limit = base::TimeDelta::FromMinutes(80);
  known_user.SetOfflineSigninLimit(kDefaultAccountId, offline_signin_limit);

  EXPECT_EQ(known_user.GetOfflineSigninLimit(kDefaultAccountId).value(),
            offline_signin_limit);
}

TEST_F(KnownUserTest, IsEnterpriseManaged) {
  KnownUser known_user(local_state());
  EXPECT_FALSE(known_user.GetIsEnterpriseManaged(kDefaultAccountId));

  known_user.SetIsEnterpriseManaged(kDefaultAccountId, true);

  EXPECT_TRUE(known_user.GetIsEnterpriseManaged(kDefaultAccountId));
}

TEST_F(KnownUserTest, AccountManager) {
  KnownUser known_user(local_state());
  {
    std::string account_manager;
    EXPECT_FALSE(
        known_user.GetAccountManager(kDefaultAccountId, &account_manager));
  }

  known_user.SetAccountManager(kDefaultAccountId, "test");

  {
    std::string account_manager;
    EXPECT_TRUE(
        known_user.GetAccountManager(kDefaultAccountId, &account_manager));
  }
}

TEST_F(KnownUserTest, UserLastLoginInputMethod) {
  KnownUser known_user(local_state());
  {
    std::string user_last_input_method;
    EXPECT_FALSE(known_user.GetUserLastInputMethod(kDefaultAccountId,
                                                   &user_last_input_method));
  }

  known_user.SetUserLastLoginInputMethod(kDefaultAccountId, "test");

  {
    std::string user_last_input_method;
    EXPECT_TRUE(known_user.GetUserLastInputMethod(kDefaultAccountId,
                                                  &user_last_input_method));
  }
}

TEST_F(KnownUserTest, UserPinLength) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetUserPinLength(kDefaultAccountId), 0);

  known_user.SetUserPinLength(kDefaultAccountId, 8);

  EXPECT_EQ(known_user.GetUserPinLength(kDefaultAccountId), 8);
}

TEST_F(KnownUserTest, PinAutosubmitBackfillNeeded) {
  KnownUser known_user(local_state());
  // If the pref is not set, returns true.
  EXPECT_TRUE(known_user.PinAutosubmitIsBackfillNeeded(kDefaultAccountId));

  known_user.PinAutosubmitSetBackfillNotNeeded(kDefaultAccountId);

  EXPECT_FALSE(known_user.PinAutosubmitIsBackfillNeeded(kDefaultAccountId));

  known_user.PinAutosubmitSetBackfillNeededForTests(kDefaultAccountId);

  EXPECT_TRUE(known_user.PinAutosubmitIsBackfillNeeded(kDefaultAccountId));
}

TEST_F(KnownUserTest, PasswordSyncToken) {
  KnownUser known_user(local_state());
  EXPECT_EQ(known_user.GetPasswordSyncToken(kDefaultAccountId), std::string());

  known_user.SetPasswordSyncToken(kDefaultAccountId, "test");

  EXPECT_EQ(known_user.GetPasswordSyncToken(kDefaultAccountId), "test");
}

TEST_F(KnownUserTest, CleanEphemeralUsersRemovesEphemeralAdOnly) {
  KnownUser known_user(local_state());
  const AccountId kAccountIdNonEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account1@gmail.com", "gaia_id_1");
  const AccountId kAccountIdEphemeralGaia =
      AccountId::FromUserEmailGaiaId("account2@gmail.com", "gaia_id_2");
  const AccountId kAccountIdNonEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account3@gmail.com", "guid_3");
  const AccountId kAccountIdEphemeralAd =
      AccountId::AdFromUserEmailObjGuid("account4@gmail.com", "guid_4");

  known_user.SaveKnownUser(kAccountIdNonEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdEphemeralGaia);
  known_user.SaveKnownUser(kAccountIdNonEphemeralAd);
  known_user.SaveKnownUser(kAccountIdEphemeralAd);
  known_user.SetIsEphemeralUser(kAccountIdEphemeralGaia,
                                /*is_ephemeral=*/true);
  known_user.SetIsEphemeralUser(kAccountIdEphemeralAd, /*is_ephemeral=*/true);

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(
                  kAccountIdNonEphemeralGaia, kAccountIdEphemeralGaia,
                  kAccountIdNonEphemeralAd, kAccountIdEphemeralAd));

  known_user.CleanEphemeralUsers();

  EXPECT_THAT(known_user.GetKnownAccountIds(),
              testing::UnorderedElementsAre(kAccountIdNonEphemeralGaia,
                                            kAccountIdEphemeralGaia,
                                            kAccountIdNonEphemeralAd));
}

TEST_F(KnownUserTest, CleanObsoletePrefs) {
  KnownUser known_user(local_state());
  const std::string kObsoletePrefName = "minimal_migration_attempted";
  const std::string kCustomPrefName = "custom_pref";

  // Set an obsolete pref.
  known_user.SetBooleanPref(kDefaultAccountId, kObsoletePrefName, true);
  // Set a custom pref.
  known_user.SetBooleanPref(kDefaultAccountId, kCustomPrefName, true);
  // Set a reserved, non-obsolete pref.
  known_user.SetIsEnterpriseManaged(kDefaultAccountId, true);

  known_user.CleanObsoletePrefs();

  // Verify that only the obsolete pref has been removed.
  EXPECT_FALSE(known_user.GetBooleanPref(kDefaultAccountId, kObsoletePrefName,
                                         /*out_value=*/nullptr));

  bool custom_pref_value = false;
  EXPECT_TRUE(known_user.GetBooleanPref(kDefaultAccountId, kCustomPrefName,
                                        &custom_pref_value));
  EXPECT_TRUE(custom_pref_value);

  EXPECT_TRUE(known_user.GetIsEnterpriseManaged(kDefaultAccountId));
}

//
// =============================================================================
// Type-parametrized unittests for Set{String,Boolean,Integer,}Pref and
// Get{String,Boolean,Integer,}Pref.
// For every type (string, boolean, integer, raw base::Value) a PrefTypeInfo
// struct is declared which is then referenced in the generic test code.

// Test type holder for known_user string prefs.
struct PrefTypeInfoString {
  using PrefType = std::string;
  using PrefTypeForReading = std::string;

  static constexpr auto SetFunc = &KnownUser::SetStringPref;
  static constexpr auto GetFunc = &KnownUser::GetStringPref;

  static PrefType CreatePrefValue() { return std::string("test"); }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == "test";
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_string() && read_value.GetString() == "test";
  }
};

// Test type holder for known_user integer prefs.
struct PrefTypeInfoInteger {
  using PrefType = int;
  using PrefTypeForReading = int;

  static constexpr auto SetFunc = &KnownUser::SetIntegerPref;
  static constexpr auto GetFunc = &KnownUser::GetIntegerPref;

  static PrefType CreatePrefValue() { return 7; }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == 7;
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_int() && read_value.GetInt() == 7;
  }
};

// Test type holder for known_user boolean prefs.
struct PrefTypeInfoBoolean {
  using PrefType = bool;
  using PrefTypeForReading = bool;

  static constexpr auto SetFunc = &KnownUser::SetBooleanPref;
  static constexpr auto GetFunc = &KnownUser::GetBooleanPref;

  static PrefType CreatePrefValue() { return true; }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return read_value == true;
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value.is_bool() && read_value.GetBool() == true;
  }
};

// Test type holder for known_user base::Value prefs.
struct PrefTypeInfoValue {
  using PrefType = base::Value;
  using PrefTypeForReading = const base::Value*;

  static constexpr auto SetFunc = &KnownUser::SetPref;
  static constexpr auto GetFunc = &KnownUser::GetPref;

  static PrefType CreatePrefValue() { return base::Value("test"); }
  static bool CheckPrefValue(PrefTypeForReading read_value) {
    return *read_value == CreatePrefValue();
  }
  static bool CheckPrefValueAsBaseValue(const base::Value& read_value) {
    return read_value == CreatePrefValue();
  }
};

template <typename PrefTypeInfo>
class KnownUserWithPrefTypeTest : public KnownUserTest {
 public:
  KnownUserWithPrefTypeTest() = default;
  ~KnownUserWithPrefTypeTest() = default;
};

TYPED_TEST_SUITE_P(KnownUserWithPrefTypeTest);

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadOnNonExistingUser) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kNonExistingUser =
      AccountId::FromUserEmail("account1@gmail.com");

  typename TypeParam::PrefTypeForReading read_result;
  bool read_success = (known_user.*TypeParam::GetFunc)(kNonExistingUser,
                                                       kPrefName, &read_result);
  EXPECT_FALSE(read_success);
}

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadMissingPrefOnExistingUser) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kUser = AccountId::FromUserEmail("account1@gmail.com");
  known_user.SaveKnownUser(kUser);

  typename TypeParam::PrefTypeForReading read_result;
  bool read_success =
      (known_user.*TypeParam::GetFunc)(kUser, kPrefName, &read_result);
  EXPECT_FALSE(read_success);
}

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadExistingPref) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kUser = AccountId::FromUserEmail("account1@gmail.com");

  // Set* implicitly creates the known_user user entry.
  (known_user.*TypeParam::SetFunc)(kUser, kPrefName,
                                   TypeParam::CreatePrefValue());

  typename TypeParam::PrefTypeForReading read_result;
  bool read_success =
      (known_user.*TypeParam::GetFunc)(kUser, kPrefName, &read_result);
  EXPECT_TRUE(read_success);
  TypeParam::CheckPrefValue(read_result);
}

TYPED_TEST_P(KnownUserWithPrefTypeTest, ReadExistingPrefAsValue) {
  KnownUser known_user(KnownUserTest::local_state());

  constexpr char kPrefName[] = "some_pref";
  const AccountId kUser = AccountId::FromUserEmail("account1@gmail.com");

  // Set* implicitly creates the known_user user entry.
  (known_user.*TypeParam::SetFunc)(kUser, kPrefName,
                                   TypeParam::CreatePrefValue());

  const base::Value* read_result;
  bool read_success = known_user.GetPref(kUser, kPrefName, &read_result);
  EXPECT_TRUE(read_success);
  ASSERT_TRUE(read_result);
  TypeParam::CheckPrefValueAsBaseValue(*read_result);
}

REGISTER_TYPED_TEST_SUITE_P(KnownUserWithPrefTypeTest,
                            // All test functions must be listed:
                            ReadOnNonExistingUser,
                            ReadMissingPrefOnExistingUser,
                            ReadExistingPref,
                            ReadExistingPrefAsValue);

// This must be an alias because the preprocessor does not understand <> so if
// it was directly embedded in the INSTANTIATE_TYPED_TEST_SUITE_P macro the
// prepocessor would be confused on the comma.
using AllTypeInfos = testing::Types<PrefTypeInfoString,
                                    PrefTypeInfoInteger,
                                    PrefTypeInfoBoolean,
                                    PrefTypeInfoValue>;

INSTANTIATE_TYPED_TEST_SUITE_P(AllTypes,
                               KnownUserWithPrefTypeTest,
                               AllTypeInfos);

}  // namespace user_manager
