// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PDF_API_H_
#define PPAPI_THUNK_PPB_PDF_API_H_

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/singleton_resource_id.h"

namespace ppapi {
namespace thunk {

class PPB_PDF_API {
 public:
  virtual PP_Var GetLocalizedString(PP_ResourceString string_id) = 0;
  virtual PP_Resource GetResourceImage(PP_ResourceImage image_id) = 0;
  virtual void SearchString(const unsigned short* input_string,
                            const unsigned short* input_term,
                            bool case_sensitive,
                            PP_PrivateFindResult** results,
                            int* count) = 0;
  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;
  virtual void SetContentRestriction(int restrictions) = 0;
  virtual void HistogramPDFPageCount(int count) = 0;
  virtual void UserMetricsRecordAction(const PP_Var& action) = 0;
  virtual void HasUnsupportedFeature() = 0;
  virtual void SaveAs() = 0;
  virtual PP_Bool IsFeatureEnabled(PP_PDFFeature feature) = 0;
  virtual void Print() = 0;
  virtual PP_Resource GetResourceImageForScale(PP_ResourceImage image_id,
                                               float scale) = 0;
  virtual PP_Bool IsOutOfProcess() = 0;
  virtual void SetSelectedText(const char* selected_text) = 0;
  virtual void SetLinkUnderCursor(const char* url) = 0;

  static const SingletonResourceID kSingletonResourceID = PDF_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_PDF_API_H_
