// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histograms_monitor.h"

#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "content/browser/metrics/histogram_synchronizer.h"

namespace content {

HistogramsMonitor::HistogramsMonitor() = default;

HistogramsMonitor::~HistogramsMonitor() = default;

void HistogramsMonitor::StartMonitoring(const std::string& query) {
  query_ = query;
  FetchHistograms();
  histograms_snapshot_.clear();
  // Save a snapshot of all current histograms that will be used as a baseline.
  for (const auto* const histogram : base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(), query_)) {
    histograms_snapshot_[histogram->histogram_name()] =
        histogram->SnapshotSamples();
  }
}

base::ListValue HistogramsMonitor::GetDiff() {
  FetchHistograms();
  base::StatisticsRecorder::Histograms histograms =
      base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
          base::StatisticsRecorder::GetHistograms(), query_));
  return GetDiffInternal(histograms);
}

void HistogramsMonitor::FetchHistograms() {
  base::StatisticsRecorder::ImportProvidedHistograms();
  HistogramSynchronizer::FetchHistograms();
}

base::ListValue HistogramsMonitor::GetDiffInternal(
    const base::StatisticsRecorder::Histograms& histograms) {
  base::ListValue histograms_list;
  for (const base::HistogramBase* const histogram : histograms) {
    std::unique_ptr<base::HistogramSamples> snapshot =
        histogram->SnapshotSamples();
    auto it = histograms_snapshot_.find(histogram->histogram_name());
    if (it != histograms_snapshot_.end()) {
      snapshot->Subtract(*it->second.get());
    }
    if (snapshot->TotalCount() > 0) {
      base::DictionaryValue histogram_dict = snapshot->ToGraphDict(
          histogram->histogram_name(), histogram->flags());
      histograms_list.Append(std::move(histogram_dict));
    }
  }
  return histograms_list;
}

}  // namespace content
