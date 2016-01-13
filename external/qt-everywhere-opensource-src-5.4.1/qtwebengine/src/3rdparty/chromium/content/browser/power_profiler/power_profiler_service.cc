// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_profiler/power_profiler_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PowerProfilerService::PowerProfilerService()
    : status_(UNINITIALIZED),
      data_provider_(PowerDataProvider::Create()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // No provider supported for current platform.
  if (!data_provider_.get())
    return;
  sample_period_ = data_provider_->GetSamplingRate();
  status_ = INITIALIZED;
  task_runner_ = BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
      BrowserThread::GetBlockingPool()->GetSequenceToken());
}

PowerProfilerService::PowerProfilerService(
    scoped_ptr<PowerDataProvider> provider,
    scoped_refptr<base::TaskRunner> task_runner,
    const base::TimeDelta& sample_period)
    : task_runner_(task_runner),
      status_(UNINITIALIZED),
      sample_period_(sample_period),
      data_provider_(provider.Pass()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (data_provider_.get())
      status_ = INITIALIZED;
}

PowerProfilerService::~PowerProfilerService() {
}

bool PowerProfilerService::IsAvailable() {
  return status_ !=  UNINITIALIZED;
}

PowerProfilerService* PowerProfilerService::GetInstance() {
  return Singleton<PowerProfilerService>::get();
}

void PowerProfilerService::AddObserver(PowerProfilerObserver* observer) {
  if (status_ == UNINITIALIZED)
    return;

  observers_.AddObserver(observer);
  if (status_ != PROFILING)
    Start();
}

void PowerProfilerService::RemoveObserver(PowerProfilerObserver* observer) {
  observers_.RemoveObserver(observer);

  if (status_ == PROFILING && !observers_.might_have_observers())
    Stop();
}

void PowerProfilerService::Start() {
  DCHECK(status_ == INITIALIZED);
  status_ = PROFILING;

  // Send out power events immediately.
  QueryData();

  query_power_timer_.Start(FROM_HERE,
      sample_period_, this, &PowerProfilerService::QueryData);
}

void PowerProfilerService::Stop() {
  DCHECK(status_ == PROFILING);

  query_power_timer_.Stop();
  status_ = INITIALIZED;
}

void PowerProfilerService::QueryData() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&PowerProfilerService::QueryDataOnTaskRunner,
                            base::Unretained(this)));
}

void PowerProfilerService::Notify(const PowerEventVector& events) {
  FOR_EACH_OBSERVER(PowerProfilerObserver, observers_, OnPowerEvent(events));
}

void PowerProfilerService::QueryDataOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(status_ == PROFILING);

  // Get data and notify.
  PowerEventVector events = data_provider_->GetData();
  if (events.size() != 0) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &PowerProfilerService::Notify, base::Unretained(this), events));
  }
}

}  // namespace content
