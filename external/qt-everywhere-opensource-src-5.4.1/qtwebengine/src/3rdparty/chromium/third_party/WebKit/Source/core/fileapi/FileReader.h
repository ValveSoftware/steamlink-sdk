/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef FileReader_h
#define FileReader_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/fileapi/FileError.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Blob;
class ExceptionState;
class ExecutionContext;

class FileReader FINAL : public RefCountedWillBeRefCountedGarbageCollected<FileReader>, public ScriptWrappable, public ActiveDOMObject, public FileReaderLoaderClient, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(FileReader);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(FileReader);
public:
    static PassRefPtrWillBeRawPtr<FileReader> create(ExecutionContext*);

    virtual ~FileReader();

    enum ReadyState {
        EMPTY = 0,
        LOADING = 1,
        DONE = 2
    };

    void readAsArrayBuffer(Blob*, ExceptionState&);
    void readAsBinaryString(Blob*, ExceptionState&);
    void readAsText(Blob*, const String& encoding, ExceptionState&);
    void readAsText(Blob*, ExceptionState&);
    void readAsDataURL(Blob*, ExceptionState&);
    void abort();

    void doAbort();

    ReadyState readyState() const { return m_state; }
    PassRefPtrWillBeRawPtr<FileError> error() { return m_error; }
    FileReaderLoader::ReadType readType() const { return m_readType; }
    PassRefPtr<ArrayBuffer> arrayBufferResult() const;
    String stringResult();

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }

    // FileReaderLoaderClient
    virtual void didStartLoading() OVERRIDE;
    virtual void didReceiveData() OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFail(FileError::ErrorCode) OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(load);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadend);

    virtual void trace(Visitor*) OVERRIDE;

private:
    class ThrottlingController;

    FileReader(ExecutionContext*);

    void terminate();
    void readInternal(Blob*, FileReaderLoader::ReadType, ExceptionState&);
    void fireErrorEvent(int httpStatusCode);
    void fireEvent(const AtomicString& type);

    static ThreadSpecific<ThrottlingController>& throttlingController();
    void executePendingRead();

    ReadyState m_state;

    // Internal loading state, which could differ from ReadyState as it's
    // for script-visible state while this one's for internal state.
    enum LoadingState {
        LoadingStateNone,
        LoadingStatePending,
        LoadingStateLoading,
        LoadingStateAborted
    };
    LoadingState m_loadingState;

    String m_blobType;
    RefPtr<BlobDataHandle> m_blobDataHandle;
    FileReaderLoader::ReadType m_readType;
    String m_encoding;

    OwnPtr<FileReaderLoader> m_loader;
    RefPtrWillBeMember<FileError> m_error;
    double m_lastProgressNotificationTimeMS;
};

} // namespace WebCore

#endif // FileReader_h
