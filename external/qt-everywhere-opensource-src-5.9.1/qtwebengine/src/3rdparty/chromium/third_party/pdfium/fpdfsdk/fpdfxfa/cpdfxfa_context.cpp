// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"

#include "core/fpdfapi/parser/cpdf_document.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"
#include "fpdfsdk/fsdk_define.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/ijs_runtime.h"
#include "public/fpdf_formfill.h"
#include "third_party/base/ptr_util.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/xfa_ffapp.h"
#include "xfa/fxfa/xfa_ffdoc.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffpageview.h"
#include "xfa/fxfa/xfa_ffwidgethandler.h"
#include "xfa/fxfa/xfa_fontmgr.h"

#ifndef _WIN32
extern void SetLastError(int err);
extern int GetLastError();
#endif

CPDFXFA_Context::CPDFXFA_Context(std::unique_ptr<CPDF_Document> pPDFDoc)
    : m_iDocType(DOCTYPE_PDF),
      m_pPDFDoc(std::move(pPDFDoc)),
      m_pFormFillEnv(nullptr),
      m_pXFADocView(nullptr),
      m_nLoadStatus(FXFA_LOADSTATUS_PRELOAD),
      m_nPageCount(0),
      m_DocEnv(this) {
  m_pXFAApp = pdfium::MakeUnique<CXFA_FFApp>(this);
  m_pXFAApp->SetDefaultFontMgr(pdfium::MakeUnique<CXFA_DefFontMgr>());
}

CPDFXFA_Context::~CPDFXFA_Context() {
  m_nLoadStatus = FXFA_LOADSTATUS_CLOSING;

  // Must happen before we remove the form fill environment.
  CloseXFADoc();

  if (m_pFormFillEnv) {
    m_pFormFillEnv->ClearAllFocusedAnnots();
    // Once we're deleted the FormFillEnvironment will point at a bad underlying
    // doc so we need to reset it ...
    m_pFormFillEnv->ResetXFADocument();
    m_pFormFillEnv = nullptr;
  }

  m_nLoadStatus = FXFA_LOADSTATUS_CLOSED;
}

void CPDFXFA_Context::CloseXFADoc() {
  if (!m_pXFADoc)
    return;
  m_pXFADoc->CloseDoc();
  m_pXFADoc.reset();
  m_pXFADocView = nullptr;
}

void CPDFXFA_Context::SetFormFillEnv(
    CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  // The layout data can have pointers back into the script context. That
  // context will be different if the form fill environment closes, so, force
  // the layout data to clear.
  if (m_pXFADoc && m_pXFADoc->GetXFADoc())
    m_pXFADoc->GetXFADoc()->ClearLayoutData();

  m_pFormFillEnv = pFormFillEnv;
}

bool CPDFXFA_Context::LoadXFADoc() {
  m_nLoadStatus = FXFA_LOADSTATUS_LOADING;

  if (!m_pPDFDoc)
    return false;

  m_XFAPageList.RemoveAll();

  CXFA_FFApp* pApp = GetXFAApp();
  if (!pApp)
    return false;

  m_pXFADoc.reset(pApp->CreateDoc(&m_DocEnv, m_pPDFDoc.get()));
  if (!m_pXFADoc) {
    SetLastError(FPDF_ERR_XFALOAD);
    return false;
  }

  CXFA_FFDocHandler* pDocHandler = pApp->GetDocHandler();
  if (!pDocHandler) {
    SetLastError(FPDF_ERR_XFALOAD);
    return false;
  }

  m_pXFADoc->StartLoad();
  int iStatus = m_pXFADoc->DoLoad(nullptr);
  if (iStatus != XFA_PARSESTATUS_Done) {
    CloseXFADoc();
    SetLastError(FPDF_ERR_XFALOAD);
    return false;
  }
  m_pXFADoc->StopLoad();
  m_pXFADoc->GetXFADoc()->InitScriptContext(GetJSERuntime());

  if (m_pXFADoc->GetDocType() == XFA_DOCTYPE_Dynamic)
    m_iDocType = DOCTYPE_DYNAMIC_XFA;
  else
    m_iDocType = DOCTYPE_STATIC_XFA;

  m_pXFADocView = m_pXFADoc->CreateDocView(XFA_DOCVIEW_View);
  if (m_pXFADocView->StartLayout() < 0) {
    CloseXFADoc();
    SetLastError(FPDF_ERR_XFALAYOUT);
    return false;
  }

  m_pXFADocView->DoLayout(nullptr);
  m_pXFADocView->StopLayout();
  m_nLoadStatus = FXFA_LOADSTATUS_LOADED;

  return true;
}

int CPDFXFA_Context::GetPageCount() const {
  if (!m_pPDFDoc && !m_pXFADoc)
    return 0;

  switch (m_iDocType) {
    case DOCTYPE_PDF:
    case DOCTYPE_STATIC_XFA:
      if (m_pPDFDoc)
        return m_pPDFDoc->GetPageCount();
    case DOCTYPE_DYNAMIC_XFA:
      if (m_pXFADoc)
        return m_pXFADocView->CountPageViews();
    default:
      return 0;
  }
}

CPDFXFA_Page* CPDFXFA_Context::GetXFAPage(int page_index) {
  if (page_index < 0)
    return nullptr;

  CPDFXFA_Page* pPage = nullptr;
  int nCount = m_XFAPageList.GetSize();
  if (nCount > 0 && page_index < nCount) {
    pPage = m_XFAPageList.GetAt(page_index);
    if (pPage)
      pPage->Retain();
  } else {
    m_nPageCount = GetPageCount();
    m_XFAPageList.SetSize(m_nPageCount);
  }
  if (pPage)
    return pPage;

  pPage = new CPDFXFA_Page(this, page_index);
  if (!pPage->LoadPage()) {
    pPage->Release();
    return nullptr;
  }
  m_XFAPageList.SetAt(page_index, pPage);
  return pPage;
}

CPDFXFA_Page* CPDFXFA_Context::GetXFAPage(CXFA_FFPageView* pPage) const {
  if (!pPage)
    return nullptr;

  if (!m_pXFADoc)
    return nullptr;

  if (m_iDocType != DOCTYPE_DYNAMIC_XFA)
    return nullptr;

  int nSize = m_XFAPageList.GetSize();
  for (int i = 0; i < nSize; i++) {
    CPDFXFA_Page* pTempPage = m_XFAPageList.GetAt(i);
    if (!pTempPage)
      continue;
    if (pTempPage->GetXFAPageView() && pTempPage->GetXFAPageView() == pPage)
      return pTempPage;
  }

  return nullptr;
}

void CPDFXFA_Context::DeletePage(int page_index) {
  // Delete from the document first because, if GetPage was never called for
  // this |page_index| then |m_XFAPageList| may have size < |page_index| even
  // if it's a valid page in the document.
  if (m_pPDFDoc)
    m_pPDFDoc->DeletePage(page_index);

  if (page_index < 0 || page_index >= m_XFAPageList.GetSize())
    return;

  if (CPDFXFA_Page* pPage = m_XFAPageList.GetAt(page_index))
    pPage->Release();
}

void CPDFXFA_Context::RemovePage(CPDFXFA_Page* page) {
  m_XFAPageList.SetAt(page->GetPageIndex(), nullptr);
}

void CPDFXFA_Context::ClearChangeMark() {
  if (m_pFormFillEnv)
    m_pFormFillEnv->ClearChangeMark();
}

v8::Isolate* CPDFXFA_Context::GetJSERuntime() const {
  if (!m_pFormFillEnv)
    return nullptr;

  // XFA requires V8, if we have V8 then we have a CJS_Runtime and not the stub.
  CJS_Runtime* runtime =
      static_cast<CJS_Runtime*>(m_pFormFillEnv->GetJSRuntime());
  return runtime->GetIsolate();
}

void CPDFXFA_Context::GetAppName(CFX_WideString& wsName) {
  if (m_pFormFillEnv)
    wsName = m_pFormFillEnv->FFI_GetAppName();
}

void CPDFXFA_Context::GetLanguage(CFX_WideString& wsLanguage) {
  if (m_pFormFillEnv)
    wsLanguage = m_pFormFillEnv->GetLanguage();
}

void CPDFXFA_Context::GetPlatform(CFX_WideString& wsPlatform) {
  if (m_pFormFillEnv)
    wsPlatform = m_pFormFillEnv->GetPlatform();
}

void CPDFXFA_Context::Beep(uint32_t dwType) {
  if (m_pFormFillEnv)
    m_pFormFillEnv->JS_appBeep(dwType);
}

int32_t CPDFXFA_Context::MsgBox(const CFX_WideString& wsMessage,
                                const CFX_WideString& wsTitle,
                                uint32_t dwIconType,
                                uint32_t dwButtonType) {
  if (!m_pFormFillEnv)
    return -1;

  uint32_t iconType = 0;
  int iButtonType = 0;
  switch (dwIconType) {
    case XFA_MBICON_Error:
      iconType |= 0;
      break;
    case XFA_MBICON_Warning:
      iconType |= 1;
      break;
    case XFA_MBICON_Question:
      iconType |= 2;
      break;
    case XFA_MBICON_Status:
      iconType |= 3;
      break;
  }
  switch (dwButtonType) {
    case XFA_MB_OK:
      iButtonType |= 0;
      break;
    case XFA_MB_OKCancel:
      iButtonType |= 1;
      break;
    case XFA_MB_YesNo:
      iButtonType |= 2;
      break;
    case XFA_MB_YesNoCancel:
      iButtonType |= 3;
      break;
  }
  int32_t iRet = m_pFormFillEnv->JS_appAlert(wsMessage.c_str(), wsTitle.c_str(),
                                             iButtonType, iconType);
  switch (iRet) {
    case 1:
      return XFA_IDOK;
    case 2:
      return XFA_IDCancel;
    case 3:
      return XFA_IDNo;
    case 4:
      return XFA_IDYes;
  }
  return XFA_IDYes;
}

CFX_WideString CPDFXFA_Context::Response(const CFX_WideString& wsQuestion,
                                         const CFX_WideString& wsTitle,
                                         const CFX_WideString& wsDefaultAnswer,
                                         bool bMark) {
  CFX_WideString wsAnswer;
  if (!m_pFormFillEnv)
    return wsAnswer;

  int nLength = 2048;
  char* pBuff = new char[nLength];
  nLength = m_pFormFillEnv->JS_appResponse(wsQuestion.c_str(), wsTitle.c_str(),
                                           wsDefaultAnswer.c_str(), nullptr,
                                           bMark, pBuff, nLength);
  if (nLength > 0) {
    nLength = nLength > 2046 ? 2046 : nLength;
    pBuff[nLength] = 0;
    pBuff[nLength + 1] = 0;
    wsAnswer = CFX_WideString::FromUTF16LE(
        reinterpret_cast<const unsigned short*>(pBuff),
        nLength / sizeof(unsigned short));
  }
  delete[] pBuff;
  return wsAnswer;
}

IFX_SeekableReadStream* CPDFXFA_Context::DownloadURL(
    const CFX_WideString& wsURL) {
  return m_pFormFillEnv ? m_pFormFillEnv->DownloadFromURL(wsURL.c_str())
                        : nullptr;
}

bool CPDFXFA_Context::PostRequestURL(const CFX_WideString& wsURL,
                                     const CFX_WideString& wsData,
                                     const CFX_WideString& wsContentType,
                                     const CFX_WideString& wsEncode,
                                     const CFX_WideString& wsHeader,
                                     CFX_WideString& wsResponse) {
  if (!m_pFormFillEnv)
    return false;

  wsResponse = m_pFormFillEnv->PostRequestURL(
      wsURL.c_str(), wsData.c_str(), wsContentType.c_str(), wsEncode.c_str(),
      wsHeader.c_str());
  return true;
}

bool CPDFXFA_Context::PutRequestURL(const CFX_WideString& wsURL,
                                    const CFX_WideString& wsData,
                                    const CFX_WideString& wsEncode) {
  return m_pFormFillEnv &&
         m_pFormFillEnv->PutRequestURL(wsURL.c_str(), wsData.c_str(),
                                       wsEncode.c_str());
}

void CPDFXFA_Context::LoadString(int32_t iStringID, CFX_WideString& wsString) {
  switch (iStringID) {
    case XFA_IDS_ValidateFailed:
      wsString = L"%s validation failed";
      return;
    case XFA_IDS_CalcOverride:
      wsString = L"Calculate Override";
      return;
    case XFA_IDS_ModifyField:
      wsString = L"Are you sure you want to modify this field?";
      return;
    case XFA_IDS_NotModifyField:
      wsString = L"You are not allowed to modify this field.";
      return;
    case XFA_IDS_AppName:
      wsString = L"pdfium";
      return;
    case XFA_IDS_Unable_TO_SET:
      wsString = L"Unable to set ";
      return;
    case XFA_IDS_INVAlID_PROP_SET:
      wsString = L"Invalid property set operation.";
      return;
    case XFA_IDS_NOT_DEFAUL_VALUE:
      wsString = L" doesn't have a default property.";
      return;
    case XFA_IDS_UNABLE_SET_LANGUAGE:
      wsString = L"Unable to set language value.";
      return;
    case XFA_IDS_UNABLE_SET_NUMPAGES:
      wsString = L"Unable to set numPages value.";
      return;
    case XFA_IDS_UNABLE_SET_PLATFORM:
      wsString = L"Unable to set platform value.";
      return;
    case XFA_IDS_UNABLE_SET_VARIATION:
      wsString = L"Unable to set variation value.";
      return;
    case XFA_IDS_UNABLE_SET_VERSION:
      wsString = L"Unable to set version value.";
      return;
    case XFA_IDS_UNABLE_SET_READY:
      wsString = L"Unable to set ready value.";
      return;
    case XFA_IDS_COMPILER_ERROR:
      wsString = L"Compiler error.";
      return;
    case XFA_IDS_DIVIDE_ZERO:
      wsString = L"Divide by zero.";
      return;
    case XFA_IDS_ACCESS_PROPERTY_IN_NOT_OBJECT:
      wsString =
          L"An attempt was made to reference property '%s' of a non-object in "
          L"SOM expression %s.";
      return;
    case XFA_IDS_INDEX_OUT_OF_BOUNDS:
      wsString = L"Index value is out of bounds.";
      return;
    case XFA_IDS_INCORRECT_NUMBER_OF_METHOD:
      wsString = L"Incorrect number of parameters calling method '%s'.";
      return;
    case XFA_IDS_ARGUMENT_MISMATCH:
      wsString = L"Argument mismatch in property or function argument.";
      return;
    case XFA_IDS_NOT_HAVE_PROPERTY:
      wsString = L"'%s' doesn't have property '%s'.";
      return;
    case XFA_IDS_VIOLATE_BOUNDARY:
      wsString =
          L"The element [%s] has violated its allowable number of occurrences.";
      return;
    case XFA_IDS_SERVER_DENY:
      wsString = L"Server does not permit.";
      return;
    case XFA_IDS_ValidateLimit:
      wsString =
          L"Message limit exceeded. Remaining %d validation errors not "
          L"reported.";
      return;
    case XFA_IDS_ValidateNullWarning:
      wsString =
          L"%s cannot be blank. To ignore validations for %s, click Ignore.";
      return;
    case XFA_IDS_ValidateNullError:
      wsString = L"%s cannot be blank.";
      return;
    case XFA_IDS_ValidateWarning:
      wsString =
          L"The value you entered for %s is invalid. To ignore validations for "
          L"%s, click Ignore.";
      return;
    case XFA_IDS_ValidateError:
      wsString = L"The value you entered for %s is invalid.";
      return;
  }
}

IFWL_AdapterTimerMgr* CPDFXFA_Context::GetTimerMgr() {
  CXFA_FWLAdapterTimerMgr* pAdapter = nullptr;
  if (m_pFormFillEnv)
    pAdapter = new CXFA_FWLAdapterTimerMgr(m_pFormFillEnv);
  return pAdapter;
}
