// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_storage_helper.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread_restrictions.h"
#include "components/browsing_data/content/browsing_data_helper_browsertest.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace browsing_data {
namespace {

using TestCompletionCallback =
    BrowsingDataHelperCallback<content::StorageUsageInfo>;

const char kOrigin1[] = "http://www.chromium.org";
const char kOrigin2[] = "http://www.google.com";
// This is only here to test that state for non-web-storage schemes is not
// listed by the helper. Web storage schemes are http, https, file, ftp, ws,
// and wss.
const char kOrigin3[] = "chrome://settings";

bool PutTestData(blink::mojom::StorageArea* area) {
  base::RunLoop run_loop;
  bool success = false;
  area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, absl::nullopt, "source",
            base::BindLambdaForTesting([&](bool success_in) {
              run_loop.Quit();
              success = success_in;
            }));
  run_loop.Run();
  return success;
}

class LocalStorageHelperTest : public content::ContentBrowserTest {
 protected:
  storage::mojom::LocalStorageControl* GetLocalStorageControl() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetLocalStorageControl();
  }

  void CreateLocalStorageDataForTest() {
    for (const char* origin_str : {kOrigin1, kOrigin2, kOrigin3}) {
      mojo::Remote<blink::mojom::StorageArea> area;
      url::Origin origin = url::Origin::Create(GURL(origin_str));
      ASSERT_FALSE(origin.opaque());
      GetLocalStorageControl()->BindStorageArea(
          origin, area.BindNewPipeAndPassReceiver());
      ASSERT_TRUE(PutTestData(area.get()));
    }
  }
};

// This class is notified by LocalStorageHelper on the UI thread
// once it finishes fetching the local storage data.
class StopTestOnCallback {
 public:
  explicit StopTestOnCallback(LocalStorageHelper* local_storage_helper)
      : local_storage_helper_(local_storage_helper) {
    DCHECK(local_storage_helper_);
  }

  void Callback(
      const std::list<content::StorageUsageInfo>& local_storage_info) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // There's no guarantee on the order, ensure each of the two http origins
    // are there exactly once.
    ASSERT_EQ(2u, local_storage_info.size());
    bool origin1_found = false, origin2_found = false;
    for (const auto& info : local_storage_info) {
      if (info.origin.Serialize() == kOrigin1) {
        EXPECT_FALSE(origin1_found);
        origin1_found = true;
      } else {
        ASSERT_EQ(info.origin.Serialize(), kOrigin2);
        EXPECT_FALSE(origin2_found);
        origin2_found = true;
      }
    }
    EXPECT_TRUE(origin1_found);
    EXPECT_TRUE(origin2_found);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  LocalStorageHelper* local_storage_helper_;
};

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CallbackCompletes) {
  scoped_refptr<LocalStorageHelper> local_storage_helper(
      new LocalStorageHelper(shell()->web_contents()->GetBrowserContext()));
  CreateLocalStorageDataForTest();
  StopTestOnCallback stop_test_on_callback(local_storage_helper.get());
  local_storage_helper->StartFetching(base::BindOnce(
      &StopTestOnCallback::Callback, base::Unretained(&stop_test_on_callback)));
  // Blocks until StopTestOnCallback::Callback is notified.
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, DeleteSingleOrigin) {
  scoped_refptr<LocalStorageHelper> local_storage_helper(
      new LocalStorageHelper(shell()->web_contents()->GetBrowserContext()));
  CreateLocalStorageDataForTest();
  base::RunLoop delete_run_loop;
  local_storage_helper->DeleteOrigin(url::Origin::Create(GURL(kOrigin1)),
                                     delete_run_loop.QuitClosure());
  delete_run_loop.Run();

  // Ensure the origin has been deleted, but other origins are intact.
  std::vector<storage::mojom::StorageUsageInfoPtr> usage_infos;
  base::RunLoop get_usage_run_loop;
  GetLocalStorageControl()->GetUsage(base::BindLambdaForTesting(
      [&](std::vector<storage::mojom::StorageUsageInfoPtr> usage_infos_in) {
        usage_infos.swap(usage_infos_in);
        get_usage_run_loop.Quit();
      }));
  get_usage_run_loop.Run();

  // There's no guarantee on the order, ensure each of the two non-deleted
  // origins are there exactly once.
  ASSERT_EQ(2u, usage_infos.size());
  bool origin2_found = false, origin3_found = false;
  for (const auto& info : usage_infos) {
    if (info->origin.Serialize() == kOrigin2) {
      EXPECT_FALSE(origin2_found);
      origin2_found = true;
    } else {
      ASSERT_EQ(info->origin.Serialize(), kOrigin3);
      EXPECT_FALSE(origin3_found);
      origin3_found = true;
    }
  }
  EXPECT_TRUE(origin2_found);
  EXPECT_TRUE(origin3_found);
}

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CannedAddLocalStorage) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");

  scoped_refptr<CannedLocalStorageHelper> helper(new CannedLocalStorageHelper(
      shell()->web_contents()->GetBrowserContext()));
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));

  TestCompletionCallback callback;
  helper->StartFetching(base::BindOnce(&TestCompletionCallback::callback,
                                       base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(2u, result.size());
  auto info = result.begin();
  EXPECT_EQ(origin1, info->origin.GetURL());
  info++;
  EXPECT_EQ(origin2, info->origin.GetURL());
}

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");

  scoped_refptr<CannedLocalStorageHelper> helper(new CannedLocalStorageHelper(
      shell()->web_contents()->GetBrowserContext()));
  helper->Add(url::Origin::Create(origin));
  helper->Add(url::Origin::Create(origin));

  TestCompletionCallback callback;
  helper->StartFetching(base::BindOnce(&TestCompletionCallback::callback,
                                       base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(origin, result.begin()->origin.GetURL());
}

}  // namespace
}  // namespace browsing_data
