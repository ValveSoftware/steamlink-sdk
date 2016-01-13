// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_audio_input_stream.h"

#include <list>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/capture/web_contents_tracker.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::WithArgs;

using media::AudioInputStream;
using media::AudioOutputStream;
using media::AudioParameters;
using media::SineWaveAudioSource;
using media::VirtualAudioInputStream;
using media::VirtualAudioOutputStream;

namespace content {

namespace {

const int kRenderProcessId = 123;
const int kRenderViewId = 456;
const int kAnotherRenderProcessId = 789;
const int kAnotherRenderViewId = 1;

const AudioParameters& TestAudioParameters() {
  static const AudioParameters params(
      AudioParameters::AUDIO_FAKE,
      media::CHANNEL_LAYOUT_STEREO,
      AudioParameters::kAudioCDSampleRate, 16,
      AudioParameters::kAudioCDSampleRate / 100);
  return params;
}

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MockAudioMirroringManager() : AudioMirroringManager() {}
  virtual ~MockAudioMirroringManager() {}

  MOCK_METHOD3(StartMirroring,
               void(int render_process_id, int render_view_id,
                    MirroringDestination* destination));
  MOCK_METHOD3(StopMirroring,
               void(int render_process_id, int render_view_id,
                    MirroringDestination* destination));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioMirroringManager);
};

class MockWebContentsTracker : public WebContentsTracker {
 public:
  MockWebContentsTracker() : WebContentsTracker() {}

  MOCK_METHOD3(Start,
               void(int render_process_id, int render_view_id,
                    const ChangeCallback& callback));
  MOCK_METHOD0(Stop, void());

 private:
  virtual ~MockWebContentsTracker() {}

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsTracker);
};

// A fully-functional VirtualAudioInputStream, but methods are mocked to allow
// tests to check how/when they are invoked.
class MockVirtualAudioInputStream : public VirtualAudioInputStream {
 public:
  explicit MockVirtualAudioInputStream(
      const scoped_refptr<base::MessageLoopProxy>& worker_loop)
      : VirtualAudioInputStream(TestAudioParameters(), worker_loop,
                                VirtualAudioInputStream::AfterCloseCallback()),
        real_(TestAudioParameters(), worker_loop,
              base::Bind(&MockVirtualAudioInputStream::OnRealStreamHasClosed,
                         base::Unretained(this))),
        real_stream_is_closed_(false) {
    // Set default actions of mocked methods to delegate to the concrete
    // implementation.
    ON_CALL(*this, Open())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Open));
    ON_CALL(*this, Start(_))
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Start));
    ON_CALL(*this, Stop())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Stop));
    ON_CALL(*this, Close())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::Close));
    ON_CALL(*this, GetMaxVolume())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::GetMaxVolume));
    ON_CALL(*this, SetVolume(_))
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::SetVolume));
    ON_CALL(*this, GetVolume())
        .WillByDefault(Invoke(&real_, &VirtualAudioInputStream::GetVolume));
    ON_CALL(*this, SetAutomaticGainControl(_))
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::SetAutomaticGainControl));
    ON_CALL(*this, GetAutomaticGainControl())
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::GetAutomaticGainControl));
    ON_CALL(*this, AddOutputStream(NotNull(), _))
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::AddOutputStream));
    ON_CALL(*this, RemoveOutputStream(NotNull(), _))
        .WillByDefault(
            Invoke(&real_, &VirtualAudioInputStream::RemoveOutputStream));
  }

  ~MockVirtualAudioInputStream() {
    DCHECK(real_stream_is_closed_);
  }

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioInputStream::AudioInputCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(GetMaxVolume, double());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, void(bool));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD2(AddOutputStream, void(VirtualAudioOutputStream*,
                                     const AudioParameters&));
  MOCK_METHOD2(RemoveOutputStream, void(VirtualAudioOutputStream*,
                                        const AudioParameters&));

 private:
  void OnRealStreamHasClosed(VirtualAudioInputStream* stream) {
    DCHECK_EQ(&real_, stream);
    DCHECK(!real_stream_is_closed_);
    real_stream_is_closed_ = true;
  }

  VirtualAudioInputStream real_;
  bool real_stream_is_closed_;

  DISALLOW_COPY_AND_ASSIGN(MockVirtualAudioInputStream);
};

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockAudioInputCallback() {}

  MOCK_METHOD4(OnData,
               void(AudioInputStream* stream,
                    const media::AudioBus* src,
                    uint32 hardware_delay_bytes,
                    double volume));
  MOCK_METHOD1(OnError, void(AudioInputStream* stream));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputCallback);
};

}  // namespace

class WebContentsAudioInputStreamTest : public testing::Test {
 public:
  WebContentsAudioInputStreamTest()
      : audio_thread_("Audio thread"),
        io_thread_(BrowserThread::IO),
        mock_mirroring_manager_(new MockAudioMirroringManager()),
        mock_tracker_(new MockWebContentsTracker()),
        mock_vais_(NULL),
        wcais_(NULL),
        destination_(NULL),
        current_render_process_id_(kRenderProcessId),
        current_render_view_id_(kRenderViewId),
        on_data_event_(false, false) {
    audio_thread_.Start();
    io_thread_.Start();
  }

  virtual ~WebContentsAudioInputStreamTest() {
    audio_thread_.Stop();
    io_thread_.Stop();

    DCHECK(!mock_vais_);
    DCHECK(!wcais_);
    EXPECT_FALSE(destination_);
    DCHECK(streams_.empty());
    DCHECK(sources_.empty());
  }

  void Open() {
    mock_vais_ =
        new MockVirtualAudioInputStream(audio_thread_.message_loop_proxy());
    EXPECT_CALL(*mock_vais_, Open());
    EXPECT_CALL(*mock_vais_, Close());  // At Close() time.

    ASSERT_EQ(kRenderProcessId, current_render_process_id_);
    ASSERT_EQ(kRenderViewId, current_render_view_id_);
    EXPECT_CALL(*mock_tracker_.get(), Start(kRenderProcessId, kRenderViewId, _))
        .WillOnce(DoAll(
             SaveArg<2>(&change_callback_),
             WithArgs<0, 1>(Invoke(&change_callback_,
                                   &WebContentsTracker::ChangeCallback::Run))));
    EXPECT_CALL(*mock_tracker_.get(), Stop());  // At Close() time.

    wcais_ = new WebContentsAudioInputStream(
        current_render_process_id_, current_render_view_id_,
        mock_mirroring_manager_.get(),
        mock_tracker_, mock_vais_);
    wcais_->Open();
  }

  void Start() {
    EXPECT_CALL(*mock_vais_, Start(&mock_input_callback_));
    EXPECT_CALL(*mock_vais_, Stop());  // At Stop() time.

    EXPECT_CALL(*mock_mirroring_manager_,
                StartMirroring(kRenderProcessId, kRenderViewId, NotNull()))
        .WillOnce(SaveArg<2>(&destination_))
        .RetiresOnSaturation();
    // At Stop() time, or when the mirroring target changes:
    EXPECT_CALL(*mock_mirroring_manager_,
                StopMirroring(kRenderProcessId, kRenderViewId, NotNull()))
        .WillOnce(Assign(
            &destination_,
            static_cast<AudioMirroringManager::MirroringDestination*>(NULL)))
        .RetiresOnSaturation();

    EXPECT_CALL(mock_input_callback_, OnData(NotNull(), NotNull(), _, _))
        .WillRepeatedly(
            InvokeWithoutArgs(&on_data_event_, &base::WaitableEvent::Signal));

    wcais_->Start(&mock_input_callback_);

    // Test plumbing of volume controls and automatic gain controls.  Calls to
    // wcais_ methods should delegate directly to mock_vais_.
    EXPECT_CALL(*mock_vais_, GetVolume());
    double volume = wcais_->GetVolume();
    EXPECT_CALL(*mock_vais_, GetMaxVolume());
    const double max_volume = wcais_->GetMaxVolume();
    volume *= 2.0;
    if (volume < max_volume) {
      volume = max_volume;
    }
    EXPECT_CALL(*mock_vais_, SetVolume(volume));
    wcais_->SetVolume(volume);
    EXPECT_CALL(*mock_vais_, GetAutomaticGainControl());
    bool auto_gain = wcais_->GetAutomaticGainControl();
    auto_gain = !auto_gain;
    EXPECT_CALL(*mock_vais_, SetAutomaticGainControl(auto_gain));
    wcais_->SetAutomaticGainControl(auto_gain);
  }

  void AddAnotherInput() {
    // Note: WCAIS posts a task to invoke
    // MockAudioMirroringManager::StartMirroring() on the IO thread, which
    // causes our mock to set |destination_|.  Block until that has happened.
    base::WaitableEvent done(false, false);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(
            &base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
    ASSERT_TRUE(destination_);

    EXPECT_CALL(*mock_vais_, AddOutputStream(NotNull(), _))
        .RetiresOnSaturation();
    // Later, when stream is closed:
    EXPECT_CALL(*mock_vais_, RemoveOutputStream(NotNull(), _))
        .RetiresOnSaturation();

    const AudioParameters& params = TestAudioParameters();
    AudioOutputStream* const out = destination_->AddInput(params);
    ASSERT_TRUE(out);
    streams_.push_back(out);
    EXPECT_TRUE(out->Open());
    SineWaveAudioSource* const source = new SineWaveAudioSource(
        params.channel_layout(), 200.0, params.sample_rate());
    sources_.push_back(source);
    out->Start(source);
  }

  void RemoveOneInputInFIFOOrder() {
    ASSERT_FALSE(streams_.empty());
    AudioOutputStream* const out = streams_.front();
    streams_.pop_front();
    out->Stop();
    out->Close();  // Self-deletes.
    ASSERT_TRUE(!sources_.empty());
    delete sources_.front();
    sources_.pop_front();
  }

  void ChangeMirroringTarget() {
    const int next_render_process_id =
        current_render_process_id_ == kRenderProcessId ?
            kAnotherRenderProcessId : kRenderProcessId;
    const int next_render_view_id =
        current_render_view_id_ == kRenderViewId ?
            kAnotherRenderViewId : kRenderViewId;

    EXPECT_CALL(*mock_mirroring_manager_,
                StartMirroring(next_render_process_id, next_render_view_id,
                               NotNull()))
        .WillOnce(SaveArg<2>(&destination_))
        .RetiresOnSaturation();
    // At Stop() time, or when the mirroring target changes:
    EXPECT_CALL(*mock_mirroring_manager_,
                StopMirroring(next_render_process_id, next_render_view_id,
                              NotNull()))
        .WillOnce(Assign(
            &destination_,
            static_cast<AudioMirroringManager::MirroringDestination*>(NULL)))
        .RetiresOnSaturation();

    // Simulate OnTargetChange() callback from WebContentsTracker.
    EXPECT_FALSE(change_callback_.is_null());
    change_callback_.Run(next_render_process_id, next_render_view_id);

    current_render_process_id_ = next_render_process_id;
    current_render_view_id_ = next_render_view_id;
  }

  void LoseMirroringTarget() {
    EXPECT_CALL(mock_input_callback_, OnError(_));

    // Simulate OnTargetChange() callback from WebContentsTracker.
    EXPECT_FALSE(change_callback_.is_null());
    change_callback_.Run(-1, -1);
  }

  void Stop() {
    wcais_->Stop();
  }

  void Close() {
    // WebContentsAudioInputStream self-destructs on Close().  Its internal
    // objects hang around until they are no longer referred to (e.g., as tasks
    // on other threads shut things down).
    wcais_->Close();
    wcais_ = NULL;
    mock_vais_ = NULL;
  }

  void RunOnAudioThread(const base::Closure& closure) {
    audio_thread_.message_loop()->PostTask(FROM_HERE, closure);
  }

  // Block the calling thread until OnData() callbacks are being made.
  void WaitForData() {
    // Note: Arbitrarily chosen, but more iterations causes tests to take
    // significantly more time.
    static const int kNumIterations = 3;
    for (int i = 0; i < kNumIterations; ++i)
      on_data_event_.Wait();
  }

 private:
  base::Thread audio_thread_;
  BrowserThreadImpl io_thread_;

  scoped_ptr<MockAudioMirroringManager> mock_mirroring_manager_;
  scoped_refptr<MockWebContentsTracker> mock_tracker_;

  MockVirtualAudioInputStream* mock_vais_;  // Owned by wcais_.
  WebContentsAudioInputStream* wcais_;  // Self-destructs on Close().

  // Mock consumer of audio data.
  MockAudioInputCallback mock_input_callback_;

  // Provided by WebContentsAudioInputStream to the mock WebContentsTracker.
  // This callback is saved here, and test code will invoke it to simulate
  // target change events.
  WebContentsTracker::ChangeCallback change_callback_;

  // Provided by WebContentsAudioInputStream to the mock AudioMirroringManager.
  // A pointer to the implementation is saved here, and test code will invoke it
  // to simulate: 1) calls to AddInput(); and 2) diverting audio data.
  AudioMirroringManager::MirroringDestination* destination_;

  // Current target RenderView.  These get flipped in ChangedMirroringTarget().
  int current_render_process_id_;
  int current_render_view_id_;

  // Streams provided by calls to WebContentsAudioInputStream::AddInput().  Each
  // is started with a simulated source of audio data.
  std::list<AudioOutputStream*> streams_;
  std::list<SineWaveAudioSource*> sources_;  // 1:1 with elements in streams_.

  base::WaitableEvent on_data_event_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAudioInputStreamTest);
};

#define RUN_ON_AUDIO_THREAD(method) \
  RunOnAudioThread(base::Bind(&WebContentsAudioInputStreamTest::method,  \
                              base::Unretained(this)))

TEST_F(WebContentsAudioInputStreamTest, OpenedButNeverStarted) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringNothing) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringOutputOutlivesSession) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringOutputWithinSession) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringNothingWithTargetChange) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringOneStreamAfterTargetChange) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringOneStreamWithTargetChange) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringLostTarget) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(LoseMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

TEST_F(WebContentsAudioInputStreamTest, MirroringMultipleStreamsAndTargets) {
  RUN_ON_AUDIO_THREAD(Open);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  WaitForData();
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(AddAnotherInput);
  WaitForData();
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  WaitForData();
  RUN_ON_AUDIO_THREAD(ChangeMirroringTarget);
  RUN_ON_AUDIO_THREAD(RemoveOneInputInFIFOOrder);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
}

}  // namespace content
