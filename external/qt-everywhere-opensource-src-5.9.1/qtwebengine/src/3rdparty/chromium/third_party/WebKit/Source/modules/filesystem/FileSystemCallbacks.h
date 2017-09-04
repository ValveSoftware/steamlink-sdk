/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FileSystemCallbacks_h
#define FileSystemCallbacks_h

#include "core/fileapi/FileError.h"
#include "modules/filesystem/EntriesCallback.h"
#include "platform/AsyncFileSystemCallbacks.h"
#include "platform/FileSystemType.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class DOMFileSystemBase;
class DirectoryReaderBase;
class BlobCallback;
class EntriesCallback;
class EntryCallback;
class ErrorCallback;
class FileMetadata;
class FileSystemCallback;
class FileWriterBase;
class FileWriterBaseCallback;
class MetadataCallback;
class ExecutionContext;
class VoidCallback;

// Passed to DOMFileSystem implementations that may report errors. Subclasses
// may capture the error for throwing on return to script (for synchronous APIs)
// or call an actual script callback (for asynchronous APIs).
class ErrorCallbackBase : public GarbageCollectedFinalized<ErrorCallbackBase> {
 public:
  virtual ~ErrorCallbackBase() {}
  DEFINE_INLINE_VIRTUAL_TRACE() {}
  virtual void invoke(FileError::ErrorCode) = 0;
};

class FileSystemCallbacksBase : public AsyncFileSystemCallbacks {
 public:
  ~FileSystemCallbacksBase() override;

  // For ErrorCallback.
  void didFail(int code) final;

  // Other callback methods are implemented by each subclass.

 protected:
  FileSystemCallbacksBase(ErrorCallbackBase*,
                          DOMFileSystemBase*,
                          ExecutionContext*);

  bool shouldScheduleCallback() const;

  template <typename CB, typename CBArg>
  void invokeOrScheduleCallback(CB*, CBArg);

  template <typename CB, typename CBArg>
  void handleEventOrScheduleCallback(CB*, CBArg*);

  template <typename CB>
  void handleEventOrScheduleCallback(CB*);

  Persistent<ErrorCallbackBase> m_errorCallback;
  Persistent<DOMFileSystemBase> m_fileSystem;
  Persistent<ExecutionContext> m_executionContext;
  int m_asyncOperationId;
};

// Subclasses ----------------------------------------------------------------

// Wraps a script-provided callback for use in DOMFileSystem operations.
class ScriptErrorCallback final : public ErrorCallbackBase {
 public:
  static ScriptErrorCallback* wrap(ErrorCallback*);
  virtual ~ScriptErrorCallback() {}
  DECLARE_VIRTUAL_TRACE();

  void invoke(FileError::ErrorCode) override;

 private:
  explicit ScriptErrorCallback(ErrorCallback*);
  Member<ErrorCallback> m_callback;
};

class EntryCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(
      EntryCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DOMFileSystemBase*,
      const String& expectedPath,
      bool isDirectory);
  void didSucceed() override;

 private:
  EntryCallbacks(EntryCallback*,
                 ErrorCallbackBase*,
                 ExecutionContext*,
                 DOMFileSystemBase*,
                 const String& expectedPath,
                 bool isDirectory);
  Persistent<EntryCallback> m_successCallback;
  String m_expectedPath;
  bool m_isDirectory;
};

class EntriesCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(
      EntriesCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DirectoryReaderBase*,
      const String& basePath);
  void didReadDirectoryEntry(const String& name, bool isDirectory) override;
  void didReadDirectoryEntries(bool hasMore) override;

 private:
  EntriesCallbacks(EntriesCallback*,
                   ErrorCallbackBase*,
                   ExecutionContext*,
                   DirectoryReaderBase*,
                   const String& basePath);
  Persistent<EntriesCallback> m_successCallback;
  Persistent<DirectoryReaderBase> m_directoryReader;
  String m_basePath;
  PersistentHeapVector<Member<Entry>> m_entries;
};

class FileSystemCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(FileSystemCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          FileSystemType);
  void didOpenFileSystem(const String& name, const KURL& rootURL) override;

 private:
  FileSystemCallbacks(FileSystemCallback*,
                      ErrorCallbackBase*,
                      ExecutionContext*,
                      FileSystemType);
  Persistent<FileSystemCallback> m_successCallback;
  FileSystemType m_type;
};

class ResolveURICallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(EntryCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*);
  void didResolveURL(const String& name,
                     const KURL& rootURL,
                     FileSystemType,
                     const String& filePath,
                     bool isDirectry) override;

 private:
  ResolveURICallbacks(EntryCallback*, ErrorCallbackBase*, ExecutionContext*);
  Persistent<EntryCallback> m_successCallback;
};

class MetadataCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(MetadataCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          DOMFileSystemBase*);
  void didReadMetadata(const FileMetadata&) override;

 private:
  MetadataCallbacks(MetadataCallback*,
                    ErrorCallbackBase*,
                    ExecutionContext*,
                    DOMFileSystemBase*);
  Persistent<MetadataCallback> m_successCallback;
};

class FileWriterBaseCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(
      FileWriterBase*,
      FileWriterBaseCallback*,
      ErrorCallbackBase*,
      ExecutionContext*);
  void didCreateFileWriter(std::unique_ptr<WebFileWriter>,
                           long long length) override;

 private:
  FileWriterBaseCallbacks(FileWriterBase*,
                          FileWriterBaseCallback*,
                          ErrorCallbackBase*,
                          ExecutionContext*);
  Persistent<FileWriterBase> m_fileWriter;
  Persistent<FileWriterBaseCallback> m_successCallback;
};

class SnapshotFileCallback final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(DOMFileSystemBase*,
                                                          const String& name,
                                                          const KURL&,
                                                          BlobCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*);
  virtual void didCreateSnapshotFile(const FileMetadata&,
                                     PassRefPtr<BlobDataHandle> snapshot);

 private:
  SnapshotFileCallback(DOMFileSystemBase*,
                       const String& name,
                       const KURL&,
                       BlobCallback*,
                       ErrorCallbackBase*,
                       ExecutionContext*);
  String m_name;
  KURL m_url;
  Persistent<BlobCallback> m_successCallback;
};

class VoidCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> create(VoidCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          DOMFileSystemBase*);
  void didSucceed() override;

 private:
  VoidCallbacks(VoidCallback*,
                ErrorCallbackBase*,
                ExecutionContext*,
                DOMFileSystemBase*);
  Persistent<VoidCallback> m_successCallback;
};

}  // namespace blink

#endif  // FileSystemCallbacks_h
