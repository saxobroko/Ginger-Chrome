// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/metrics_collector.h"

#include <set>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_attached_data.h"

namespace performance_manager {

class MetricsReportRecordHolder
    : public ExternalNodeAttachedDataImpl<MetricsReportRecordHolder> {
 public:
  explicit MetricsReportRecordHolder(const PageNode* unused_page_node) {}
  ~MetricsReportRecordHolder() override = default;
  MetricsCollector::MetricsReportRecord metrics_report_record;
};

class UkmCollectionStateHolder
    : public ExternalNodeAttachedDataImpl<UkmCollectionStateHolder> {
 public:
  explicit UkmCollectionStateHolder(const PageNode* unused_page_node) {}
  ~UkmCollectionStateHolder() override = default;
  MetricsCollector::UkmCollectionState ukm_collection_state;
};

// Delay the metrics report from for 5 minutes from when the main frame
// navigation is committed.
const base::TimeDelta kMetricsReportDelayTimeout =
    base::TimeDelta::FromMinutes(5);

const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstFaviconUpdated";
const char kTabFromBackgroundedToFirstTitleUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstTitleUpdated";
const char kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[] =
    "TabManager.Heuristics."
    "FromBackgroundedToFirstNonPersistentNotificationCreated";

const int kDefaultFrequencyUkmEQTReported = 5u;

MetricsCollector::MetricsCollector() = default;

MetricsCollector::~MetricsCollector() = default;

void MetricsCollector::OnNonPersistentNotificationCreated(
    const FrameNode* frame_node) {
  // Only record metrics while a page is backgrounded.
  auto* page_node = frame_node->GetPageNode();
  if (page_node->IsVisible() || !ShouldReportMetrics(page_node))
    return;

  auto* record = GetMetricsReportRecord(page_node);
  record->first_non_persistent_notification_created.OnSignalReceived(
      frame_node->IsMainFrame(), page_node->GetTimeSinceLastVisibilityChange(),
      graph_->GetUkmRecorder());
}

void MetricsCollector::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  RegisterObservers(graph);
}

void MetricsCollector::OnTakenFromGraph(Graph* graph) {
  UnregisterObservers(graph);
  graph_ = nullptr;
}

void MetricsCollector::OnIsVisibleChanged(const PageNode* page_node) {
  // The page becomes visible again, clear all records in order to
  // report metrics when page becomes invisible next time.
  if (page_node->IsVisible())
    ResetMetricsReportRecord(page_node);
}

void MetricsCollector::OnUkmSourceIdChanged(const PageNode* page_node) {
  ukm::SourceId ukm_source_id = page_node->GetUkmSourceID();
  UpdateUkmSourceIdForPage(page_node, ukm_source_id);
  auto* record = GetMetricsReportRecord(page_node);
  record->UpdateUkmSourceID(ukm_source_id);
}

void MetricsCollector::OnFaviconUpdated(const PageNode* page_node) {
  // Only record metrics while it is backgrounded.
  if (page_node->IsVisible() || !ShouldReportMetrics(page_node))
    return;
  auto* record = GetMetricsReportRecord(page_node);
  record->first_favicon_updated.OnSignalReceived(
      true, page_node->GetTimeSinceLastVisibilityChange(),
      graph_->GetUkmRecorder());
}

void MetricsCollector::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  const base::TimeDelta lifetime =
      base::Time::Now() - process_node->GetLaunchTime();
  if (lifetime > base::TimeDelta()) {
    // Do not record in the rare case system time was adjusted and now < launch
    // time.
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.ProcessLifetime.HighResolution",
                               lifetime, base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromMinutes(5), 100);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.ProcessLifetime.LowResolution",
                               lifetime, base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);
  }
}

void MetricsCollector::OnTitleUpdated(const PageNode* page_node) {
  // Only record metrics while it is backgrounded.
  if (page_node->IsVisible() || !ShouldReportMetrics(page_node))
    return;
  auto* record = GetMetricsReportRecord(page_node);
  record->first_title_updated.OnSignalReceived(
      true, page_node->GetTimeSinceLastVisibilityChange(),
      graph_->GetUkmRecorder());
}

// static
MetricsCollector::MetricsReportRecord* MetricsCollector::GetMetricsReportRecord(
    const PageNode* page_node) {
  auto* holder = MetricsReportRecordHolder::GetOrCreate(page_node);
  return &holder->metrics_report_record;
}

// static
MetricsCollector::UkmCollectionState* MetricsCollector::GetUkmCollectionState(
    const PageNode* page_node) {
  auto* holder = UkmCollectionStateHolder::GetOrCreate(page_node);
  return &holder->ukm_collection_state;
}

void MetricsCollector::RegisterObservers(Graph* graph) {
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void MetricsCollector::UnregisterObservers(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

bool MetricsCollector::ShouldReportMetrics(const PageNode* page_node) {
  return page_node->GetTimeSinceLastNavigation() > kMetricsReportDelayTimeout;
}

void MetricsCollector::UpdateUkmSourceIdForPage(const PageNode* page_node,
                                                ukm::SourceId ukm_source_id) {
  auto* state = GetUkmCollectionState(page_node);
  state->ukm_source_id = ukm_source_id;
}

void MetricsCollector::ResetMetricsReportRecord(const PageNode* page_node) {
  auto* record = GetMetricsReportRecord(page_node);
  record->Reset();
}

MetricsCollector::MetricsReportRecord::MetricsReportRecord() = default;

MetricsCollector::MetricsReportRecord::MetricsReportRecord(
    const MetricsReportRecord& other) = default;

void MetricsCollector::MetricsReportRecord::UpdateUkmSourceID(
    ukm::SourceId ukm_source_id) {
  first_favicon_updated.SetUkmSourceID(ukm_source_id);
  first_non_persistent_notification_created.SetUkmSourceID(ukm_source_id);
  first_title_updated.SetUkmSourceID(ukm_source_id);
}

void MetricsCollector::MetricsReportRecord::Reset() {
  first_favicon_updated.Reset();
  first_non_persistent_notification_created.Reset();
  first_title_updated.Reset();
}

}  // namespace performance_manager
