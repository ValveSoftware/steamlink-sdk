// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/pdf_resource.h"

#include <stdlib.h>
#include <string.h>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"

namespace ppapi {
namespace proxy {

namespace {

// TODO(raymes): This is just copied from render_thread_impl.cc. We should have
// generic code somewhere to get the locale in the plugin.
std::string GetLocale() {
  // The browser process should have passed the locale to the plugin via the
  // --lang command line flag.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::string& lang = parsed_command_line.GetSwitchValueASCII("lang");
  DCHECK(!lang.empty());
  return lang;
}

}  // namespace

PDFResource::PDFResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_PDF_Create());
}

PDFResource::~PDFResource() {
}

thunk::PPB_PDF_API* PDFResource::AsPPB_PDF_API() {
  return this;
}

PP_Var PDFResource::GetLocalizedString(PP_ResourceString string_id) {
  std::string localized_string;
  int32_t result = SyncCall<PpapiPluginMsg_PDF_GetLocalizedStringReply>(
      RENDERER, PpapiHostMsg_PDF_GetLocalizedString(string_id),
      &localized_string);
  if (result != PP_OK)
    return PP_MakeUndefined();
  return ppapi::StringVar::StringToPPVar(localized_string);
}

void PDFResource::SearchString(const unsigned short* input_string,
                               const unsigned short* input_term,
                               bool case_sensitive,
                               PP_PrivateFindResult** results, int* count) {
  if (locale_.empty())
    locale_ = GetLocale();
  const base::char16* string =
      reinterpret_cast<const base::char16*>(input_string);
  const base::char16* term =
      reinterpret_cast<const base::char16*>(input_term);

  UErrorCode status = U_ZERO_ERROR;
  UStringSearch* searcher = usearch_open(term, -1, string, -1, locale_.c_str(),
                                         0, &status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);
  UCollationStrength strength = case_sensitive ? UCOL_TERTIARY : UCOL_PRIMARY;

  UCollator* collator = usearch_getCollator(searcher);
  if (ucol_getStrength(collator) != strength) {
    ucol_setStrength(collator, strength);
    usearch_reset(searcher);
  }

  status = U_ZERO_ERROR;
  int match_start = usearch_first(searcher, &status);
  DCHECK(status == U_ZERO_ERROR);

  std::vector<PP_PrivateFindResult> pp_results;
  while (match_start != USEARCH_DONE) {
    size_t matched_length = usearch_getMatchedLength(searcher);
    PP_PrivateFindResult result;
    result.start_index = match_start;
    result.length = matched_length;
    pp_results.push_back(result);
    match_start = usearch_next(searcher, &status);
    DCHECK(status == U_ZERO_ERROR);
  }

  *count = pp_results.size();
  if (*count) {
    *results = reinterpret_cast<PP_PrivateFindResult*>(malloc(
        *count * sizeof(PP_PrivateFindResult)));
    memcpy(*results, &pp_results[0], *count * sizeof(PP_PrivateFindResult));
  } else {
    *results = NULL;
  }

  usearch_close(searcher);
}

void PDFResource::DidStartLoading() {
  Post(RENDERER, PpapiHostMsg_PDF_DidStartLoading());
}

void PDFResource::DidStopLoading() {
  Post(RENDERER, PpapiHostMsg_PDF_DidStopLoading());
}

void PDFResource::SetContentRestriction(int restrictions) {
  Post(RENDERER, PpapiHostMsg_PDF_SetContentRestriction(restrictions));
}

void PDFResource::HistogramPDFPageCount(int count) {
  UMA_HISTOGRAM_COUNTS_10000("PDF.PageCount", count);
}

void PDFResource::UserMetricsRecordAction(const PP_Var& action) {
  scoped_refptr<ppapi::StringVar> action_str(
      ppapi::StringVar::FromPPVar(action));
  if (action_str.get()) {
    Post(RENDERER,
         PpapiHostMsg_PDF_UserMetricsRecordAction(action_str->value()));
  }
}

void PDFResource::HasUnsupportedFeature() {
  Post(RENDERER, PpapiHostMsg_PDF_HasUnsupportedFeature());
}

void PDFResource::Print() {
  Post(RENDERER, PpapiHostMsg_PDF_Print());
}

void PDFResource::SaveAs() {
  Post(RENDERER, PpapiHostMsg_PDF_SaveAs());
}

PP_Bool PDFResource::IsFeatureEnabled(PP_PDFFeature feature) {
  PP_Bool result = PP_FALSE;
  switch (feature) {
    case PP_PDFFEATURE_HIDPI:
      result = PP_TRUE;
      break;
    case PP_PDFFEATURE_PRINTING:
      // TODO(raymes): Use PrintWebViewHelper::IsPrintingEnabled.
      result = PP_FALSE;
      break;
  }
  return result;
}

PP_Resource PDFResource::GetResourceImageForScale(PP_ResourceImage image_id,
                                                  float scale) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  int32_t result = GenericSyncCall(
      RENDERER, PpapiHostMsg_PDF_GetResourceImage(image_id, scale), &reply,
      &reply_params);
  if (result != PP_OK)
    return 0;

  HostResource resource;
  PP_ImageDataDesc image_desc;
  if (!UnpackMessage<PpapiPluginMsg_PDF_GetResourceImageReply>(
      reply, &resource, &image_desc)) {
    return 0;
  }

  if (resource.is_null())
    return 0;
  if (!PPB_ImageData_Shared::IsImageDataDescValid(image_desc))
    return 0;

  base::SharedMemoryHandle handle;
  if (!reply_params.TakeSharedMemoryHandleAtIndex(0, &handle))
    return 0;
  return (new SimpleImageData(resource, image_desc, handle))->GetReference();
}

PP_Resource PDFResource::GetResourceImage(PP_ResourceImage image_id) {
  return GetResourceImageForScale(image_id, 1.0f);
}

PP_Bool PDFResource::IsOutOfProcess() {
  return PP_TRUE;
}

void PDFResource::SetSelectedText(const char* selected_text) {
  Post(RENDERER,
       PpapiHostMsg_PDF_SetSelectedText(base::UTF8ToUTF16(selected_text)));
}

void PDFResource::SetLinkUnderCursor(const char* url) {
  Post(RENDERER, PpapiHostMsg_PDF_SetLinkUnderCursor(url));
}

}  // namespace proxy
}  // namespace ppapi
