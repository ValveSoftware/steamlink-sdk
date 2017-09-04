// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FSDK_DEFINE_H_
#define FPDFSDK_FSDK_DEFINE_H_

#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fxge/fx_dib.h"
#include "public/fpdfview.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#endif  // PDF_ENABLE_XFA

#ifdef _WIN32
#include <math.h>
#include <tchar.h>
#endif

class CPDF_Annot;
class CPDF_Page;
class CPDF_PageRenderContext;
class IFSDK_PAUSE_Adapter;

class CPDF_CustomAccess final : public IFX_SeekableReadStream {
 public:
  explicit CPDF_CustomAccess(FPDF_FILEACCESS* pFileAccess);
  ~CPDF_CustomAccess() override {}

  // IFX_SeekableReadStream
  FX_FILESIZE GetSize() override;
  void Release() override;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override;

 private:
  FPDF_FILEACCESS m_FileAccess;
};

#ifdef PDF_ENABLE_XFA
class CFPDF_FileStream : public IFX_SeekableStream {
 public:
  explicit CFPDF_FileStream(FPDF_FILEHANDLER* pFS);
  ~CFPDF_FileStream() override {}

  // IFX_SeekableStream:
  IFX_SeekableStream* Retain() override;
  void Release() override;
  FX_FILESIZE GetSize() override;
  bool IsEOF() override;
  FX_FILESIZE GetPosition() override;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override;
  size_t ReadBlock(void* buffer, size_t size) override;
  bool WriteBlock(const void* buffer, FX_FILESIZE offset, size_t size) override;
  bool Flush() override;

  void SetPosition(FX_FILESIZE pos) { m_nCurPos = pos; }

 protected:
  FPDF_FILEHANDLER* m_pFS;
  FX_FILESIZE m_nCurPos;
};
#endif  // PDF_ENABLE_XFA

// Object types for public FPDF_ types; these correspond to next layer down
// from fpdfsdk. For master, these are CPDF_ types, but for XFA, these are
// CPDFXFA_ types.
#ifndef PDF_ENABLE_XFA
using UnderlyingDocumentType = CPDF_Document;
using UnderlyingPageType = CPDF_Page;
#else   // PDF_ENABLE_XFA
using UnderlyingDocumentType = CPDFXFA_Context;
using UnderlyingPageType = CPDFXFA_Page;
#endif  // PDF_ENABLE_XFA

// Conversions to/from underlying types.
UnderlyingDocumentType* UnderlyingFromFPDFDocument(FPDF_DOCUMENT doc);
FPDF_DOCUMENT FPDFDocumentFromUnderlying(UnderlyingDocumentType* doc);

UnderlyingPageType* UnderlyingFromFPDFPage(FPDF_PAGE page);

// Conversions to/from FPDF_ types.
CPDF_Document* CPDFDocumentFromFPDFDocument(FPDF_DOCUMENT doc);
FPDF_DOCUMENT FPDFDocumentFromCPDFDocument(CPDF_Document* doc);

CPDF_Page* CPDFPageFromFPDFPage(FPDF_PAGE page);

CFX_DIBitmap* CFXBitmapFromFPDFBitmap(FPDF_BITMAP bitmap);

void FSDK_SetSandBoxPolicy(FPDF_DWORD policy, FPDF_BOOL enable);
FPDF_BOOL FSDK_IsSandBoxPolicyEnabled(FPDF_DWORD policy);
void FPDF_RenderPage_Retail(CPDF_PageRenderContext* pContext,
                            FPDF_PAGE page,
                            int start_x,
                            int start_y,
                            int size_x,
                            int size_y,
                            int rotate,
                            int flags,
                            bool bNeedToRestore,
                            IFSDK_PAUSE_Adapter* pause);

void CheckUnSupportError(CPDF_Document* pDoc, uint32_t err_code);
void CheckUnSupportAnnot(CPDF_Document* pDoc, const CPDF_Annot* pPDFAnnot);
void ProcessParseError(CPDF_Parser::Error err);

#endif  // FPDFSDK_FSDK_DEFINE_H_
