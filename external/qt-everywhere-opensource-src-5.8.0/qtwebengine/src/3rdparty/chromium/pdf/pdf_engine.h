// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ENGINE_H_
#define PDF_PDF_ENGINE_H_

#include <stdint.h>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>
#include <vector>

#include "base/strings/string16.h"

#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var_array.h"

namespace pp {
class InputEvent;
class VarDictionary;
}

namespace chrome_pdf {

class Stream;

#if defined(OS_MACOSX)
const uint32_t kDefaultKeyModifier = PP_INPUTEVENT_MODIFIER_METAKEY;
#else  // !OS_MACOSX
const uint32_t kDefaultKeyModifier = PP_INPUTEVENT_MODIFIER_CONTROLKEY;
#endif  // OS_MACOSX

// Do one time initialization of the SDK.
bool InitializeSDK();
// Tells the SDK that we're shutting down.
void ShutdownSDK();

// This class encapsulates a PDF rendering engine.
class PDFEngine {
 public:
  enum DocumentPermission {
    PERMISSION_COPY,
    PERMISSION_COPY_ACCESSIBLE,
    PERMISSION_PRINT_LOW_QUALITY,
    PERMISSION_PRINT_HIGH_QUALITY,
  };

  // The interface that's provided to the rendering engine.
  class Client {
   public:
    virtual ~Client() {}

    // Informs the client about the document's size in pixels.
    virtual void DocumentSizeUpdated(const pp::Size& size) = 0;

    // Informs the client that the given rect needs to be repainted.
    virtual void Invalidate(const pp::Rect& rect) = 0;

    // Informs the client to scroll the plugin area by the given offset.
    virtual void Scroll(const pp::Point& point) = 0;

    // Scroll the horizontal/vertical scrollbars to a given position.
    virtual void ScrollToX(int position) = 0;
    virtual void ScrollToY(int position) = 0;

    // Scroll to the specified page.
    virtual void ScrollToPage(int page) = 0;

    // Navigate to the given url.
    virtual void NavigateTo(const std::string& url, bool open_in_new_tab) = 0;

    // Updates the cursor.
    virtual void UpdateCursor(PP_CursorType_Dev cursor) = 0;

    // Updates the tick marks in the vertical scrollbar.
    virtual void UpdateTickMarks(const std::vector<pp::Rect>& tickmarks) = 0;

    // Updates the number of find results for the current search term.  If
    // there are no matches 0 should be passed in.  Only when the plugin has
    // finished searching should it pass in the final count with final_result
    // set to true.
    virtual void NotifyNumberOfFindResultsChanged(int total,
                                                  bool final_result) = 0;

    // Updates the index of the currently selected search item.
    virtual void NotifySelectedFindResultChanged(int current_find_index) = 0;

    // Prompts the user for a password to open this document. The callback is
    // called when the password is retrieved.
    virtual void GetDocumentPassword(
        pp::CompletionCallbackWithOutput<pp::Var> callback) = 0;

    // Puts up an alert with the given message.
    virtual void Alert(const std::string& message) = 0;

    // Puts up a confirm with the given message, and returns true if the user
    // presses OK, or false if they press cancel.
    virtual bool Confirm(const std::string& message) = 0;

    // Puts up a prompt with the given message and default answer and returns
    // the answer.
    virtual std::string Prompt(const std::string& question,
                               const std::string& default_answer) = 0;

    // Returns the url of the pdf.
    virtual std::string GetURL() = 0;

    // Send an email.
    virtual void Email(const std::string& to,
                       const std::string& cc,
                       const std::string& bcc,
                       const std::string& subject,
                       const std::string& body) = 0;

    // Put up the print dialog.
    virtual void Print() = 0;

    // Submit the data using HTTP POST.
    virtual void SubmitForm(const std::string& url,
                            const void* data,
                            int length) = 0;

    // Pops up a file selection dialog and returns the result.
    virtual std::string ShowFileSelectionDialog() = 0;

    // Creates and returns new URL loader for partial document requests.
    virtual pp::URLLoader CreateURLLoader() = 0;

    // Calls the client's OnCallback() function in delay_in_ms with the given
    // id.
    virtual void ScheduleCallback(int id, int delay_in_ms) = 0;

    // Searches the given string for "term" and returns the results.  Unicode-
    // aware.
    struct SearchStringResult {
      int start_index;
      int length;
    };
    virtual void SearchString(const base::char16* string,
                              const base::char16* term,
                              bool case_sensitive,
                              std::vector<SearchStringResult>* results) = 0;

    // Notifies the client that the engine has painted a page from the document.
    virtual void DocumentPaintOccurred() = 0;

    // Notifies the client that the document has finished loading.
    virtual void DocumentLoadComplete(int page_count) = 0;

    // Notifies the client that the document has failed to load.
    virtual void DocumentLoadFailed() = 0;

    virtual pp::Instance* GetPluginInstance() = 0;

    // Notifies that an unsupported feature in the PDF was encountered.
    virtual void DocumentHasUnsupportedFeature(const std::string& feature) = 0;

    // Notifies the client about document load progress.
    virtual void DocumentLoadProgress(uint32_t available,
                                      uint32_t doc_size) = 0;

    // Notifies the client about focus changes for form text fields.
    virtual void FormTextFieldFocusChange(bool in_focus) = 0;

    // Returns true if the plugin has been opened within print preview.
    virtual bool IsPrintPreview() = 0;

    // Get the background color of the PDF.
    virtual uint32_t GetBackgroundColor() = 0;

    // Sets selection status.
    virtual void IsSelectingChanged(bool is_selecting) {}
  };

  // Factory method to create an instance of the PDF Engine.
  static PDFEngine* Create(Client* client);

  virtual ~PDFEngine() {}

  // Most of these functions are similar to the Pepper functions of the same
  // name, so not repeating the description here unless it's different.
  virtual bool New(const char* url, const char* headers) = 0;
  virtual void PageOffsetUpdated(const pp::Point& page_offset) = 0;
  virtual void PluginSizeUpdated(const pp::Size& size) = 0;
  virtual void ScrolledToXPosition(int position) = 0;
  virtual void ScrolledToYPosition(int position) = 0;
  // Paint is called a series of times. Before these n calls are made, PrePaint
  // is called once. After Paint is called n times, PostPaint is called once.
  virtual void PrePaint() = 0;
  virtual void Paint(const pp::Rect& rect,
                     pp::ImageData* image_data,
                     std::vector<pp::Rect>* ready,
                     std::vector<pp::Rect>* pending) = 0;
  virtual void PostPaint() = 0;
  virtual bool HandleDocumentLoad(const pp::URLLoader& loader) = 0;
  virtual bool HandleEvent(const pp::InputEvent& event) = 0;
  virtual uint32_t QuerySupportedPrintOutputFormats() = 0;
  virtual void PrintBegin() = 0;
  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings) = 0;
  virtual void PrintEnd() = 0;
  virtual void StartFind(const std::string& text, bool case_sensitive) = 0;
  virtual bool SelectFindResult(bool forward) = 0;
  virtual void StopFind() = 0;
  virtual void ZoomUpdated(double new_zoom_level) = 0;
  virtual void RotateClockwise() = 0;
  virtual void RotateCounterclockwise() = 0;
  virtual std::string GetSelectedText() = 0;
  virtual std::string GetLinkAtPosition(const pp::Point& point) = 0;
  // Checks the permissions associated with this document.
  virtual bool HasPermission(DocumentPermission permission) const = 0;
  virtual void SelectAll() = 0;
  // Gets the number of pages in the document.
  virtual int GetNumberOfPages() = 0;
  // Gets the 0-based page number of |destination|, or -1 if it does not exist.
  virtual int GetNamedDestinationPage(const std::string& destination) = 0;
  // Gets the index of the most visible page, or -1 if none are visible.
  virtual int GetMostVisiblePage() = 0;
  // Gets the rectangle of the page including shadow.
  virtual pp::Rect GetPageRect(int index) = 0;
  // Gets the rectangle of the page not including the shadow.
  virtual pp::Rect GetPageBoundsRect(int index) = 0;
  // Gets the rectangle of the page excluding any additional areas.
  virtual pp::Rect GetPageContentsRect(int index) = 0;
  // Returns a page's rect in screen coordinates, as well as its surrounding
  // border areas and bottom separator.
  virtual pp::Rect GetPageScreenRect(int page_index) const = 0;
  // Gets the offset of the vertical scrollbar from the top in document
  // coordinates.
  virtual int GetVerticalScrollbarYPosition() = 0;
  // Set color / grayscale rendering modes.
  virtual void SetGrayscale(bool grayscale) = 0;
  // Callback for timer that's set with ScheduleCallback().
  virtual void OnCallback(int id) = 0;
  // Get the number of characters on a given page.
  virtual int GetCharCount(int page_index) = 0;
  // Get the bounds in page pixels of a character on a given page.
  virtual pp::FloatRect GetCharBounds(int page_index, int char_index) = 0;
  // Get a given unicode character on a given page.
  virtual uint32_t GetCharUnicode(int page_index, int char_index) = 0;
  // Given a start char index, find the longest continuous run of text that's
  // in a single direction and with the same style and font size. Return the
  // length of that sequence and its font size and bounding box.
  virtual void GetTextRunInfo(int page_index,
                              int start_char_index,
                              uint32_t* out_len,
                              double* out_font_size,
                              pp::FloatRect* out_bounds) = 0;
  // Gets the PDF document's print scaling preference. True if the document can
  // be scaled to fit.
  virtual bool GetPrintScaling() = 0;
  // Returns number of copies to be printed.
  virtual int GetCopiesToPrint() = 0;
  // Returns the duplex setting.
  virtual int GetDuplexType() = 0;
  // Returns true if all the pages are the same size.
  virtual bool GetPageSizeAndUniformity(pp::Size* size) = 0;

  // Returns a VarArray of Bookmarks, each a VarDictionary containing the
  // following key/values:
  // - "title" - a string Var.
  // - "page" - an int Var.
  // - "children" - a VarArray(), with each entry containing a VarDictionary of
  //   the same structure.
  virtual pp::VarArray GetBookmarks() = 0;

  // Append blank pages to make a 1-page document to a |num_pages| document.
  // Always retain the first page data.
  virtual void AppendBlankPages(int num_pages) = 0;
  // Append the first page of the document loaded with the |engine| to this
  // document at page |index|.
  virtual void AppendPage(PDFEngine* engine, int index) = 0;

  // Allow client to query and reset scroll positions in document coordinates.
  // Note that this is meant for cases where the device scale factor changes,
  // and not for general scrolling - the engine will not repaint due to this.
  virtual pp::Point GetScrollPosition() = 0;
  virtual void SetScrollPosition(const pp::Point& position) = 0;

  virtual bool IsProgressiveLoad() = 0;

  virtual std::string GetMetadata(const std::string& key) = 0;
};

// Interface for exports that wrap the PDF engine.
class PDFEngineExports {
 public:
  struct RenderingSettings {
    RenderingSettings(int dpi_x,
                      int dpi_y,
                      const pp::Rect& bounds,
                      bool fit_to_bounds,
                      bool stretch_to_bounds,
                      bool keep_aspect_ratio,
                      bool center_in_bounds,
                      bool autorotate)
        : dpi_x(dpi_x), dpi_y(dpi_y), bounds(bounds),
        fit_to_bounds(fit_to_bounds), stretch_to_bounds(stretch_to_bounds),
        keep_aspect_ratio(keep_aspect_ratio),
        center_in_bounds(center_in_bounds), autorotate(autorotate) {
    }
    int dpi_x;
    int dpi_y;
    pp::Rect bounds;
    bool fit_to_bounds;
    bool stretch_to_bounds;
    bool keep_aspect_ratio;
    bool center_in_bounds;
    bool autorotate;
  };

  PDFEngineExports() {}
  virtual ~PDFEngineExports() {}
  static PDFEngineExports* Get();

#if defined(OS_WIN)
  // See the definition of RenderPDFPageToDC in pdf.cc for details.
  virtual bool RenderPDFPageToDC(const void* pdf_buffer,
                                 int buffer_size,
                                 int page_number,
                                 const RenderingSettings& settings,
                                 HDC dc) = 0;
#endif  // OS_WIN
  // See the definition of RenderPDFPageToBitmap in pdf.cc for details.
  virtual bool RenderPDFPageToBitmap(const void* pdf_buffer,
                                     int pdf_buffer_size,
                                     int page_number,
                                     const RenderingSettings& settings,
                                     void* bitmap_buffer) = 0;

  virtual bool GetPDFDocInfo(const void* pdf_buffer,
                             int buffer_size,
                             int* page_count,
                             double* max_page_width) = 0;

  // See the definition of GetPDFPageSizeByIndex in pdf.cc for details.
  virtual bool GetPDFPageSizeByIndex(const void* pdf_buffer,
                                     int pdf_buffer_size, int page_number,
                                     double* width, double* height) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ENGINE_H_
