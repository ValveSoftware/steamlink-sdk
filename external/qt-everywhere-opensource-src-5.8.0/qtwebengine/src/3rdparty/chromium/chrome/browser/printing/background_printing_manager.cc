// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/background_printing_manager.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

using content::BrowserThread;
using content::WebContents;

namespace printing {

class BackgroundPrintingManager::Observer
    : public content::WebContentsObserver {
 public:
  Observer(BackgroundPrintingManager* manager, WebContents* web_contents);

 private:
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;

  BackgroundPrintingManager* manager_;
};

BackgroundPrintingManager::Observer::Observer(
    BackgroundPrintingManager* manager, WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manager_(manager) {
}

void BackgroundPrintingManager::Observer::RenderProcessGone(
    base::TerminationStatus status) {
  manager_->DeletePreviewContents(web_contents());
}
void BackgroundPrintingManager::Observer::WebContentsDestroyed() {
  manager_->DeletePreviewContents(web_contents());
}

BackgroundPrintingManager::BackgroundPrintingManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundPrintingManager::~BackgroundPrintingManager() {
  DCHECK(CalledOnValidThread());
  // The might be some WebContentses still in |printing_contents_map_| at this
  // point (e.g. when the last remaining tab closes and there is still a print
  // preview WebContents trying to print). In such a case it will fail to print,
  // but we should at least clean up the observers.
  // TODO(thestig): Handle this case better.
  STLDeleteValues(&printing_contents_map_);
}

void BackgroundPrintingManager::OwnPrintPreviewDialog(
    WebContents* preview_dialog) {
  DCHECK(CalledOnValidThread());
  DCHECK(PrintPreviewDialogController::IsPrintPreviewDialog(preview_dialog));
  CHECK(!HasPrintPreviewDialog(preview_dialog));

  printing_contents_map_[preview_dialog] = new Observer(this, preview_dialog);

  // Watch for print jobs finishing. Everything else is watched for by the
  // Observer. TODO(avi, cait): finish the job of removing this last
  // notification.
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                 content::Source<WebContents>(preview_dialog));

  // Activate the initiator.
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return;
  WebContents* initiator = dialog_controller->GetInitiator(preview_dialog);
  if (!initiator)
    return;
  initiator->GetDelegate()->ActivateContents(initiator);
}

void BackgroundPrintingManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_RELEASED, type);
  DeletePreviewContents(content::Source<WebContents>(source).ptr());
}

void BackgroundPrintingManager::DeletePreviewContents(
    WebContents* preview_contents) {
  WebContentsObserverMap::iterator i =
      printing_contents_map_.find(preview_contents);
  if (i == printing_contents_map_.end()) {
    // Everyone is racing to be the first to delete the |preview_contents|. If
    // this case is hit, someone else won the race, so there is no need to
    // continue. <http://crbug.com/100806>
    return;
  }

  // Stop all observation ...
  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                    content::Source<WebContents>(preview_contents));
  Observer* observer = i->second;
  printing_contents_map_.erase(i);
  delete observer;

  // ... and mortally wound the contents. (Deletion immediately is not a good
  // idea in case this was called from RenderViewGone.)
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, preview_contents);
}

std::set<content::WebContents*> BackgroundPrintingManager::CurrentContentSet() {
  std::set<content::WebContents*> result;
  for (WebContentsObserverMap::iterator i = printing_contents_map_.begin();
       i != printing_contents_map_.end(); ++i) {
    result.insert(i->first);
  }
  return result;
}

bool BackgroundPrintingManager::HasPrintPreviewDialog(
    WebContents* preview_dialog) {
  return ContainsKey(printing_contents_map_, preview_dialog);
}

}  // namespace printing
