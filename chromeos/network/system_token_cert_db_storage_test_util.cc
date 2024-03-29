// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/system_token_cert_db_storage_test_util.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chromeos/network/system_token_cert_db_storage.h"
#include "net/cert/nss_cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

GetSystemTokenCertDbCallbackWrapper::GetSystemTokenCertDbCallbackWrapper() =
    default;
GetSystemTokenCertDbCallbackWrapper::~GetSystemTokenCertDbCallbackWrapper() =
    default;

SystemTokenCertDbStorage::GetDatabaseCallback
GetSystemTokenCertDbCallbackWrapper::GetCallback() {
  return base::BindOnce(&GetSystemTokenCertDbCallbackWrapper::OnDbRetrieved,
                        weak_ptr_factory_.GetWeakPtr());
}

void GetSystemTokenCertDbCallbackWrapper::Wait() {
  run_loop_.Run();
}

bool GetSystemTokenCertDbCallbackWrapper::IsCallbackCalled() const {
  return done_;
}

bool GetSystemTokenCertDbCallbackWrapper::IsDbRetrievalSucceeded() const {
  return nss_cert_database_ != nullptr;
}

void GetSystemTokenCertDbCallbackWrapper::OnDbRetrieved(
    net::NSSCertDatabase* nss_cert_database) {
  EXPECT_FALSE(done_);
  done_ = true;
  nss_cert_database_ = nss_cert_database;
  run_loop_.Quit();
}

}  // namespace chromeos
