// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_STREAM_H_
#define CORE_FXCRT_FX_STREAM_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#include <direct.h>

class CFindFileDataA;

typedef CFindFileDataA FX_FileHandle;
#define FX_FILESIZE int32_t
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif  // O_BINARY

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif  // O_LARGEFILE

typedef DIR FX_FileHandle;
#define FX_FILESIZE off_t
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_

FX_FileHandle* FX_OpenFolder(const FX_CHAR* path);
bool FX_GetNextFile(FX_FileHandle* handle,
                    CFX_ByteString* filename,
                    bool* bFolder);
void FX_CloseFolder(FX_FileHandle* handle);
FX_WCHAR FX_GetFolderSeparator();

#define FX_GETBYTEOFFSET32(a) 0
#define FX_GETBYTEOFFSET40(a) 0
#define FX_GETBYTEOFFSET48(a) 0
#define FX_GETBYTEOFFSET56(a) 0
#define FX_GETBYTEOFFSET24(a) ((uint8_t)(a >> 24))
#define FX_GETBYTEOFFSET16(a) ((uint8_t)(a >> 16))
#define FX_GETBYTEOFFSET8(a) ((uint8_t)(a >> 8))
#define FX_GETBYTEOFFSET0(a) ((uint8_t)(a))
#define FX_FILEMODE_Write 0
#define FX_FILEMODE_ReadOnly 1
#define FX_FILEMODE_Truncate 2

class IFX_WriteStream {
 public:
  virtual ~IFX_WriteStream() {}
  virtual void Release() = 0;
  virtual bool WriteBlock(const void* pData, size_t size) = 0;
};

class IFX_ReadStream {
 public:
  virtual ~IFX_ReadStream() {}

  virtual void Release() = 0;
  virtual bool IsEOF() = 0;
  virtual FX_FILESIZE GetPosition() = 0;
  virtual size_t ReadBlock(void* buffer, size_t size) = 0;
};

class IFX_SeekableWriteStream : public IFX_WriteStream {
 public:
  // IFX_WriteStream:
  bool WriteBlock(const void* pData, size_t size) override;
  virtual FX_FILESIZE GetSize() = 0;
  virtual bool Flush() = 0;
  virtual bool WriteBlock(const void* pData,
                          FX_FILESIZE offset,
                          size_t size) = 0;
};

class IFX_SeekableReadStream : public IFX_ReadStream {
 public:
  // IFX_ReadStream:
  void Release() override = 0;
  bool IsEOF() override;
  FX_FILESIZE GetPosition() override;
  size_t ReadBlock(void* buffer, size_t size) override;

  virtual bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) = 0;
  virtual FX_FILESIZE GetSize() = 0;
};

IFX_SeekableReadStream* FX_CreateFileRead(const FX_CHAR* filename);
IFX_SeekableReadStream* FX_CreateFileRead(const FX_WCHAR* filename);

class IFX_SeekableStream : public IFX_SeekableReadStream,
                           public IFX_SeekableWriteStream {
 public:
  virtual IFX_SeekableStream* Retain() = 0;

  // IFX_SeekableReadStream:
  void Release() override = 0;
  bool IsEOF() override = 0;
  FX_FILESIZE GetPosition() override = 0;
  size_t ReadBlock(void* buffer, size_t size) override = 0;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override = 0;
  FX_FILESIZE GetSize() override = 0;

  // IFX_SeekableWriteStream:
  bool WriteBlock(const void* buffer,
                  FX_FILESIZE offset,
                  size_t size) override = 0;
  bool WriteBlock(const void* buffer, size_t size) override;
  bool Flush() override = 0;
};

IFX_SeekableStream* FX_CreateFileStream(const FX_CHAR* filename,
                                        uint32_t dwModes);
IFX_SeekableStream* FX_CreateFileStream(const FX_WCHAR* filename,
                                        uint32_t dwModes);

#ifdef PDF_ENABLE_XFA
class IFX_FileAccess {
 public:
  virtual ~IFX_FileAccess() {}
  virtual void Release() = 0;
  virtual IFX_FileAccess* Retain() = 0;
  virtual void GetPath(CFX_WideString& wsPath) = 0;
  virtual IFX_SeekableStream* CreateFileStream(uint32_t dwModes) = 0;
};
IFX_FileAccess* FX_CreateDefaultFileAccess(const CFX_WideStringC& wsPath);
#endif  // PDF_ENABLE_XFA

class IFX_MemoryStream : public IFX_SeekableStream {
 public:
  virtual bool IsConsecutive() const = 0;
  virtual void EstimateSize(size_t nInitSize, size_t nGrowSize) = 0;
  virtual uint8_t* GetBuffer() const = 0;
  virtual void AttachBuffer(uint8_t* pBuffer,
                            size_t nSize,
                            bool bTakeOver = false) = 0;
  virtual void DetachBuffer() = 0;
};

IFX_MemoryStream* FX_CreateMemoryStream(uint8_t* pBuffer,
                                        size_t nSize,
                                        bool bTakeOver = false);
IFX_MemoryStream* FX_CreateMemoryStream(bool bConsecutive = false);

class IFX_BufferRead : public IFX_ReadStream {
 public:
  // IFX_ReadStream:
  void Release() override = 0;
  bool IsEOF() override = 0;
  FX_FILESIZE GetPosition() override = 0;
  size_t ReadBlock(void* buffer, size_t size) override = 0;

  virtual bool ReadNextBlock(bool bRestart = false) = 0;
  virtual const uint8_t* GetBlockBuffer() = 0;
  virtual size_t GetBlockSize() = 0;
  virtual FX_FILESIZE GetBlockOffset() = 0;
};

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
class CFindFileData {
 public:
  virtual ~CFindFileData() {}
  HANDLE m_Handle;
  bool m_bEnd;
};

class CFindFileDataA : public CFindFileData {
 public:
  ~CFindFileDataA() override {}
  WIN32_FIND_DATAA m_FindData;
};

class CFindFileDataW : public CFindFileData {
 public:
  ~CFindFileDataW() override {}
  WIN32_FIND_DATAW m_FindData;
};
#endif

#endif  // CORE_FXCRT_FX_STREAM_H_
