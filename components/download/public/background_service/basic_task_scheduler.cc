// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/basic_task_scheduler.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/download/public/background_service/download_service.h"

namespace download {

BasicTaskScheduler::BasicTaskScheduler(
    const base::RepeatingCallback<DownloadService*()>& get_download_service)
    : get_download_service_(get_download_service) {}

BasicTaskScheduler::~BasicTaskScheduler() = default;

void BasicTaskScheduler::ScheduleTask(download::DownloadTaskType task_type,
                                      bool require_unmetered_network,
                                      bool require_charging,
                                      int optimal_battery_percentage,
                                      int64_t window_start_time_seconds,
                                      int64_t window_end_time_seconds) {
  scheduled_tasks_[task_type].Reset(
      base::BindOnce(&BasicTaskScheduler::RunScheduledTask,
                     weak_factory_.GetWeakPtr(), task_type));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, scheduled_tasks_[task_type].callback(),
      base::TimeDelta::FromSeconds(window_start_time_seconds));
}

void BasicTaskScheduler::CancelTask(download::DownloadTaskType task_type) {
  scheduled_tasks_[task_type].Cancel();
}

void BasicTaskScheduler::RunScheduledTask(
    download::DownloadTaskType task_type) {
  get_download_service_.Run()->OnStartScheduledTask(
      task_type, base::BindOnce(&BasicTaskScheduler::OnTaskFinished,
                                weak_factory_.GetWeakPtr()));
}

void BasicTaskScheduler::OnTaskFinished(bool reschedule) {
  // TODO(shaktisahu): Cache the original scheduling params and re-post task in
  // case it needs reschedule.
}

}  // namespace download
