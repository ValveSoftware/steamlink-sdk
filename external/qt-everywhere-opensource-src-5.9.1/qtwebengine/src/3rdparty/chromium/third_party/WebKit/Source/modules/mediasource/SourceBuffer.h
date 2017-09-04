/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef SourceBuffer_h
#define SourceBuffer_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/mediasource/TrackDefaultList.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebSourceBufferClient.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class AudioTrackList;
class DOMArrayBuffer;
class DOMArrayBufferView;
class ExceptionState;
class GenericEventQueue;
class MediaSource;
class TimeRanges;
class VideoTrackList;
class WebSourceBuffer;

class SourceBuffer final : public EventTargetWithInlineData,
                           public ActiveScriptWrappable,
                           public ActiveDOMObject,
                           public WebSourceBufferClient {
  USING_GARBAGE_COLLECTED_MIXIN(SourceBuffer);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(SourceBuffer, dispose);

 public:
  static SourceBuffer* create(std::unique_ptr<WebSourceBuffer>,
                              MediaSource*,
                              GenericEventQueue*);
  static const AtomicString& segmentsKeyword();
  static const AtomicString& sequenceKeyword();

  ~SourceBuffer() override;

  // SourceBuffer.idl methods
  const AtomicString& mode() const { return m_mode; }
  void setMode(const AtomicString&, ExceptionState&);
  bool updating() const { return m_updating; }
  TimeRanges* buffered(ExceptionState&) const;
  double timestampOffset() const;
  void setTimestampOffset(double, ExceptionState&);
  void appendBuffer(DOMArrayBuffer* data, ExceptionState&);
  void appendBuffer(DOMArrayBufferView* data, ExceptionState&);
  void abort(ExceptionState&);
  void remove(double start, double end, ExceptionState&);
  double appendWindowStart() const;
  void setAppendWindowStart(double, ExceptionState&);
  double appendWindowEnd() const;
  void setAppendWindowEnd(double, ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(updatestart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(update);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(updateend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
  TrackDefaultList* trackDefaults() const { return m_trackDefaults.get(); }
  void setTrackDefaults(TrackDefaultList*, ExceptionState&);

  AudioTrackList& audioTracks();
  VideoTrackList& videoTracks();

  void removedFromMediaSource();
  double highestPresentationTimestamp();

  // ScriptWrappable
  bool hasPendingActivity() const final;

  // ActiveDOMObject
  void suspend() override;
  void resume() override;
  void contextDestroyed() override;

  // EventTarget interface
  ExecutionContext* getExecutionContext() const override;
  const AtomicString& interfaceName() const override;

  // WebSourceBufferClient interface
  bool initializationSegmentReceived(const WebVector<MediaTrackInfo>&) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  enum AppendError { NoDecodeError, DecodeError };

  SourceBuffer(std::unique_ptr<WebSourceBuffer>,
               MediaSource*,
               GenericEventQueue*);
  void dispose();

  bool isRemoved() const;
  void scheduleEvent(const AtomicString& eventName);

  bool prepareAppend(size_t newDataSize, ExceptionState&);
  bool evictCodedFrames(size_t newDataSize);
  void appendBufferInternal(const unsigned char*, unsigned, ExceptionState&);
  void appendBufferAsyncPart();
  void appendError(AppendError);

  void removeAsyncPart();

  void cancelRemove();
  void abortIfUpdating();

  void removeMediaTracks();

  const TrackDefault* getTrackDefault(
      const AtomicString& trackType,
      const AtomicString& byteStreamTrackID) const;
  AtomicString defaultTrackLabel(const AtomicString& trackType,
                                 const AtomicString& byteStreamTrackID) const;
  AtomicString defaultTrackLanguage(
      const AtomicString& trackType,
      const AtomicString& byteStreamTrackID) const;

  std::unique_ptr<WebSourceBuffer> m_webSourceBuffer;
  Member<MediaSource> m_source;
  Member<TrackDefaultList> m_trackDefaults;
  Member<GenericEventQueue> m_asyncEventQueue;

  AtomicString m_mode;
  bool m_updating;
  double m_timestampOffset;
  Member<AudioTrackList> m_audioTracks;
  Member<VideoTrackList> m_videoTracks;
  double m_appendWindowStart;
  double m_appendWindowEnd;
  bool m_firstInitializationSegmentReceived;

  Vector<unsigned char> m_pendingAppendData;
  size_t m_pendingAppendDataOffset;
  Member<AsyncMethodRunner<SourceBuffer>> m_appendBufferAsyncPartRunner;

  double m_pendingRemoveStart;
  double m_pendingRemoveEnd;
  Member<AsyncMethodRunner<SourceBuffer>> m_removeAsyncPartRunner;
};

}  // namespace blink

#endif  // SourceBuffer_h
