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

// Based on chrome/browser/printing/printing_message_filter.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing_message_filter_qt.h"

#include "web_engine_context.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"

using content::BrowserThread;

namespace QtWebEngineCore {

PrintingMessageFilterQt::PrintingMessageFilterQt(int render_process_id)
    : BrowserMessageFilter(PrintMsgStart),
      render_process_id_(render_process_id),
      queue_(WebEngineContext::current()->getPrintJobManager()->queue()) {
  DCHECK(queue_.get());
}

PrintingMessageFilterQt::~PrintingMessageFilterQt() {
}

void PrintingMessageFilterQt::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
}

bool PrintingMessageFilterQt::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintingMessageFilterQt, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_IsPrintingEnabled, OnIsPrintingEnabled)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_UpdatePrintSettings,
                                    OnUpdatePrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintingMessageFilterQt::OnIsPrintingEnabled(bool* is_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  *is_enabled = true;
}

void PrintingMessageFilterQt::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_refptr<printing::PrinterQuery> printer_query;

  printer_query = queue_->PopPrinterQuery(0);
  if (!printer_query.get()) {
    printer_query =
        queue_->CreatePrinterQuery(render_process_id_, reply_msg->routing_id());
  }

  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(
      printing::PrinterQuery::GetSettingsAskParam::DEFAULTS,
      0,
      false,
      printing::DEFAULT_MARGINS,
      false,
      base::Bind(&PrintingMessageFilterQt::OnGetDefaultPrintSettingsReply,
                 this,
                 printer_query,
                 reply_msg));
}

void PrintingMessageFilterQt::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  PrintMsg_Print_Params params;
  if (!printer_query.get() ||
      printer_query->last_status() != printing::PrintingContext::OK) {
    params.Reset();
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If printing was enabled.
  if (printer_query.get()) {
    // If user hasn't cancelled.
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      queue_->QueuePrinterQuery(printer_query.get());
    } else {
      printer_query->StopWorker();
    }
  }
}

void PrintingMessageFilterQt::OnScriptedPrint(
    const PrintHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query =
      queue_->PopPrinterQuery(params.cookie);
  if (!printer_query.get()) {
    printer_query =
        queue_->CreatePrinterQuery(render_process_id_, reply_msg->routing_id());
  }
  printer_query->GetSettings(
      printing::PrinterQuery::GetSettingsAskParam::ASK_USER,
      params.expected_pages_count,
      params.has_selection,
      params.margin_type,
      params.is_scripted,
      base::Bind(&PrintingMessageFilterQt::OnScriptedPrintReply,
                 this,
                 printer_query,
                 reply_msg));
}

void PrintingMessageFilterQt::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  PrintMsg_PrintPages_Params params;

  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    params.Reset();
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages = printing::PageRange::GetPages(printer_query->settings().ranges());
  }
  PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (params.params.dpi && params.params.document_cookie) {
    queue_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

void PrintingMessageFilterQt::OnUpdatePrintSettings(
    int document_cookie, const base::DictionaryValue& job_settings,
    IPC::Message* reply_msg) {
  std::unique_ptr<base::DictionaryValue> new_settings(job_settings.DeepCopy());

  scoped_refptr<printing::PrinterQuery> printer_query;
  printer_query = queue_->PopPrinterQuery(document_cookie);
  if (!printer_query.get()) {
    int host_id = render_process_id_;
    int routing_id = reply_msg->routing_id();
    if (!new_settings->GetInteger(printing::kPreviewInitiatorHostId,
                                  &host_id) ||
        !new_settings->GetInteger(printing::kPreviewInitiatorRoutingId,
                                  &routing_id)) {
      host_id = content::ChildProcessHost::kInvalidUniqueID;
      routing_id = content::ChildProcessHost::kInvalidUniqueID;
    }
    printer_query = queue_->CreatePrinterQuery(host_id, routing_id);
  }
  printer_query->SetSettings(
      std::move(new_settings),
      base::Bind(&PrintingMessageFilterQt::OnUpdatePrintSettingsReply, this,
                 printer_query, reply_msg));
}

void PrintingMessageFilterQt::OnUpdatePrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  PrintMsg_PrintPages_Params params;
  if (!printer_query.get() ||
      printer_query->last_status() != printing::PrintingContext::OK) {
    params.Reset();
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages = printing::PageRange::GetPages(printer_query->settings().ranges());
  }

  PrintHostMsg_UpdatePrintSettings::WriteReplyParams(
      reply_msg,
      params,
      printer_query.get() &&
          (printer_query->last_status() == printing::PrintingContext::CANCEL));
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query.get()) {
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      queue_->QueuePrinterQuery(printer_query.get());
    } else {
      printer_query->StopWorker();
    }
  }
}

void PrintingMessageFilterQt::OnCheckForCancel(int32_t preview_ui_id,
                                             int preview_request_id,
                                             bool* cancel) {
  *cancel = false;
}

}  // namespace printing
