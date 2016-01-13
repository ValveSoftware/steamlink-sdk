// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_flash_font_file_api.h"
#include "ppapi/thunk/ppb_pdf_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Var GetLocalizedString(PP_Instance instance, PP_ResourceString string_id) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetLocalizedString(string_id);
}

PP_Resource GetResourceImage(PP_Instance instance,
                             PP_ResourceImage image_id) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->GetResourceImage(image_id);
}

PP_Resource GetFontFileWithFallback(
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description* description,
    PP_PrivateFontCharset charset) {
  // TODO(raymes): Eventually we should replace the use of this function with
  // either PPB_Flash_Font_File or PPB_TrueType_Font directly in the PDF code.
  // For now just call into PPB_Flash_Font_File which has the exact same API.
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashFontFile(instance, description, charset);
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
  // TODO(raymes): Eventually we should replace the use of this function with
  // either PPB_Flash_Font_File or PPB_TrueType_Font directly in the PDF code.
  // For now just call into PPB_Flash_Font_File which has the exact same API.
  EnterResource<PPB_Flash_FontFile_API> enter(font_file, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_ToBool(enter.object()->GetFontTable(table, output, output_length));
}

void SearchString(PP_Instance instance,
                  const unsigned short* string,
                  const unsigned short* term,
                  bool case_sensitive,
                  PP_PrivateFindResult** results,
                  int* count) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SearchString(string, term, case_sensitive, results, count);
}

void DidStartLoading(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->DidStartLoading();
}

void DidStopLoading(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->DidStopLoading();
}

void SetContentRestriction(PP_Instance instance, int restrictions) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->SetContentRestriction(restrictions);
}

void HistogramPDFPageCount(PP_Instance instance, int count) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->HistogramPDFPageCount(count);
}

void UserMetricsRecordAction(PP_Instance instance, PP_Var action) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->UserMetricsRecordAction(action);
}

void HasUnsupportedFeature(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->HasUnsupportedFeature();
}

void SaveAs(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->SaveAs();
}

void Print(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->Print();
}

PP_Bool IsFeatureEnabled(PP_Instance instance, PP_PDFFeature feature) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFeatureEnabled(feature);
}

PP_Resource GetResourceImageForScale(PP_Instance instance,
                                     PP_ResourceImage image_id,
                                     float scale) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->GetResourceImageForScale(image_id, scale);
}

PP_Var ModalPromptForPassword(PP_Instance instance_id,
                              PP_Var message) {
  // TODO(raymes): Implement or remove this function.
  NOTIMPLEMENTED();
  return PP_MakeUndefined();
}

PP_Bool IsOutOfProcess(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsOutOfProcess();
}

void SetSelectedText(PP_Instance instance,
                     const char* selected_text) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->SetSelectedText(selected_text);
}

void SetLinkUnderCursor(PP_Instance instance, const char* url) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SetLinkUnderCursor(url);
}

const PPB_PDF g_ppb_pdf_thunk = {
  &GetLocalizedString,
  &GetResourceImage,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
  &SearchString,
  &DidStartLoading,
  &DidStopLoading,
  &SetContentRestriction,
  &HistogramPDFPageCount,
  &UserMetricsRecordAction,
  &HasUnsupportedFeature,
  &SaveAs,
  &Print,
  &IsFeatureEnabled,
  &GetResourceImageForScale,
  &ModalPromptForPassword,
  &IsOutOfProcess,
  &SetSelectedText,
  &SetLinkUnderCursor,
};

}  // namespace

const PPB_PDF* GetPPB_PDF_Thunk() {
  return &g_ppb_pdf_thunk;
}

}  // namespace thunk
}  // namespace ppapi
