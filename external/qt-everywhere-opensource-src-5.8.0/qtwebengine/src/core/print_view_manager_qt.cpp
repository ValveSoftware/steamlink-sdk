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

#include "print_view_manager_qt.h"

#include "type_conversion.h"
#include "web_engine_context.h"

#include <QtGui/qpagelayout.h>
#include <QtGui/qpagesize.h>

#include "base/values.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/common/print_messages.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/web_preferences.h"
#include "printing/pdf_metafile_skia.h"
#include "printing/print_job_constants.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(QtWebEngineCore::PrintViewManagerQt);

namespace {
static const qreal kMicronsToMillimeter = 1000.0f;

static std::vector<char>
GetStdVectorFromHandle(base::SharedMemoryHandle handle, uint32_t data_size) {
    std::unique_ptr<base::SharedMemory> shared_buf(
                new base::SharedMemory(handle, true));

    if (!shared_buf->Map(data_size)) {
        return std::vector<char>();
    }

    char* data = static_cast<char*>(shared_buf->memory());
    return std::vector<char>(data, data + data_size);
}

static scoped_refptr<base::RefCountedBytes>
GetBytesFromHandle(base::SharedMemoryHandle handle, uint32_t data_size) {
     std::unique_ptr<base::SharedMemory> shared_buf(
                 new base::SharedMemory(handle, true));

    if (!shared_buf->Map(data_size)) {
       return NULL;
    }

    unsigned char* data = static_cast<unsigned char*>(shared_buf->memory());
    std::vector<unsigned char> dataVector(data, data + data_size);
    return base::RefCountedBytes::TakeVector(&dataVector);
}

// Write the PDF file to disk.
static void SavePdfFile(scoped_refptr<base::RefCountedBytes> data,
                        const base::FilePath& path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
    DCHECK_GT(data->size(), 0U);

    printing::PdfMetafileSkia metafile(printing::PDF_SKIA_DOCUMENT_TYPE);
    metafile.InitFromData(static_cast<const void*>(data->front()), data->size());

    base::File file(path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (file.IsValid())
        metafile.SaveTo(&file);
}

static base::DictionaryValue *createPrintSettings()
{
    base::DictionaryValue *printSettings = new base::DictionaryValue();
    // TO DO: Check if we can use the request ID from Qt here somehow.
    static int internalRequestId = 0;

    printSettings->SetBoolean(printing::kIsFirstRequest, internalRequestId++ == 0);
    printSettings->SetInteger(printing::kPreviewRequestID, internalRequestId);

    // The following are standard settings that Chromium expects to be set.
    printSettings->SetBoolean(printing::kSettingPrintToPDF, true);
    printSettings->SetBoolean(printing::kSettingCloudPrintDialog, false);
    printSettings->SetBoolean(printing::kSettingPrintWithPrivet, false);
    printSettings->SetBoolean(printing::kSettingPrintWithExtension, false);

    printSettings->SetBoolean(printing::kSettingGenerateDraftData, false);
    printSettings->SetBoolean(printing::kSettingPreviewModifiable, false);
    printSettings->SetInteger(printing::kSettingDuplexMode, printing::SIMPLEX);
    printSettings->SetInteger(printing::kSettingCopies, 1);
    printSettings->SetBoolean(printing::kSettingCollate, false);
    printSettings->SetBoolean(printing::kSettingGenerateDraftData, false);
    printSettings->SetBoolean(printing::kSettingPreviewModifiable, false);

    printSettings->SetBoolean(printing::kSettingShouldPrintSelectionOnly, false);
    printSettings->SetBoolean(printing::kSettingShouldPrintBackgrounds, true);
    printSettings->SetBoolean(printing::kSettingHeaderFooterEnabled, false);
    printSettings->SetString(printing::kSettingDeviceName, "");
    printSettings->SetInteger(printing::kPreviewUIID, 12345678);

    return printSettings;
}

static base::DictionaryValue *createPrintSettingsFromQPageLayout(const QPageLayout &pageLayout)
{
    base::DictionaryValue *printSettings = createPrintSettings();

    //Set page size attributes, chromium expects these in micrometers
    QSizeF pageSizeInMilimeter = pageLayout.pageSize().size(QPageSize::Millimeter);
    std::unique_ptr<base::DictionaryValue> sizeDict(new base::DictionaryValue);
    sizeDict->SetInteger(printing::kSettingMediaSizeWidthMicrons, pageSizeInMilimeter.width() * kMicronsToMillimeter);
    sizeDict->SetInteger(printing::kSettingMediaSizeHeightMicrons, pageSizeInMilimeter.height() * kMicronsToMillimeter);
    printSettings->Set(printing::kSettingMediaSize, std::move(sizeDict));

    // Apply page margins
    QMargins pageMarginsInPoints = pageLayout.marginsPoints();
    std::unique_ptr<base::DictionaryValue> marginsDict(new base::DictionaryValue);
    marginsDict->SetInteger(printing::kSettingMarginTop, pageMarginsInPoints.top());
    marginsDict->SetInteger(printing::kSettingMarginBottom, pageMarginsInPoints.bottom());
    marginsDict->SetInteger(printing::kSettingMarginLeft, pageMarginsInPoints.left());
    marginsDict->SetInteger(printing::kSettingMarginRight, pageMarginsInPoints.right());

    printSettings->Set(printing::kSettingMarginsCustom, std::move(marginsDict));
    printSettings->SetInteger(printing::kSettingMarginsType, printing::CUSTOM_MARGINS);

    printSettings->SetBoolean(printing::kSettingLandscape, pageLayout.orientation() == QPageLayout::Landscape);

    return printSettings;
}

} // namespace

namespace QtWebEngineCore {

PrintViewManagerQt::~PrintViewManagerQt()
{
}

#if defined(ENABLE_BASIC_PRINTING)
bool PrintViewManagerQt::PrintToPDF(const QPageLayout &pageLayout, bool printInColor, const QString &filePath)
{
    if (m_printSettings || !filePath.length())
        return false;

    m_pdfOutputPath = toFilePath(filePath);
    if (!PrintToPDFInternal(pageLayout, printInColor)) {
        resetPdfState();
        return false;
    }
    return true;
}

bool PrintViewManagerQt::PrintToPDFWithCallback(const QPageLayout &pageLayout, bool printInColor, const PrintToPDFCallback& callback)
{
    if (callback.is_null())
        return false;

    // If there already is a pending print in progress, don't try starting another one.
    if (m_printSettings) {
            content::BrowserThread::PostTask(content::BrowserThread::UI,
                                             FROM_HERE,
                                             base::Bind(callback, std::vector<char>()));
        return false;
    }

    m_pdfPrintCallback = callback;
    if (!PrintToPDFInternal(pageLayout, printInColor)) {
        content::BrowserThread::PostTask(content::BrowserThread::UI,
                                         FROM_HERE,
                                         base::Bind(callback, std::vector<char>()));

        resetPdfState();
        return false;
    }
    return true;
}

bool PrintViewManagerQt::PrintToPDFInternal(const QPageLayout &pageLayout, bool printInColor)
{
    if (!pageLayout.isValid())
        return false;

    m_printSettings.reset(createPrintSettingsFromQPageLayout(pageLayout));
    m_printSettings->SetBoolean(printing::kSettingShouldPrintBackgrounds
        , web_contents()->GetRenderViewHost()->GetWebkitPreferences().should_print_backgrounds);
    m_printSettings->SetInteger(printing::kSettingColor, printInColor ? printing::COLOR : printing::GRAYSCALE);
    return Send(new PrintMsg_InitiatePrintPreview(routing_id(), false));
}

#endif // defined(ENABLE_BASIC_PRINTING)

// PrintedPagesSource implementation.
base::string16 PrintViewManagerQt::RenderSourceName()
{
     return toString16(QLatin1String(""));
}

PrintViewManagerQt::PrintViewManagerQt(content::WebContents *contents)
    : PrintViewManagerBaseQt(contents)
{

}

// content::WebContentsObserver implementation.
bool PrintViewManagerQt::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PrintViewManagerQt, message)
      IPC_MESSAGE_HANDLER(PrintHostMsg_DidShowPrintDialog, OnDidShowPrintDialog)
      IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                                 OnRequestPrintPreview)
      IPC_MESSAGE_HANDLER(PrintHostMsg_MetafileReadyForPrinting,
                                 OnMetafileReadyForPrinting);
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled || PrintManager::OnMessageReceived(message);
}

void PrintViewManagerQt::resetPdfState()
{
    m_pdfOutputPath.clear();
    m_pdfPrintCallback.Reset();
    m_printSettings.reset();
}

// IPC handlers

void PrintViewManagerQt::OnRequestPrintPreview(
    const PrintHostMsg_RequestPrintPreview_Params& params)
{
    Send(new PrintMsg_PrintPreview(routing_id(), *m_printSettings));
}

void PrintViewManagerQt::OnMetafileReadyForPrinting(
    const PrintHostMsg_DidPreviewDocument_Params& params)
{
    StopWorker(params.document_cookie);

    // Create local copies so we can reset the state and take a new pdf print job.
    base::Callback<void(const std::vector<char>&)> pdf_print_callback = m_pdfPrintCallback;
    base::FilePath pdfOutputPath = m_pdfOutputPath;

    resetPdfState();

    if (!pdf_print_callback.is_null()) {
        std::vector<char> data_vector = GetStdVectorFromHandle(params.metafile_data_handle, params.data_size);
        content::BrowserThread::PostTask(content::BrowserThread::UI,
                                         FROM_HERE,
                                         base::Bind(pdf_print_callback, data_vector));
    } else {
        scoped_refptr<base::RefCountedBytes> data_bytes = GetBytesFromHandle(params.metafile_data_handle, params.data_size);
        content::BrowserThread::PostTask(content::BrowserThread::FILE,
               FROM_HERE,
               base::Bind(&SavePdfFile, data_bytes, pdfOutputPath));
    }
}

void PrintViewManagerQt::OnDidShowPrintDialog()
{
}

// content::WebContentsObserver implementation.
void PrintViewManagerQt::DidStartLoading()
{
}

// content::WebContentsObserver implementation.
// Cancels the print job.
void PrintViewManagerQt::NavigationStopped()
{
    if (!m_pdfPrintCallback.is_null()) {
        content::BrowserThread::PostTask(content::BrowserThread::UI,
                                         FROM_HERE,
                                         base::Bind(m_pdfPrintCallback, std::vector<char>()));
    }
    resetPdfState();
}

void PrintViewManagerQt::RenderProcessGone(base::TerminationStatus status)
{
    PrintViewManagerBaseQt::RenderProcessGone(status);
    if (!m_pdfPrintCallback.is_null()) {
        content::BrowserThread::PostTask(content::BrowserThread::UI,
                                         FROM_HERE,
                                         base::Bind(m_pdfPrintCallback, std::vector<char>()));
    }
    resetPdfState();
}


} // namespace QtWebEngineCore
