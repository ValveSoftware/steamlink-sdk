// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/handlers/gcm_handler_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/proto/push_message.pb.h"
#include "components/gcm_driver/gcm_driver.h"

using gcm::GCMClient;

namespace {

// TODO(ckehoe): Move this to a common library.
bool Base64Decode(std::string data, std::string* out) {
  // Convert from URL-safe.
  base::ReplaceChars(data, "-", "+", &data);
  base::ReplaceChars(data, "_", "/", &data);

  // Add padding if needed.
  while (data.size() % 4)
    data.push_back('=');

  // Decode.
  return base::Base64Decode(data, out);
}

}  // namespace

namespace copresence {

const char GCMHandlerImpl::kCopresenceAppId[] =
    "com.google.android.gms.location.copresence";
const char GCMHandlerImpl::kCopresenceSenderId[] = "745476177629";
const char GCMHandlerImpl::kGcmMessageKey[] = "PUSH_MESSAGE";


// Public functions.

GCMHandlerImpl::GCMHandlerImpl(gcm::GCMDriver* gcm_driver,
                               DirectiveHandler* directive_handler,
                               const MessagesCallback& new_messages_callback)
    : driver_(gcm_driver),
      directive_handler_(directive_handler),
      new_messages_callback_(new_messages_callback),
      registration_callback_(base::Bind(&GCMHandlerImpl::RegistrationComplete,
                                        base::Unretained(this))) {
  DCHECK(driver_);
  DCHECK(directive_handler_);

  driver_->AddAppHandler(kCopresenceAppId, this);
  driver_->Register(kCopresenceAppId,
                    std::vector<std::string>(1, kCopresenceSenderId),
                    registration_callback_.callback());
}

GCMHandlerImpl::~GCMHandlerImpl() {
  if (driver_)
    driver_->RemoveAppHandler(kCopresenceAppId);
}

void GCMHandlerImpl::GetGcmId(const RegistrationCallback& callback) {
  if (gcm_id_.empty()) {
    pending_id_requests_.push_back(callback);
  } else {
    callback.Run(gcm_id_);
  }
}

void GCMHandlerImpl::ShutdownHandler() {
  // The GCMDriver is going away. Make sure we don't try to contact it.
  driver_ = nullptr;
}

void GCMHandlerImpl::OnMessage(const std::string& app_id,
                               const gcm::IncomingMessage& message) {
  DCHECK_EQ(kCopresenceAppId, app_id);
  DVLOG(2) << "Incoming GCM message";

  const auto& content = message.data.find(kGcmMessageKey);
  if (content == message.data.end()) {
    LOG(ERROR) << "GCM message missing data key";
    return;
  }

  std::string serialized_message;
  if (!Base64Decode(content->second, &serialized_message)) {
    LOG(ERROR) << "Couldn't decode GCM message";
    return;
  }

  PushMessage push_message;
  if (!push_message.ParseFromString(serialized_message)) {
    LOG(ERROR) << "GCM message contained invalid proto";
    return;
  }

  if (push_message.type() != PushMessage::REPORT) {
    DVLOG(2) << "Discarding non-report GCM message";
    return;
  }

  DVLOG(3) << "Processing " << push_message.report().directive_size()
           << " directive(s) from GCM message";
  for (const Directive& directive : push_message.report().directive()) {
    // TODO(ckehoe): Use a callback here so GCMHandler
    // is decoupled from DirectiveHandler.
    directive_handler_->AddDirective(directive);
  }

  new_messages_callback_.Run(push_message.report().subscribed_message());
}

void GCMHandlerImpl::OnMessagesDeleted(const std::string& app_id) {
  DCHECK_EQ(kCopresenceAppId, app_id);
  DVLOG(2) << "GCM message overflow reported";
}

void GCMHandlerImpl::OnSendError(
    const std::string& /* app_id */,
    const GCMClient::SendErrorDetails& /* send_error_details */) {
  NOTREACHED() << "Copresence clients should not be sending GCM messages";
}

void GCMHandlerImpl::OnSendAcknowledged(const std::string& /* app_id */,
                                        const std::string& /* message_id */) {
  NOTREACHED() << "Copresence clients should not be sending GCM messages";
}

bool GCMHandlerImpl::CanHandle(const std::string& app_id) const {
  return app_id == kCopresenceAppId;
}


// Private functions.

void GCMHandlerImpl::RegistrationComplete(const std::string& registration_id,
                                          GCMClient::Result result) {
  if (result == GCMClient::SUCCESS) {
    DVLOG(2) << "GCM registration successful. ID: " << registration_id;
    gcm_id_ = registration_id;
  } else {
    LOG(ERROR) << "GCM registration failed with error " << result;
  }

  for (const RegistrationCallback& callback : pending_id_requests_) {
    callback.Run(result == GCMClient::SUCCESS ?
        registration_id : std::string());
  }
  pending_id_requests_.clear();
}

}  // namespace copresence
