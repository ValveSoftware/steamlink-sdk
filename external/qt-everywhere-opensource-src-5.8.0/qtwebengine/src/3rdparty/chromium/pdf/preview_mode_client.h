// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PREVIEW_MODE_CLIENT_H_
#define PDF_PREVIEW_MODE_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "pdf/pdf_engine.h"

namespace chrome_pdf {

// The interface that's provided to the print preview rendering engine.
class PreviewModeClient : public PDFEngine::Client {
 public:
  class Client {
   public:
    virtual void PreviewDocumentLoadFailed() = 0;
    virtual void PreviewDocumentLoadComplete() = 0;
  };

  explicit PreviewModeClient(Client* client);
  ~PreviewModeClient() override {}

  // PDFEngine::Client implementation.
  void DocumentSizeUpdated(const pp::Size& size) override;
  void Invalidate(const pp::Rect& rect) override;
  void Scroll(const pp::Point& point) override;
  void ScrollToX(int position) override;
  void ScrollToY(int position) override;
  void ScrollToPage(int page) override;
  void NavigateTo(const std::string& url, bool open_in_new_tab) override;
  void UpdateCursor(PP_CursorType_Dev cursor) override;
  void UpdateTickMarks(const std::vector<pp::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
  void GetDocumentPassword(
      pp::CompletionCallbackWithOutput<pp::Var> callback) override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  std::string GetURL() override;
  void Email(const std::string& to,
             const std::string& cc,
             const std::string& bcc,
             const std::string& subject,
             const std::string& body) override;
  void Print() override;
  void SubmitForm(const std::string& url,
                  const void* data,
                  int length) override;
  std::string ShowFileSelectionDialog() override;
  pp::URLLoader CreateURLLoader() override;
  void ScheduleCallback(int id, int delay_in_ms) override;
  void SearchString(const base::char16* string,
                    const base::char16* term,
                    bool case_sensitive,
                    std::vector<SearchStringResult>* results) override;
  void DocumentPaintOccurred() override;
  void DocumentLoadComplete(int page_count) override;
  void DocumentLoadFailed() override;
  pp::Instance* GetPluginInstance() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void DocumentLoadProgress(uint32_t available, uint32_t doc_size) override;
  void FormTextFieldFocusChange(bool in_focus) override;
  bool IsPrintPreview() override;
  uint32_t GetBackgroundColor() override;

 private:
  Client* const client_;
};

}  // namespace chrome_pdf

#endif  // PDF_PREVIEW_MODE_CLIENT_H_
