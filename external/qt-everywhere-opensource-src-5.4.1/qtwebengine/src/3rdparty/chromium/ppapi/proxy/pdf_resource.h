// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PDF_RESOURCE_H_
#define PPAPI_PROXY_PDF_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_pdf_api.h"

namespace ppapi {
namespace proxy {

class PluginDispatcher;

class PPAPI_PROXY_EXPORT PDFResource
    : public PluginResource,
      public thunk::PPB_PDF_API {
 public:
  PDFResource(Connection connection, PP_Instance instance);
  virtual ~PDFResource();

  // For unittesting with a given locale.
  void SetLocaleForTest(const std::string& locale) {
    locale_ = locale;
  }

  // Resource override.
  virtual thunk::PPB_PDF_API* AsPPB_PDF_API() OVERRIDE;

  // PPB_PDF_API implementation.
  PP_Var GetLocalizedString(PP_ResourceString string_id) OVERRIDE;
  virtual void SearchString(const unsigned short* input_string,
                            const unsigned short* input_term,
                            bool case_sensitive,
                            PP_PrivateFindResult** results,
                            int* count) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void SetContentRestriction(int restrictions) OVERRIDE;
  virtual void HistogramPDFPageCount(int count) OVERRIDE;
  virtual void UserMetricsRecordAction(const PP_Var& action) OVERRIDE;
  virtual void HasUnsupportedFeature() OVERRIDE;
  virtual void Print() OVERRIDE;
  virtual void SaveAs() OVERRIDE;
  virtual PP_Bool IsFeatureEnabled(PP_PDFFeature feature) OVERRIDE;
  virtual PP_Resource GetResourceImageForScale(PP_ResourceImage image_id,
                                               float scale) OVERRIDE;
  virtual PP_Resource GetResourceImage(PP_ResourceImage image_id) OVERRIDE;
  virtual PP_Bool IsOutOfProcess() OVERRIDE;
  virtual void SetSelectedText(const char* selected_text) OVERRIDE;
  virtual void SetLinkUnderCursor(const char* url) OVERRIDE;

 private:
  std::string locale_;

  DISALLOW_COPY_AND_ASSIGN(PDFResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PDF_RESOURCE_H_
