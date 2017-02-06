/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINT_VIEW_MANAGER_BASE_QT_H
#define PRINT_VIEW_MANAGER_BASE_QT_H

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_member.h"
#include "components/printing/browser/print_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "printing/printed_pages_source.h"

struct PrintHostMsg_DidPrintPage_Params;

namespace content {
class RenderViewHost;
}

namespace printing {
class JobEventDetails;
class MetafilePlayer;
class PrintJob;
class PrintJobWorkerOwner;
class PrintQueriesQueue;
}

namespace QtWebEngineCore {
class PrintViewManagerBaseQt
        : public content::NotificationObserver
        , public printing::PrintManager
        , public printing::PrintedPagesSource
{
public:
    ~PrintViewManagerBaseQt() override;

    // PrintedPagesSource implementation.
    base::string16 RenderSourceName() override;

protected:
    explicit PrintViewManagerBaseQt(content::WebContents*);

    // content::WebContentsObserver implementation.
    // Cancels the print job.
    void NavigationStopped() override;

    // Terminates or cancels the print job if one was pending.
    void RenderProcessGone(base::TerminationStatus status) override;

    // content::WebContentsObserver implementation.
    bool OnMessageReceived(const IPC::Message& message) override;

    // IPC Message handlers.
    void OnDidPrintPage(const PrintHostMsg_DidPrintPage_Params& params);
    void OnShowInvalidPrinterSettingsError();

    // Processes a NOTIFY_PRINT_JOB_EVENT notification.
    void OnNotifyPrintJobEvent(const printing::JobEventDetails& event_details);

    int number_pages_;  // Number of pages to print in the print job.
    int cookie_;
    std::unique_ptr<base::DictionaryValue> m_printSettings;

    // content::NotificationObserver implementation.
    void Observe(int,
                 const content::NotificationSource&,
                 const content::NotificationDetails&) override;
    void StopWorker(int document_cookie);

    // In the case of Scripted Printing, where the renderer is controlling the
    // control flow, print_job_ is initialized whenever possible. No-op is
    // print_job_ is initialized.
    bool OpportunisticallyCreatePrintJob(int cookie);

    // Requests the RenderView to render all the missing pages for the print job.
    // No-op if no print job is pending. Returns true if at least one page has
    // been requested to the renderer.
    bool RenderAllMissingPagesNow();

    // Quits the current message loop if these conditions hold true: a document is
    // loaded and is complete and waiting_for_pages_to_be_rendered_ is true. This
    // function is called in DidPrintPage() or on ALL_PAGES_REQUESTED
    // notification. The inner message loop is created was created by
    // RenderAllMissingPagesNow().
    void ShouldQuitFromInnerMessageLoop();

    bool RunInnerMessageLoop();

    void TerminatePrintJob(bool cancel);
    void PrintingDone(bool success);
    void DisconnectFromCurrentPrintJob();

    bool CreateNewPrintJob(printing::PrintJobWorkerOwner* job);
    void ReleasePrintJob();
    void ReleasePrinterQuery();

private:
    content::NotificationRegistrar m_registrar;
    scoped_refptr<printing::PrintJob> m_printJob;

    bool m_isInsideInnerMessageLoop;
    bool m_isExpectingFirstPage;
    bool m_didPrintingSucceed;
    scoped_refptr<printing::PrintQueriesQueue> m_printerQueriesQueue;
    // content::WebContentsObserver implementation.
    void DidStartLoading() override;
    DISALLOW_COPY_AND_ASSIGN(PrintViewManagerBaseQt);
};

} // namespace QtWebEngineCore
#endif // PRINT_VIEW_MANAGER_BASE_QT_H

