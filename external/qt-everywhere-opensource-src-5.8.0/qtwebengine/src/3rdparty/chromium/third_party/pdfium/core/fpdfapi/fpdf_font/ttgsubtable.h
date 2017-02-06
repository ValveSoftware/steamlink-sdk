// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FPDF_FONT_TTGSUBTABLE_H_
#define CORE_FPDFAPI_FPDF_FONT_TTGSUBTABLE_H_

#include <stdint.h>

#include <map>

#include "core/fxcrt/include/fx_basic.h"
#include "core/fxge/include/fx_font.h"
#include "core/fxge/include/fx_freetype.h"

class CFX_GlyphMap {
 public:
  CFX_GlyphMap();
  ~CFX_GlyphMap();

  void SetAt(int key, int value);
  FX_BOOL Lookup(int key, int& value);

 protected:
  CFX_BinaryBuf m_Buffer;
};

class CFX_CTTGSUBTable {
 public:
  CFX_CTTGSUBTable();
  explicit CFX_CTTGSUBTable(FT_Bytes gsub);
  virtual ~CFX_CTTGSUBTable();

  bool IsOk() const;
  bool LoadGSUBTable(FT_Bytes gsub);
  bool GetVerticalGlyph(uint32_t glyphnum, uint32_t* vglyphnum);

 private:
  struct tt_gsub_header {
    uint32_t Version;
    uint16_t ScriptList;
    uint16_t FeatureList;
    uint16_t LookupList;
  };
  struct TLangSys {
    TLangSys()
        : LookupOrder(0),
          ReqFeatureIndex(0),
          FeatureCount(0),
          FeatureIndex(nullptr) {}
    ~TLangSys() { delete[] FeatureIndex; }

    uint16_t LookupOrder;
    uint16_t ReqFeatureIndex;
    uint16_t FeatureCount;
    uint16_t* FeatureIndex;

   private:
    TLangSys(const TLangSys&);
    TLangSys& operator=(const TLangSys&);
  };
  struct TLangSysRecord {
    TLangSysRecord() : LangSysTag(0) {}

    uint32_t LangSysTag;
    struct TLangSys LangSys;

   private:
    TLangSysRecord(const TLangSysRecord&);
    TLangSysRecord& operator=(const TLangSysRecord&);
  };
  struct TScript {
    TScript() : DefaultLangSys(0), LangSysCount(0), LangSysRecord(nullptr) {}
    ~TScript() { delete[] LangSysRecord; }

    uint16_t DefaultLangSys;
    uint16_t LangSysCount;
    // TODO(weili): Replace with a smart pointer type, pdfium:518.
    struct TLangSysRecord* LangSysRecord;

   private:
    TScript(const TScript&);
    TScript& operator=(const TScript&);
  };
  struct TScriptRecord {
    TScriptRecord() : ScriptTag(0) {}

    uint32_t ScriptTag;
    struct TScript Script;

   private:
    TScriptRecord(const TScriptRecord&);
    TScriptRecord& operator=(const TScriptRecord&);
  };
  struct TScriptList {
    TScriptList() : ScriptCount(0), ScriptRecord(nullptr) {}
    ~TScriptList() { delete[] ScriptRecord; }

    uint16_t ScriptCount;
    struct TScriptRecord* ScriptRecord;

   private:
    TScriptList(const TScriptList&);
    TScriptList& operator=(const TScriptList&);
  };
  struct TFeature {
    TFeature() : FeatureParams(0), LookupCount(0), LookupListIndex(nullptr) {}
    ~TFeature() { delete[] LookupListIndex; }

    uint16_t FeatureParams;
    int LookupCount;
    uint16_t* LookupListIndex;

   private:
    TFeature(const TFeature&);
    TFeature& operator=(const TFeature&);
  };
  struct TFeatureRecord {
    TFeatureRecord() : FeatureTag(0) {}

    uint32_t FeatureTag;
    struct TFeature Feature;

   private:
    TFeatureRecord(const TFeatureRecord&);
    TFeatureRecord& operator=(const TFeatureRecord&);
  };
  struct TFeatureList {
    TFeatureList() : FeatureCount(0), FeatureRecord(nullptr) {}
    ~TFeatureList() { delete[] FeatureRecord; }

    int FeatureCount;
    struct TFeatureRecord* FeatureRecord;

   private:
    TFeatureList(const TFeatureList&);
    TFeatureList& operator=(const TFeatureList&);
  };
  enum TLookupFlag {
    LOOKUPFLAG_RightToLeft = 0x0001,
    LOOKUPFLAG_IgnoreBaseGlyphs = 0x0002,
    LOOKUPFLAG_IgnoreLigatures = 0x0004,
    LOOKUPFLAG_IgnoreMarks = 0x0008,
    LOOKUPFLAG_Reserved = 0x00F0,
    LOOKUPFLAG_MarkAttachmentType = 0xFF00,
  };
  struct TCoverageFormatBase {
    TCoverageFormatBase() : CoverageFormat(0) {}
    virtual ~TCoverageFormatBase() {}

    uint16_t CoverageFormat;
    CFX_GlyphMap m_glyphMap;

   private:
    TCoverageFormatBase(const TCoverageFormatBase&);
    TCoverageFormatBase& operator=(const TCoverageFormatBase&);
  };
  struct TCoverageFormat1 : public TCoverageFormatBase {
    TCoverageFormat1();
    ~TCoverageFormat1() override;

    uint16_t GlyphCount;
    uint16_t* GlyphArray;

   private:
    TCoverageFormat1(const TCoverageFormat1&);
    TCoverageFormat1& operator=(const TCoverageFormat1&);
  };
  struct TRangeRecord {
    TRangeRecord();

    friend bool operator>(const TRangeRecord& r1, const TRangeRecord& r2) {
      return r1.Start > r2.Start;
    }

    uint16_t Start;
    uint16_t End;
    uint16_t StartCoverageIndex;

   private:
    TRangeRecord(const TRangeRecord&);
  };
  struct TCoverageFormat2 : public TCoverageFormatBase {
    TCoverageFormat2();
    ~TCoverageFormat2() override;

    uint16_t RangeCount;
    struct TRangeRecord* RangeRecord;

   private:
    TCoverageFormat2(const TCoverageFormat2&);
    TCoverageFormat2& operator=(const TCoverageFormat2&);
  };
  struct TClassDefFormatBase {
    TClassDefFormatBase() : ClassFormat(0) {}
    virtual ~TClassDefFormatBase() {}

    uint16_t ClassFormat;

   private:
    TClassDefFormatBase(const TClassDefFormatBase&);
    TClassDefFormatBase& operator=(const TClassDefFormatBase&);
  };
  struct TClassDefFormat1 : public TClassDefFormatBase {
    TClassDefFormat1();
    ~TClassDefFormat1() override;

    uint16_t StartGlyph;
    uint16_t GlyphCount;
    uint16_t* ClassValueArray;

   private:
    TClassDefFormat1(const TClassDefFormat1&);
    TClassDefFormat1& operator=(const TClassDefFormat1&);
  };
  struct TClassRangeRecord {
    TClassRangeRecord() : Start(0), End(0), Class(0) {}

    uint16_t Start;
    uint16_t End;
    uint16_t Class;

   private:
    TClassRangeRecord(const TClassRangeRecord&);
    TClassRangeRecord& operator=(const TClassRangeRecord&);
  };
  struct TClassDefFormat2 : public TClassDefFormatBase {
    TClassDefFormat2();
    ~TClassDefFormat2() override;

    uint16_t ClassRangeCount;
    struct TClassRangeRecord* ClassRangeRecord;

   private:
    TClassDefFormat2(const TClassDefFormat2&);
    TClassDefFormat2& operator=(const TClassDefFormat2&);
  };
  struct TDevice {
    TDevice() : StartSize(0), EndSize(0), DeltaFormat(0) {}

    uint16_t StartSize;
    uint16_t EndSize;
    uint16_t DeltaFormat;

   private:
    TDevice(const TDevice&);
    TDevice& operator=(const TDevice&);
  };
  struct TSubTableBase {
    TSubTableBase() : SubstFormat(0) {}
    virtual ~TSubTableBase() {}

    uint16_t SubstFormat;

   private:
    TSubTableBase(const TSubTableBase&);
    TSubTableBase& operator=(const TSubTableBase&);
  };
  struct TSingleSubstFormat1 : public TSubTableBase {
    TSingleSubstFormat1();
    ~TSingleSubstFormat1() override;

    TCoverageFormatBase* Coverage;
    int16_t DeltaGlyphID;

   private:
    TSingleSubstFormat1(const TSingleSubstFormat1&);
    TSingleSubstFormat1& operator=(const TSingleSubstFormat1&);
  };
  struct TSingleSubstFormat2 : public TSubTableBase {
    TSingleSubstFormat2();
    ~TSingleSubstFormat2() override;

    TCoverageFormatBase* Coverage;
    uint16_t GlyphCount;
    uint16_t* Substitute;

   private:
    TSingleSubstFormat2(const TSingleSubstFormat2&);
    TSingleSubstFormat2& operator=(const TSingleSubstFormat2&);
  };
  struct TLookup {
    TLookup();
    ~TLookup();

    uint16_t LookupType;
    uint16_t LookupFlag;
    uint16_t SubTableCount;
    struct TSubTableBase** SubTable;

   private:
    TLookup(const TLookup&);
    TLookup& operator=(const TLookup&);
  };
  struct TLookupList {
    TLookupList() : LookupCount(0), Lookup(nullptr) {}
    ~TLookupList() { delete[] Lookup; }

    int LookupCount;
    struct TLookup* Lookup;

   private:
    TLookupList(const TLookupList&);
    TLookupList& operator=(const TLookupList&);
  };

  bool Parse(FT_Bytes scriptlist, FT_Bytes featurelist, FT_Bytes lookuplist);
  void ParseScriptList(FT_Bytes raw, TScriptList* rec);
  void ParseScript(FT_Bytes raw, TScript* rec);
  void ParseLangSys(FT_Bytes raw, TLangSys* rec);
  void ParseFeatureList(FT_Bytes raw, TFeatureList* rec);
  void ParseFeature(FT_Bytes raw, TFeature* rec);
  void ParseLookupList(FT_Bytes raw, TLookupList* rec);
  void ParseLookup(FT_Bytes raw, TLookup* rec);
  void ParseCoverage(FT_Bytes raw, TCoverageFormatBase** rec);
  void ParseCoverageFormat1(FT_Bytes raw, TCoverageFormat1* rec);
  void ParseCoverageFormat2(FT_Bytes raw, TCoverageFormat2* rec);
  void ParseSingleSubst(FT_Bytes raw, TSubTableBase** rec);
  void ParseSingleSubstFormat1(FT_Bytes raw, TSingleSubstFormat1* rec);
  void ParseSingleSubstFormat2(FT_Bytes raw, TSingleSubstFormat2* rec);

  bool GetVerticalGlyphSub(uint32_t glyphnum,
                           uint32_t* vglyphnum,
                           struct TFeature* Feature) const;
  bool GetVerticalGlyphSub2(uint32_t glyphnum,
                            uint32_t* vglyphnum,
                            struct TLookup* Lookup) const;
  int GetCoverageIndex(struct TCoverageFormatBase* Coverage, uint32_t g) const;

  uint8_t GetUInt8(FT_Bytes& p) const;
  int16_t GetInt16(FT_Bytes& p) const;
  uint16_t GetUInt16(FT_Bytes& p) const;
  int32_t GetInt32(FT_Bytes& p) const;
  uint32_t GetUInt32(FT_Bytes& p) const;

  std::map<uint32_t, uint32_t> m_featureMap;
  FX_BOOL m_bFeautureMapLoad;
  bool loaded;
  struct tt_gsub_header header;
  struct TScriptList ScriptList;
  struct TFeatureList FeatureList;
  struct TLookupList LookupList;
};

#endif  // CORE_FPDFAPI_FPDF_FONT_TTGSUBTABLE_H_
