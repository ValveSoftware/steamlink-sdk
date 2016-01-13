// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_PDF_H_
#define PPAPI_CPP_PRIVATE_PDF_H_

#include <string>

#include "ppapi/c/private/ppb_pdf.h"

struct PP_BrowserFont_Trusted_Description;

namespace pp {

class ImageData;
class InstanceHandle;
class Var;

class PDF {
 public:
  // Returns true if the required interface is available.
  static bool IsAvailable();

  static Var GetLocalizedString(const InstanceHandle& instance,
                                PP_ResourceString string_id);
  static ImageData GetResourceImage(const InstanceHandle& instance,
                                    PP_ResourceImage image_id);
  static PP_Resource GetFontFileWithFallback(
      const InstanceHandle& instance,
      const PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset);
  static bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                             uint32_t table,
                                             void* output,
                                             uint32_t* output_length);
  static void SearchString(const InstanceHandle& instance,
                           const unsigned short* string,
                           const unsigned short* term,
                           bool case_sensitive,
                           PP_PrivateFindResult** results,
                           int* count);
  static void DidStartLoading(const InstanceHandle& instance);
  static void DidStopLoading(const InstanceHandle& instance);
  static void SetContentRestriction(const InstanceHandle& instance,
                                    int restrictions);
  static void HistogramPDFPageCount(const InstanceHandle& instance,
                                    int count);
  static void UserMetricsRecordAction(const InstanceHandle& instance,
                                      const Var& action);
  static void HasUnsupportedFeature(const InstanceHandle& instance);
  static void SaveAs(const InstanceHandle& instance);
  static void Print(const InstanceHandle& instance);
  static bool IsFeatureEnabled(const InstanceHandle& instance,
                               PP_PDFFeature feature);
  static ImageData GetResourceImageForScale(const InstanceHandle& instance,
                                            PP_ResourceImage image_id,
                                            float scale);
  static Var ModalPromptForPassword(const InstanceHandle& instance,
                                    Var message);
  static bool IsOutOfProcess(const InstanceHandle& instance);
  static void SetSelectedText(const InstanceHandle& instance,
                              const char* selected_text);
  static void SetLinkUnderCursor(const InstanceHandle& instance,
                                 const char* url);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_PDF_H_
