// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_
#define BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "blimp/client/feature/compositor/blob_image_serialization_processor.h"
#include "blimp/client/session/assignment_source.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/blimp_message_processor.h"

namespace net {
class IPEndPoint;
}

namespace blimp {

class BlimpMessageProcessor;
class BlimpMessageThreadPipe;
class BlobChannelReceiver;
class BrowserConnectionHandler;
class ClientConnectionManager;
class HeliumBlobReceiverDelegate;
class ThreadPipeManager;

namespace client {

class ClientNetworkComponents;
class NavigationFeature;
class ImeFeature;
class RenderWidgetFeature;
class SettingsFeature;
class TabControlFeature;

class NetworkEventObserver {
 public:
  NetworkEventObserver() {}
  virtual ~NetworkEventObserver() {}

  virtual void OnConnected() = 0;
  virtual void OnDisconnected(int result) = 0;
};

// BlimpClientSession represents a single active session of Blimp on the client
// regardless of whether or not the client application is in the background or
// foreground.  The only time this session is invalid is during initialization
// and shutdown of this particular client process (or Activity on Android).
//
// This session glues together the feature proxy components and the network
// layer.  The network components must be interacted with on the IO thread.  The
// feature proxies must be interacted with on the UI thread.
class BlimpClientSession
    : public NetworkEventObserver,
      public BlobImageSerializationProcessor::ErrorDelegate {
 public:
  explicit BlimpClientSession(const GURL& assigner_endpoint);

  // Uses the AssignmentSource to get an Assignment and then uses the assignment
  // configuration to connect to the Blimplet.
  // |client_auth_token| is the OAuth2 access token to use when querying
  // for an assignment.  This token needs the OAuth2 scope of userinfo.email and
  // only needs to be an access token, not a refresh token.
  void Connect(const std::string& client_auth_token);

  TabControlFeature* GetTabControlFeature() const;
  NavigationFeature* GetNavigationFeature() const;
  ImeFeature* GetImeFeature() const;
  RenderWidgetFeature* GetRenderWidgetFeature() const;
  SettingsFeature* GetSettingsFeature() const;

  BlimpConnectionStatistics* GetBlimpConnectionStatistics() const;

  // The AssignmentCallback for when an assignment is ready. This will trigger
  // a connection to the engine.
  virtual void ConnectWithAssignment(AssignmentSource::Result result,
                                     const Assignment& assignment);

 protected:
  ~BlimpClientSession() override;

  // Notified every time the AssignmentSource returns the result of an attempted
  // assignment request.
  virtual void OnAssignmentConnectionAttempted(AssignmentSource::Result result,
                                               const Assignment& assignment);

 private:
  void RegisterFeatures();

  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int result) override;

  // BlobImageSerializationProcessor::ErrorDelegate implementation.
  void OnImageDecodeError() override;

  base::Thread io_thread_;

  // Receives blob BlimpMessages and relays them to BlobChannelReceiver.
  // Owned by BlobChannelReceiver, therefore stored as a raw pointer here.
  HeliumBlobReceiverDelegate* blob_delegate_ = nullptr;

  // Retrieves and decodes image data from |blob_receiver_|. Must outlive
  // |blob_receiver_|.
  BlobImageSerializationProcessor blob_image_processor_;

  std::unique_ptr<BlobChannelReceiver> blob_receiver_;
  std::unique_ptr<TabControlFeature> tab_control_feature_;
  std::unique_ptr<NavigationFeature> navigation_feature_;
  std::unique_ptr<ImeFeature> ime_feature_;
  std::unique_ptr<RenderWidgetFeature> render_widget_feature_;
  std::unique_ptr<SettingsFeature> settings_feature_;

  // The AssignmentSource is used when the user of BlimpClientSession calls
  // Connect() to get a valid assignment and later connect to the engine.
  std::unique_ptr<AssignmentSource> assignment_source_;

  // Collects details of network, such as number of commits and bytes
  // transferred over network. Ownership is maintained on the IO thread.
  BlimpConnectionStatistics* blimp_connection_statistics_;

  // Container struct for network components.
  // Must be deleted on the IO thread.
  std::unique_ptr<ClientNetworkComponents> net_components_;

  std::unique_ptr<ThreadPipeManager> thread_pipe_manager_;

  base::WeakPtrFactory<BlimpClientSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSession);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_H_
