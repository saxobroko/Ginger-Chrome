// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CODE_CACHE_GENERATED_CODE_CACHE_CONTEXT_H_
#define CONTENT_BROWSER_CODE_CACHE_GENERATED_CODE_CACHE_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class GeneratedCodeCache;

// One instance exists per disk-backed (non in-memory) storage contexts. This
// owns the instance of GeneratedCodeCache that is used to store the data
// generated by the renderer (for ex: code caches for script resources).
// The instance of this class (|this|) is constructed and lives on the
// UI thread. All methods must be called on UI thread.
class CONTENT_EXPORT GeneratedCodeCacheContext
    : public base::RefCountedThreadSafe<GeneratedCodeCacheContext> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  GeneratedCodeCacheContext();

  // Initialize is called on the UI thread when the StoragePartition is
  // being setup.
  void Initialize(const base::FilePath& path, int max_bytes);

  void Shutdown();

  GeneratedCodeCache* generated_js_code_cache() const;
  GeneratedCodeCache* generated_wasm_code_cache() const;

 private:
  friend class base::RefCountedThreadSafe<GeneratedCodeCacheContext>;
  ~GeneratedCodeCacheContext();

  // Created, used and deleted on the UI thread.
  std::unique_ptr<GeneratedCodeCache> generated_js_code_cache_;
  std::unique_ptr<GeneratedCodeCache> generated_wasm_code_cache_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedCodeCacheContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CODE_CACHE_GENERATED_CODE_CACHE_CONTEXT_H_
