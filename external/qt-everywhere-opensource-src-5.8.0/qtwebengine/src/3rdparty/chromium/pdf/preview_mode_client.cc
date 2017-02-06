// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/preview_mode_client.h"

#include <stdint.h>

#include "base/logging.h"

namespace chrome_pdf {

PreviewModeClient::PreviewModeClient(Client* client)
    : client_(client) {
}

void PreviewModeClient::DocumentSizeUpdated(const pp::Size& size) {
}

void PreviewModeClient::Invalidate(const pp::Rect& rect) {
  NOTREACHED();
}

void PreviewModeClient::Scroll(const pp::Point& point) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToX(int position) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToY(int position) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToPage(int page) {
  NOTREACHED();
}

void PreviewModeClient::NavigateTo(const std::string& url,
                                   bool open_in_new_tab) {
  NOTREACHED();
}

void PreviewModeClient::UpdateCursor(PP_CursorType_Dev cursor) {
  NOTREACHED();
}

void PreviewModeClient::UpdateTickMarks(
    const std::vector<pp::Rect>& tickmarks) {
  NOTREACHED();
}

void PreviewModeClient::NotifyNumberOfFindResultsChanged(int total,
                                                         bool final_result) {
  NOTREACHED();
}

void PreviewModeClient::NotifySelectedFindResultChanged(
    int current_find_index) {
  NOTREACHED();
}

void PreviewModeClient::GetDocumentPassword(
    pp::CompletionCallbackWithOutput<pp::Var> callback) {
  callback.Run(PP_ERROR_FAILED);
}

void PreviewModeClient::Alert(const std::string& message) {
  NOTREACHED();
}

bool PreviewModeClient::Confirm(const std::string& message) {
  NOTREACHED();
  return false;
}

std::string PreviewModeClient::Prompt(const std::string& question,
                                      const std::string& default_answer) {
  NOTREACHED();
  return std::string();
}

std::string PreviewModeClient::GetURL() {
  NOTREACHED();
  return std::string();
}

void PreviewModeClient::Email(const std::string& to,
                              const std::string& cc,
                              const std::string& bcc,
                              const std::string& subject,
                              const std::string& body) {
  NOTREACHED();
}

void PreviewModeClient::Print() {
  NOTREACHED();
}

void PreviewModeClient::SubmitForm(const std::string& url,
                                   const void* data,
                                   int length) {
  NOTREACHED();
}

std::string PreviewModeClient::ShowFileSelectionDialog() {
  NOTREACHED();
  return std::string();
}

pp::URLLoader PreviewModeClient::CreateURLLoader() {
  NOTREACHED();
  return pp::URLLoader();
}

void PreviewModeClient::ScheduleCallback(int id, int delay_in_ms) {
  NOTREACHED();
}

void PreviewModeClient::SearchString(const base::char16* string,
                                     const base::char16* term,
                                     bool case_sensitive,
                                     std::vector<SearchStringResult>* results) {
  NOTREACHED();
}

void PreviewModeClient::DocumentPaintOccurred() {
  NOTREACHED();
}

void PreviewModeClient::DocumentLoadComplete(int page_count) {
  client_->PreviewDocumentLoadComplete();
}

void PreviewModeClient::DocumentLoadFailed() {
  client_->PreviewDocumentLoadFailed();
}

pp::Instance* PreviewModeClient::GetPluginInstance() {
  return nullptr;
}

void PreviewModeClient::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  NOTREACHED();
}

void PreviewModeClient::DocumentLoadProgress(uint32_t available,
                                             uint32_t doc_size) {}

void PreviewModeClient::FormTextFieldFocusChange(bool in_focus) {
  NOTREACHED();
}

bool PreviewModeClient::IsPrintPreview() {
  NOTREACHED();
  return false;
}

uint32_t PreviewModeClient::GetBackgroundColor() {
  NOTREACHED();
  return 0;
}

}  // namespace chrome_pdf
