// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_RENDER_INT_H_
#define CORE_FPDFAPI_RENDER_RENDER_INT_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_clippath.h"
#include "core/fpdfapi/page/cpdf_countedobject.h"
#include "core/fpdfapi/page/cpdf_graphicstates.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_renderdevice.h"

class CCodec_Jbig2Context;
class CCodec_ScanlineDecoder;
class CFX_GlyphBitmap;
class CFX_ImageTransformer;
class CFX_PathData;
class CPDF_Color;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Font;
class CPDF_FormObject;
class CPDF_ImageCacheEntry;
class CPDF_ImageLoaderHandle;
class CPDF_ImageObject;
class CPDF_ImageRenderer;
class CPDF_Object;
class CPDF_PageObject;
class CPDF_PageObjectHolder;
class CPDF_PageRenderCache;
class CPDF_PathObject;
class CPDF_RenderStatus;
class CPDF_ShadingObject;
class CPDF_ShadingPattern;
class CPDF_Stream;
class CPDF_TilingPattern;
class CPDF_TransferFunc;
class CPDF_Type3Cache;
class CPDF_Type3Glyphs;
class CPDF_Type3Char;
class CPDF_Type3Font;

bool IsAvailableMatrix(const CFX_Matrix& matrix);

class CPDF_TransferFunc {
 public:
  explicit CPDF_TransferFunc(CPDF_Document* pDoc);

  FX_COLORREF TranslateColor(FX_COLORREF src) const;
  CFX_DIBSource* TranslateImage(const CFX_DIBSource* pSrc, bool bAutoDropSrc);

  CPDF_Document* const m_pPDFDoc;
  bool m_bIdentity;
  uint8_t m_Samples[256 * 3];
};

class CPDF_RenderStatus {
 public:
  CPDF_RenderStatus();
  ~CPDF_RenderStatus();

  bool Initialize(class CPDF_RenderContext* pContext,
                  CFX_RenderDevice* pDevice,
                  const CFX_Matrix* pDeviceMatrix,
                  const CPDF_PageObject* pStopObj,
                  const CPDF_RenderStatus* pParentStatus,
                  const CPDF_GraphicStates* pInitialStates,
                  const CPDF_RenderOptions* pOptions,
                  int transparency,
                  bool bDropObjects,
                  CPDF_Dictionary* pFormResource = nullptr,
                  bool bStdCS = false,
                  CPDF_Type3Char* pType3Char = nullptr,
                  FX_ARGB fill_color = 0,
                  uint32_t GroupFamily = 0,
                  bool bLoadMask = false);
  void RenderObjectList(const CPDF_PageObjectHolder* pObjectHolder,
                        const CFX_Matrix* pObj2Device);
  void RenderSingleObject(CPDF_PageObject* pObj, const CFX_Matrix* pObj2Device);
  bool ContinueSingleObject(CPDF_PageObject* pObj,
                            const CFX_Matrix* pObj2Device,
                            IFX_Pause* pPause);
  CPDF_RenderContext* GetContext() { return m_pContext; }

#if defined _SKIA_SUPPORT_
  void DebugVerifyDeviceIsPreMultiplied() const;
#endif

  CPDF_RenderOptions m_Options;
  CPDF_Dictionary* m_pFormResource;
  CPDF_Dictionary* m_pPageResource;
  CFX_ArrayTemplate<CPDF_Type3Font*> m_Type3FontCache;

 protected:
  friend class CPDF_ImageRenderer;
  friend class CPDF_RenderContext;

  void ProcessClipPath(CPDF_ClipPath ClipPath, const CFX_Matrix* pObj2Device);
  void DrawClipPath(CPDF_ClipPath ClipPath, const CFX_Matrix* pObj2Device);
  bool ProcessTransparency(CPDF_PageObject* PageObj,
                           const CFX_Matrix* pObj2Device);
  void ProcessObjectNoClip(CPDF_PageObject* PageObj,
                           const CFX_Matrix* pObj2Device);
  void DrawObjWithBackground(CPDF_PageObject* pObj,
                             const CFX_Matrix* pObj2Device);
  bool DrawObjWithBlend(CPDF_PageObject* pObj, const CFX_Matrix* pObj2Device);
  bool ProcessPath(CPDF_PathObject* pPathObj, const CFX_Matrix* pObj2Device);
  void ProcessPathPattern(CPDF_PathObject* pPathObj,
                          const CFX_Matrix* pObj2Device,
                          int& filltype,
                          bool& bStroke);
  void DrawPathWithPattern(CPDF_PathObject* pPathObj,
                           const CFX_Matrix* pObj2Device,
                           const CPDF_Color* pColor,
                           bool bStroke);
  void DrawTilingPattern(CPDF_TilingPattern* pPattern,
                         CPDF_PageObject* pPageObj,
                         const CFX_Matrix* pObj2Device,
                         bool bStroke);
  void DrawShadingPattern(CPDF_ShadingPattern* pPattern,
                          const CPDF_PageObject* pPageObj,
                          const CFX_Matrix* pObj2Device,
                          bool bStroke);
  bool SelectClipPath(const CPDF_PathObject* pPathObj,
                      const CFX_Matrix* pObj2Device,
                      bool bStroke);
  bool ProcessImage(CPDF_ImageObject* pImageObj, const CFX_Matrix* pObj2Device);
  bool OutputBitmapAlpha(CPDF_ImageObject* pImageObj,
                         const CFX_Matrix* pImage2Device);
  bool OutputImage(CPDF_ImageObject* pImageObj,
                   const CFX_Matrix* pImage2Device);
  bool OutputDIBSource(const CFX_DIBSource* pOutputBitmap,
                       FX_ARGB fill_argb,
                       int bitmap_alpha,
                       const CFX_Matrix* pImage2Device,
                       CPDF_ImageCacheEntry* pImageCache,
                       uint32_t flags);
  void CompositeDIBitmap(CFX_DIBitmap* pDIBitmap,
                         int left,
                         int top,
                         FX_ARGB mask_argb,
                         int bitmap_alpha,
                         int blend_mode,
                         int bIsolated);
  void ProcessShading(const CPDF_ShadingObject* pShadingObj,
                      const CFX_Matrix* pObj2Device);
  void DrawShading(CPDF_ShadingPattern* pPattern,
                   CFX_Matrix* pMatrix,
                   FX_RECT& clip_rect,
                   int alpha,
                   bool bAlphaMode);
  bool ProcessType3Text(CPDF_TextObject* textobj,
                        const CFX_Matrix* pObj2Device);
  bool ProcessText(CPDF_TextObject* textobj,
                   const CFX_Matrix* pObj2Device,
                   CFX_PathData* pClippingPath);
  void DrawTextPathWithPattern(const CPDF_TextObject* textobj,
                               const CFX_Matrix* pObj2Device,
                               CPDF_Font* pFont,
                               FX_FLOAT font_size,
                               const CFX_Matrix* pTextMatrix,
                               bool bFill,
                               bool bStroke);
  bool ProcessForm(const CPDF_FormObject* pFormObj,
                   const CFX_Matrix* pObj2Device);
  CFX_DIBitmap* GetBackdrop(const CPDF_PageObject* pObj,
                            const FX_RECT& rect,
                            int& left,
                            int& top,
                            bool bBackAlphaRequired);
  CFX_DIBitmap* LoadSMask(CPDF_Dictionary* pSMaskDict,
                          FX_RECT* pClipRect,
                          const CFX_Matrix* pMatrix);
  void Init(CPDF_RenderContext* pParent);
  static class CPDF_Type3Cache* GetCachedType3(CPDF_Type3Font* pFont);
  static CPDF_GraphicStates* CloneObjStates(const CPDF_GraphicStates* pPathObj,
                                            bool bStroke);
  CPDF_TransferFunc* GetTransferFunc(CPDF_Object* pObject) const;
  FX_ARGB GetFillArgb(CPDF_PageObject* pObj, bool bType3 = false) const;
  FX_ARGB GetStrokeArgb(CPDF_PageObject* pObj) const;
  bool GetObjectClippedRect(const CPDF_PageObject* pObj,
                            const CFX_Matrix* pObj2Device,
                            bool bLogical,
                            FX_RECT& rect) const;
  void GetScaledMatrix(CFX_Matrix& matrix) const;

  static const int kRenderMaxRecursionDepth = 64;
  static int s_CurrentRecursionDepth;

  CPDF_RenderContext* m_pContext;
  bool m_bStopped;
  CFX_RenderDevice* m_pDevice;
  CFX_Matrix m_DeviceMatrix;
  CPDF_ClipPath m_LastClipPath;
  const CPDF_PageObject* m_pCurObj;
  const CPDF_PageObject* m_pStopObj;
  CPDF_GraphicStates m_InitialStates;
  int m_HalftoneLimit;
  std::unique_ptr<CPDF_ImageRenderer> m_pImageRenderer;
  bool m_bPrint;
  int m_Transparency;
  bool m_bDropObjects;
  bool m_bStdCS;
  uint32_t m_GroupFamily;
  bool m_bLoadMask;
  CPDF_Type3Char* m_pType3Char;
  FX_ARGB m_T3FillColor;
  int m_curBlend;
};

class CPDF_ImageLoader {
 public:
  CPDF_ImageLoader()
      : m_pBitmap(nullptr),
        m_pMask(nullptr),
        m_MatteColor(0),
        m_bCached(false),
        m_nDownsampleWidth(0),
        m_nDownsampleHeight(0) {}
  ~CPDF_ImageLoader();

  bool Start(const CPDF_ImageObject* pImage,
             CPDF_PageRenderCache* pCache,
             std::unique_ptr<CPDF_ImageLoaderHandle>* pLoadHandle,
             bool bStdCS = false,
             uint32_t GroupFamily = 0,
             bool bLoadMask = false,
             CPDF_RenderStatus* pRenderStatus = nullptr,
             int32_t nDownsampleWidth = 0,
             int32_t nDownsampleHeight = 0);
  bool Continue(CPDF_ImageLoaderHandle* LoadHandle, IFX_Pause* pPause);

  CFX_DIBSource* m_pBitmap;
  CFX_DIBSource* m_pMask;
  uint32_t m_MatteColor;
  bool m_bCached;

 protected:
  int32_t m_nDownsampleWidth;
  int32_t m_nDownsampleHeight;
};

class CPDF_ImageLoaderHandle {
 public:
  CPDF_ImageLoaderHandle();
  ~CPDF_ImageLoaderHandle();

  bool Start(CPDF_ImageLoader* pImageLoader,
             const CPDF_ImageObject* pImage,
             CPDF_PageRenderCache* pCache,
             bool bStdCS = false,
             uint32_t GroupFamily = 0,
             bool bLoadMask = false,
             CPDF_RenderStatus* pRenderStatus = nullptr,
             int32_t nDownsampleWidth = 0,
             int32_t nDownsampleHeight = 0);
  bool Continue(IFX_Pause* pPause);

 protected:
  void HandleFailure();

  CPDF_ImageLoader* m_pImageLoader;
  CPDF_PageRenderCache* m_pCache;
  CPDF_ImageObject* m_pImage;
  int32_t m_nDownsampleWidth;
  int32_t m_nDownsampleHeight;
};

class CPDF_ImageRenderer {
 public:
  CPDF_ImageRenderer();
  ~CPDF_ImageRenderer();

  bool Start(CPDF_RenderStatus* pStatus,
             CPDF_PageObject* pObj,
             const CFX_Matrix* pObj2Device,
             bool bStdCS,
             int blendType = FXDIB_BLEND_NORMAL);
  bool Continue(IFX_Pause* pPause);

  bool Start(CPDF_RenderStatus* pStatus,
             const CFX_DIBSource* pDIBSource,
             FX_ARGB bitmap_argb,
             int bitmap_alpha,
             const CFX_Matrix* pImage2Device,
             uint32_t flags,
             bool bStdCS,
             int blendType = FXDIB_BLEND_NORMAL);

  bool m_Result;

 protected:
  bool StartBitmapAlpha();
  bool StartDIBSource();
  bool StartRenderDIBSource();
  bool StartLoadDIBSource();
  bool DrawMaskedImage();
  bool DrawPatternImage(const CFX_Matrix* pObj2Device);

  CPDF_RenderStatus* m_pRenderStatus;
  CPDF_ImageObject* m_pImageObject;
  int m_Status;
  const CFX_Matrix* m_pObj2Device;
  CFX_Matrix m_ImageMatrix;
  CPDF_ImageLoader m_Loader;
  const CFX_DIBSource* m_pDIBSource;
  std::unique_ptr<CFX_DIBitmap> m_pClone;
  int m_BitmapAlpha;
  bool m_bPatternColor;
  CPDF_Pattern* m_pPattern;
  FX_ARGB m_FillArgb;
  uint32_t m_Flags;
  std::unique_ptr<CFX_ImageTransformer> m_pTransformer;
  void* m_DeviceHandle;
  std::unique_ptr<CPDF_ImageLoaderHandle> m_LoadHandle;
  bool m_bStdCS;
  int m_BlendType;
};

class CPDF_ScaledRenderBuffer {
 public:
  CPDF_ScaledRenderBuffer();
  ~CPDF_ScaledRenderBuffer();

  bool Initialize(CPDF_RenderContext* pContext,
                  CFX_RenderDevice* pDevice,
                  const FX_RECT& pRect,
                  const CPDF_PageObject* pObj,
                  const CPDF_RenderOptions* pOptions = nullptr,
                  int max_dpi = 0);
  CFX_RenderDevice* GetDevice() {
    return m_pBitmapDevice ? m_pBitmapDevice.get() : m_pDevice;
  }
  CFX_Matrix* GetMatrix() { return &m_Matrix; }
  void OutputToDevice();

 private:
  CFX_RenderDevice* m_pDevice;
  CPDF_RenderContext* m_pContext;
  FX_RECT m_Rect;
  const CPDF_PageObject* m_pObject;
  std::unique_ptr<CFX_FxgeDevice> m_pBitmapDevice;
  CFX_Matrix m_Matrix;
};

class CPDF_DeviceBuffer {
 public:
  CPDF_DeviceBuffer();
  ~CPDF_DeviceBuffer();
  bool Initialize(CPDF_RenderContext* pContext,
                  CFX_RenderDevice* pDevice,
                  FX_RECT* pRect,
                  const CPDF_PageObject* pObj,
                  int max_dpi = 0);
  void OutputToDevice();
  CFX_DIBitmap* GetBitmap() const { return m_pBitmap.get(); }
  const CFX_Matrix* GetMatrix() const { return &m_Matrix; }

 private:
  CFX_RenderDevice* m_pDevice;
  CPDF_RenderContext* m_pContext;
  FX_RECT m_Rect;
  const CPDF_PageObject* m_pObject;
  std::unique_ptr<CFX_DIBitmap> m_pBitmap;
  CFX_Matrix m_Matrix;
};

class CPDF_ImageCacheEntry {
 public:
  CPDF_ImageCacheEntry(CPDF_Document* pDoc, CPDF_Stream* pStream);
  ~CPDF_ImageCacheEntry();

  void Reset(const CFX_DIBitmap* pBitmap);
  bool GetCachedBitmap(CFX_DIBSource*& pBitmap,
                       CFX_DIBSource*& pMask,
                       uint32_t& MatteColor,
                       CPDF_Dictionary* pPageResources,
                       bool bStdCS = false,
                       uint32_t GroupFamily = 0,
                       bool bLoadMask = false,
                       CPDF_RenderStatus* pRenderStatus = nullptr,
                       int32_t downsampleWidth = 0,
                       int32_t downsampleHeight = 0);
  uint32_t EstimateSize() const { return m_dwCacheSize; }
  uint32_t GetTimeCount() const { return m_dwTimeCount; }
  CPDF_Stream* GetStream() const { return m_pStream; }
  void SetTimeCount(uint32_t dwTimeCount) { m_dwTimeCount = dwTimeCount; }
  int m_dwTimeCount;

 public:
  int StartGetCachedBitmap(CPDF_Dictionary* pFormResources,
                           CPDF_Dictionary* pPageResources,
                           bool bStdCS = false,
                           uint32_t GroupFamily = 0,
                           bool bLoadMask = false,
                           CPDF_RenderStatus* pRenderStatus = nullptr,
                           int32_t downsampleWidth = 0,
                           int32_t downsampleHeight = 0);
  int Continue(IFX_Pause* pPause);
  CFX_DIBSource* DetachBitmap();
  CFX_DIBSource* DetachMask();
  CFX_DIBSource* m_pCurBitmap;
  CFX_DIBSource* m_pCurMask;
  uint32_t m_MatteColor;
  CPDF_RenderStatus* m_pRenderStatus;

 protected:
  void ContinueGetCachedBitmap();

  CPDF_Document* m_pDocument;
  CPDF_Stream* m_pStream;
  CFX_DIBSource* m_pCachedBitmap;
  CFX_DIBSource* m_pCachedMask;
  uint32_t m_dwCacheSize;
  void CalcSize();
};
typedef struct {
  FX_FLOAT m_DecodeMin;
  FX_FLOAT m_DecodeStep;
  int m_ColorKeyMin;
  int m_ColorKeyMax;
} DIB_COMP_DATA;

class CPDF_DIBSource : public CFX_DIBSource {
 public:
  CPDF_DIBSource();
  ~CPDF_DIBSource() override;

  bool Load(CPDF_Document* pDoc,
            const CPDF_Stream* pStream,
            CPDF_DIBSource** ppMask,
            uint32_t* pMatteColor,
            CPDF_Dictionary* pFormResources,
            CPDF_Dictionary* pPageResources,
            bool bStdCS = false,
            uint32_t GroupFamily = 0,
            bool bLoadMask = false);

  // CFX_DIBSource
  bool SkipToScanline(int line, IFX_Pause* pPause) const override;
  uint8_t* GetBuffer() const override;
  const uint8_t* GetScanline(int line) const override;
  void DownSampleScanline(int line,
                          uint8_t* dest_scan,
                          int dest_bpp,
                          int dest_width,
                          bool bFlipX,
                          int clip_left,
                          int clip_width) const override;

  CFX_DIBitmap* GetBitmap() const;
  void ReleaseBitmap(CFX_DIBitmap* pBitmap) const;
  uint32_t GetMatteColor() const { return m_MatteColor; }

  int StartLoadDIBSource(CPDF_Document* pDoc,
                         const CPDF_Stream* pStream,
                         bool bHasMask,
                         CPDF_Dictionary* pFormResources,
                         CPDF_Dictionary* pPageResources,
                         bool bStdCS = false,
                         uint32_t GroupFamily = 0,
                         bool bLoadMask = false);
  int ContinueLoadDIBSource(IFX_Pause* pPause);
  int StratLoadMask();
  int StartLoadMaskDIB();
  int ContinueLoadMaskDIB(IFX_Pause* pPause);
  int ContinueToLoadMask();
  CPDF_DIBSource* DetachMask();

 private:
  bool LoadColorInfo(const CPDF_Dictionary* pFormResources,
                     const CPDF_Dictionary* pPageResources);
  DIB_COMP_DATA* GetDecodeAndMaskArray(bool& bDefaultDecode, bool& bColorKey);
  CPDF_DIBSource* LoadMask(uint32_t& MatteColor);
  CPDF_DIBSource* LoadMaskDIB(CPDF_Stream* pMask);
  void LoadJpxBitmap();
  void LoadPalette();
  int CreateDecoder();
  void TranslateScanline24bpp(uint8_t* dest_scan,
                              const uint8_t* src_scan) const;
  void ValidateDictParam();
  void DownSampleScanline1Bit(int orig_Bpp,
                              int dest_Bpp,
                              uint32_t src_width,
                              const uint8_t* pSrcLine,
                              uint8_t* dest_scan,
                              int dest_width,
                              bool bFlipX,
                              int clip_left,
                              int clip_width) const;
  void DownSampleScanline8Bit(int orig_Bpp,
                              int dest_Bpp,
                              uint32_t src_width,
                              const uint8_t* pSrcLine,
                              uint8_t* dest_scan,
                              int dest_width,
                              bool bFlipX,
                              int clip_left,
                              int clip_width) const;
  void DownSampleScanline32Bit(int orig_Bpp,
                               int dest_Bpp,
                               uint32_t src_width,
                               const uint8_t* pSrcLine,
                               uint8_t* dest_scan,
                               int dest_width,
                               bool bFlipX,
                               int clip_left,
                               int clip_width) const;
  bool TransMask() const;

  CPDF_Document* m_pDocument;
  const CPDF_Stream* m_pStream;
  std::unique_ptr<CPDF_StreamAcc> m_pStreamAcc;
  const CPDF_Dictionary* m_pDict;
  CPDF_ColorSpace* m_pColorSpace;
  uint32_t m_Family;
  uint32_t m_bpc;
  uint32_t m_bpc_orig;
  uint32_t m_nComponents;
  uint32_t m_GroupFamily;
  uint32_t m_MatteColor;
  bool m_bLoadMask;
  bool m_bDefaultDecode;
  bool m_bImageMask;
  bool m_bDoBpcCheck;
  bool m_bColorKey;
  bool m_bHasMask;
  bool m_bStdCS;
  DIB_COMP_DATA* m_pCompData;
  uint8_t* m_pLineBuf;
  uint8_t* m_pMaskedLine;
  std::unique_ptr<CFX_DIBitmap> m_pCachedBitmap;
  std::unique_ptr<CCodec_ScanlineDecoder> m_pDecoder;
  CPDF_DIBSource* m_pMask;
  std::unique_ptr<CPDF_StreamAcc> m_pGlobalStream;
  std::unique_ptr<CCodec_Jbig2Context> m_pJbig2Context;
  CPDF_Stream* m_pMaskStream;
  int m_Status;
};

#define FPDF_HUGE_IMAGE_SIZE 60000000
class CPDF_DIBTransferFunc : public CFX_FilteredDIB {
 public:
  explicit CPDF_DIBTransferFunc(const CPDF_TransferFunc* pTransferFunc);
  ~CPDF_DIBTransferFunc() override;

  // CFX_FilteredDIB
  FXDIB_Format GetDestFormat() override;
  FX_ARGB* GetDestPalette() override;
  void TranslateScanline(const uint8_t* src_buf,
                         std::vector<uint8_t>* dest_buf) const override;
  void TranslateDownSamples(uint8_t* dest_buf,
                            const uint8_t* src_buf,
                            int pixels,
                            int Bpp) const override;

  const uint8_t* m_RampR;
  const uint8_t* m_RampG;
  const uint8_t* m_RampB;
};

#endif  // CORE_FPDFAPI_RENDER_RENDER_INT_H_
