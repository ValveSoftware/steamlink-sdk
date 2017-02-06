// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_LOOP_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_LOOP_H_

#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"

// MediaCodecLoop is based on Android's MediaCodec API.
// The MediaCodec API is required to play encrypted (as in EME) content on
// Android. It is also a way to employ hardware-accelerated decoding.
// One MediaCodecLoop instance owns a single MediaCodec(Bridge) instance, and
// drives it to perform decoding in conjunction with a MediaCodecLoop::Client.
// The Client provides the input data and consumes the output data.  A typical
// example is AndroidVideoDecodeAccelerator.

// Implementation notes.
//
// The MediaCodec works by exchanging buffers between the client and the codec
// itself.  On the input side, an "empty" buffer has to be dequeued from the
// codec, filled with data, and queued back.  On the output side, a "full"
// buffer with data should be dequeued, the data used, and the buffer returned
// back to the codec.
//
// Neither input nor output dequeue operations are guaranteed to succeed: the
// codec might not have available input buffers yet, or not every encoded buffer
// has arrived to complete an output frame. In such case the client should try
// to dequeue a buffer again at a later time.
//
// There is also a special situation related to an encrypted stream, where the
// enqueuing of a filled input buffer might fail due to lack of the relevant key
// in the CDM module.  While MediaCodecLoop does not handle the CDM directly,
// it does understand the codec state.
//
// Much of that logic is common to all users of MediaCodec.  The MediaCodecLoop
// provides the main driver loop to talk to MediaCodec.  Via the Client
// interface, MediaCodecLoop can request input data, send output buffers, etc.
// MediaCodecLoop does not construct a MediaCodec, but instead takes ownership
// of one that it provided during construction.
//
// Although one can specify a delay in the MediaCodec's dequeue operations,
// this implementation uses polling.  We can't tell in advance if a call into
// MediaCodec will block indefinitely, and we don't want to block the thread
// waiting for it.
//
// This implementation detects that the MediaCodec is idle (no input or output
// buffer processing) and after being idle for a predefined time the timer
// stops.  The client is responsible for signalling us to wake up via a call
// to DoPendingWork.  It is okay for the client to call this even when the timer
// is already running.
//
// The current implementation is single threaded. Every method is supposed to
// run on the same thread.
//
// State diagram.
//
//  -[Any state]-
//     |
// (MediaCodec error)
//     |
//   [Error]
//
//  [Ready]
//     |
// (EOS enqueued)
//     |
//  [Draining]
//     |
// (EOS dequeued on output)
//     |
//   [Drained]
//
//      [Ready]
//         |
// (MEDIA_CODEC_NO_KEY)
//         |
//   [WaitingForKey]
//         |
//    (OnKeyAdded)
//         |
//      [Ready]
//
//     -[Any state]-
//    |             |
// (Flush ok) (Flush fails)
//    |             |
// [Ready]       [Error]

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MEDIA_EXPORT MediaCodecLoop {
 public:
  // TODO(liberato): this exists in video_decoder.h and audio_decoder.h too.
  using InitCB = base::Callback<void(bool success)>;
  using DecodeCB = base::Callback<void(DecodeStatus status)>;

  // Data that the client wants to put into an input buffer.
  struct InputData {
    InputData();
    InputData(const InputData&);
    ~InputData();

    const uint8_t* memory = nullptr;
    size_t length = 0;

    std::string key_id;
    std::string iv;
    std::vector<SubsampleEntry> subsamples;

    base::TimeDelta presentation_time;

    bool is_eos = false;
    bool is_encrypted = false;
  };

  // Handy enum for "no buffer".
  enum { kInvalidBufferIndex = -1 };

  // Information about a MediaCodec output buffer.
  struct OutputBuffer {
    // The codec output buffers are referred to by this index.
    int index = kInvalidBufferIndex;

    // Position in the buffer where data starts.
    size_t offset = 0;

    // The size of the buffer (includes offset).
    size_t size = 0;

    // Presentation timestamp.
    base::TimeDelta pts;

    // True if this buffer is the end of stream.
    bool is_eos = false;

    // True if this buffer is a key frame.
    bool is_key_frame = false;
  };

  class Client {
   public:
    // Return true if and only if there is input that is pending to be
    // queued with MediaCodec.  ProvideInputData() will not be called more than
    // once in response to this returning true once.  It is not guaranteed that
    // ProvideInputData will be called at all.  If ProvideInputData is called,
    // then OnInputDataQueued will also be called before calling again.
    virtual bool IsAnyInputPending() const = 0;

    // Fills and returns an input buffer for MediaCodecLoop to queue.  It is
    // an error for MediaCodecLoop to call this while !IsAnyInputPending().
    virtual InputData ProvideInputData() = 0;

    // Called to notify the client that the previous data (or eos) provided by
    // ProvideInputData has been queued with the codec.  IsAnyInputPending and
    // ProvideInputData will not be called again until this is called.
    // Note that if the codec is flushed while a call back is pending, then that
    // call back won't happen.
    virtual void OnInputDataQueued(bool success) = 0;

    // Called when an EOS buffer is dequeued from the output.
    virtual void OnDecodedEos(const OutputBuffer& out) = 0;

    // Processes the output buffer after it comes from MediaCodec.  The client
    // has the responsibility to release the codec buffer, though it doesn't
    // need to do so before this call returns.  If it does not do so before
    // returning, then the client must call DoPendingWork when it releases it.
    // If this returns false, then we transition to STATE_ERROR.
    virtual bool OnDecodedFrame(const OutputBuffer& out) = 0;

    // Processes the output format change on |media_codec|.  Returns true on
    // success, or false to transition to the error state.
    virtual bool OnOutputFormatChanged() = 0;

    // Notify the client when our state transitions to STATE_ERROR.
    virtual void OnCodecLoopError() = 0;

   protected:
    virtual ~Client() {}
  };

  // We will take ownership of |media_codec|.  We will not destroy it until
  // we are destructed.  |media_codec| may not be null.
  MediaCodecLoop(Client* client, std::unique_ptr<MediaCodecBridge> media_codec);
  ~MediaCodecLoop();

  // Does the MediaCodec processing cycle: enqueues an input buffer, then
  // dequeues output buffers.  This should be called by the client when more
  // work becomes available, such as when new input data arrives.  If codec
  // output buffers are freed after OnDecodedFrame returns, then this should
  // also be called.
  void DoPendingWork();

  // Try to flush this media codec.  Returns true on success, false on failure.
  // Failures can result in a state change to the Error state.  If this returns
  // false but the state is still READY, then the codec may continue to be used.
  // In that case, we may still call back into the client for decoding later.
  // The client must handle this case if it really does want to switch codecs.
  // If it immediately destroys us, then that's fine.
  bool TryFlush();

  // This should be called when a new key is added.  Decoding will resume if it
  // was stopped in the WAITING_FOR_KEY state.
  void OnKeyAdded();

  // Return our codec, if we have one.
  MediaCodecBridge* GetCodec() const;

 protected:
  enum State {
    STATE_READY,
    STATE_WAITING_FOR_KEY,
    STATE_DRAINING,
    STATE_DRAINED,
    STATE_ERROR,
  };

  // Information about dequeued input buffer.
  struct InputBuffer {
    InputBuffer() {}
    InputBuffer(int i, bool p) : index(i), is_pending(p) {}

    // The codec input buffers are referred to by this index.
    int index = kInvalidBufferIndex;

    // True if we tried to enqueue this buffer before.
    bool is_pending = false;
  };

  // Enqueues one pending input buffer into MediaCodec if MediaCodec has room,
  // and if the client has any input to give us.
  // Returns true if any input was processed.
  bool ProcessOneInputBuffer();

  // Get data for |input_buffer| from the client and queue it.
  void EnqueueInputBuffer(const InputBuffer& input_buffer);

  // Dequeues an empty input buffer from the codec and returns the information
  // about it. InputBuffer.index is the index of the dequeued buffer or -1 if
  // the codec is busy or an error occured.  InputBuffer.is_pending is set to
  // true if we tried to enqueue this buffer before. In this case the buffer is
  // already filled with data.
  // In the case of an error sets STATE_ERROR.
  InputBuffer DequeueInputBuffer();

  // Dequeues one output buffer from MediaCodec if it is immediately available,
  // and sends it to the client.
  // Returns true if an output buffer was received from MediaCodec.
  bool ProcessOneOutputBuffer();

  // Start the timer immediately if |start| is true or stop it based on elapsed
  // idle time if |start| is false.
  void ManageTimer(bool start);

  // Helper method to change the state.
  void SetState(State new_state);

  // A helper function for logging.
  static const char* AsString(State state);

  // Used to post tasks. This class is single threaded and every method should
  // run on this task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  State state_;

  // The client that we notify about MediaCodec events.
  Client* client_;

  // The MediaCodec instance that we're using.
  std::unique_ptr<MediaCodecBridge> media_codec_;

  // Repeating timer that kicks MediaCodec operation.
  base::RepeatingTimer io_timer_;

  // Time at which we last did useful work on |io_timer_|.
  base::TimeTicks idle_time_begin_;

  // Index of the dequeued and filled buffer that we keep trying to enqueue.
  // Such buffer appears in MEDIA_CODEC_NO_KEY processing. The -1 value means
  // there is no such buffer.
  int pending_input_buf_index_;

  // When processing a pending input buffer, this is the data that was returned
  // to us by the client.  |memory| has been cleared, since the codec has it.
  InputData pending_input_buf_data_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaCodecLoop> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecLoop);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_LOOP_H_
