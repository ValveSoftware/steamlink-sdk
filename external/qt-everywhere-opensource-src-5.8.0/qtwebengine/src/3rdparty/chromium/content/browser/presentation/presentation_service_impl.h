// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"

namespace content {

struct FrameNavigateParams;
struct LoadCommittedDetails;
struct PresentationSessionMessage;
class RenderFrameHost;

// Implementation of Mojo PresentationService.
// It handles Presentation API requests coming from Blink / renderer process
// and delegates the requests to the embedder's media router via
// PresentationServiceDelegate.
// An instance of this class tied to a RenderFrameHost and listens to events
// related to the RFH via implementing WebContentsObserver.
// This class is instantiated on-demand via Mojo's ConnectToRemoteService
// from the renderer when the first presentation API request is handled.
class CONTENT_EXPORT PresentationServiceImpl
    : public NON_EXPORTED_BASE(blink::mojom::PresentationService),
      public WebContentsObserver,
      public PresentationServiceDelegate::Observer {
 public:
  using NewSessionCallback =
      base::Callback<void(blink::mojom::PresentationSessionInfoPtr,
                          blink::mojom::PresentationErrorPtr)>;

  ~PresentationServiceImpl() override;

  // Static factory method to create an instance of PresentationServiceImpl.
  // |render_frame_host|: The RFH the instance is associated with.
  // |request|: The instance will be bound to this request. Used for Mojo setup.
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<blink::mojom::PresentationService> request);

 private:
  friend class PresentationServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, Reset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, DidNavigateThisFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DidNavigateOtherFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, ThisRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      OtherRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, DelegateFails);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      SetDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      SetSameDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ClearDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ListenForDefaultSessionStart);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ListenForDefaultSessionStartAfterSet);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DefaultSessionStartReset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiveSessionMessagesAfterReset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingStartSessionRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingJoinSessionRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionStateChange);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionClose);

  // Maximum number of pending JoinSession requests at any given time.
  static const int kMaxNumQueuedSessionRequests = 10;

  using SessionMessagesCallback =
      base::Callback<void(mojo::Array<blink::mojom::SessionMessagePtr>)>;
  using SendSessionMessageCallback = base::Callback<void(bool)>;

  // Listener implementation owned by PresentationServiceImpl. An instance of
  // this is created when PresentationRequest.getAvailability() is resolved.
  // The instance receives screen availability results from the embedder and
  // propagates results back to PresentationServiceImpl.
  class CONTENT_EXPORT ScreenAvailabilityListenerImpl
      : public PresentationScreenAvailabilityListener {
   public:
    ScreenAvailabilityListenerImpl(
        const std::string& availability_url,
        PresentationServiceImpl* service);
    ~ScreenAvailabilityListenerImpl() override;

    // PresentationScreenAvailabilityListener implementation.
    std::string GetAvailabilityUrl() const override;
    void OnScreenAvailabilityChanged(bool available) override;
    void OnScreenAvailabilityNotSupported() override;

   private:
    const std::string availability_url_;
    PresentationServiceImpl* const service_;
  };

  // Ensures the provided NewSessionCallback is invoked exactly once
  // before it goes out of scope.
  class NewSessionCallbackWrapper {
   public:
    explicit NewSessionCallbackWrapper(
        const NewSessionCallback& callback);
    ~NewSessionCallbackWrapper();

    void Run(blink::mojom::PresentationSessionInfoPtr session,
             blink::mojom::PresentationErrorPtr error);

   private:
    NewSessionCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(NewSessionCallbackWrapper);
  };

  // |render_frame_host|: The RFH this instance is associated with.
  // |web_contents|: The WebContents to observe.
  // |delegate|: Where Presentation API requests are delegated to. Not owned
  // by this class.
  PresentationServiceImpl(
      RenderFrameHost* render_frame_host,
      WebContents* web_contents,
      PresentationServiceDelegate* delegate);

  // PresentationService implementation.
  void SetDefaultPresentationURL(const mojo::String& url) override;
  void SetClient(blink::mojom::PresentationServiceClientPtr client) override;
  void ListenForScreenAvailability(const mojo::String& url) override;
  void StopListeningForScreenAvailability(const mojo::String& url) override;
  void StartSession(
      const mojo::String& presentation_url,
      const NewSessionCallback& callback) override;
  void JoinSession(
      const mojo::String& presentation_url,
      const mojo::String& presentation_id,
      const NewSessionCallback& callback) override;
  void SendSessionMessage(blink::mojom::PresentationSessionInfoPtr session_info,
                          blink::mojom::SessionMessagePtr session_message,
                          const SendSessionMessageCallback& callback) override;
  void CloseConnection(const mojo::String& presentation_url,
                       const mojo::String& presentation_id) override;
  void Terminate(const mojo::String& presentation_url,
                 const mojo::String& presentation_id) override;
  void ListenForSessionMessages(
      blink::mojom::PresentationSessionInfoPtr session) override;

  // Creates a binding between this object and |request|.
  void Bind(mojo::InterfaceRequest<blink::mojom::PresentationService> request);

  // WebContentsObserver override.
  void DidNavigateAnyFrame(
      content::RenderFrameHost* render_frame_host,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // PresentationServiceDelegate::Observer
  void OnDelegateDestroyed() override;

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when default presentation has started.
  void OnDefaultPresentationStarted(
      const PresentationSessionInfo& session_info);

  // Finds the callback from |pending_join_session_cbs_| using
  // |request_session_id|.
  // If it exists, invoke it with |session| and |error|, then erase it from
  // |pending_join_session_cbs_|.
  // Returns true if the callback was found.
  bool RunAndEraseJoinSessionMojoCallback(
      int request_session_id,
      blink::mojom::PresentationSessionInfoPtr session,
      blink::mojom::PresentationErrorPtr error);

  // Removes all listeners and resets default presentation URL on this instance
  // and informs the PresentationServiceDelegate of such.
  void Reset();

  // These functions are bound as base::Callbacks and passed to
  // embedder's implementation of PresentationServiceDelegate for later
  // invocation.
  void OnStartSessionSucceeded(
      int request_session_id,
      const PresentationSessionInfo& session_info);
  void OnStartSessionError(
      int request_session_id,
      const PresentationError& error);
  void OnJoinSessionSucceeded(
      int request_session_id,
      const PresentationSessionInfo& session_info);
  void OnJoinSessionError(
      int request_session_id,
      const PresentationError& error);
  void OnSendMessageCallback(bool sent);

  // Calls to |delegate_| to start listening for state changes for |connection|.
  // State changes will be returned via |OnConnectionStateChanged|.
  void ListenForConnectionStateChange(
      const PresentationSessionInfo& connection);

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when session messages arrive.
  void OnSessionMessages(
      const content::PresentationSessionInfo& session,
      const ScopedVector<PresentationSessionMessage>& messages,
      bool pass_ownership);

  // Associates a JoinSession |callback| with a unique request ID and
  // stores it in a map.
  // Returns a positive value on success.
  int RegisterJoinSessionCallback(const NewSessionCallback& callback);

  // Invoked by the embedder's PresentationServiceDelegate when a
  // PresentationConnection's state has changed.
  void OnConnectionStateChanged(
      const PresentationSessionInfo& connection,
      const PresentationConnectionStateChangeInfo& info);

  // Returns true if this object is associated with |render_frame_host|.
  bool FrameMatches(content::RenderFrameHost* render_frame_host) const;

  // Embedder-specific delegate to forward Presentation requests to.
  // May be null if embedder does not support Presentation API.
  PresentationServiceDelegate* delegate_;

  // Proxy to the PresentationServiceClient to send results (e.g., screen
  // availability) to.
  blink::mojom::PresentationServiceClientPtr client_;

  std::string default_presentation_url_;

  using ScreenAvailabilityListenerMap =
      std::map<std::string, std::unique_ptr<ScreenAvailabilityListenerImpl>>;
  ScreenAvailabilityListenerMap screen_availability_listeners_;

  // For StartSession requests.
  // Set to a positive value when a StartSession request is being processed.
  int start_session_request_id_;
  std::unique_ptr<NewSessionCallbackWrapper> pending_start_session_cb_;

  // For JoinSession requests.
  base::hash_map<int, linked_ptr<NewSessionCallbackWrapper>>
      pending_join_session_cbs_;

  // RAII binding of |this| to an Presentation interface request.
  // The binding is removed when binding_ is cleared or goes out of scope.
  std::unique_ptr<mojo::Binding<blink::mojom::PresentationService>> binding_;

  // There can be only one send message request at a time.
  std::unique_ptr<SendSessionMessageCallback> send_message_callback_;

  std::unique_ptr<SessionMessagesCallback> on_session_messages_callback_;

  // ID of the RenderFrameHost this object is associated with.
  int render_process_id_;
  int render_frame_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PresentationServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PresentationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
