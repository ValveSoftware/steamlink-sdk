// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_image.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/render_int.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxge/fx_dib.h"

CPDF_Image::CPDF_Image(CPDF_Document* pDoc) : m_pDocument(pDoc) {}

CPDF_Image::CPDF_Image(CPDF_Document* pDoc,
                       std::unique_ptr<CPDF_Stream> pStream)
    : m_bIsInline(true),
      m_pDocument(pDoc),
      m_pStream(pStream.get()),
      m_pOwnedStream(std::move(pStream)) {
  m_pOwnedDict =
      ToDictionary(std::unique_ptr<CPDF_Object>(m_pStream->GetDict()->Clone()));
  m_pDict = m_pOwnedDict.get();
  FinishInitialization();
}

CPDF_Image::CPDF_Image(CPDF_Document* pDoc, uint32_t dwStreamObjNum)
    : m_pDocument(pDoc),
      m_pStream(ToStream(pDoc->GetIndirectObject(dwStreamObjNum))) {
  m_pDict = m_pStream->GetDict();
  FinishInitialization();
}

CPDF_Image::~CPDF_Image() {}

void CPDF_Image::FinishInitialization() {
  m_pOC = m_pDict->GetDictFor("OC");
  m_bIsMask =
      !m_pDict->KeyExist("ColorSpace") || m_pDict->GetIntegerFor("ImageMask");
  m_bInterpolate = !!m_pDict->GetIntegerFor("Interpolate");
  m_Height = m_pDict->GetIntegerFor("Height");
  m_Width = m_pDict->GetIntegerFor("Width");
}

CPDF_Image* CPDF_Image::Clone() {
  CPDF_Image* pImage = new CPDF_Image(m_pDocument);
  pImage->m_bIsInline = m_bIsInline;
  if (m_pOwnedStream) {
    pImage->m_pOwnedStream = ToStream(m_pOwnedStream->Clone());
    pImage->m_pStream = pImage->m_pOwnedStream.get();
  } else {
    pImage->m_pStream = m_pStream;
  }
  if (m_pOwnedDict) {
    pImage->m_pOwnedDict = ToDictionary(m_pOwnedDict->Clone());
    pImage->m_pDict = pImage->m_pOwnedDict.get();
  } else {
    pImage->m_pDict = m_pDict;
  }
  return pImage;
}

void CPDF_Image::ConvertStreamToIndirectObject() {
  if (!m_pStream->IsInline())
    return;

  ASSERT(m_pOwnedStream);
  m_pDocument->AddIndirectObject(std::move(m_pOwnedStream));
}

CPDF_Dictionary* CPDF_Image::InitJPEG(uint8_t* pData, uint32_t size) {
  int32_t width;
  int32_t height;
  int32_t num_comps;
  int32_t bits;
  bool color_trans;
  if (!CPDF_ModuleMgr::Get()->GetJpegModule()->LoadInfo(
          pData, size, &width, &height, &num_comps, &bits, &color_trans)) {
    return nullptr;
  }

  CPDF_Dictionary* pDict =
      new CPDF_Dictionary(m_pDocument->GetByteStringPool());
  pDict->SetNameFor("Type", "XObject");
  pDict->SetNameFor("Subtype", "Image");
  pDict->SetIntegerFor("Width", width);
  pDict->SetIntegerFor("Height", height);
  const FX_CHAR* csname = nullptr;
  if (num_comps == 1) {
    csname = "DeviceGray";
  } else if (num_comps == 3) {
    csname = "DeviceRGB";
  } else if (num_comps == 4) {
    csname = "DeviceCMYK";
    CPDF_Array* pDecode = new CPDF_Array;
    for (int n = 0; n < 4; n++) {
      pDecode->AddNew<CPDF_Number>(1);
      pDecode->AddNew<CPDF_Number>(0);
    }
    pDict->SetFor("Decode", pDecode);
  }
  pDict->SetNameFor("ColorSpace", csname);
  pDict->SetIntegerFor("BitsPerComponent", bits);
  pDict->SetNameFor("Filter", "DCTDecode");
  if (!color_trans) {
    CPDF_Dictionary* pParms =
        new CPDF_Dictionary(m_pDocument->GetByteStringPool());
    pDict->SetFor("DecodeParms", pParms);
    pParms->SetIntegerFor("ColorTransform", 0);
  }
  m_bIsMask = false;
  m_Width = width;
  m_Height = height;
  if (!m_pStream) {
    m_pOwnedStream = pdfium::MakeUnique<CPDF_Stream>();
    m_pStream = m_pOwnedStream.get();
  }
  return pDict;
}

void CPDF_Image::SetJpegImage(IFX_SeekableReadStream* pFile) {
  uint32_t size = (uint32_t)pFile->GetSize();
  if (!size)
    return;

  uint32_t dwEstimateSize = std::min(size, 8192U);
  std::vector<uint8_t> data(dwEstimateSize);
  pFile->ReadBlock(data.data(), 0, dwEstimateSize);
  CPDF_Dictionary* pDict = InitJPEG(data.data(), dwEstimateSize);
  if (!pDict && size > dwEstimateSize) {
    data.resize(size);
    pFile->ReadBlock(data.data(), 0, size);
    pDict = InitJPEG(data.data(), size);
  }
  if (!pDict)
    return;

  m_pStream->InitStreamFromFile(pFile, pDict);
}

void CPDF_Image::SetImage(const CFX_DIBitmap* pBitmap, int32_t iCompress) {
  int32_t BitmapWidth = pBitmap->GetWidth();
  int32_t BitmapHeight = pBitmap->GetHeight();
  if (BitmapWidth < 1 || BitmapHeight < 1) {
    return;
  }
  uint8_t* src_buf = pBitmap->GetBuffer();
  int32_t src_pitch = pBitmap->GetPitch();
  int32_t bpp = pBitmap->GetBPP();

  CPDF_Dictionary* pDict =
      new CPDF_Dictionary(m_pDocument->GetByteStringPool());
  pDict->SetNameFor("Type", "XObject");
  pDict->SetNameFor("Subtype", "Image");
  pDict->SetIntegerFor("Width", BitmapWidth);
  pDict->SetIntegerFor("Height", BitmapHeight);
  uint8_t* dest_buf = nullptr;
  FX_STRSIZE dest_pitch = 0, dest_size = 0, opType = -1;
  if (bpp == 1) {
    int32_t reset_a = 0, reset_r = 0, reset_g = 0, reset_b = 0;
    int32_t set_a = 0, set_r = 0, set_g = 0, set_b = 0;
    if (!pBitmap->IsAlphaMask()) {
      ArgbDecode(pBitmap->GetPaletteArgb(0), reset_a, reset_r, reset_g,
                 reset_b);
      ArgbDecode(pBitmap->GetPaletteArgb(1), set_a, set_r, set_g, set_b);
    }
    if (set_a == 0 || reset_a == 0) {
      pDict->SetFor("ImageMask", new CPDF_Boolean(true));
      if (reset_a == 0) {
        CPDF_Array* pArray = new CPDF_Array;
        pArray->AddNew<CPDF_Number>(1);
        pArray->AddNew<CPDF_Number>(0);
        pDict->SetFor("Decode", pArray);
      }
    } else {
      CPDF_Array* pCS = new CPDF_Array;
      pCS->AddNew<CPDF_Name>("Indexed");
      pCS->AddNew<CPDF_Name>("DeviceRGB");
      pCS->AddNew<CPDF_Number>(1);
      CFX_ByteString ct;
      FX_CHAR* pBuf = ct.GetBuffer(6);
      pBuf[0] = (FX_CHAR)reset_r;
      pBuf[1] = (FX_CHAR)reset_g;
      pBuf[2] = (FX_CHAR)reset_b;
      pBuf[3] = (FX_CHAR)set_r;
      pBuf[4] = (FX_CHAR)set_g;
      pBuf[5] = (FX_CHAR)set_b;
      ct.ReleaseBuffer(6);
      pCS->AddNew<CPDF_String>(ct, true);
      pDict->SetFor("ColorSpace", pCS);
    }
    pDict->SetIntegerFor("BitsPerComponent", 1);
    dest_pitch = (BitmapWidth + 7) / 8;
    if ((iCompress & 0x03) == PDF_IMAGE_NO_COMPRESS) {
      opType = 1;
    } else {
      opType = 0;
    }
  } else if (bpp == 8) {
    int32_t iPalette = pBitmap->GetPaletteSize();
    if (iPalette > 0) {
      CPDF_Array* pCS = m_pDocument->NewIndirect<CPDF_Array>();
      pCS->AddNew<CPDF_Name>("Indexed");
      pCS->AddNew<CPDF_Name>("DeviceRGB");
      pCS->AddNew<CPDF_Number>(iPalette - 1);
      uint8_t* pColorTable = FX_Alloc2D(uint8_t, iPalette, 3);
      uint8_t* ptr = pColorTable;
      for (int32_t i = 0; i < iPalette; i++) {
        uint32_t argb = pBitmap->GetPaletteArgb(i);
        ptr[0] = (uint8_t)(argb >> 16);
        ptr[1] = (uint8_t)(argb >> 8);
        ptr[2] = (uint8_t)argb;
        ptr += 3;
      }
      CPDF_Stream* pCTS = m_pDocument->NewIndirect<CPDF_Stream>(
          pColorTable, iPalette * 3,
          new CPDF_Dictionary(m_pDocument->GetByteStringPool()));
      pCS->AddNew<CPDF_Reference>(m_pDocument, pCTS->GetObjNum());
      pDict->SetReferenceFor("ColorSpace", m_pDocument, pCS);
    } else {
      pDict->SetNameFor("ColorSpace", "DeviceGray");
    }
    pDict->SetIntegerFor("BitsPerComponent", 8);
    if ((iCompress & 0x03) == PDF_IMAGE_NO_COMPRESS) {
      dest_pitch = BitmapWidth;
      opType = 1;
    } else {
      opType = 0;
    }
  } else {
    pDict->SetNameFor("ColorSpace", "DeviceRGB");
    pDict->SetIntegerFor("BitsPerComponent", 8);
    if ((iCompress & 0x03) == PDF_IMAGE_NO_COMPRESS) {
      dest_pitch = BitmapWidth * 3;
      opType = 2;
    } else {
      opType = 0;
    }
  }
  const CFX_DIBitmap* pMaskBitmap = nullptr;
  bool bDeleteMask = false;
  if (pBitmap->HasAlpha()) {
    pMaskBitmap = pBitmap->GetAlphaMask();
    bDeleteMask = true;
  }
  if (pMaskBitmap) {
    int32_t maskWidth = pMaskBitmap->GetWidth();
    int32_t maskHeight = pMaskBitmap->GetHeight();
    uint8_t* mask_buf = nullptr;
    FX_STRSIZE mask_size = 0;
    CPDF_Dictionary* pMaskDict =
        new CPDF_Dictionary(m_pDocument->GetByteStringPool());
    pMaskDict->SetNameFor("Type", "XObject");
    pMaskDict->SetNameFor("Subtype", "Image");
    pMaskDict->SetIntegerFor("Width", maskWidth);
    pMaskDict->SetIntegerFor("Height", maskHeight);
    pMaskDict->SetNameFor("ColorSpace", "DeviceGray");
    pMaskDict->SetIntegerFor("BitsPerComponent", 8);
    if (pMaskBitmap->GetBPP() == 8 &&
        (iCompress & PDF_IMAGE_MASK_LOSSY_COMPRESS) != 0) {
    } else if (pMaskBitmap->GetFormat() == FXDIB_1bppMask) {
    } else {
      mask_buf = FX_Alloc2D(uint8_t, maskHeight, maskWidth);
      mask_size = maskHeight * maskWidth;  // Safe since checked alloc returned.
      for (int32_t a = 0; a < maskHeight; a++) {
        FXSYS_memcpy(mask_buf + a * maskWidth, pMaskBitmap->GetScanline(a),
                     maskWidth);
      }
    }
    pMaskDict->SetIntegerFor("Length", mask_size);
    pDict->SetReferenceFor(
        "SMask", m_pDocument,
        m_pDocument->NewIndirect<CPDF_Stream>(mask_buf, mask_size, pMaskDict));
    if (bDeleteMask)
      delete pMaskBitmap;
  }
  if (opType == 0) {
    if (iCompress & PDF_IMAGE_LOSSLESS_COMPRESS) {
    } else {
      if (pBitmap->GetBPP() == 1) {
      } else if (pBitmap->GetBPP() >= 8 && pBitmap->GetPalette()) {
        CFX_DIBitmap* pNewBitmap = new CFX_DIBitmap();
        pNewBitmap->Copy(pBitmap);
        pNewBitmap->ConvertFormat(FXDIB_Rgb);
        SetImage(pNewBitmap, iCompress);
        delete pDict;
        pDict = nullptr;
        FX_Free(dest_buf);
        dest_buf = nullptr;
        dest_size = 0;
        delete pNewBitmap;
        return;
      }
    }
  } else if (opType == 1) {
    dest_buf = FX_Alloc2D(uint8_t, dest_pitch, BitmapHeight);
    dest_size = dest_pitch * BitmapHeight;  // Safe as checked alloc returned.

    uint8_t* pDest = dest_buf;
    for (int32_t i = 0; i < BitmapHeight; i++) {
      FXSYS_memcpy(pDest, src_buf, dest_pitch);
      pDest += dest_pitch;
      src_buf += src_pitch;
    }
  } else if (opType == 2) {
    dest_buf = FX_Alloc2D(uint8_t, dest_pitch, BitmapHeight);
    dest_size = dest_pitch * BitmapHeight;  // Safe as checked alloc returned.

    uint8_t* pDest = dest_buf;
    int32_t src_offset = 0;
    int32_t dest_offset = 0;
    for (int32_t row = 0; row < BitmapHeight; row++) {
      src_offset = row * src_pitch;
      for (int32_t column = 0; column < BitmapWidth; column++) {
        FX_FLOAT alpha = 1;
        pDest[dest_offset] = (uint8_t)(src_buf[src_offset + 2] * alpha);
        pDest[dest_offset + 1] = (uint8_t)(src_buf[src_offset + 1] * alpha);
        pDest[dest_offset + 2] = (uint8_t)(src_buf[src_offset] * alpha);
        dest_offset += 3;
        src_offset += bpp == 24 ? 3 : 4;
      }

      pDest += dest_pitch;
      dest_offset = 0;
    }
  }
  if (!m_pStream) {
    m_pOwnedStream = pdfium::MakeUnique<CPDF_Stream>();
    m_pStream = m_pOwnedStream.get();
  }
  m_pStream->InitStream(dest_buf, dest_size, pDict);
  m_bIsMask = pBitmap->IsAlphaMask();
  m_Width = BitmapWidth;
  m_Height = BitmapHeight;
  FX_Free(dest_buf);
}

void CPDF_Image::ResetCache(CPDF_Page* pPage, const CFX_DIBitmap* pBitmap) {
  pPage->GetRenderCache()->ResetBitmap(m_pStream, pBitmap);
}

CFX_DIBSource* CPDF_Image::LoadDIBSource(CFX_DIBSource** ppMask,
                                         uint32_t* pMatteColor,
                                         bool bStdCS,
                                         uint32_t GroupFamily,
                                         bool bLoadMask) const {
  std::unique_ptr<CPDF_DIBSource> source(new CPDF_DIBSource);
  if (source->Load(m_pDocument, m_pStream,
                   reinterpret_cast<CPDF_DIBSource**>(ppMask), pMatteColor,
                   nullptr, nullptr, bStdCS, GroupFamily, bLoadMask)) {
    return source.release();
  }
  return nullptr;
}

CFX_DIBSource* CPDF_Image::DetachBitmap() {
  CFX_DIBSource* pBitmap = m_pDIBSource;
  m_pDIBSource = nullptr;
  return pBitmap;
}

CFX_DIBSource* CPDF_Image::DetachMask() {
  CFX_DIBSource* pBitmap = m_pMask;
  m_pMask = nullptr;
  return pBitmap;
}

bool CPDF_Image::StartLoadDIBSource(CPDF_Dictionary* pFormResource,
                                    CPDF_Dictionary* pPageResource,
                                    bool bStdCS,
                                    uint32_t GroupFamily,
                                    bool bLoadMask) {
  std::unique_ptr<CPDF_DIBSource> source(new CPDF_DIBSource);
  int ret =
      source->StartLoadDIBSource(m_pDocument, m_pStream, true, pFormResource,
                                 pPageResource, bStdCS, GroupFamily, bLoadMask);
  if (ret == 2) {
    m_pDIBSource = source.release();
    return true;
  }
  if (!ret) {
    m_pDIBSource = nullptr;
    return false;
  }
  m_pMask = source->DetachMask();
  m_MatteColor = source->GetMatteColor();
  m_pDIBSource = source.release();
  return false;
}

bool CPDF_Image::Continue(IFX_Pause* pPause) {
  CPDF_DIBSource* pSource = static_cast<CPDF_DIBSource*>(m_pDIBSource);
  int ret = pSource->ContinueLoadDIBSource(pPause);
  if (ret == 2) {
    return true;
  }
  if (!ret) {
    delete m_pDIBSource;
    m_pDIBSource = nullptr;
    return false;
  }
  m_pMask = pSource->DetachMask();
  m_MatteColor = pSource->GetMatteColor();
  return false;
}
