// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_XFA_FFDOC_H_
#define XFA_FXFA_XFA_FFDOC_H_

#include <map>
#include <memory>

#include "xfa/fxfa/fxfa.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_document_parser.h"

class CXFA_ChecksumContext;
class CXFA_FFApp;
class CXFA_FFNotify;
class CXFA_FFDocView;

struct FX_IMAGEDIB_AND_DPI {
  CFX_DIBSource* pDibSource;
  int32_t iImageXDpi;
  int32_t iImageYDpi;
};

class CXFA_FFDoc {
 public:
  CXFA_FFDoc(CXFA_FFApp* pApp, IXFA_DocEnvironment* pDocEnvironment);
  ~CXFA_FFDoc();

  IXFA_DocEnvironment* GetDocEnvironment() const { return m_pDocEnvironment; }
  uint32_t GetDocType();
  void SetDocType(uint32_t dwType);

  int32_t StartLoad();
  int32_t DoLoad(IFX_Pause* pPause = nullptr);
  void StopLoad();

  CXFA_FFDocView* CreateDocView(uint32_t dwView = 0);

  bool OpenDoc(IFX_SeekableReadStream* pStream, bool bTakeOverFile);
  bool OpenDoc(CPDF_Document* pPDFDoc);
  bool CloseDoc();

  CXFA_Document* GetXFADoc() { return m_pDocumentParser->GetDocument(); }
  CXFA_FFApp* GetApp() { return m_pApp; }
  CXFA_FFDocView* GetDocView(CXFA_LayoutProcessor* pLayout);
  CXFA_FFDocView* GetDocView();
  CPDF_Document* GetPDFDoc();
  CFX_DIBitmap* GetPDFNamedImage(const CFX_WideStringC& wsName,
                                 int32_t& iImageXDpi,
                                 int32_t& iImageYDpi);

  bool SavePackage(XFA_HashCode code,
                   IFX_SeekableWriteStream* pFile,
                   CXFA_ChecksumContext* pCSContext);
  bool ImportData(IFX_SeekableReadStream* pStream, bool bXDP = true);

 protected:
  IXFA_DocEnvironment* const m_pDocEnvironment;
  std::unique_ptr<CXFA_DocumentParser> m_pDocumentParser;
  IFX_SeekableReadStream* m_pStream;
  CXFA_FFApp* m_pApp;
  std::unique_ptr<CXFA_FFNotify> m_pNotify;
  CPDF_Document* m_pPDFDoc;
  std::map<uint32_t, FX_IMAGEDIB_AND_DPI> m_HashToDibDpiMap;
  std::map<uint32_t, std::unique_ptr<CXFA_FFDocView>> m_TypeToDocViewMap;
  uint32_t m_dwDocType;
  bool m_bOwnStream;
};

#endif  // XFA_FXFA_XFA_FFDOC_H_
