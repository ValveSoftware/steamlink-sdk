// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/stun_field_trial.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/p2p/stunprober/stunprober.h"

namespace base {
class WaitableEvent;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace rtc {
class NetworkManager;
class PacketSocketFactory;
class Thread;
}

namespace blink {
class WebFrame;
class WebMediaConstraints;
class WebMediaStream;
class WebMediaStreamSource;
class WebMediaStreamTrack;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
}

namespace content {

class IpcNetworkManager;
class IpcPacketSocketFactory;
class WebRtcAudioDeviceImpl;
class WebRtcLoggingHandlerImpl;
class WebRtcLoggingMessageFilter;
class WebRtcVideoCapturerAdapter;
struct StreamDeviceInfo;

// Object factory for RTC PeerConnections.
class CONTENT_EXPORT PeerConnectionDependencyFactory
    : NON_EXPORTED_BASE(base::MessageLoop::DestructionObserver),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  PeerConnectionDependencyFactory(
      P2PSocketDispatcher* p2p_socket_dispatcher);
  ~PeerConnectionDependencyFactory() override;

  // Create a RTCPeerConnectionHandler object that implements the
  // WebKit WebRTCPeerConnectionHandler interface.
  blink::WebRTCPeerConnectionHandler* CreateRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);

  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  // Creates an implementation of a cricket::VideoCapturer object that can be
  // used when creating a libjingle webrtc::VideoTrackSourceInterface object.
  virtual WebRtcVideoCapturerAdapter* CreateVideoCapturer(
      bool is_screen_capture);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack(
      const std::string& id,
      webrtc::VideoTrackSourceInterface* source);

  // Asks the PeerConnection factory to create a Video Source.
  // The video source takes ownership of |capturer|.
  virtual scoped_refptr<webrtc::VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer);

  // Asks the libjingle PeerConnection factory to create a libjingle
  // PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(
          const webrtc::PeerConnectionInterface::RTCConfiguration& config,
          blink::WebFrame* web_frame,
          webrtc::PeerConnectionObserver* observer);

  // Creates a libjingle representation of a Session description. Used by a
  // RTCPeerConnectionHandler instance.
  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& type,
      const std::string& sdp,
      webrtc::SdpParseError* error);

  // Creates a libjingle representation of an ice candidate.
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp);

  // Starts recording an RTC event log.
  virtual bool StartRtcEventLog(base::PlatformFile file);

  // Starts recording an RTC event log.
  virtual void StopRtcEventLog();

  WebRtcAudioDeviceImpl* GetWebRtcAudioDevice();

  void EnsureInitialized();
  scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcWorkerThread() const;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcSignalingThread()
      const;

 protected:
  // Asks the PeerConnection factory to create a Local VideoTrack object with
  // the video source using |capturer|.
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            cricket::VideoCapturer* capturer);

  virtual const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
      GetPcFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Helper method to create a WebRtcAudioDeviceImpl.
  void EnsureWebRtcAudioDeviceImpl();

 private:
  // Implement base::MessageLoop::DestructionObserver.
  // This makes sure the libjingle PeerConnectionFactory is released before
  // the renderer message loop is destroyed.
  void WillDestroyCurrentMessageLoop() override;

  // Functions related to Stun probing trial to determine how fast we could send
  // Stun request without being dropped by NAT.
  void TryScheduleStunProbeTrial();
  void StartStunProbeTrialOnWorkerThread(const std::string& params);

  // Creates |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  void CreatePeerConnectionFactory();

  void InitializeSignalingThread(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      base::WaitableEvent* event);

  void InitializeWorkerThread(rtc::Thread** thread,
                              base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnWorkerThread(base::WaitableEvent* event);
  void DeleteIpcNetworkManager();
  void CleanupPeerConnectionFactory();

  // We own network_manager_, must be deleted on the worker thread.
  // The network manager uses |p2p_socket_dispatcher_|.
  IpcNetworkManager* network_manager_;
  std::unique_ptr<IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device_;

  std::unique_ptr<StunProberTrial> stun_trial_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  base::Thread chrome_signaling_thread_;
  base::Thread chrome_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionDependencyFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
