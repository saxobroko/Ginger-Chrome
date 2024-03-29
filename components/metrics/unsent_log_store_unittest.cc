// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

const char kTestPrefName[] = "TestPref";
const char kTestMetaDataPrefName[] = "TestMetaDataPref";

const size_t kLogCountLimit = 3;
const size_t kLogByteLimit = 1000;

// Compresses |log_data| and returns the result.
std::string Compress(const std::string& log_data) {
  std::string compressed_log_data;
  EXPECT_TRUE(compression::GzipCompress(log_data, &compressed_log_data));
  return compressed_log_data;
}

// Generates and returns log data such that its size after compression is at
// least |min_compressed_size|.
std::string GenerateLogWithMinCompressedSize(size_t min_compressed_size) {
  // Since the size check is done against a compressed log, generate enough
  // data that compresses to larger than |log_size|.
  std::string rand_bytes = base::RandBytesAsString(min_compressed_size);
  while (Compress(rand_bytes).size() < min_compressed_size)
    rand_bytes.append(base::RandBytesAsString(min_compressed_size));
  std::string base64_data_for_logging;
  base::Base64Encode(rand_bytes, &base64_data_for_logging);
  SCOPED_TRACE(testing::Message() << "Using random data "
                                  << base64_data_for_logging);
  return rand_bytes;
}

class UnsentLogStoreTest : public testing::Test {
 public:
  UnsentLogStoreTest() {
    prefs_.registry()->RegisterListPref(kTestPrefName);
    prefs_.registry()->RegisterDictionaryPref(kTestMetaDataPrefName);
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnsentLogStoreTest);
};

class TestUnsentLogStoreMetrics : public UnsentLogStoreMetrics {
 public:
  TestUnsentLogStoreMetrics() = default;

  void RecordLastUnsentLogMetadataMetrics(int unsent_samples_count,
                                          int sent_samples_count,
                                          int persisted_size_in_kb) override {
    unsent_samples_count_ = unsent_samples_count;
    sent_samples_count_ = sent_samples_count;
    persisted_size_in_kb_ = persisted_size_in_kb;
  }

  int unsent_samples_count() const { return unsent_samples_count_; }
  int sent_samples_count() const { return sent_samples_count_; }
  int persisted_size_in_kb() const { return persisted_size_in_kb_; }

 private:
  int unsent_samples_count_ = 0;
  int sent_samples_count_ = 0;
  int persisted_size_in_kb_ = 0;
};

class TestUnsentLogStore : public UnsentLogStore {
 public:
  TestUnsentLogStore(PrefService* service, size_t min_log_bytes)
      : UnsentLogStore(std::make_unique<UnsentLogStoreMetricsImpl>(),
                       service,
                       kTestPrefName,
                       nullptr,
                       kLogCountLimit,
                       min_log_bytes,
                       /* max_log_size= */ 0,
                       std::string()) {}
  TestUnsentLogStore(PrefService* service,
                     size_t min_log_bytes,
                     const std::string& signing_key)
      : UnsentLogStore(std::make_unique<UnsentLogStoreMetricsImpl>(),
                       service,
                       kTestPrefName,
                       nullptr,
                       kLogCountLimit,
                       min_log_bytes,
                       /* max_log_size = */ 0,
                       signing_key) {}
  TestUnsentLogStore(std::unique_ptr<UnsentLogStoreMetrics> metrics,
                     PrefService* service,
                     size_t max_log_size)
      : UnsentLogStore(std::move(metrics),
                       service,
                       kTestPrefName,
                       kTestMetaDataPrefName,
                       kLogCountLimit,
                       /* min_log_bytes= */ 1,
                       max_log_size,
                       std::string()) {}

  // Stages and removes the next log, while testing it's value.
  void ExpectNextLog(const std::string& expected_log) {
    StageNextLog();
    EXPECT_EQ(staged_log(), Compress(expected_log));
    DiscardStagedLog();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestUnsentLogStore);
};

}  // namespace

// Store and retrieve empty list_value.
TEST_F(UnsentLogStoreTest, EmptyLogList) {
  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);

  unsent_log_store.TrimAndPersistUnsentLogs();
  const base::ListValue* list_value = prefs_.GetList(kTestPrefName);
  EXPECT_EQ(0U, list_value->GetSize());

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  EXPECT_EQ(0U, result_unsent_log_store.size());
}

// Store and retrieve a single log value.
TEST_F(UnsentLogStoreTest, SingleElementLogList) {
  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);

  unsent_log_store.StoreLog("Hello world!", absl::nullopt);
  unsent_log_store.TrimAndPersistUnsentLogs();

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  EXPECT_EQ(1U, result_unsent_log_store.size());

  // Verify that the result log matches the initial log.
  unsent_log_store.StageNextLog();
  result_unsent_log_store.StageNextLog();
  EXPECT_EQ(unsent_log_store.staged_log(),
            result_unsent_log_store.staged_log());
  EXPECT_EQ(unsent_log_store.staged_log_hash(),
            result_unsent_log_store.staged_log_hash());
  EXPECT_EQ(unsent_log_store.staged_log_signature(),
            result_unsent_log_store.staged_log_signature());
  EXPECT_EQ(unsent_log_store.staged_log_timestamp(),
            result_unsent_log_store.staged_log_timestamp());
}

// Store a set of logs over the length limit, but smaller than the min number of
// bytes. This should leave the logs unchanged.
TEST_F(UnsentLogStoreTest, LongButTinyLogList) {
  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);

  size_t log_count = kLogCountLimit * 5;
  for (size_t i = 0; i < log_count; ++i)
    unsent_log_store.StoreLog("x", absl::nullopt);

  EXPECT_EQ(log_count, unsent_log_store.size());
  unsent_log_store.TrimAndPersistUnsentLogs();
  EXPECT_EQ(log_count, unsent_log_store.size());

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  EXPECT_EQ(unsent_log_store.size(), result_unsent_log_store.size());

  result_unsent_log_store.ExpectNextLog("x");
}

// Store a set of logs over the length limit, but that doesn't reach the minimum
// number of bytes until after passing the length limit.
TEST_F(UnsentLogStoreTest, LongButSmallLogList) {
  size_t log_count = kLogCountLimit * 5;
  size_t log_size = 50;

  std::string first_kept = "First to keep";
  first_kept.resize(log_size, ' ');

  std::string blank_log = std::string(log_size, ' ');

  std::string last_kept = "Last to keep";
  last_kept.resize(log_size, ' ');

  // Set the byte limit enough to keep everything but the first two logs.
  const size_t min_log_bytes =
      Compress(first_kept).length() + Compress(last_kept).length() +
      (log_count - 4) * Compress(blank_log).length();
  TestUnsentLogStore unsent_log_store(&prefs_, min_log_bytes);

  unsent_log_store.StoreLog("one", absl::nullopt);
  unsent_log_store.StoreLog("two", absl::nullopt);
  unsent_log_store.StoreLog(first_kept, absl::nullopt);
  for (size_t i = unsent_log_store.size(); i < log_count - 1; ++i) {
    unsent_log_store.StoreLog(blank_log, absl::nullopt);
  }
  unsent_log_store.StoreLog(last_kept, absl::nullopt);

  size_t original_size = unsent_log_store.size();
  unsent_log_store.TrimAndPersistUnsentLogs();
  // New size has been reduced.
  EXPECT_EQ(original_size - 2, unsent_log_store.size());

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  // Prefs should be the same size.
  EXPECT_EQ(unsent_log_store.size(), result_unsent_log_store.size());

  result_unsent_log_store.ExpectNextLog(last_kept);
  while (result_unsent_log_store.size() > 1) {
    result_unsent_log_store.ExpectNextLog(blank_log);
  }
  result_unsent_log_store.ExpectNextLog(first_kept);
}

// Store a set of logs within the length limit, but well over the minimum
// number of bytes. This should leave the logs unchanged.
TEST_F(UnsentLogStoreTest, ShortButLargeLogList) {
  // Make the total byte count about twice the minimum.
  size_t log_count = kLogCountLimit;
  size_t log_size = (kLogByteLimit / log_count) * 2;
  std::string log_data = GenerateLogWithMinCompressedSize(log_size);

  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);
  for (size_t i = 0; i < log_count; ++i) {
    unsent_log_store.StoreLog(log_data, absl::nullopt);
  }
  unsent_log_store.TrimAndPersistUnsentLogs();

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  // Both have expected number of logs (original amount).
  EXPECT_EQ(kLogCountLimit, unsent_log_store.size());
  EXPECT_EQ(kLogCountLimit, result_unsent_log_store.size());
}

// Store a set of logs over the length limit, and over the minimum number of
// bytes. This will trim the set of logs.
TEST_F(UnsentLogStoreTest, LongAndLargeLogList) {
  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);

  // Include twice the max number of logs.
  size_t log_count = kLogCountLimit * 2;
  // Make the total byte count about four times the minimum.
  size_t log_size = (kLogByteLimit / log_count) * 4;

  std::string target_log = "First to keep";
  target_log += GenerateLogWithMinCompressedSize(log_size);

  std::string log_data = GenerateLogWithMinCompressedSize(log_size);
  for (size_t i = 0; i < log_count; ++i) {
    if (i == log_count - kLogCountLimit)
      unsent_log_store.StoreLog(target_log, absl::nullopt);
    else
      unsent_log_store.StoreLog(log_data, absl::nullopt);
  }

  unsent_log_store.TrimAndPersistUnsentLogs();

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  // Both original log and persisted are reduced to limit.
  EXPECT_EQ(kLogCountLimit, unsent_log_store.size());
  EXPECT_EQ(kLogCountLimit, result_unsent_log_store.size());

  while (result_unsent_log_store.size() > 1) {
    result_unsent_log_store.ExpectNextLog(log_data);
  }
  result_unsent_log_store.ExpectNextLog(target_log);
}

// Check that the store/stage/discard functions work as expected.
TEST_F(UnsentLogStoreTest, Staging) {
  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);
  std::string tmp;

  EXPECT_FALSE(unsent_log_store.has_staged_log());
  unsent_log_store.StoreLog("one", absl::nullopt);
  EXPECT_FALSE(unsent_log_store.has_staged_log());
  unsent_log_store.StoreLog("two", absl::nullopt);
  unsent_log_store.StageNextLog();
  EXPECT_TRUE(unsent_log_store.has_staged_log());
  EXPECT_EQ(unsent_log_store.staged_log(), Compress("two"));
  unsent_log_store.StoreLog("three", absl::nullopt);
  EXPECT_EQ(unsent_log_store.staged_log(), Compress("two"));
  EXPECT_EQ(unsent_log_store.size(), 3U);
  unsent_log_store.DiscardStagedLog();
  EXPECT_FALSE(unsent_log_store.has_staged_log());
  EXPECT_EQ(unsent_log_store.size(), 2U);
  unsent_log_store.StageNextLog();
  EXPECT_EQ(unsent_log_store.staged_log(), Compress("three"));
  unsent_log_store.DiscardStagedLog();
  unsent_log_store.StageNextLog();
  EXPECT_EQ(unsent_log_store.staged_log(), Compress("one"));
  unsent_log_store.DiscardStagedLog();
  EXPECT_FALSE(unsent_log_store.has_staged_log());
  EXPECT_EQ(unsent_log_store.size(), 0U);
}

TEST_F(UnsentLogStoreTest, DiscardOrder) {
  // Ensure that the correct log is discarded if new logs are pushed while
  // a log is staged.
  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);

  unsent_log_store.StoreLog("one", absl::nullopt);
  unsent_log_store.StageNextLog();
  unsent_log_store.StoreLog("two", absl::nullopt);
  unsent_log_store.DiscardStagedLog();
  unsent_log_store.TrimAndPersistUnsentLogs();

  TestUnsentLogStore result_unsent_log_store(&prefs_, kLogByteLimit);
  result_unsent_log_store.LoadPersistedUnsentLogs();
  EXPECT_EQ(1U, result_unsent_log_store.size());
  result_unsent_log_store.ExpectNextLog("two");
}


TEST_F(UnsentLogStoreTest, Hashes) {
  const char kFooText[] = "foo";
  const std::string foo_hash = base::SHA1HashString(kFooText);

  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);
  unsent_log_store.StoreLog(kFooText, absl::nullopt);
  unsent_log_store.StageNextLog();

  EXPECT_EQ(Compress(kFooText), unsent_log_store.staged_log());
  EXPECT_EQ(foo_hash, unsent_log_store.staged_log_hash());
}

TEST_F(UnsentLogStoreTest, Signatures) {
  const char kFooText[] = "foo";

  TestUnsentLogStore unsent_log_store(&prefs_, kLogByteLimit);
  unsent_log_store.StoreLog(kFooText, absl::nullopt);
  unsent_log_store.StageNextLog();

  EXPECT_EQ(Compress(kFooText), unsent_log_store.staged_log());

  // The expected signature as a base 64 encoded string. The value was obtained
  // by running the test with an empty expected_signature_base64 and taking the
  // actual value from the test failure message. Can be verifying by the
  // following python code:
  // import hmac, hashlib, base64
  // key = ''
  // print(base64.b64encode(
  //   hmac.new(key, msg='foo', digestmod=hashlib.sha256).digest()).decode())
  std::string expected_signature_base64 =
      "DA2Y9+PZ1F5y6Id7wbEEMn77nAexjy/+ztdtgTB/H/8=";

  std::string actual_signature_base64;
  base::Base64Encode(unsent_log_store.staged_log_signature(),
                     &actual_signature_base64);
  EXPECT_EQ(expected_signature_base64, actual_signature_base64);

  // Test a different key results in a different signature.
  std::string key = "secret key, don't tell anyone";
  TestUnsentLogStore unsent_log_store_different_key(&prefs_, kLogByteLimit,
    key);

  unsent_log_store_different_key.StoreLog(kFooText, absl::nullopt);
  unsent_log_store_different_key.StageNextLog();

  EXPECT_EQ(Compress(kFooText), unsent_log_store_different_key.staged_log());

  // Base 64 encoded signature obtained in similar fashion to previous
  // signature. To use previous python code change:
  // key = "secret key, don't tell anyone"
  expected_signature_base64 = "DV7z8wdDrjLkQrCzrXR3UjWsR3/YVM97tIhMnhUvfXM=";
  base::Base64Encode(unsent_log_store_different_key.staged_log_signature(),
                     &actual_signature_base64);

  EXPECT_EQ(expected_signature_base64, actual_signature_base64);
}

TEST_F(UnsentLogStoreTest, UnsentLogMetadataMetrics) {
  std::unique_ptr<TestUnsentLogStoreMetrics> metrics =
      std::make_unique<TestUnsentLogStoreMetrics>();
  TestUnsentLogStoreMetrics* m = metrics.get();
  TestUnsentLogStore unsent_log_store(std::move(metrics), &prefs_,
                                      kLogByteLimit * 10);

  // Prepare 4 logs.
  const char kFooText[] = "foo";
  const base::HistogramBase::Count kFooSampleCount = 3;

  // The |foobar_log| whose compressed size is over 1kb will be staged first, so
  // the persisted_size_in_kb shall be reduced by 1kb afterwards.
  std::string foobar_log = GenerateLogWithMinCompressedSize(1024);
  const base::HistogramBase::Count kFooBarSampleCount = 5;

  // The |oversize_log| shall not be persisted.
  std::string oversize_log =
      GenerateLogWithMinCompressedSize(kLogByteLimit * 10 + 1);
  const base::HistogramBase::Count kOversizeLogSampleCount = 50;

  // The log without the SampleCount will not be counted to metrics.
  const char kNoSampleLog[] = "no sample log";

  unsent_log_store.StoreLog(
      oversize_log,
      absl::make_optional<base::HistogramBase::Count>(kOversizeLogSampleCount));
  unsent_log_store.StoreLog(kNoSampleLog, absl::nullopt);
  unsent_log_store.StoreLog(
      kFooText, absl::optional<base::HistogramBase::Count>(kFooSampleCount));
  // The foobar_log will be staged first.
  unsent_log_store.StoreLog(
      foobar_log,
      absl::optional<base::HistogramBase::Count>(kFooBarSampleCount));

  unsent_log_store.TrimAndPersistUnsentLogs();

  unsent_log_store.RecordMetaDataMertics();
  // The |oversize_log| was ignored, the kNoSampleLog won't be counted to
  // metrics,
  EXPECT_EQ(kFooSampleCount + kFooBarSampleCount, m->unsent_samples_count());
  EXPECT_EQ(0, m->sent_samples_count());
  EXPECT_EQ(2, m->persisted_size_in_kb());

  // Pretend to send log.
  unsent_log_store.StageNextLog();
  unsent_log_store.MarkStagedLogAsSent();
  unsent_log_store.DiscardStagedLog();
  unsent_log_store.TrimAndPersistUnsentLogs();
  unsent_log_store.RecordMetaDataMertics();

  // The |foobar_log| shall be sent.
  EXPECT_EQ(kFooSampleCount, m->unsent_samples_count());
  EXPECT_EQ(kFooBarSampleCount, m->sent_samples_count());
  EXPECT_EQ(1, m->persisted_size_in_kb());

  // Pretend |kFooText| upload failure.
  unsent_log_store.StageNextLog();
  unsent_log_store.DiscardStagedLog();
  unsent_log_store.TrimAndPersistUnsentLogs();
  unsent_log_store.RecordMetaDataMertics();

  // Verify the failed upload wasn't added to the sent samples count.
  EXPECT_EQ(0, m->unsent_samples_count());
  EXPECT_EQ(kFooBarSampleCount, m->sent_samples_count());
  EXPECT_EQ(1, m->persisted_size_in_kb());
}

}  // namespace metrics
