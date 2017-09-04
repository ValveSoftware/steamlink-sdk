// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_IMAGE_H_
#define CORE_FPDFAPI_PAGE_CPDF_IMAGE_H_

#include <memory>

#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_system.h"

#define PDF_IMAGE_NO_COMPRESS 0x0000
#define PDF_IMAGE_LOSSY_COMPRESS 0x0001
#define PDF_IMAGE_LOSSLESS_COMPRESS 0x0002
#define PDF_IMAGE_MASK_LOSSY_COMPRESS 0x0004

class CFX_DIBSource;
class CFX_DIBitmap;
class CPDF_Document;
class CPDF_Page;
class IFX_Pause;
class IFX_SeekableReadStream;

class CPDF_Image {
 public:
  explicit CPDF_Image(CPDF_Document* pDoc);
  CPDF_Image(CPDF_Document* pDoc, std::unique_ptr<CPDF_Stream> pStream);
  CPDF_Image(CPDF_Document* pDoc, uint32_t dwStreamObjNum);
  ~CPDF_Image();

  CPDF_Image* Clone();
  void ConvertStreamToIndirectObject();

  CPDF_Dictionary* GetInlineDict() const { return m_pDict; }
  CPDF_Stream* GetStream() const { return m_pStream; }
  CPDF_Dictionary* GetDict() const {
    return m_pStream ? m_pStream->GetDict() : nullptr;
  }
  CPDF_Dictionary* GetOC() const { return m_pOC; }
  CPDF_Document* GetDocument() const { return m_pDocument; }

  int32_t GetPixelHeight() const { return m_Height; }
  int32_t GetPixelWidth() const { return m_Width; }

  bool IsInline() const { return m_bIsInline; }
  bool IsMask() const { return m_bIsMask; }
  bool IsInterpol() const { return m_bInterpolate; }

  CFX_DIBSource* LoadDIBSource(CFX_DIBSource** ppMask = nullptr,
                               uint32_t* pMatteColor = nullptr,
                               bool bStdCS = false,
                               uint32_t GroupFamily = 0,
                               bool bLoadMask = false) const;

  void SetImage(const CFX_DIBitmap* pDIBitmap, int32_t iCompress);
  void SetJpegImage(IFX_SeekableReadStream* pFile);

  void ResetCache(CPDF_Page* pPage, const CFX_DIBitmap* pDIBitmap);

  bool StartLoadDIBSource(CPDF_Dictionary* pFormResource,
                          CPDF_Dictionary* pPageResource,
                          bool bStdCS = false,
                          uint32_t GroupFamily = 0,
                          bool bLoadMask = false);
  bool Continue(IFX_Pause* pPause);
  CFX_DIBSource* DetachBitmap();
  CFX_DIBSource* DetachMask();

  CFX_DIBSource* m_pDIBSource = nullptr;
  CFX_DIBSource* m_pMask = nullptr;
  uint32_t m_MatteColor = 0;

 private:
  void FinishInitialization();
  CPDF_Dictionary* InitJPEG(uint8_t* pData, uint32_t size);

  int32_t m_Height = 0;
  int32_t m_Width = 0;
  bool m_bIsInline = false;
  bool m_bIsMask = false;
  bool m_bInterpolate = false;
  CPDF_Document* const m_pDocument;
  CPDF_Stream* m_pStream = nullptr;
  CPDF_Dictionary* m_pDict = nullptr;
  std::unique_ptr<CPDF_Stream> m_pOwnedStream;
  std::unique_ptr<CPDF_Dictionary> m_pOwnedDict;
  CPDF_Dictionary* m_pOC = nullptr;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_IMAGE_H_
