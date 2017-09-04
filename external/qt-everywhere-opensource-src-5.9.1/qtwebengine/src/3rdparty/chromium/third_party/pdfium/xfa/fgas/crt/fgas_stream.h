// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_CRT_FGAS_STREAM_H_
#define XFA_FGAS_CRT_FGAS_STREAM_H_

#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/fx_system.h"

enum FX_STREAMACCESS {
  FX_STREAMACCESS_Binary = 0x00,
  FX_STREAMACCESS_Text = 0x01,
  FX_STREAMACCESS_Read = 0x02,
  FX_STREAMACCESS_Write = 0x04,
  FX_STREAMACCESS_Truncate = 0x10,
  FX_STREAMACCESS_Append = 0x20,
  FX_STREAMACCESS_Create = 0x80,
};

enum FX_STREAMSEEK {
  FX_STREAMSEEK_Begin = 0,
  FX_STREAMSEEK_Current,
  FX_STREAMSEEK_End,
};

class IFX_Stream {
 public:
  static IFX_Stream* CreateStream(IFX_SeekableReadStream* pFileRead,
                                  uint32_t dwAccess);
  static IFX_Stream* CreateStream(IFX_SeekableWriteStream* pFileWrite,
                                  uint32_t dwAccess);
  static IFX_Stream* CreateStream(uint8_t* pData,
                                  int32_t length,
                                  uint32_t dwAccess);
  static IFX_Stream* CreateTextStream(IFX_Stream* pBaseStream,
                                      bool bDeleteOnRelease);
  virtual ~IFX_Stream() {}
  virtual void Release() = 0;
  virtual IFX_Stream* Retain() = 0;

  virtual IFX_Stream* CreateSharedStream(uint32_t dwAccess,
                                         int32_t iOffset,
                                         int32_t iLength) = 0;

  virtual uint32_t GetAccessModes() const = 0;
  virtual int32_t GetLength() const = 0;
  virtual int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) = 0;
  virtual int32_t GetPosition() = 0;
  virtual bool IsEOF() const = 0;
  virtual int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) = 0;
  virtual int32_t ReadString(FX_WCHAR* pStr,
                             int32_t iMaxLength,
                             bool& bEOS) = 0;
  virtual int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) = 0;
  virtual int32_t WriteString(const FX_WCHAR* pStr, int32_t iLength) = 0;
  virtual void Flush() = 0;
  virtual bool SetLength(int32_t iLength) = 0;
  virtual int32_t GetBOM(uint8_t bom[4]) const = 0;
  virtual uint16_t GetCodePage() const = 0;
  virtual uint16_t SetCodePage(uint16_t wCodePage) = 0;
};

IFX_SeekableReadStream* FX_CreateFileRead(IFX_Stream* pBaseStream,
                                          bool bReleaseStream);

#endif  // XFA_FGAS_CRT_FGAS_STREAM_H_
