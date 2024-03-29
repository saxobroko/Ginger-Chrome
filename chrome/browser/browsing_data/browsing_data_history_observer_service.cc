// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"

#include "base/callback_helpers.h"
#include "chrome/browser/browsing_data/navigation_entry_remover.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/special_storage_policy.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

namespace {

// This method takes in parameter |urls|, which is a map,
// {Origin -> {Count, LastVisitTime}}, keyed by origin to the
// count of its URLs in the history and its last visit time.
// Here, we iterate over the map to return the set of origins
// which doesn't have any of its urls present in the history.
// A flat_set is returned for efficient lookups
// in the Contains method below. The insertion
// time is O(n.logn) as we invoke the move
// constructor with std::vector.
base::flat_set<GURL> GetDeletedOrigins(
    const history::OriginCountAndLastVisitMap& urls) {
  std::vector<GURL> deleted_origins;
  // Iterating over the map {Origin -> {Count, LastVisitTime}}.
  for (const auto& key_value : urls) {
    // If the Count is greater than 0, it means few of the origin's URL(s) are
    // still in the history, so we shouldn't mark the origin for deletion.
    if (key_value.second.first > 0)
      continue;
    deleted_origins.push_back(key_value.first);
  }
  return base::flat_set<GURL>(std::move(deleted_origins));
}

bool Contains(const base::flat_set<GURL>& deleted_origins,
              const GURL& template_gurl) {
  return deleted_origins.contains(template_gurl.GetOrigin());
}

void DeleteTemplateUrlsForTimeRange(TemplateURLService* keywords_model,
                                    base::Time delete_begin,
                                    base::Time delete_end) {
  if (!keywords_model->loaded()) {
    keywords_model->RegisterOnLoadedCallback(
        base::BindOnce(&DeleteTemplateUrlsForTimeRange, keywords_model,
                       delete_begin, delete_end));
    keywords_model->Load();
    return;
  }

  keywords_model->RemoveAutoGeneratedBetween(delete_begin, delete_end);
}

void DeleteTemplateUrlsForDeletedOrigins(TemplateURLService* keywords_model,
                                         base::flat_set<GURL> deleted_origins) {
  if (!keywords_model->loaded()) {
    keywords_model->RegisterOnLoadedCallback(
        base::BindOnce(&DeleteTemplateUrlsForDeletedOrigins, keywords_model,
                       std::move(deleted_origins)));
    keywords_model->Load();
    return;
  }

  keywords_model->RemoveAutoGeneratedForUrlsBetween(
      base::BindRepeating(&Contains, std::move(deleted_origins)),
      base::Time::Min(), base::Time::Max());
}

bool DoesOriginMatchPredicate(
    base::OnceCallback<bool(const url::Origin&)> predicate,
    const url::Origin& origin,
    storage::SpecialStoragePolicy* policy) {
  if (!std::move(predicate).Run(origin))
    return false;

  if (policy && policy->IsStorageProtected(origin.GetURL()))
    return false;

  return true;
}

void DeleteStoragePartitionDataWithFilter(
    content::StoragePartition* storage_partition,
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
    base::Time delete_begin,
    base::Time delete_end) {
  content::StoragePartition::OriginMatcherFunction matcher_function =
      filter_builder ? base::BindRepeating(&DoesOriginMatchPredicate,
                                           filter_builder->BuildOriginFilter())
                     : base::NullCallback();

  const uint32_t removal_mask =
      content::StoragePartition::REMOVE_DATA_MASK_CONVERSIONS;
  const uint32_t quota_removal_mask = 0;
  storage_partition->ClearData(
      removal_mask, quota_removal_mask, std::move(matcher_function),
      nullptr /* cookie_deletion_filter */, false /* perform_storage_cleanup */,
      delete_begin, delete_end, base::DoNothing());
}

void DeleteStoragePartitionDataForTimeRange(
    content::StoragePartition* storage_partition,
    base::Time delete_begin,
    base::Time delete_end,
    const absl::optional<std::set<GURL>>& urls) {
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder;
  if (urls) {
    filter_builder = content::BrowsingDataFilterBuilder::Create(
        content::BrowsingDataFilterBuilder::Mode::kDelete);
    for (const auto& url : *urls) {
      url::Origin origin = url::Origin::Create(url);
      if (!origin.opaque())
        filter_builder->AddOrigin(std::move(origin));
    }
  }

  DeleteStoragePartitionDataWithFilter(
      storage_partition, std::move(filter_builder), delete_begin, delete_end);
}

void DeleteStoragePartitionDataForDeletedOrigins(
    content::StoragePartition* storage_partition,
    const base::flat_set<GURL>& deleted_origins) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  for (const auto& url : deleted_origins) {
    url::Origin origin = url::Origin::Create(url);
    if (!origin.opaque())
      filter_builder->AddOrigin(std::move(origin));
  }
  DeleteStoragePartitionDataWithFilter(storage_partition,
                                       std::move(filter_builder), base::Time(),
                                       base::Time::Max());
}

}  // namespace

BrowsingDataHistoryObserverService::BrowsingDataHistoryObserverService(
    Profile* profile)
    : profile_(profile) {
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service)
    history_observation_.Observe(history_service);
}

BrowsingDataHistoryObserverService::~BrowsingDataHistoryObserverService() {}

void BrowsingDataHistoryObserverService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration())
    browsing_data::RemoveNavigationEntries(profile_, deletion_info);

  // Deleting Template URLs. This also handles expiration events.
  TemplateURLService* keywords_model =
      TemplateURLServiceFactory::GetForProfile(profile_);

  content::StoragePartition* storage_partition =
      storage_partition_for_testing_ ? storage_partition_for_testing_
                                     : profile_->GetDefaultStoragePartition();

  if (deletion_info.time_range().IsValid()) {
    if (keywords_model) {
      DeleteTemplateUrlsForTimeRange(keywords_model,
                                     deletion_info.time_range().begin(),
                                     deletion_info.time_range().end());
    }

    if (storage_partition) {
      DeleteStoragePartitionDataForTimeRange(
          storage_partition, deletion_info.time_range().begin(),
          deletion_info.time_range().end(), deletion_info.restrict_urls());
    }
  } else {
    // If the history deletion did not have a time range, delete data by origin.
    auto deleted_origins =
        GetDeletedOrigins(deletion_info.deleted_urls_origin_map());

    if (storage_partition) {
      DeleteStoragePartitionDataForDeletedOrigins(storage_partition,
                                                  deleted_origins);
    }

    // Move the deleted origins to avoid an expensive copy.
    if (keywords_model) {
      DeleteTemplateUrlsForDeletedOrigins(keywords_model,
                                          std::move(deleted_origins));
    }
  }
}

void BrowsingDataHistoryObserverService::OverrideStoragePartitionForTesting(
    content::StoragePartition* partition) {
  storage_partition_for_testing_ = partition;
}

// static
BrowsingDataHistoryObserverService::Factory*
BrowsingDataHistoryObserverService::Factory::GetInstance() {
  return base::Singleton<BrowsingDataHistoryObserverService::Factory>::get();
}

BrowsingDataHistoryObserverService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingDataHistoryObserverService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(SessionServiceFactory::GetInstance());
#endif
}

KeyedService*
BrowsingDataHistoryObserverService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsOffTheRecord() || profile->IsGuestSession())
    return nullptr;
  return new BrowsingDataHistoryObserverService(profile);
}

bool BrowsingDataHistoryObserverService::Factory::
    ServiceIsCreatedWithBrowserContext() const {
  // Create this service at startup to receive all deletion events.
  return true;
}
