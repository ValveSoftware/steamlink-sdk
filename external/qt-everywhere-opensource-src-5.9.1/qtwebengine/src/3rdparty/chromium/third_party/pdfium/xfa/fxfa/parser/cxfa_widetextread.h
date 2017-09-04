// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_WIDETEXTREAD_H_
#define XFA_FXFA_PARSER_CXFA_WIDETEXTREAD_H_

#include "xfa/fgas/crt/fgas_stream.h"

class CXFA_WideTextRead : public IFX_Stream {
 public:
  CXFA_WideTextRead(const CFX_WideString& wsBuffer);

  // IFX_Stream
  void Release() override;
  IFX_Stream* Retain() override;
  uint32_t GetAccessModes() const override;
  int32_t GetLength() const override;
  int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) override;
  int32_t GetPosition() override;
  bool IsEOF() const override;
  int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t ReadString(FX_WCHAR* pStr, int32_t iMaxLength, bool& bEOS) override;
  int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t WriteString(const FX_WCHAR* pStr, int32_t iLength) override;
  void Flush() override {}
  bool SetLength(int32_t iLength) override;
  int32_t GetBOM(uint8_t bom[4]) const override;
  uint16_t GetCodePage() const override;
  uint16_t SetCodePage(uint16_t wCodePage) override;
  IFX_Stream* CreateSharedStream(uint32_t dwAccess,
                                 int32_t iOffset,
                                 int32_t iLength) override;

  CFX_WideString GetSrcText() const;

 protected:
  CFX_WideString m_wsBuffer;
  int32_t m_iPosition;
  int32_t m_iRefCount;
};

#endif  // XFA_FXFA_PARSER_CXFA_WIDETEXTREAD_H_
