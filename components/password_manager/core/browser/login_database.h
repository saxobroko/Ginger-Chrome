// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"

#if defined(OS_IOS)
#include "base/gtest_prod_util.h"
#endif

#if defined(OS_MAC)
#include "components/password_manager/core/browser/password_recovery_util_mac.h"
#endif

namespace password_manager {

class SQLTableBuilder;

extern const int kCurrentVersionNumber;
extern const int kCompatibleVersionNumber;

// Interface to the database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
class LoginDatabase : public PasswordStoreSync::MetadataStore {
 public:
  LoginDatabase(const base::FilePath& db_path, IsAccountStore is_account_store);
  ~LoginDatabase() override;

  // Deletes any database files for the given |db_path| from the disk. Must not
  // be called while a LoginDatabase instance for this path exists!
  // This does blocking I/O, so must only be called from a thread that allows
  // this (in particular, *not* from the UI thread).
  static void DeleteDatabaseFile(const base::FilePath& db_path);

  // Returns whether this is the profile-scoped or the account-scoped storage:
  // true:  Gaia-account-scoped store, which is used for signed-in but not
  //        syncing users.
  // false: Profile-scoped store, which is used for local storage and for
  //        syncing users.
  bool is_account_store() const { return is_account_store_.value(); }

  // Actually creates/opens the database. If false is returned, no other method
  // should be called.
  virtual bool Init();

#if defined(OS_MAC)
  // Registers utility which is used to save password recovery status on MacOS.
  void InitPasswordRecoveryUtil(
      std::unique_ptr<PasswordRecoveryUtilMac> password_recovery_util);
#endif

  // Reports usage metrics to UMA.
  void ReportMetrics(const std::string& sync_username,
                     bool custom_passphrase_sync_enabled,
                     BulkCheckDone bulk_check_done);

  // Adds |form| to the list of remembered password forms. Returns the list of
  // changes applied ({}, {ADD}, {REMOVE, ADD}). If it returns {REMOVE, ADD}
  // then the REMOVE is associated with the form that was added. Thus only the
  // primary key columns contain the values associated with the removed form. In
  // case of error, it sets |error| if |error| isn't null.
  PasswordStoreChangeList AddLogin(const PasswordForm& form,
                                   AddLoginError* error = nullptr)
      WARN_UNUSED_RESULT;

  // Updates existing password form. Returns the list of applied changes ({},
  // {UPDATE}). The password is looked up by the tuple {origin,
  // username_element, username_value, password_element, signon_realm}. These
  // columns stay intact. In case of error, it sets |error| if |error| isn't
  // null.
  PasswordStoreChangeList UpdateLogin(const PasswordForm& form,
                                      UpdateLoginError* error = nullptr)
      WARN_UNUSED_RESULT;

  // Removes |form| from the list of remembered password forms. Returns true if
  // |form| was successfully removed from the database. If |changes| is not be
  // null, it will be used to populate the change list of the removed forms if
  // any.
  bool RemoveLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) WARN_UNUSED_RESULT;

  // Removes the form with |primary_key| from the list of remembered password
  // forms. Returns true if the form was successfully removed from the database.
  bool RemoveLoginByPrimaryKey(FormPrimaryKey primary_key,
                               PasswordStoreChangeList* changes)
      WARN_UNUSED_RESULT;

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction. If |changes| is not be null, it will be used to
  // populate the change list of the removed forms if any.
  bool RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end,
                                  PasswordStoreChangeList* changes);

  // Sets the 'skip_zero_click' flag on all forms on |origin| to 'true'.
  bool DisableAutoSignInForOrigin(const GURL& origin);

  // All Get* methods below overwrite |forms| with the returned credentials. On
  // success, those methods return true.

  // Gets a list of credentials matching |form|, including blocklisted matches
  // and federated credentials.
  bool GetLogins(const PasswordStore::FormDigest& form,
                 std::vector<std::unique_ptr<PasswordForm>>* forms)
      WARN_UNUSED_RESULT;

  // Gets a list of credentials with password_value=|plain_text_password|.
  bool GetLoginsByPassword(const std::u16string& plain_text_password,
                           std::vector<std::unique_ptr<PasswordForm>>* forms)
      WARN_UNUSED_RESULT;

  // Gets all logins created from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction. |key_to_form_map| must not be null and will be used to return
  // the results. The key of the map is the DB primary key.
  bool GetLoginsCreatedBetween(base::Time begin,
                               base::Time end,
                               PrimaryKeyToFormMap* key_to_form_map)
      WARN_UNUSED_RESULT;

  // Gets the complete list of all credentials.
  FormRetrievalResult GetAllLogins(PrimaryKeyToFormMap* key_to_form_map)
      WARN_UNUSED_RESULT;

  // Gets list of logins which match |signon_realm| and |username|.
  FormRetrievalResult GetLoginsBySignonRealmAndUsername(
      const std::string& signon_realm,
      const std::u16string& username,
      PrimaryKeyToFormMap& key_to_form_map) WARN_UNUSED_RESULT;

  // Gets the complete list of not blocklisted credentials.
  bool GetAutofillableLogins(std::vector<std::unique_ptr<PasswordForm>>* forms)
      WARN_UNUSED_RESULT;

  // Gets the complete list of blocklisted credentials.
  bool GetBlocklistLogins(std::vector<std::unique_ptr<PasswordForm>>* forms)
      WARN_UNUSED_RESULT;

  // Gets the list of auto-sign-inable credentials.
  bool GetAutoSignInLogins(PrimaryKeyToFormMap* key_to_form_map)
      WARN_UNUSED_RESULT;

  // Deletes the login database file on disk, and creates a new, empty database.
  // This can be used after migrating passwords to some other store, to ensure
  // that SQLite doesn't leave fragments of passwords in the database file.
  // Returns true on success; otherwise, whether the file was deleted and
  // whether further use of this login database will succeed is unspecified.
  bool DeleteAndRecreateDatabaseFile();

  bool IsEmpty();

  // On MacOS, it deletes all logins from the database that cannot be decrypted
  // when encryption key from Keychain is available. If the Keychain is locked,
  // it does nothing and returns ENCRYPTION_UNAVAILABLE. If it's not running on
  // MacOS, it does nothing and returns SUCCESS. This can be used when syncing
  // logins from the cloud to rewrite entries that can't be used anymore (due to
  // modification of the encryption key). If one of the logins couldn't be
  // removed from the database, returns ITEM_FAILURE.
  DatabaseCleanupResult DeleteUndecryptableLogins();

  // PasswordStoreSync::MetadataStore implementation.
  std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata() override;
  void DeleteAllSyncMetadata() override;
  bool UpdateSyncMetadata(syncer::ModelType model_type,
                          const std::string& storage_key,
                          const sync_pb::EntityMetadata& metadata) override;
  bool ClearSyncMetadata(syncer::ModelType model_type,
                         const std::string& storage_key) override;
  bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) override;
  bool ClearModelTypeState(syncer::ModelType model_type) override;
  void SetDeletionsHaveSyncedCallback(
      base::RepeatingCallback<void(bool)> callback) override;
  bool HasUnsyncedDeletions() override;

  // Callers that requires transaction support should call these methods to
  // begin, rollback and commit transactions. They delegate to the transaction
  // support of the underlying database. Only one transaction may exist at a
  // time.
  bool BeginTransaction();
  void RollbackTransaction();
  bool CommitTransaction();

  StatisticsTable& stats_table() { return stats_table_; }
  InsecureCredentialsTable& insecure_credentials_table() {
    return insecure_credentials_table_;
  }

  FieldInfoTable& field_info_table() { return field_info_table_; }

#if defined(OS_POSIX) && !defined(OS_APPLE)
  void enable_encryption() { use_encryption_ = true; }
  // This instance should not encrypt/decrypt password values using OSCrypt.
  void disable_encryption() { use_encryption_ = false; }
#endif  // defined(OS_POSIX) && !defined(OS_APPLE)

 private:
  struct PrimaryKeyAndPassword;
#if defined(OS_IOS)
  friend class LoginDatabaseIOSTest;
  FRIEND_TEST_ALL_PREFIXES(LoginDatabaseIOSTest, KeychainStorage);

  // Removes the keychain item corresponding to the look-up key |cipher_text|.
  // It's stored as the encrypted password value.
  static void DeleteEncryptedPasswordFromKeychain(
      const std::string& cipher_text);

  // On iOS, removes the keychain item that is used to store the encrypted
  // password for the supplied primary key |id|.
  void DeleteEncryptedPasswordById(int id);

  // Returns the encrypted password value for the specified |id|.  Returns an
  // empty string if the row for this |form| is not found.
  std::string GetEncryptedPasswordById(int id) const;
#endif

  // Returns a suffix (infix, really) to be used in histogram names to
  // differentiate the profile store from the account store.
  base::StringPiece GetMetricsSuffixForStore() const;

  void ReportNumberOfAccountsMetrics(bool custom_passphrase_sync_enabled);
  void ReportTimesPasswordUsedMetrics(bool custom_passphrase_sync_enabled);
  void ReportSyncingAccountStateMetrics(const std::string& sync_username);
  void ReportEmptyUsernamesMetrics();
  void ReportLoginsWithSchemesMetrics();
  void ReportBubbleSuppressionMetrics();
  void ReportInaccessiblePasswordsMetrics();
  void ReportDuplicateCredentialsMetrics();

  // Result values for encryption/decryption actions.
  enum EncryptionResult {
    // Success.
    ENCRYPTION_RESULT_SUCCESS,
    // Failure for a specific item (e.g., the encrypted value was manually
    // moved from another machine, and can't be decrypted on this machine).
    // This is presumed to be a permanent failure.
    ENCRYPTION_RESULT_ITEM_FAILURE,
    // A service-level failure (e.g., on a platform using a keyring, the keyring
    // is temporarily unavailable).
    // This is presumed to be a temporary failure.
    ENCRYPTION_RESULT_SERVICE_FAILURE,
  };

  // Encrypts plain_text, setting the value of cipher_text and returning true if
  // successful, or returning false and leaving cipher_text unchanged if
  // encryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  EncryptionResult EncryptedString(const std::u16string& plain_text,
                                   std::string* cipher_text) const
      WARN_UNUSED_RESULT;

  // Decrypts cipher_text, setting the value of plain_text and returning true if
  // successful, or returning false and leaving plain_text unchanged if
  // decryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  EncryptionResult DecryptedString(const std::string& cipher_text,
                                   std::u16string* plain_text) const
      WARN_UNUSED_RESULT;

  // Fills |form| from the values in the given statement (which is assumed to be
  // of the form used by the Get*Logins methods). Fills the corresponding DB
  // primary key in |primary_key|. If |decrypt_and_fill_password_value| is set
  // to true, it tries to decrypt the stored password and returns the
  // EncryptionResult from decrypting the password in |s|; if not
  // ENCRYPTION_RESULT_SUCCESS, |form| is not filled. If
  // |decrypt_and_fill_password_value| is set to false, it always returns
  // ENCRYPTION_RESULT_SUCCESS.
  EncryptionResult InitPasswordFormFromStatement(
      const sql::Statement& s,
      bool decrypt_and_fill_password_value,
      int* primary_key,
      PasswordForm* form) const WARN_UNUSED_RESULT;

  // Gets all blocklisted or all non-blocklisted (depending on |blocklisted|)
  // credentials. On success returns true and overwrites |forms| with the
  // result.
  bool GetAllLoginsWithBlocklistSetting(
      bool blocklisted,
      std::vector<std::unique_ptr<PasswordForm>>* forms);

  // Returns the DB primary key for the specified |form| and decrypted/encrypted
  // password. Returns {-1, "", ""} if the row for this |form| is not found.
  PrimaryKeyAndPassword GetPrimaryKeyAndPassword(
      const PasswordForm& form) const;

  // Reads all the stored sync entities metadata in a MetadataBatch. Returns
  // nullptr in case of failure.
  std::unique_ptr<syncer::MetadataBatch> GetAllSyncEntityMetadata();

  // Reads the stored ModelTypeState. Returns nullptr in case of failure.
  std::unique_ptr<sync_pb::ModelTypeState> GetModelTypeState();

  // Overwrites |key_to_form_map| with credentials retrieved from |statement|.
  // If |matched_form| is not null, filters out all results but those
  // PSL-matching
  // |*matched_form| or federated credentials for it. If feature for recovering
  // passwords is enabled, it removes all passwords that couldn't be decrypted
  // when encryption was available from the database. On success returns true.
  // |key_to_form_map| must not be null and will be used to return the results.
  // The key of the map is the DB primary key.
  FormRetrievalResult StatementToForms(
      sql::Statement* statement,
      const PasswordStore::FormDigest* matched_form,
      PrimaryKeyToFormMap* key_to_form_map) WARN_UNUSED_RESULT;

  // Initializes all the *_statement_ data members with appropriate SQL
  // fragments based on |builder|.
  void InitializeStatementStrings(const SQLTableBuilder& builder);

  // Sets the `in_store` member of `form` to either kProfileStore or
  // kAccountStore depending on the value of `is_account_store_`.
  void FillFormInStore(PasswordForm* form) const;

  const base::FilePath db_path_;
  const IsAccountStore is_account_store_;

  mutable sql::Database db_;
  sql::MetaTable meta_table_;
  StatisticsTable stats_table_;
  FieldInfoTable field_info_table_;
  InsecureCredentialsTable insecure_credentials_table_;

  // These cached strings are used to build SQL statements.
  std::string add_statement_;
  std::string add_replace_statement_;
  std::string update_statement_;
  std::string delete_statement_;
  std::string delete_by_id_statement_;
  std::string autosignin_statement_;
  std::string get_statement_;
  std::string get_statement_psl_;
  std::string get_statement_federated_;
  std::string get_statement_psl_federated_;
  std::string get_statement_username_;
  std::string created_statement_;
  std::string blocklisted_statement_;
  std::string encrypted_password_statement_by_id_;
  std::string id_and_password_statement_;

#if defined(OS_MAC)
  std::unique_ptr<PasswordRecoveryUtilMac> password_recovery_util_;
#endif

#if defined(OS_POSIX) && !defined(OS_APPLE)
  // Whether password values should be encrypted.
  // TODO(crbug.com/571003) Only linux doesn't use encryption. Remove this once
  // Linux is fully migrated into LoginDatabase.
  bool use_encryption_ = true;
#endif  // defined(OS_POSIX)

  // A callback to be invoked whenever all pending deletions have been processed
  // by Sync - see
  // PasswordStoreSync::MetadataStore::SetDeletionsHaveSyncedCallback for more
  // details.
  base::RepeatingCallback<void(bool)> deletions_have_synced_callback_;

  DISALLOW_COPY_AND_ASSIGN(LoginDatabase);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LOGIN_DATABASE_H_
