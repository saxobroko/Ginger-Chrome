// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/test_profile_oauth2_token_service_delegate_chromeos.h"

#include <limits>

#include "ash/components/account_manager/account_manager_ash.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

TestProfileOAuth2TokenServiceDelegateChromeOS::
    TestProfileOAuth2TokenServiceDelegateChromeOS(
        AccountTrackerService* account_tracker_service,
        crosapi::AccountManagerAsh* account_manager_ash,
        bool is_regular_profile) {
  if (!network::TestNetworkConnectionTracker::HasInstance()) {
    owned_tracker_ = network::TestNetworkConnectionTracker::CreateInstance();
  }

  mojo::Remote<crosapi::mojom::AccountManager> remote;
  account_manager_ash->BindReceiver(remote.BindNewPipeAndPassReceiver());
  account_manager_facade_ =
      std::make_unique<account_manager::AccountManagerFacadeImpl>(
          std::move(remote),
          std::numeric_limits<uint32_t>::max() /* remote_version */);

  delegate_ = std::make_unique<ProfileOAuth2TokenServiceDelegateChromeOS>(
      account_tracker_service,
      network::TestNetworkConnectionTracker::GetInstance(),
      account_manager_facade_.get(), is_regular_profile);
  delegate_->AddObserver(this);
}

TestProfileOAuth2TokenServiceDelegateChromeOS::
    ~TestProfileOAuth2TokenServiceDelegateChromeOS() {
  delegate_->RemoveObserver(this);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
TestProfileOAuth2TokenServiceDelegateChromeOS::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer);
}

bool TestProfileOAuth2TokenServiceDelegateChromeOS::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  return delegate_->RefreshTokenIsAvailable(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  delegate_->UpdateAuthError(account_id, error);
}

GoogleServiceAuthError
TestProfileOAuth2TokenServiceDelegateChromeOS::GetAuthError(
    const CoreAccountId& account_id) const {
  return delegate_->GetAuthError(account_id);
}

std::vector<CoreAccountId>
TestProfileOAuth2TokenServiceDelegateChromeOS::GetAccounts() const {
  return delegate_->GetAccounts();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  // In tests |LoadCredentials| may be called twice, in this case we call
  // |FireRefreshTokensLoaded| again to notify that credentials are loaded.
  if (load_credentials_state() ==
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    FireRefreshTokensLoaded();
    return;
  }

  if (load_credentials_state() !=
      signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED) {
    return;
  }

  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);
  delegate_->LoadCredentials(primary_account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  delegate_->UpdateCredentials(account_id, refresh_token);
}

scoped_refptr<network::SharedURLLoaderFactory>
TestProfileOAuth2TokenServiceDelegateChromeOS::GetURLLoaderFactory() const {
  return delegate_->GetURLLoaderFactory();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::RevokeCredentials(
    const CoreAccountId& account_id) {
  delegate_->RevokeCredentials(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::RevokeAllCredentials() {
  delegate_->RevokeAllCredentials();
}

const net::BackoffEntry*
TestProfileOAuth2TokenServiceDelegateChromeOS::BackoffEntry() const {
  return delegate_->BackoffEntry();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  FireRefreshTokenAvailable(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  FireRefreshTokenRevoked(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnEndBatchChanges() {
  FireEndBatchChanges();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnRefreshTokensLoaded() {
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  FireRefreshTokensLoaded();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  FireAuthErrorChanged(account_id, auth_error);
}

}  // namespace signin
