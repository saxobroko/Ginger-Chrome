// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_UNMOUNT_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_UNMOUNT_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
// TODO(https://crbug.com/1164001): forward declare when moved ash
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "storage/browser/file_system/async_file_util.h"

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace ash {
namespace file_system_provider {
namespace operations {

// Bridge between fileManagerPrivate's unmount operation and providing
// extension's unmount request. Created per request.
class Unmount : public Operation {
 public:
  Unmount(extensions::EventRouter* event_router,
          const ProvidedFileSystemInfo& file_system_info,
          storage::AsyncFileUtil::StatusCallback callback);
  ~Unmount() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 std::unique_ptr<RequestValue> result,
                 bool has_more) override;
  void OnError(int request_id,
               std::unique_ptr<RequestValue> result,
               base::File::Error error) override;

 private:
  storage::AsyncFileUtil::StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(Unmount);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_UNMOUNT_H_
