// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/presentation_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/presentation/presentation_connection_client.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace {

blink::WebPresentationError::ErrorType GetWebPresentationErrorTypeFromMojo(
    blink::mojom::PresentationErrorType mojoErrorType) {
  switch (mojoErrorType) {
    case blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS:
      return blink::WebPresentationError::ErrorTypeNoAvailableScreens;
    case blink::mojom::PresentationErrorType::SESSION_REQUEST_CANCELLED:
      return blink::WebPresentationError::ErrorTypeSessionRequestCancelled;
    case blink::mojom::PresentationErrorType::NO_PRESENTATION_FOUND:
      return blink::WebPresentationError::ErrorTypeNoPresentationFound;
    case blink::mojom::PresentationErrorType::UNKNOWN:
    default:
      return blink::WebPresentationError::ErrorTypeUnknown;
  }
}

blink::WebPresentationConnectionState GetWebPresentationConnectionStateFromMojo(
    blink::mojom::PresentationConnectionState mojoSessionState) {
  switch (mojoSessionState) {
    // TODO(imcheng): Add Connecting state to Blink (crbug.com/575351).
    case blink::mojom::PresentationConnectionState::CONNECTED:
      return blink::WebPresentationConnectionState::Connected;
    case blink::mojom::PresentationConnectionState::CLOSED:
      return blink::WebPresentationConnectionState::Closed;
    case blink::mojom::PresentationConnectionState::TERMINATED:
      return blink::WebPresentationConnectionState::Terminated;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionState::Terminated;
  }
}

blink::WebPresentationConnectionCloseReason
GetWebPresentationConnectionCloseReasonFromMojo(
    blink::mojom::PresentationConnectionCloseReason mojoConnectionCloseReason) {
  switch (mojoConnectionCloseReason) {
    case blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR:
      return blink::WebPresentationConnectionCloseReason::Error;
    case blink::mojom::PresentationConnectionCloseReason::CLOSED:
      return blink::WebPresentationConnectionCloseReason::Closed;
    case blink::mojom::PresentationConnectionCloseReason::WENT_AWAY:
      return blink::WebPresentationConnectionCloseReason::WentAway;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionCloseReason::Error;
  }
}

}  // namespace

namespace content {

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      controller_(nullptr),
      binding_(this) {
}

PresentationDispatcher::~PresentationDispatcher() {
  // Controller should be destroyed before the dispatcher when frame is
  // destroyed.
  DCHECK(!controller_);
}

void PresentationDispatcher::setController(
    blink::WebPresentationController* controller) {
  // There shouldn't be any swapping from one non-null controller to another.
  DCHECK(controller != controller_ && (!controller || !controller_));
  controller_ = controller;
  // The controller is set to null when the frame is about to be detached.
  // Nothing is listening for screen availability anymore but the Mojo service
  // will know about the frame being detached anyway.
}

void PresentationDispatcher::startSession(
    const blink::WebString& presentationUrl,
    blink::WebPresentationConnectionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->StartSession(
      presentationUrl.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
          base::Unretained(this),
          base::Owned(callback)));
}

void PresentationDispatcher::joinSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    blink::WebPresentationConnectionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->JoinSession(
      presentationUrl.utf8(),
      presentationId.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
          base::Unretained(this),
          base::Owned(callback)));
}

void PresentationDispatcher::sendString(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message) {
  if (message.utf8().size() > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Limit the size of individual messages to 64k
    // for now. Consider throwing DOMException or splitting bigger messages
    // into smaller chunks later.
    LOG(WARNING) << "message size exceeded limit!";
    return;
  }

  message_request_queue_.push(base::WrapUnique(
      CreateSendTextMessageRequest(presentationUrl, presentationId, message)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendArrayBuffer(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push(base::WrapUnique(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      blink::mojom::PresentationMessageType::ARRAY_BUFFER, data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendBlobData(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push(base::WrapUnique(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      blink::mojom::PresentationMessageType::BLOB, data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::DoSendMessage(SendMessageRequest* request) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->SendSessionMessage(
      std::move(request->session_info), std::move(request->message),
      base::Bind(&PresentationDispatcher::HandleSendMessageRequests,
                 base::Unretained(this)));
}

void PresentationDispatcher::HandleSendMessageRequests(bool success) {
  // In normal cases, message_request_queue_ should not be empty at this point
  // of time, but when DidCommitProvisionalLoad() is invoked before receiving
  // the callback for previous send mojo call, queue would have been emptied.
  if (message_request_queue_.empty())
    return;

  if (!success) {
    // PresentationServiceImpl is informing that Frame has been detached or
    // navigated away. Invalidate all pending requests.
    MessageRequestQueue empty;
    std::swap(message_request_queue_, empty);
    return;
  }

  message_request_queue_.pop();
  if (!message_request_queue_.empty()) {
    DoSendMessage(message_request_queue_.front().get());
  }
}

void PresentationDispatcher::closeSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->CloseConnection(presentationUrl.utf8(),
                                         presentationId.utf8());
}

void PresentationDispatcher::terminateSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->Terminate(presentationUrl.utf8(),
                                   presentationId.utf8());
}

void PresentationDispatcher::getAvailability(
    const blink::WebString& availabilityUrl,
    blink::WebPresentationAvailabilityCallbacks* callbacks) {
  const std::string& availability_url = availabilityUrl.utf8();
  AvailabilityStatus* status = nullptr;
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end()) {
    status = new AvailabilityStatus(availability_url);
    availability_status_[availability_url] = base::WrapUnique(status);
  } else {
    status = status_it->second.get();
  }
  DCHECK(status);

  if (status->listening_state == ListeningState::ACTIVE) {
    callbacks->onSuccess(status->last_known_availability);
    delete callbacks;
    return;
  }

  status->availability_callbacks.Add(callbacks);
  UpdateListeningState(status);
}

void PresentationDispatcher::startListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  const std::string& availability_url = observer->url().string().utf8();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end()) {
    DLOG(WARNING) << "Start listening for availability for unknown URL "
                  << availability_url;
    return;
  }
  status_it->second->availability_observers.insert(observer);
  UpdateListeningState(status_it->second.get());
}

void PresentationDispatcher::stopListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  const std::string& availability_url = observer->url().string().utf8();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end()) {
    DLOG(WARNING) << "Stop listening for availability for unknown URL "
                  << availability_url;
    return;
  }
  status_it->second->availability_observers.erase(observer);
  UpdateListeningState(status_it->second.get());
}

void PresentationDispatcher::setDefaultPresentationUrl(
    const blink::WebString& url) {
  ConnectToPresentationServiceIfNeeded();
  presentation_service_->SetDefaultPresentationURL(url.utf8());
}

void PresentationDispatcher::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // If not top-level navigation.
  if (frame->parent() || is_same_page_navigation)
    return;

  // Remove all pending send message requests.
  MessageRequestQueue empty;
  std::swap(message_request_queue_, empty);
}

void PresentationDispatcher::OnDestruct() {
  delete this;
}

void PresentationDispatcher::OnScreenAvailabilityUpdated(
    const mojo::String& url, bool available) {
  const std::string& availability_url = url.get();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end())
    return;
  AvailabilityStatus* status = status_it->second.get();
  DCHECK(status);

  if (status->listening_state == ListeningState::WAITING)
    status->listening_state = ListeningState::ACTIVE;

  for (auto observer : status->availability_observers)
    observer->availabilityChanged(available);

  for (AvailabilityCallbacksMap::iterator iter(&status->availability_callbacks);
       !iter.IsAtEnd(); iter.Advance()) {
    iter.GetCurrentValue()->onSuccess(available);
  }
  status->last_known_availability = available;
  status->availability_callbacks.Clear();
  UpdateListeningState(status);
}

void PresentationDispatcher::OnScreenAvailabilityNotSupported(
    const mojo::String& url) {
  const std::string& availability_url = url.get();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end())
    return;
  AvailabilityStatus* status = status_it->second.get();
  DCHECK(status);
  DCHECK(status->listening_state == ListeningState::WAITING);

  const blink::WebString& not_supported_error = blink::WebString::fromUTF8(
      "getAvailability() isn't supported at the moment. It can be due to "
      "a permanent or temporary system limitation. It is recommended to "
      "try to blindly start a session in that case.");
  for (AvailabilityCallbacksMap::iterator iter(&status->availability_callbacks);
       !iter.IsAtEnd(); iter.Advance()) {
    iter.GetCurrentValue()->onError(blink::WebPresentationError(
        blink::WebPresentationError::ErrorTypeAvailabilityNotSupported,
        not_supported_error));
  }
  status->availability_callbacks.Clear();
  UpdateListeningState(status);
}

void PresentationDispatcher::OnDefaultSessionStarted(
    blink::mojom::PresentationSessionInfoPtr session_info) {
  if (!controller_)
    return;

  if (!session_info.is_null()) {
    presentation_service_->ListenForSessionMessages(session_info.Clone());
    controller_->didStartDefaultSession(
        new PresentationConnectionClient(std::move(session_info)));
  }
}

void PresentationDispatcher::OnSessionCreated(
    blink::WebPresentationConnectionClientCallbacks* callback,
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::PresentationErrorPtr error) {
  DCHECK(callback);
  if (!error.is_null()) {
    DCHECK(session_info.is_null());
    callback->onError(blink::WebPresentationError(
        GetWebPresentationErrorTypeFromMojo(error->error_type),
        blink::WebString::fromUTF8(error->message)));
    return;
  }

  DCHECK(!session_info.is_null());
  presentation_service_->ListenForSessionMessages(session_info.Clone());
  callback->onSuccess(base::WrapUnique(
      new PresentationConnectionClient(std::move(session_info))));
}

void PresentationDispatcher::OnConnectionStateChanged(
    blink::mojom::PresentationSessionInfoPtr connection,
    blink::mojom::PresentationConnectionState state) {
  if (!controller_)
    return;

  DCHECK(!connection.is_null());
  controller_->didChangeSessionState(
      new PresentationConnectionClient(std::move(connection)),
      GetWebPresentationConnectionStateFromMojo(state));
}

void PresentationDispatcher::OnConnectionClosed(
    blink::mojom::PresentationSessionInfoPtr connection,
    blink::mojom::PresentationConnectionCloseReason reason,
    const mojo::String& message) {
  if (!controller_)
    return;

  DCHECK(!connection.is_null());
  controller_->didCloseConnection(
      new PresentationConnectionClient(std::move(connection)),
      GetWebPresentationConnectionCloseReasonFromMojo(reason),
      blink::WebString::fromUTF8(message));
}

void PresentationDispatcher::OnSessionMessagesReceived(
    blink::mojom::PresentationSessionInfoPtr session_info,
    mojo::Array<blink::mojom::SessionMessagePtr> messages) {
  if (!controller_)
    return;

  for (size_t i = 0; i < messages.size(); ++i) {
    // Note: Passing batches of messages to the Blink layer would be more
    // efficient.
    std::unique_ptr<PresentationConnectionClient> session_client(
        new PresentationConnectionClient(session_info->url, session_info->id));
    switch (messages[i]->type) {
      case blink::mojom::PresentationMessageType::TEXT: {
        controller_->didReceiveSessionTextMessage(
            session_client.release(),
            blink::WebString::fromUTF8(messages[i]->message));
        break;
      }
      case blink::mojom::PresentationMessageType::ARRAY_BUFFER:
      case blink::mojom::PresentationMessageType::BLOB: {
        controller_->didReceiveSessionBinaryMessage(
            session_client.release(), &(messages[i]->data.front()),
            messages[i]->data.size());
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }
  }
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_.get())
    return;

  render_frame()->GetRemoteInterfaces()->GetInterface(&presentation_service_);
  presentation_service_->SetClient(binding_.CreateInterfacePtrAndBind());
}

void PresentationDispatcher::UpdateListeningState(AvailabilityStatus* status) {
  bool should_listen = !status->availability_callbacks.IsEmpty() ||
                       !status->availability_observers.empty();
  bool is_listening = status->listening_state != ListeningState::INACTIVE;

  if (should_listen == is_listening)
    return;

  ConnectToPresentationServiceIfNeeded();
  if (should_listen) {
    status->listening_state = ListeningState::WAITING;
    presentation_service_->ListenForScreenAvailability(status->url);
  } else {
    status->listening_state = ListeningState::INACTIVE;
    presentation_service_->StopListeningForScreenAvailability(status->url);
  }
}

PresentationDispatcher::SendMessageRequest::SendMessageRequest(
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::SessionMessagePtr message)
    : session_info(std::move(session_info)), message(std::move(message)) {}

PresentationDispatcher::SendMessageRequest::~SendMessageRequest() {}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendTextMessageRequest(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message) {
  blink::mojom::PresentationSessionInfoPtr session_info =
      blink::mojom::PresentationSessionInfo::New();
  session_info->url = presentationUrl.utf8();
  session_info->id = presentationId.utf8();

  blink::mojom::SessionMessagePtr session_message =
      blink::mojom::SessionMessage::New();
  session_message->type = blink::mojom::PresentationMessageType::TEXT;
  session_message->message = message.utf8();
  return new SendMessageRequest(std::move(session_info),
                                std::move(session_message));
}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendBinaryMessageRequest(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    blink::mojom::PresentationMessageType type,
    const uint8_t* data,
    size_t length) {
  blink::mojom::PresentationSessionInfoPtr session_info =
      blink::mojom::PresentationSessionInfo::New();
  session_info->url = presentationUrl.utf8();
  session_info->id = presentationId.utf8();

  blink::mojom::SessionMessagePtr session_message =
      blink::mojom::SessionMessage::New();
  session_message->type = type;
  std::vector<uint8_t> tmp_data_vector(data, data + length);
  session_message->data.Swap(&tmp_data_vector);
  return new SendMessageRequest(std::move(session_info),
                                std::move(session_message));
}

PresentationDispatcher::AvailabilityStatus::AvailabilityStatus(
    const std::string& availability_url)
    : url(availability_url),
      last_known_availability(false),
      listening_state(ListeningState::INACTIVE) {}

PresentationDispatcher::AvailabilityStatus::~AvailabilityStatus() {
}

}  // namespace content
