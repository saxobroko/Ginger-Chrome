// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace blink {

ResourceCacheKey::ResourceCacheKey(const IntSize& size,
                                   const CanvasResourceParams& params,
                                   bool is_origin_top_left)
    : size(size), params(params), is_origin_top_left(is_origin_top_left) {}

bool ResourceCacheKey::operator==(const ResourceCacheKey& other) const {
  return (size == other.size &&
          params.ColorSpace() == other.params.ColorSpace() &&
          params.GetSkColorType() == other.params.GetSkColorType() &&
          params.GetSkAlphaType() == other.params.GetSkAlphaType() &&
          is_origin_top_left == other.is_origin_top_left);
}

bool ResourceCacheKey::operator!=(const ResourceCacheKey& other) const {
  return !(*this == other);
}

RecyclableCanvasResource::RecyclableCanvasResource(
    std::unique_ptr<CanvasResourceProvider> resource_provider,
    const ResourceCacheKey& cache_key,
    base::WeakPtr<WebGPURecyclableResourceCache> cache)
    : resource_provider_(std::move(resource_provider)),
      cache_key_(cache_key),
      cache_(cache) {}

RecyclableCanvasResource::~RecyclableCanvasResource() {
  if (!resource_provider_)
    return;

  // If the cache key is converted to a different value in
  // CanvasResourceProvider creation, it will cause cache miss, such as
  // kBGRA_8888_SkColorType to kRGBA_8888_SkColorType.
  // TODO(magchen@):Remove the DCHECKs if we must create CanvasResourceProvider
  // with unsupported parameters and if it's fine to lose the cache. Or, we can
  // save the cache key in |unused_providers_| and only compare the saved cache
  // key instead of the one in CanvasResourceProvider.
  DCHECK(cache_key_.size == resource_provider_->Size());
  DCHECK(cache_key_.params.ColorSpace() ==
         resource_provider_->ColorParams().ColorSpace());
  DCHECK(cache_key_.params.GetSkColorType() ==
         resource_provider_->ColorParams().GetSkColorType());
  DCHECK(cache_key_.params.GetSkAlphaType() ==
         resource_provider_->ColorParams().GetSkAlphaType());
  DCHECK(cache_key_.is_origin_top_left ==
         resource_provider_->IsOriginTopLeft());

  if (cache_) {
    cache_->OnDestroyRecyclableResource(std::move(resource_provider_));
  }
}

WebGPURecyclableResourceCache::WebGPURecyclableResourceCache(
    gpu::webgpu::WebGPUInterface* webgpu_interface,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : webgpu_interface_(webgpu_interface),
      task_runner_(std::move(task_runner)) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  timer_func_ = WTF::BindRepeating(
      &WebGPURecyclableResourceCache::ReleaseStaleResources, weak_ptr_);

  DCHECK_LE(kTimerDurationInSeconds, kCleanUpDelayInSeconds);
}

std::unique_ptr<RecyclableCanvasResource>
WebGPURecyclableResourceCache::GetOrCreateCanvasResource(
    const IntSize& size,
    const CanvasResourceParams& params,
    bool is_origin_top_left) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const ResourceCacheKey cache_key(size, params, is_origin_top_left);
  std::unique_ptr<CanvasResourceProvider> provider =
      AcquireCachedProvider(cache_key);
  if (!provider) {
    provider = CanvasResourceProvider::CreateWebGPUImageProvider(
        size, params, is_origin_top_left);
    if (!provider)
      return nullptr;
  }

  return std::make_unique<RecyclableCanvasResource>(std::move(provider),
                                                    cache_key, weak_ptr_);
}

void WebGPURecyclableResourceCache::OnDestroyRecyclableResource(
    std::unique_ptr<CanvasResourceProvider> resource_provider) {
  int resource_size = resource_provider->Size().Width() *
                      resource_provider->Size().Height() *
                      resource_provider->ColorParams().BytesPerPixel();
  total_unused_resources_in_bytes_ += resource_size;

  // WaitSyncToken on the canvas resource.
  gpu::SyncToken finished_access_token;
  webgpu_interface_->GenUnverifiedSyncTokenCHROMIUM(
      finished_access_token.GetData());
  resource_provider->OnDestroyRecyclableCanvasResource(finished_access_token);

  unused_providers_.push_front(
      Resource(std::move(resource_provider), current_timer_id_, resource_size));

  if (last_seen_max_unused_resources_in_bytes_ <
      total_unused_resources_in_bytes_) {
    last_seen_max_unused_resources_in_bytes_ = total_unused_resources_in_bytes_;
  }
  if (last_seen_max_unused_resources_ < unused_providers_.size()) {
    last_seen_max_unused_resources_ = unused_providers_.size();
  }

  // If the cache is full, release LRU from the back.
  while (total_unused_resources_in_bytes_ >
         kMaxRecyclableResourceCachesInBytes) {
    total_unused_resources_in_bytes_ -= unused_providers_.back().resource_size_;
    unused_providers_.pop_back();
  }

  StartResourceCleanUpTimer();
}

std::unique_ptr<CanvasResourceProvider>
WebGPURecyclableResourceCache::AcquireCachedProvider(
    const ResourceCacheKey& cache_key) {
  // Loop from MRU to LRU
  DequeResourceProvider::iterator it;
  for (it = unused_providers_.begin(); it != unused_providers_.end(); ++it) {
    CanvasResourceProvider* resource_provider = it->resource_provider_.get();
    const auto it_cache_key = ResourceCacheKey(
        resource_provider->Size(), resource_provider->ColorParams(),
        resource_provider->IsOriginTopLeft());

    if (cache_key == it_cache_key) {
      break;
    }
  }

  // Found one.
  if (it != unused_providers_.end()) {
    std::unique_ptr<CanvasResourceProvider> provider =
        (std::move(it->resource_provider_));
    total_unused_resources_in_bytes_ -= it->resource_size_;
    // TODO(magchen@): If the cache capacity increases a lot, will erase(it)
    // becomes inefficient?
    // Remove the provider from the |unused_providers_|.
    unused_providers_.erase(it);
    provider->OnAcquireRecyclableCanvasResource();

    return provider;
  }
  return nullptr;
}

void WebGPURecyclableResourceCache::ReleaseStaleResources() {
  timer_is_running_ = false;

  // Loop from LRU to MRU
  int stale_resource_count = 0;
  for (auto it = unused_providers_.rbegin(); it != unused_providers_.rend();
       ++it) {
    auto timer_id_ = it->timer_id_;
    int delta;
    delta = current_timer_id_ - timer_id_;
    if ((current_timer_id_ - it->timer_id_) < kTimerIdDeltaForDeletion) {
      // These are the resources which are recycled and stay in the cache for
      // less than kCleanUpDelayInSeconds. They are not to be deleted this time.
      break;
    }
    stale_resource_count++;
  }

  // Delete all stale resources.
  for (int i = 0; i < stale_resource_count; ++i) {
    total_unused_resources_in_bytes_ -= unused_providers_.back().resource_size_;
    unused_providers_.pop_back();
  }

  // The number of stale Resources released this time in this function.
  base::UmaHistogramCustomCounts("Blink.Canvas.WebGPUStaleResourceCount",
                                 stale_resource_count, /*min=*/0,
                                 /*max=*/300,
                                 /*buckets=*/50);

  // The maximum number of total resource memory size between two
  // ReleaseStaleResources() calls.
  // UmaHistogramCustomCounts only takes the int type as input.
  int last_seen_max_unused_resources_in_kb =
      static_cast<int>(last_seen_max_unused_resources_in_bytes_ / 1024);
  base::UmaHistogramCustomCounts("Blink.Canvas.WebGPUMaxRecycledResourcesInKB",
                                 last_seen_max_unused_resources_in_kb,
                                 /*min=*/0,
                                 /*max=*/kMaxRecyclableResourceCachesInKB,
                                 /*buckets=*/50);
  last_seen_max_unused_resources_in_bytes_ = 0;

  // The maximum number of unused resources between two ReleaseStaleResources()
  // calls.
  base::UmaHistogramCustomCounts("Blink.Canvas.WebGPUMaxRecycledResourcesCount",
                                 last_seen_max_unused_resources_,
                                 /*min=*/0,
                                 /*max=*/300,
                                 /*buckets=*/50);
  last_seen_max_unused_resources_ = 0;

  current_timer_id_++;
  StartResourceCleanUpTimer();
}
void WebGPURecyclableResourceCache::StartResourceCleanUpTimer() {
  if (unused_providers_.size() > 0 && !timer_is_running_) {
    task_runner_->PostDelayedTask(
        FROM_HERE, timer_func_,
        base::TimeDelta::FromSeconds(kTimerDurationInSeconds));
    timer_is_running_ = true;
  }
}

wtf_size_t
WebGPURecyclableResourceCache::CleanUpResourcesAndReturnSizeForTesting() {
  ReleaseStaleResources();
  return unused_providers_.size();
}

}  // namespace blink
