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

#ifndef PRINT_VIEW_MANAGER_QT_H
#define PRINT_VIEW_MANAGER_QT_H

#include "print_view_manager_base_qt.h"

#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_member.h"
#include "components/printing/browser/print_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/printed_pages_source.h"

struct PrintHostMsg_RequestPrintPreview_Params;
struct PrintHostMsg_DidPreviewDocument_Params;

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

QT_BEGIN_NAMESPACE
class QPageLayout;
class QString;
QT_END_NAMESPACE

namespace QtWebEngineCore {
class PrintViewManagerQt
        : PrintViewManagerBaseQt
        , public content::WebContentsUserData<PrintViewManagerQt>
{
public:
    ~PrintViewManagerQt() override;
    typedef base::Callback<void(const std::vector<char> &result)> PrintToPDFCallback;
#if defined(ENABLE_BASIC_PRINTING)
    // Method to print a page to a Pdf document with page size \a pageSize in location \a filePath.
    bool PrintToPDF(const QPageLayout &pageLayout, bool printInColor, const QString &filePath);
    bool PrintToPDFWithCallback(const QPageLayout &pageLayout, bool printInColor, const PrintToPDFCallback &callback);
#endif  // ENABLE_BASIC_PRINTING

    // PrintedPagesSource implementation.
    base::string16 RenderSourceName() override;

protected:
    explicit PrintViewManagerQt(content::WebContents*);

    // content::WebContentsObserver implementation.
    // Cancels the print job.
    void NavigationStopped() override;

    // Terminates or cancels the print job if one was pending.
    void RenderProcessGone(base::TerminationStatus status) override;

    // content::WebContentsObserver implementation.
    bool OnMessageReceived(const IPC::Message& message) override;

    // IPC handlers
    void OnDidShowPrintDialog();
    void OnRequestPrintPreview(const PrintHostMsg_RequestPrintPreview_Params&);
    void OnMetafileReadyForPrinting(const PrintHostMsg_DidPreviewDocument_Params& params);

#if defined(ENABLE_BASIC_PRINTING)
    bool PrintToPDFInternal(const QPageLayout &, bool printInColor);
#endif //

    base::FilePath m_pdfOutputPath;
    PrintToPDFCallback m_pdfPrintCallback;

private:
    friend class content::WebContentsUserData<PrintViewManagerQt>;

    void resetPdfState();

    // content::WebContentsObserver implementation.
    void DidStartLoading() override;
    DISALLOW_COPY_AND_ASSIGN(PrintViewManagerQt);
};

} // namespace QtWebEngineCore
#endif // PRINT_VIEW_MANAGER_QT_H

