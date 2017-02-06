// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "printing/units.h"

// Used when doing hit detection.
#define kTolerance 20.0

using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using printing::kPixelsPerInch;

namespace {

pp::FloatRect FloatPageRectToPixelRect(FPDF_PAGE page,
                                       const pp::FloatRect& input) {
  int output_width = FPDF_GetPageWidth(page);
  int output_height = FPDF_GetPageHeight(page);

  int min_x;
  int min_y;
  int max_x;
  int max_y;
  FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0, input.x(),
                    input.y(), &min_x, &min_y);
  FPDF_PageToDevice(page, 0, 0, output_width, output_height, 0, input.right(),
                    input.bottom(), &max_x, &max_y);

  if (max_x < min_x)
    std::swap(min_x, max_x);
  if (max_y < min_y)
    std::swap(min_y, max_y);

  pp::FloatRect output_rect(
      ConvertUnitDouble(min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(min_y, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(max_x - min_x, kPointsPerInch, kPixelsPerInch),
      ConvertUnitDouble(max_y - min_y, kPointsPerInch, kPixelsPerInch));
  return output_rect;
}

pp::FloatRect GetFloatCharRectInPixels(FPDF_PAGE page,
                                       FPDF_TEXTPAGE text_page,
                                       int index) {
  double left, right, bottom, top;
  FPDFText_GetCharBox(text_page, index, &left, &right, &bottom, &top);
  if (right < left)
    std::swap(left, right);
  if (bottom < top)
    std::swap(top, bottom);
  pp::FloatRect page_coords(left, top, right - left, bottom - top);
  return FloatPageRectToPixelRect(page, page_coords);
}

bool OverlapsOnYAxis(const pp::FloatRect &a, const pp::FloatRect& b) {
  return !(a.IsEmpty() || b.IsEmpty() ||
           a.bottom() < b.y() || b.bottom() < a.y());
}

}  // namespace

namespace chrome_pdf {

PDFiumPage::PDFiumPage(PDFiumEngine* engine,
                       int i,
                       const pp::Rect& r,
                       bool available)
    : engine_(engine),
      page_(NULL),
      text_page_(NULL),
      index_(i),
      loading_count_(0),
      rect_(r),
      calculated_links_(false),
      available_(available) {
}

PDFiumPage::~PDFiumPage() {
  DCHECK_EQ(0, loading_count_);
}

void PDFiumPage::Unload() {
  // Do not unload while in the middle of a load.
  if (loading_count_)
    return;

  if (text_page_) {
    FPDFText_ClosePage(text_page_);
    text_page_ = NULL;
  }

  if (page_) {
    if (engine_->form()) {
      FORM_OnBeforeClosePage(page_, engine_->form());
    }
    FPDF_ClosePage(page_);
    page_ = NULL;
  }
}

FPDF_PAGE PDFiumPage::GetPage() {
  ScopedUnsupportedFeature scoped_unsupported_feature(engine_);
  if (!available_)
    return NULL;
  if (!page_) {
    ScopedLoadCounter scoped_load(this);
    page_ = FPDF_LoadPage(engine_->doc(), index_);
    if (page_ && engine_->form()) {
      FORM_OnAfterLoadPage(page_, engine_->form());
    }
  }
  return page_;
}

FPDF_PAGE PDFiumPage::GetPrintPage() {
  ScopedUnsupportedFeature scoped_unsupported_feature(engine_);
  if (!available_)
    return NULL;
  if (!page_) {
    ScopedLoadCounter scoped_load(this);
    page_ = FPDF_LoadPage(engine_->doc(), index_);
  }
  return page_;
}

void PDFiumPage::ClosePrintPage() {
  // Do not close |page_| while in the middle of a load.
  if (loading_count_)
    return;

  if (page_) {
    FPDF_ClosePage(page_);
    page_ = NULL;
  }
}

FPDF_TEXTPAGE PDFiumPage::GetTextPage() {
  if (!available_)
    return NULL;
  if (!text_page_) {
    ScopedLoadCounter scoped_load(this);
    text_page_ = FPDFText_LoadPage(GetPage());
  }
  return text_page_;
}

void PDFiumPage::GetTextRunInfo(int start_char_index,
                                uint32_t* out_len,
                                double* out_font_size,
                                pp::FloatRect* out_bounds) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  int chars_count = FPDFText_CountChars(text_page);
  int char_index = start_char_index;
  while (
      char_index < chars_count &&
      base::IsUnicodeWhitespace(FPDFText_GetUnicode(text_page, char_index))) {
    char_index++;
  }
  int text_run_font_size = FPDFText_GetFontSize(text_page, char_index);
  pp::FloatRect text_run_bounds =
      GetFloatCharRectInPixels(page, text_page, char_index);
  char_index++;
  while (char_index < chars_count) {
    unsigned int character = FPDFText_GetUnicode(text_page, char_index);

    if (!base::IsUnicodeWhitespace(character)) {
      // TODO(dmazzoni): this assumes horizontal text.
      // https://crbug.com/580311
      pp::FloatRect char_rect = GetFloatCharRectInPixels(
          page, text_page, char_index);
      if (!char_rect.IsEmpty() && !OverlapsOnYAxis(text_run_bounds, char_rect))
        break;

      int font_size = FPDFText_GetFontSize(text_page, char_index);
      if (font_size != text_run_font_size)
        break;

      // Heuristic: split a text run after a space longer than 3 average
      // characters.
      double avg_char_width =
          text_run_bounds.width() / (char_index - start_char_index);
      if (char_rect.x() - text_run_bounds.right() > avg_char_width * 3)
        break;

      text_run_bounds = text_run_bounds.Union(char_rect);
    }

    char_index++;
  }

  // Some PDFs have missing or obviously bogus font sizes; substitute the
  // height of the bounding box in those cases.
  if (text_run_font_size <= 1 ||
      text_run_font_size < text_run_bounds.height() / 2 ||
      text_run_font_size > text_run_bounds.height() * 2) {
    text_run_font_size = text_run_bounds.height();
  }

  *out_len = char_index - start_char_index;
  *out_font_size = text_run_font_size;
  *out_bounds = text_run_bounds;
}

uint32_t PDFiumPage::GetCharUnicode(int char_index) {
  FPDF_TEXTPAGE text_page = GetTextPage();
  return FPDFText_GetUnicode(text_page, char_index);
}

pp::FloatRect PDFiumPage::GetCharBounds(int char_index) {
  FPDF_PAGE page = GetPage();
  FPDF_TEXTPAGE text_page = GetTextPage();
  return GetFloatCharRectInPixels(page, text_page, char_index);
}

PDFiumPage::Area PDFiumPage::GetCharIndex(const pp::Point& point,
                                          int rotation,
                                          int* char_index,
                                          int* form_type,
                                          LinkTarget* target) {
  if (!available_)
    return NONSELECTABLE_AREA;
  pp::Point point2 = point - rect_.point();
  double new_x;
  double new_y;
  FPDF_DeviceToPage(GetPage(), 0, 0, rect_.width(), rect_.height(),
        rotation, point2.x(), point2.y(), &new_x, &new_y);

  int rv = FPDFText_GetCharIndexAtPos(
      GetTextPage(), new_x, new_y, kTolerance, kTolerance);
  *char_index = rv;

  FPDF_LINK link = FPDFLink_GetLinkAtPoint(GetPage(), new_x, new_y);
  int control =
      FPDFPage_HasFormFieldAtPoint(engine_->form(), GetPage(), new_x, new_y);

  // If there is a control and link at the same point, figure out their z-order
  // to determine which is on top.
  if (link && control > FPDF_FORMFIELD_UNKNOWN) {
    int control_z_order = FPDFPage_FormFieldZOrderAtPoint(
        engine_->form(), GetPage(), new_x, new_y);
    int link_z_order = FPDFLink_GetLinkZOrderAtPoint(GetPage(), new_x, new_y);
    DCHECK_NE(control_z_order, link_z_order);
    if (control_z_order > link_z_order) {
      *form_type = control;
      return PDFiumPage::NONSELECTABLE_AREA;
    }

    // We don't handle all possible link types of the PDF. For example,
    // launch actions, cross-document links, etc.
    // In that case, GetLinkTarget() will return NONSELECTABLE_AREA
    // and we should proceed with area detection.
    PDFiumPage::Area area = GetLinkTarget(link, target);
    if (area != PDFiumPage::NONSELECTABLE_AREA)
      return area;
  } else if (link) {
    // We don't handle all possible link types of the PDF. For example,
    // launch actions, cross-document links, etc.
    // See identical block above.
    PDFiumPage::Area area = GetLinkTarget(link, target);
    if (area != PDFiumPage::NONSELECTABLE_AREA)
      return area;
  } else if (control > FPDF_FORMFIELD_UNKNOWN) {
    *form_type = control;
    return PDFiumPage::NONSELECTABLE_AREA;
  }

  if (rv < 0)
    return NONSELECTABLE_AREA;

  return GetLink(*char_index, target) != -1 ? WEBLINK_AREA : TEXT_AREA;
}

base::char16 PDFiumPage::GetCharAtIndex(int index) {
  if (!available_)
    return L'\0';
  return static_cast<base::char16>(FPDFText_GetUnicode(GetTextPage(), index));
}

int PDFiumPage::GetCharCount() {
  if (!available_)
    return 0;
  return FPDFText_CountChars(GetTextPage());
}

PDFiumPage::Area PDFiumPage::GetLinkTarget(
    FPDF_LINK link, PDFiumPage::LinkTarget* target) const {
  FPDF_DEST dest = FPDFLink_GetDest(engine_->doc(), link);
  if (dest)
    return GetDestinationTarget(dest, target);

  FPDF_ACTION action = FPDFLink_GetAction(link);
  if (action) {
    switch (FPDFAction_GetType(action)) {
      case PDFACTION_GOTO: {
          FPDF_DEST dest = FPDFAction_GetDest(engine_->doc(), action);
          if (dest)
            return GetDestinationTarget(dest, target);
          // TODO(gene): We don't fully support all types of the in-document
          // links. Need to implement that. There is a bug to track that:
          // http://code.google.com/p/chromium/issues/detail?id=55776
        } break;
      case PDFACTION_URI: {
          if (target) {
            size_t buffer_size =
                FPDFAction_GetURIPath(engine_->doc(), action, NULL, 0);
            if (buffer_size > 0) {
              PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
                  &target->url, buffer_size, true);
              void* data = api_string_adapter.GetData();
              size_t bytes_written = FPDFAction_GetURIPath(
                  engine_->doc(), action, data, buffer_size);
              api_string_adapter.Close(bytes_written);
            }
          }
          return WEBLINK_AREA;
        } break;
      // TODO(gene): We don't support PDFACTION_REMOTEGOTO and PDFACTION_LAUNCH
      // at the moment.
    }
  }

  return NONSELECTABLE_AREA;
}

PDFiumPage::Area PDFiumPage::GetDestinationTarget(
    FPDF_DEST destination, PDFiumPage::LinkTarget* target) const {
  if (target)
    target->page = FPDFDest_GetPageIndex(engine_->doc(), destination);
  return DOCLINK_AREA;
}

int PDFiumPage::GetLink(int char_index, PDFiumPage::LinkTarget* target) {
  if (!available_)
    return -1;

  CalculateLinks();

  // Get the bounding box of the rect again, since it might have moved because
  // of the tolerance above.
  double left, right, bottom, top;
  FPDFText_GetCharBox(GetTextPage(), char_index, &left, &right, &bottom, &top);

  pp::Point origin(
      PageToScreen(pp::Point(), 1.0, left, top, right, bottom, 0).point());
  for (size_t i = 0; i < links_.size(); ++i) {
    for (const auto& rect : links_[i].rects) {
      if (rect.Contains(origin)) {
        if (target)
          target->url = links_[i].url;
        return i;
      }
    }
  }
  return -1;
}

std::vector<int> PDFiumPage::GetLinks(pp::Rect text_area,
                                      std::vector<LinkTarget>* targets) {
  std::vector<int> links;
  if (!available_)
    return links;

  CalculateLinks();

  for (size_t i = 0; i < links_.size(); ++i) {
    for (const auto& rect : links_[i].rects) {
      if (rect.Intersects(text_area)) {
        if (targets) {
          LinkTarget target;
          target.url = links_[i].url;
          targets->push_back(target);
        }
        links.push_back(i);
      }
    }
  }
  return links;
}

void PDFiumPage::CalculateLinks() {
  if (calculated_links_)
    return;

  calculated_links_ = true;
  FPDF_PAGELINK links = FPDFLink_LoadWebLinks(GetTextPage());
  int count = FPDFLink_CountWebLinks(links);
  for (int i = 0; i < count; ++i) {
    base::string16 url;
    int url_length = FPDFLink_GetURL(links, i, NULL, 0);
    if (url_length > 0) {
      PDFiumAPIStringBufferAdapter<base::string16> api_string_adapter(
          &url, url_length, true);
      unsigned short* data =
          reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
      int actual_length = FPDFLink_GetURL(links, i, data, url_length);
      api_string_adapter.Close(actual_length);
    }
    Link link;
    link.url = base::UTF16ToUTF8(url);

    // If the link cannot be converted to a pp::Var, then it is not possible to
    // pass it to JS. In this case, ignore the link like other PDF viewers.
    // See http://crbug.com/312882 for an example.
    pp::Var link_var(link.url);
    if (!link_var.is_string())
      continue;

    // Make sure all the characters in the URL are valid per RFC 1738.
    // http://crbug.com/340326 has a sample bad PDF.
    // GURL does not work correctly, e.g. it just strips \t \r \n.
    bool is_invalid_url = false;
    for (size_t j = 0; j < link.url.length(); ++j) {
      // Control characters are not allowed.
      // 0x7F is also a control character.
      // 0x80 and above are not in US-ASCII.
      if (link.url[j] < ' ' || link.url[j] >= '\x7F') {
        is_invalid_url = true;
        break;
      }
    }
    if (is_invalid_url)
      continue;

    int rect_count = FPDFLink_CountRects(links, i);
    for (int j = 0; j < rect_count; ++j) {
      double left, top, right, bottom;
      FPDFLink_GetRect(links, i, j, &left, &top, &right, &bottom);
      link.rects.push_back(
          PageToScreen(pp::Point(), 1.0, left, top, right, bottom, 0));
    }
    links_.push_back(link);
  }
  FPDFLink_CloseWebLinks(links);
}

pp::Rect PDFiumPage::PageToScreen(const pp::Point& offset,
                                  double zoom,
                                  double left,
                                  double top,
                                  double right,
                                  double bottom,
                                  int rotation) const {
  if (!available_)
    return pp::Rect();

  int new_left, new_top, new_right, new_bottom;
  FPDF_PageToDevice(
      page_,
      static_cast<int>((rect_.x() - offset.x()) * zoom),
      static_cast<int>((rect_.y() - offset.y()) * zoom),
      static_cast<int>(ceil(rect_.width() * zoom)),
      static_cast<int>(ceil(rect_.height() * zoom)),
      rotation, left, top, &new_left, &new_top);
  FPDF_PageToDevice(
      page_,
      static_cast<int>((rect_.x() - offset.x()) * zoom),
      static_cast<int>((rect_.y() - offset.y()) * zoom),
      static_cast<int>(ceil(rect_.width() * zoom)),
      static_cast<int>(ceil(rect_.height() * zoom)),
      rotation, right, bottom, &new_right, &new_bottom);

  // If the PDF is rotated, the horizontal/vertical coordinates could be
  // flipped.  See
  // http://www.netl.doe.gov/publications/proceedings/03/ubc/presentations/Goeckner-pres.pdf
  if (new_right < new_left)
    std::swap(new_right, new_left);
  if (new_bottom < new_top)
    std::swap(new_bottom, new_top);

  return pp::Rect(
      new_left, new_top, new_right - new_left + 1, new_bottom - new_top + 1);
}

PDFiumPage::ScopedLoadCounter::ScopedLoadCounter(PDFiumPage* page)
    : page_(page) {
  page_->loading_count_++;
}

PDFiumPage::ScopedLoadCounter::~ScopedLoadCounter() {
  page_->loading_count_--;
}

PDFiumPage::Link::Link() {
}

PDFiumPage::Link::~Link() {
}

}  // namespace chrome_pdf
