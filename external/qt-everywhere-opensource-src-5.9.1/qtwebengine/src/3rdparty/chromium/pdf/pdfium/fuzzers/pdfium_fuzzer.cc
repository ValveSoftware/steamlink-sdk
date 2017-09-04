// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This fuzzer is simplified & cleaned up pdfium/samples/pdfium_test.cc

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "third_party/pdfium/public/fpdf_dataavail.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_text.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/pdfium/testing/test_support.h"

#include "v8/include/v8.h"

static int ExampleAppAlert(IPDF_JSPLATFORM*,
                           FPDF_WIDESTRING,
                           FPDF_WIDESTRING,
                           int,
                           int) {
  return 0;
}

static void ExampleDocGotoPage(IPDF_JSPLATFORM*, int pageNumber) {}

static void ExampleUnsupportedHandler(UNSUPPORT_INFO*, int type) {}

FPDF_BOOL Is_Data_Avail(FX_FILEAVAIL* pThis, size_t offset, size_t size) {
  return true;
}

static void Add_Segment(FX_DOWNLOADHINTS* pThis, size_t offset, size_t size) {}

static bool RenderPage(const FPDF_DOCUMENT& doc,
                       const FPDF_FORMHANDLE& form,
                       const int page_index) {
  FPDF_PAGE page = FPDF_LoadPage(doc, page_index);
  if (!page)
    return false;

  FPDF_TEXTPAGE text_page = FPDFText_LoadPage(page);
  FORM_OnAfterLoadPage(page, form);
  FORM_DoPageAAction(page, form, FPDFPAGE_AACTION_OPEN);

  const double scale = 1.0;
  int width = static_cast<int>(FPDF_GetPageWidth(page) * scale);
  int height = static_cast<int>(FPDF_GetPageHeight(page) * scale);

  FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 0);
  if (bitmap) {
    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, 0);

    FPDF_FFLDraw(form, bitmap, page, 0, 0, width, height, 0, 0);

    FPDFBitmap_Destroy(bitmap);
  }
  FORM_DoPageAAction(page, form, FPDFPAGE_AACTION_CLOSE);
  FORM_OnBeforeClosePage(page, form);
  FPDFText_ClosePage(text_page);
  FPDF_ClosePage(page);
  return !!bitmap;
}

static void RenderPdf(const char* pBuf, size_t len) {
  IPDF_JSPLATFORM platform_callbacks;
  memset(&platform_callbacks, '\0', sizeof(platform_callbacks));
  platform_callbacks.version = 3;
  platform_callbacks.app_alert = ExampleAppAlert;
  platform_callbacks.Doc_gotoPage = ExampleDocGotoPage;

  FPDF_FORMFILLINFO form_callbacks;
  memset(&form_callbacks, '\0', sizeof(form_callbacks));
  form_callbacks.version = 1;
  form_callbacks.m_pJsPlatform = &platform_callbacks;

  TestLoader loader(pBuf, len);
  FPDF_FILEACCESS file_access;
  memset(&file_access, '\0', sizeof(file_access));
  file_access.m_FileLen = static_cast<unsigned long>(len);
  file_access.m_GetBlock = TestLoader::GetBlock;
  file_access.m_Param = &loader;

  FX_FILEAVAIL file_avail;
  memset(&file_avail, '\0', sizeof(file_avail));
  file_avail.version = 1;
  file_avail.IsDataAvail = Is_Data_Avail;

  FX_DOWNLOADHINTS hints;
  memset(&hints, '\0', sizeof(hints));
  hints.version = 1;
  hints.AddSegment = Add_Segment;

  FPDF_DOCUMENT doc;
  int nRet = PDF_DATA_NOTAVAIL;
  bool bIsLinearized = false;
  FPDF_AVAIL pdf_avail = FPDFAvail_Create(&file_avail, &file_access);

  if (FPDFAvail_IsLinearized(pdf_avail) == PDF_LINEARIZED) {
    doc = FPDFAvail_GetDocument(pdf_avail, nullptr);
    if (doc) {
      while (nRet == PDF_DATA_NOTAVAIL) {
        nRet = FPDFAvail_IsDocAvail(pdf_avail, &hints);
      }
      if (nRet == PDF_DATA_ERROR) {
        return;
      }
      nRet = FPDFAvail_IsFormAvail(pdf_avail, &hints);
      if (nRet == PDF_FORM_ERROR || nRet == PDF_FORM_NOTAVAIL) {
        return;
      }
      bIsLinearized = true;
    }
  } else {
    doc = FPDF_LoadCustomDocument(&file_access, nullptr);
  }

  if (!doc) {
    FPDFAvail_Destroy(pdf_avail);
    return;
  }

  (void)FPDF_GetDocPermissions(doc);

  FPDF_FORMHANDLE form = FPDFDOC_InitFormFillEnvironment(doc, &form_callbacks);
  FPDF_SetFormFieldHighlightColor(form, 0, 0xFFE4DD);
  FPDF_SetFormFieldHighlightAlpha(form, 100);

  FORM_DoDocumentJSAction(form);
  FORM_DoDocumentOpenAction(form);

  int page_count = FPDF_GetPageCount(doc);

  for (int i = 0; i < page_count; ++i) {
    if (bIsLinearized) {
      nRet = PDF_DATA_NOTAVAIL;
      while (nRet == PDF_DATA_NOTAVAIL) {
        nRet = FPDFAvail_IsPageAvail(pdf_avail, i, &hints);
      }
      if (nRet == PDF_DATA_ERROR) {
        return;
      }
    }
    RenderPage(doc, form, i);
  }

  FORM_DoDocumentAAction(form, FPDFDOC_AACTION_WC);
  FPDFDOC_ExitFormFillEnvironment(form);
  FPDF_CloseDocument(doc);
  FPDFAvail_Destroy(pdf_avail);
}

std::string ProgramPath() {
#ifdef _MSC_VER
  wchar_t wpath[MAX_PATH];
  char path[MAX_PATH];
  DWORD res = GetModuleFileName(NULL, wpath, MAX_PATH);
  assert(res != 0);
  wcstombs(path, wpath, MAX_PATH);
  return std::string(path, res);
#else
  char* path = new char[PATH_MAX + 1];
  assert(path);
  ssize_t sz = readlink("/proc/self/exe", path, PATH_MAX);
  assert(sz > 0);
  std::string result(path, sz);
  delete[] path;
  return result;
#endif
}

struct TestCase {
  TestCase() {
    InitializeV8ForPDFium(ProgramPath(), "", &natives_blob, &snapshot_blob,
                          &platform);

    memset(&config, '\0', sizeof(config));
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);

    memset(&unsupport_info, '\0', sizeof(unsupport_info));
    unsupport_info.version = 1;
    unsupport_info.FSDK_UnSupport_Handler = ExampleUnsupportedHandler;
    FSDK_SetUnSpObjProcessHandler(&unsupport_info);
  }

  v8::Platform* platform;
  v8::StartupData natives_blob;
  v8::StartupData snapshot_blob;
  FPDF_LIBRARY_CONFIG config;
  UNSUPPORT_INFO unsupport_info;
};

static TestCase* testCase = new TestCase();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  RenderPdf(reinterpret_cast<const char*>(data), size);
  return 0;
}
