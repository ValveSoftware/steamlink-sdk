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

#include "modules/filesystem/FileWriter.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/events/ProgressEvent.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "public/platform/WebFileWriter.h"
#include "public/platform/WebURL.h"
#include "wtf/CurrentTime.h"

namespace blink {

static const int kMaxRecursionDepth = 3;
static const double progressNotificationIntervalMS = 50;

FileWriter* FileWriter::create(ExecutionContext* context) {
  FileWriter* fileWriter = new FileWriter(context);
  fileWriter->suspendIfNeeded();
  return fileWriter;
}

FileWriter::FileWriter(ExecutionContext* context)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(context),
      m_readyState(kInit),
      m_operationInProgress(OperationNone),
      m_queuedOperation(OperationNone),
      m_bytesWritten(0),
      m_bytesToWrite(0),
      m_truncateLength(-1),
      m_numAborts(0),
      m_recursionDepth(0),
      m_lastProgressNotificationTimeMS(0) {}

FileWriter::~FileWriter() {
  ASSERT(!m_recursionDepth);
  DCHECK(!writer());
}

const AtomicString& FileWriter::interfaceName() const {
  return EventTargetNames::FileWriter;
}

void FileWriter::contextDestroyed() {
  // Make sure we've actually got something to stop, and haven't already called
  // abort().
  if (writer() && m_readyState == kWriting) {
    doOperation(OperationAbort);
    m_readyState = kDone;
  }
  resetWriter();
}

bool FileWriter::hasPendingActivity() const {
  return m_operationInProgress != OperationNone ||
         m_queuedOperation != OperationNone || m_readyState == kWriting;
}

void FileWriter::write(Blob* data, ExceptionState& exceptionState) {
  if (!getExecutionContext())
    return;
  ASSERT(data);
  ASSERT(writer());
  ASSERT(m_truncateLength == -1);
  if (m_readyState == kWriting) {
    setError(FileError::kInvalidStateErr, exceptionState);
    return;
  }
  if (m_recursionDepth > kMaxRecursionDepth) {
    setError(FileError::kSecurityErr, exceptionState);
    return;
  }

  m_blobBeingWritten = data;
  m_readyState = kWriting;
  m_bytesWritten = 0;
  m_bytesToWrite = data->size();
  ASSERT(m_queuedOperation == OperationNone);
  if (m_operationInProgress != OperationNone) {
    // We must be waiting for an abort to complete, since m_readyState wasn't
    // kWriting.
    ASSERT(m_operationInProgress == OperationAbort);
    m_queuedOperation = OperationWrite;
  } else
    doOperation(OperationWrite);

  fireEvent(EventTypeNames::writestart);
}

void FileWriter::seek(long long position, ExceptionState& exceptionState) {
  if (!getExecutionContext())
    return;
  ASSERT(writer());
  if (m_readyState == kWriting) {
    setError(FileError::kInvalidStateErr, exceptionState);
    return;
  }

  ASSERT(m_truncateLength == -1);
  ASSERT(m_queuedOperation == OperationNone);
  seekInternal(position);
}

void FileWriter::truncate(long long position, ExceptionState& exceptionState) {
  if (!getExecutionContext())
    return;
  ASSERT(writer());
  ASSERT(m_truncateLength == -1);
  if (m_readyState == kWriting || position < 0) {
    setError(FileError::kInvalidStateErr, exceptionState);
    return;
  }
  if (m_recursionDepth > kMaxRecursionDepth) {
    setError(FileError::kSecurityErr, exceptionState);
    return;
  }

  m_readyState = kWriting;
  m_bytesWritten = 0;
  m_bytesToWrite = 0;
  m_truncateLength = position;
  ASSERT(m_queuedOperation == OperationNone);
  if (m_operationInProgress != OperationNone) {
    // We must be waiting for an abort to complete, since m_readyState wasn't
    // kWriting.
    ASSERT(m_operationInProgress == OperationAbort);
    m_queuedOperation = OperationTruncate;
  } else
    doOperation(OperationTruncate);
  fireEvent(EventTypeNames::writestart);
}

void FileWriter::abort(ExceptionState& exceptionState) {
  if (!getExecutionContext())
    return;
  ASSERT(writer());
  if (m_readyState != kWriting)
    return;
  ++m_numAborts;

  doOperation(OperationAbort);
  signalCompletion(FileError::kAbortErr);
}

void FileWriter::didWrite(long long bytes, bool complete) {
  if (m_operationInProgress == OperationAbort) {
    completeAbort();
    return;
  }
  DCHECK_EQ(kWriting, m_readyState);
  DCHECK_EQ(-1, m_truncateLength);
  DCHECK_EQ(OperationWrite, m_operationInProgress);
  DCHECK(!m_bytesToWrite || bytes + m_bytesWritten > 0);
  DCHECK(bytes + m_bytesWritten <= m_bytesToWrite);
  m_bytesWritten += bytes;
  DCHECK((m_bytesWritten == m_bytesToWrite) || !complete);
  setPosition(position() + bytes);
  if (position() > length())
    setLength(position());
  if (complete) {
    m_blobBeingWritten.clear();
    m_operationInProgress = OperationNone;
  }

  int numAborts = m_numAborts;
  // We could get an abort in the handler for this event. If we do, it's
  // already handled the cleanup and signalCompletion call.
  double now = currentTimeMS();
  if (complete || !m_lastProgressNotificationTimeMS ||
      (now - m_lastProgressNotificationTimeMS >
       progressNotificationIntervalMS)) {
    m_lastProgressNotificationTimeMS = now;
    fireEvent(EventTypeNames::progress);
  }

  if (complete) {
    if (numAborts == m_numAborts)
      signalCompletion(FileError::kOK);
  }
}

void FileWriter::didTruncate() {
  if (m_operationInProgress == OperationAbort) {
    completeAbort();
    return;
  }
  ASSERT(m_operationInProgress == OperationTruncate);
  ASSERT(m_truncateLength >= 0);
  setLength(m_truncateLength);
  if (position() > length())
    setPosition(length());
  m_operationInProgress = OperationNone;
  signalCompletion(FileError::kOK);
}

void FileWriter::didFail(WebFileError code) {
  DCHECK_NE(OperationNone, m_operationInProgress);
  DCHECK_NE(FileError::kOK, static_cast<FileError::ErrorCode>(code));
  if (m_operationInProgress == OperationAbort) {
    completeAbort();
    return;
  }
  DCHECK_EQ(OperationNone, m_queuedOperation);
  DCHECK_EQ(kWriting, m_readyState);
  m_blobBeingWritten.clear();
  m_operationInProgress = OperationNone;
  signalCompletion(static_cast<FileError::ErrorCode>(code));
}

void FileWriter::completeAbort() {
  ASSERT(m_operationInProgress == OperationAbort);
  m_operationInProgress = OperationNone;
  Operation operation = m_queuedOperation;
  m_queuedOperation = OperationNone;
  doOperation(operation);
}

void FileWriter::doOperation(Operation operation) {
  InspectorInstrumentation::asyncTaskScheduled(getExecutionContext(),
                                               "FileWriter", this);
  switch (operation) {
    case OperationWrite:
      DCHECK_EQ(OperationNone, m_operationInProgress);
      DCHECK_EQ(-1, m_truncateLength);
      DCHECK(m_blobBeingWritten.get());
      DCHECK_EQ(kWriting, m_readyState);
      writer()->write(position(), m_blobBeingWritten->uuid());
      break;
    case OperationTruncate:
      DCHECK_EQ(OperationNone, m_operationInProgress);
      DCHECK_GE(m_truncateLength, 0);
      DCHECK_EQ(kWriting, m_readyState);
      writer()->truncate(m_truncateLength);
      break;
    case OperationNone:
      DCHECK_EQ(OperationNone, m_operationInProgress);
      DCHECK_EQ(-1, m_truncateLength);
      DCHECK(!m_blobBeingWritten.get());
      DCHECK_EQ(kDone, m_readyState);
      break;
    case OperationAbort:
      if (m_operationInProgress == OperationWrite ||
          m_operationInProgress == OperationTruncate)
        writer()->cancel();
      else if (m_operationInProgress != OperationAbort)
        operation = OperationNone;
      m_queuedOperation = OperationNone;
      m_blobBeingWritten.clear();
      m_truncateLength = -1;
      break;
  }
  ASSERT(m_queuedOperation == OperationNone);
  m_operationInProgress = operation;
}

void FileWriter::signalCompletion(FileError::ErrorCode code) {
  m_readyState = kDone;
  m_truncateLength = -1;
  if (FileError::kOK != code) {
    m_error = FileError::createDOMException(code);
    if (FileError::kAbortErr == code)
      fireEvent(EventTypeNames::abort);
    else
      fireEvent(EventTypeNames::error);
  } else
    fireEvent(EventTypeNames::write);
  fireEvent(EventTypeNames::writeend);

  InspectorInstrumentation::asyncTaskCanceled(getExecutionContext(), this);
}

void FileWriter::fireEvent(const AtomicString& type) {
  InspectorInstrumentation::AsyncTask asyncTask(getExecutionContext(), this);
  ++m_recursionDepth;
  dispatchEvent(
      ProgressEvent::create(type, true, m_bytesWritten, m_bytesToWrite));
  --m_recursionDepth;
  ASSERT(m_recursionDepth >= 0);
}

void FileWriter::setError(FileError::ErrorCode errorCode,
                          ExceptionState& exceptionState) {
  ASSERT(errorCode);
  FileError::throwDOMException(exceptionState, errorCode);
  m_error = FileError::createDOMException(errorCode);
}

void FileWriter::dispose() {
  contextDestroyed();
}

DEFINE_TRACE(FileWriter) {
  visitor->trace(m_error);
  visitor->trace(m_blobBeingWritten);
  EventTargetWithInlineData::trace(visitor);
  FileWriterBase::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

}  // namespace blink
