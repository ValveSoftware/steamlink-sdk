// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_PAGEINT_H_
#define CORE_FPDFAPI_PAGE_PAGEINT_H_

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "core/fpdfapi/page/cpdf_contentmark.h"
#include "core/fpdfapi/page/cpdf_countedobject.h"
#include "core/fpdfapi/page/cpdf_pageobjectholder.h"
#include "core/fpdfapi/page/cpdf_streamcontentparser.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/cfx_weak_ptr.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"

class CPDF_AllStates;
class CPDF_ColorSpace;
class CPDF_ExpIntFunc;
class CPDF_Font;
class CPDF_FontEncoding;
class CPDF_Form;
class CPDF_IccProfile;
class CPDF_Image;
class CPDF_ImageObject;
class CPDF_Page;
class CPDF_Pattern;
class CPDF_SampledFunc;
class CPDF_StitchFunc;
class CPDF_StreamAcc;
class CPDF_TextObject;
class CPDF_Type3Char;

#define PARSE_STEP_LIMIT 100

class CPDF_StreamParser {
 public:
  enum SyntaxType { EndOfData, Number, Keyword, Name, Others };

  CPDF_StreamParser(const uint8_t* pData, uint32_t dwSize);
  CPDF_StreamParser(const uint8_t* pData,
                    uint32_t dwSize,
                    const CFX_WeakPtr<CFX_ByteStringPool>& pPool);
  ~CPDF_StreamParser();

  CPDF_Stream* ReadInlineStream(CPDF_Document* pDoc,
                                CPDF_Dictionary* pDict,
                                CPDF_Object* pCSObj);
  SyntaxType ParseNextElement();
  uint8_t* GetWordBuf() { return m_WordBuffer; }
  uint32_t GetWordSize() const { return m_WordSize; }
  CPDF_Object* GetObject();
  uint32_t GetPos() const { return m_Pos; }
  void SetPos(uint32_t pos) { m_Pos = pos; }
  CPDF_Object* ReadNextObject(bool bAllowNestedArray, uint32_t dwInArrayLevel);

 private:
  friend class cpdf_streamparser_ReadHexString_Test;

  void GetNextWord(bool& bIsNumber);
  CFX_ByteString ReadString();
  CFX_ByteString ReadHexString();
  bool PositionIsInBounds() const;

  const uint8_t* m_pBuf;
  uint32_t m_Size;  // Length in bytes of m_pBuf.
  uint32_t m_Pos;   // Current byte position within m_pBuf.
  uint8_t m_WordBuffer[256];
  uint32_t m_WordSize;
  CPDF_Object* m_pLastObj;
  CFX_WeakPtr<CFX_ByteStringPool> m_pPool;
};

#define _FPDF_MAX_TYPE3_FORM_LEVEL_ 4

class CPDF_ContentParser {
 public:
  enum ParseStatus { Ready, ToBeContinued, Done };

  CPDF_ContentParser();
  ~CPDF_ContentParser();

  ParseStatus GetStatus() const { return m_Status; }
  void Start(CPDF_Page* pPage);
  void Start(CPDF_Form* pForm,
             CPDF_AllStates* pGraphicStates,
             const CFX_Matrix* pParentMatrix,
             CPDF_Type3Char* pType3Char,
             int level);
  void Continue(IFX_Pause* pPause);

 private:
  enum InternalStage {
    STAGE_GETCONTENT = 1,
    STAGE_PARSE,
    STAGE_CHECKCLIP,
  };

  ParseStatus m_Status;
  InternalStage m_InternalStage;
  CPDF_PageObjectHolder* m_pObjectHolder;
  bool m_bForm;
  CPDF_Type3Char* m_pType3Char;
  uint32_t m_nStreams;
  std::unique_ptr<CPDF_StreamAcc> m_pSingleStream;
  std::vector<std::unique_ptr<CPDF_StreamAcc>> m_StreamArray;
  uint8_t* m_pData;
  uint32_t m_Size;
  uint32_t m_CurrentOffset;
  std::unique_ptr<CPDF_StreamContentParser> m_pParser;
};

class CPDF_Function {
 public:
  enum class Type {
    kTypeInvalid = -1,
    kType0Sampled = 0,
    kType2ExpotentialInterpolation = 2,
    kType3Stitching = 3,
    kType4PostScript = 4,
  };

  static std::unique_ptr<CPDF_Function> Load(CPDF_Object* pFuncObj);
  static Type IntegerToFunctionType(int iType);

  virtual ~CPDF_Function();
  bool Call(FX_FLOAT* inputs,
            uint32_t ninputs,
            FX_FLOAT* results,
            int& nresults) const;
  uint32_t CountInputs() const { return m_nInputs; }
  uint32_t CountOutputs() const { return m_nOutputs; }
  FX_FLOAT GetDomain(int i) const { return m_pDomains[i]; }
  FX_FLOAT GetRange(int i) const { return m_pRanges[i]; }

  const CPDF_SampledFunc* ToSampledFunc() const;
  const CPDF_ExpIntFunc* ToExpIntFunc() const;
  const CPDF_StitchFunc* ToStitchFunc() const;

 protected:
  explicit CPDF_Function(Type type);

  bool Init(CPDF_Object* pObj);
  virtual bool v_Init(CPDF_Object* pObj) = 0;
  virtual bool v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const = 0;

  uint32_t m_nInputs;
  uint32_t m_nOutputs;
  FX_FLOAT* m_pDomains;
  FX_FLOAT* m_pRanges;
  const Type m_Type;
};

class CPDF_ExpIntFunc : public CPDF_Function {
 public:
  CPDF_ExpIntFunc();
  ~CPDF_ExpIntFunc() override;

  // CPDF_Function
  bool v_Init(CPDF_Object* pObj) override;
  bool v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  uint32_t m_nOrigOutputs;
  FX_FLOAT m_Exponent;
  FX_FLOAT* m_pBeginValues;
  FX_FLOAT* m_pEndValues;
};

class CPDF_SampledFunc : public CPDF_Function {
 public:
  struct SampleEncodeInfo {
    FX_FLOAT encode_max;
    FX_FLOAT encode_min;
    uint32_t sizes;
  };

  struct SampleDecodeInfo {
    FX_FLOAT decode_max;
    FX_FLOAT decode_min;
  };

  CPDF_SampledFunc();
  ~CPDF_SampledFunc() override;

  // CPDF_Function
  bool v_Init(CPDF_Object* pObj) override;
  bool v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  const std::vector<SampleEncodeInfo>& GetEncodeInfo() const {
    return m_EncodeInfo;
  }
  uint32_t GetBitsPerSample() const { return m_nBitsPerSample; }
  const CPDF_StreamAcc* GetSampleStream() const {
    return m_pSampleStream.get();
  }

 private:
  std::vector<SampleEncodeInfo> m_EncodeInfo;
  std::vector<SampleDecodeInfo> m_DecodeInfo;
  uint32_t m_nBitsPerSample;
  uint32_t m_SampleMax;
  std::unique_ptr<CPDF_StreamAcc> m_pSampleStream;
};

class CPDF_StitchFunc : public CPDF_Function {
 public:
  CPDF_StitchFunc();
  ~CPDF_StitchFunc() override;

  // CPDF_Function
  bool v_Init(CPDF_Object* pObj) override;
  bool v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  const std::vector<std::unique_ptr<CPDF_Function>>& GetSubFunctions() const {
    return m_pSubFunctions;
  }
  FX_FLOAT GetBound(size_t i) const { return m_pBounds[i]; }

 private:
  std::vector<std::unique_ptr<CPDF_Function>> m_pSubFunctions;
  FX_FLOAT* m_pBounds;
  FX_FLOAT* m_pEncode;

  static const uint32_t kRequiredNumInputs = 1;
};

class CPDF_IccProfile {
 public:
  CPDF_IccProfile(const uint8_t* pData, uint32_t dwSize);
  ~CPDF_IccProfile();
  uint32_t GetComponents() const { return m_nSrcComponents; }
  bool m_bsRGB;
  void* m_pTransform;

 private:
  uint32_t m_nSrcComponents;
};

class CPDF_DeviceCS : public CPDF_ColorSpace {
 public:
  CPDF_DeviceCS(CPDF_Document* pDoc, int family);

  bool GetRGB(FX_FLOAT* pBuf,
              FX_FLOAT& R,
              FX_FLOAT& G,
              FX_FLOAT& B) const override;
  bool SetRGB(FX_FLOAT* pBuf,
              FX_FLOAT R,
              FX_FLOAT G,
              FX_FLOAT B) const override;
  bool v_GetCMYK(FX_FLOAT* pBuf,
                 FX_FLOAT& c,
                 FX_FLOAT& m,
                 FX_FLOAT& y,
                 FX_FLOAT& k) const override;
  bool v_SetCMYK(FX_FLOAT* pBuf,
                 FX_FLOAT c,
                 FX_FLOAT m,
                 FX_FLOAT y,
                 FX_FLOAT k) const override;
  void TranslateImageLine(uint8_t* pDestBuf,
                          const uint8_t* pSrcBuf,
                          int pixels,
                          int image_width,
                          int image_height,
                          bool bTransMask = false) const override;
};

class CPDF_PatternCS : public CPDF_ColorSpace {
 public:
  explicit CPDF_PatternCS(CPDF_Document* pDoc);
  ~CPDF_PatternCS() override;
  bool v_Load(CPDF_Document* pDoc, CPDF_Array* pArray) override;
  bool GetRGB(FX_FLOAT* pBuf,
              FX_FLOAT& R,
              FX_FLOAT& G,
              FX_FLOAT& B) const override;
  CPDF_ColorSpace* GetBaseCS() const override;

 private:
  CPDF_ColorSpace* m_pBaseCS;
  CPDF_CountedColorSpace* m_pCountedBaseCS;
};

#define MAX_PATTERN_COLORCOMPS 16
struct PatternValue {
  CPDF_Pattern* m_pPattern;
  CPDF_CountedPattern* m_pCountedPattern;
  int m_nComps;
  FX_FLOAT m_Comps[MAX_PATTERN_COLORCOMPS];
};

CFX_ByteStringC PDF_FindKeyAbbreviationForTesting(const CFX_ByteStringC& abbr);
CFX_ByteStringC PDF_FindValueAbbreviationForTesting(
    const CFX_ByteStringC& abbr);

#endif  // CORE_FPDFAPI_PAGE_PAGEINT_H_
