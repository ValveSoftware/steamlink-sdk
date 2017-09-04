// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/xfa_ffdoc.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_nametree.h"
#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_memory.h"
#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fxfa/app/xfa_ffnotify.h"
#include "xfa/fxfa/parser/cxfa_dataexporter.h"
#include "xfa/fxfa/parser/cxfa_dataimporter.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/xfa_checksum.h"
#include "xfa/fxfa/xfa_ffapp.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffwidget.h"
#include "xfa/fxfa/xfa_fontmgr.h"

namespace {

struct FX_BASE64DATA {
  uint32_t data1 : 2;
  uint32_t data2 : 6;
  uint32_t data3 : 4;
  uint32_t data4 : 4;
  uint32_t data5 : 6;
  uint32_t data6 : 2;
  uint32_t data7 : 8;
};

const uint8_t kStartValuesRemoved = 43;
const uint8_t kDecoderMapSize = 80;
const uint8_t g_FXBase64DecoderMap[kDecoderMapSize] = {
    0x3E, 0xFF, 0xFF, 0xFF, 0x3F, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33,
};

uint8_t base64DecoderValue(uint8_t val) {
  if (val < kStartValuesRemoved || val >= kStartValuesRemoved + kDecoderMapSize)
    return 0xFF;
  return g_FXBase64DecoderMap[val - kStartValuesRemoved];
}

void Base64DecodePiece(const FX_CHAR src[4],
                       int32_t iChars,
                       FX_BASE64DATA& dst,
                       int32_t& iBytes) {
  ASSERT(iChars > 0 && iChars < 5);
  iBytes = 1;
  dst.data2 = base64DecoderValue(static_cast<uint8_t>(src[0]));
  if (iChars > 1) {
    uint8_t b = base64DecoderValue(static_cast<uint8_t>(src[1]));
    dst.data1 = b >> 4;
    dst.data4 = b;
    if (iChars > 2) {
      iBytes = 2;
      b = base64DecoderValue(static_cast<uint8_t>(src[2]));
      dst.data3 = b >> 2;
      dst.data6 = b;
      if (iChars > 3) {
        iBytes = 3;
        dst.data5 = base64DecoderValue(static_cast<uint8_t>(src[3]));
      } else {
        dst.data5 = 0;
      }
    } else {
      dst.data3 = 0;
    }
  } else {
    dst.data1 = 0;
  }
}

int32_t Base64DecodeW(const FX_WCHAR* pSrc, int32_t iSrcLen, uint8_t* pDst) {
  ASSERT(pSrc);
  if (iSrcLen < 1) {
    return 0;
  }
  while (iSrcLen > 0 && pSrc[iSrcLen - 1] == '=') {
    iSrcLen--;
  }
  if (iSrcLen < 1) {
    return 0;
  }
  if (!pDst) {
    int32_t iDstLen = iSrcLen / 4 * 3;
    iSrcLen %= 4;
    if (iSrcLen == 1) {
      iDstLen += 1;
    } else if (iSrcLen == 2) {
      iDstLen += 1;
    } else if (iSrcLen == 3) {
      iDstLen += 2;
    }
    return iDstLen;
  }
  FX_CHAR srcData[4];
  FX_BASE64DATA dstData;
  int32_t iChars = 4, iBytes;
  uint8_t* pDstEnd = pDst;
  while (iSrcLen > 0) {
    if (iSrcLen > 3) {
      srcData[0] = (FX_CHAR)*pSrc++;
      srcData[1] = (FX_CHAR)*pSrc++;
      srcData[2] = (FX_CHAR)*pSrc++;
      srcData[3] = (FX_CHAR)*pSrc++;
      iSrcLen -= 4;
    } else {
      *((uint32_t*)&dstData) = 0;
      *((uint32_t*)srcData) = 0;
      srcData[0] = (FX_CHAR)*pSrc++;
      if (iSrcLen > 1) {
        srcData[1] = (FX_CHAR)*pSrc++;
      }
      if (iSrcLen > 2) {
        srcData[2] = (FX_CHAR)*pSrc++;
      }
      iChars = iSrcLen;
      iSrcLen = 0;
    }
    Base64DecodePiece(srcData, iChars, dstData, iBytes);
    *pDstEnd++ = ((uint8_t*)&dstData)[0];
    if (iBytes > 1) {
      *pDstEnd++ = ((uint8_t*)&dstData)[1];
    }
    if (iBytes > 2) {
      *pDstEnd++ = ((uint8_t*)&dstData)[2];
    }
  }
  return pDstEnd - pDst;
}

}  // namespace

CXFA_FFDoc::CXFA_FFDoc(CXFA_FFApp* pApp, IXFA_DocEnvironment* pDocEnvironment)
    : m_pDocEnvironment(pDocEnvironment),
      m_pDocumentParser(nullptr),
      m_pStream(nullptr),
      m_pApp(pApp),
      m_pNotify(nullptr),
      m_pPDFDoc(nullptr),
      m_dwDocType(XFA_DOCTYPE_Static),
      m_bOwnStream(true) {}

CXFA_FFDoc::~CXFA_FFDoc() {
  CloseDoc();
}

uint32_t CXFA_FFDoc::GetDocType() {
  return m_dwDocType;
}

int32_t CXFA_FFDoc::StartLoad() {
  m_pNotify.reset(new CXFA_FFNotify(this));
  m_pDocumentParser.reset(new CXFA_DocumentParser(m_pNotify.get()));
  int32_t iStatus = m_pDocumentParser->StartParse(m_pStream, XFA_XDPPACKET_XDP);
  return iStatus;
}

bool XFA_GetPDFContentsFromPDFXML(CFDE_XMLNode* pPDFElement,
                                  uint8_t*& pByteBuffer,
                                  int32_t& iBufferSize) {
  CFDE_XMLElement* pDocumentElement = nullptr;
  for (CFDE_XMLNode* pXMLNode =
           pPDFElement->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLNode; pXMLNode = pXMLNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    if (pXMLNode->GetType() == FDE_XMLNODE_Element) {
      CFX_WideString wsTagName;
      CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLNode);
      pXMLElement->GetTagName(wsTagName);
      if (wsTagName == FX_WSTRC(L"document")) {
        pDocumentElement = pXMLElement;
        break;
      }
    }
  }
  if (!pDocumentElement) {
    return false;
  }
  CFDE_XMLElement* pChunkElement = nullptr;
  for (CFDE_XMLNode* pXMLNode =
           pDocumentElement->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLNode; pXMLNode = pXMLNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    if (pXMLNode->GetType() == FDE_XMLNODE_Element) {
      CFX_WideString wsTagName;
      CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLNode);
      pXMLElement->GetTagName(wsTagName);
      if (wsTagName == FX_WSTRC(L"chunk")) {
        pChunkElement = pXMLElement;
        break;
      }
    }
  }
  if (!pChunkElement) {
    return false;
  }
  CFX_WideString wsPDFContent;
  pChunkElement->GetTextData(wsPDFContent);
  iBufferSize =
      Base64DecodeW(wsPDFContent.c_str(), wsPDFContent.GetLength(), nullptr);
  pByteBuffer = FX_Alloc(uint8_t, iBufferSize + 1);
  pByteBuffer[iBufferSize] = '0';  // FIXME: I bet this is wrong.
  Base64DecodeW(wsPDFContent.c_str(), wsPDFContent.GetLength(), pByteBuffer);
  return true;
}
void XFA_XPDPacket_MergeRootNode(CXFA_Node* pOriginRoot, CXFA_Node* pNewRoot) {
  CXFA_Node* pChildNode = pNewRoot->GetNodeItem(XFA_NODEITEM_FirstChild);
  while (pChildNode) {
    CXFA_Node* pOriginChild =
        pOriginRoot->GetFirstChildByName(pChildNode->GetNameHash());
    if (pOriginChild) {
      pChildNode = pChildNode->GetNodeItem(XFA_NODEITEM_NextSibling);
    } else {
      CXFA_Node* pNextSibling =
          pChildNode->GetNodeItem(XFA_NODEITEM_NextSibling);
      pNewRoot->RemoveChild(pChildNode);
      pOriginRoot->InsertChild(pChildNode);
      pChildNode = pNextSibling;
      pNextSibling = nullptr;
    }
  }
}

int32_t CXFA_FFDoc::DoLoad(IFX_Pause* pPause) {
  int32_t iStatus = m_pDocumentParser->DoParse(pPause);
  if (iStatus == XFA_PARSESTATUS_Done && !m_pPDFDoc)
    return XFA_PARSESTATUS_SyntaxErr;
  return iStatus;
}

void CXFA_FFDoc::StopLoad() {
  m_pApp->GetXFAFontMgr()->LoadDocFonts(this);
  m_dwDocType = XFA_DOCTYPE_Static;
  CXFA_Node* pConfig = ToNode(
      m_pDocumentParser->GetDocument()->GetXFAObject(XFA_HASHCODE_Config));
  if (!pConfig) {
    return;
  }
  CXFA_Node* pAcrobat = pConfig->GetFirstChildByClass(XFA_Element::Acrobat);
  if (!pAcrobat) {
    return;
  }
  CXFA_Node* pAcrobat7 = pAcrobat->GetFirstChildByClass(XFA_Element::Acrobat7);
  if (!pAcrobat7) {
    return;
  }
  CXFA_Node* pDynamicRender =
      pAcrobat7->GetFirstChildByClass(XFA_Element::DynamicRender);
  if (!pDynamicRender) {
    return;
  }
  CFX_WideString wsType;
  if (pDynamicRender->TryContent(wsType) && wsType == FX_WSTRC(L"required")) {
    m_dwDocType = XFA_DOCTYPE_Dynamic;
  }
}

CXFA_FFDocView* CXFA_FFDoc::CreateDocView(uint32_t dwView) {
  if (!m_TypeToDocViewMap[dwView])
    m_TypeToDocViewMap[dwView].reset(new CXFA_FFDocView(this));

  return m_TypeToDocViewMap[dwView].get();
}

CXFA_FFDocView* CXFA_FFDoc::GetDocView(CXFA_LayoutProcessor* pLayout) {
  for (const auto& pair : m_TypeToDocViewMap) {
    if (pair.second->GetXFALayout() == pLayout)
      return pair.second.get();
  }
  return nullptr;
}

CXFA_FFDocView* CXFA_FFDoc::GetDocView() {
  auto it = m_TypeToDocViewMap.begin();
  return it != m_TypeToDocViewMap.end() ? it->second.get() : nullptr;
}

bool CXFA_FFDoc::OpenDoc(IFX_SeekableReadStream* pStream, bool bTakeOverFile) {
  m_bOwnStream = bTakeOverFile;
  m_pStream = pStream;
  return true;
}
bool CXFA_FFDoc::OpenDoc(CPDF_Document* pPDFDoc) {
  if (!pPDFDoc)
    return false;

  CPDF_Dictionary* pRoot = pPDFDoc->GetRoot();
  if (!pRoot)
    return false;

  CPDF_Dictionary* pAcroForm = pRoot->GetDictFor("AcroForm");
  if (!pAcroForm)
    return false;

  CPDF_Object* pElementXFA = pAcroForm->GetDirectObjectFor("XFA");
  if (!pElementXFA)
    return false;

  std::vector<CPDF_Stream*> xfaStreams;
  if (pElementXFA->IsArray()) {
    CPDF_Array* pXFAArray = (CPDF_Array*)pElementXFA;
    for (size_t i = 0; i < pXFAArray->GetCount() / 2; i++) {
      if (CPDF_Stream* pStream = pXFAArray->GetStreamAt(i * 2 + 1))
        xfaStreams.push_back(pStream);
    }
  } else if (pElementXFA->IsStream()) {
    xfaStreams.push_back((CPDF_Stream*)pElementXFA);
  }
  if (xfaStreams.empty())
    return false;

  IFX_SeekableReadStream* pFileRead = new CXFA_FileRead(xfaStreams);
  m_pPDFDoc = pPDFDoc;
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
  m_pStream = pFileRead;
  m_bOwnStream = true;
  return true;
}

bool CXFA_FFDoc::CloseDoc() {
  for (const auto& pair : m_TypeToDocViewMap)
    pair.second->RunDocClose();

  CXFA_Document* doc =
      m_pDocumentParser ? m_pDocumentParser->GetDocument() : nullptr;
  if (doc)
    doc->ClearLayoutData();

  m_TypeToDocViewMap.clear();

  m_pNotify.reset(nullptr);
  m_pApp->GetXFAFontMgr()->ReleaseDocFonts(this);

  if (m_dwDocType != XFA_DOCTYPE_XDP && m_pStream && m_bOwnStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }

  for (const auto& pair : m_HashToDibDpiMap)
    delete pair.second.pDibSource;

  m_HashToDibDpiMap.clear();
  m_pApp->ClearEventTargets();
  return true;
}
void CXFA_FFDoc::SetDocType(uint32_t dwType) {
  m_dwDocType = dwType;
}
CPDF_Document* CXFA_FFDoc::GetPDFDoc() {
  return m_pPDFDoc;
}

CFX_DIBitmap* CXFA_FFDoc::GetPDFNamedImage(const CFX_WideStringC& wsName,
                                           int32_t& iImageXDpi,
                                           int32_t& iImageYDpi) {
  if (!m_pPDFDoc)
    return nullptr;

  uint32_t dwHash = FX_HashCode_GetW(wsName, false);
  auto it = m_HashToDibDpiMap.find(dwHash);
  if (it != m_HashToDibDpiMap.end()) {
    iImageXDpi = it->second.iImageXDpi;
    iImageYDpi = it->second.iImageYDpi;
    return static_cast<CFX_DIBitmap*>(it->second.pDibSource);
  }

  CPDF_Dictionary* pRoot = m_pPDFDoc->GetRoot();
  if (!pRoot)
    return nullptr;

  CPDF_Dictionary* pNames = pRoot->GetDictFor("Names");
  if (!pNames)
    return nullptr;

  CPDF_Dictionary* pXFAImages = pNames->GetDictFor("XFAImages");
  if (!pXFAImages)
    return nullptr;

  CPDF_NameTree nametree(pXFAImages);
  CFX_ByteString bsName = PDF_EncodeText(wsName.c_str(), wsName.GetLength());
  CPDF_Object* pObject = nametree.LookupValue(bsName);
  if (!pObject) {
    for (size_t i = 0; i < nametree.GetCount(); i++) {
      CFX_ByteString bsTemp;
      CPDF_Object* pTempObject = nametree.LookupValue(i, bsTemp);
      if (bsTemp == bsName) {
        pObject = pTempObject;
        break;
      }
    }
  }

  CPDF_Stream* pStream = ToStream(pObject);
  if (!pStream)
    return nullptr;

  CPDF_StreamAcc streamAcc;
  streamAcc.LoadAllData(pStream);

  IFX_SeekableReadStream* pImageFileRead =
      FX_CreateMemoryStream((uint8_t*)streamAcc.GetData(), streamAcc.GetSize());

  CFX_DIBitmap* pDibSource = XFA_LoadImageFromBuffer(
      pImageFileRead, FXCODEC_IMAGE_UNKNOWN, iImageXDpi, iImageYDpi);
  m_HashToDibDpiMap[dwHash] = {pDibSource, iImageXDpi, iImageYDpi};
  pImageFileRead->Release();
  return pDibSource;
}

bool CXFA_FFDoc::SavePackage(XFA_HashCode code,
                             IFX_SeekableWriteStream* pFile,
                             CXFA_ChecksumContext* pCSContext) {
  CXFA_Document* doc = m_pDocumentParser->GetDocument();

  std::unique_ptr<CXFA_DataExporter> pExport(new CXFA_DataExporter(doc));
  CXFA_Node* pNode = code == XFA_HASHCODE_Xfa ? doc->GetRoot()
                                              : ToNode(doc->GetXFAObject(code));
  if (!pNode)
    return !!pExport->Export(pFile);

  CFX_ByteString bsChecksum;
  if (pCSContext)
    bsChecksum = pCSContext->GetChecksum();

  return !!pExport->Export(
      pFile, pNode, 0, bsChecksum.GetLength() ? bsChecksum.c_str() : nullptr);
}

bool CXFA_FFDoc::ImportData(IFX_SeekableReadStream* pStream, bool bXDP) {
  std::unique_ptr<CXFA_DataImporter> importer(
      new CXFA_DataImporter(m_pDocumentParser->GetDocument()));
  return importer->ImportData(pStream);
}
