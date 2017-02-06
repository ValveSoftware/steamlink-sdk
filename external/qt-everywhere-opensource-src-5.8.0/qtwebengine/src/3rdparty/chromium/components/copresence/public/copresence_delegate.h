// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_DELEGATE_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

namespace gcm {
class GCMDriver;
}

namespace net {
class URLRequestContextGetter;
}

namespace audio_modem {
class WhispernetClient;
}

namespace copresence {

class Message;

// AUDIO_FAIL indicates that we weren't able to hear the audio token that
// we were playing.
enum CopresenceStatus { SUCCESS, FAIL, AUDIO_FAIL };

// Callback type to send a status.
using StatusCallback = base::Callback<void(CopresenceStatus)>;

// A delegate interface for users of Copresence.
class CopresenceDelegate {
 public:
  // This method will be called when we have subscribed messages
  // that need to be sent to their respective apps.
  virtual void HandleMessages(
      const std::string& app_id,
      const std::string& subscription_id,
      const std::vector<Message>& messages) = 0;

  virtual void HandleStatusUpdate(CopresenceStatus status) = 0;

  // Thw URLRequestContextGetter must outlive the CopresenceManager.
  virtual net::URLRequestContextGetter* GetRequestContext() const = 0;

  virtual std::string GetPlatformVersionString() const = 0;

  virtual std::string GetAPIKey(const std::string& app_id) const = 0;

  // The WhispernetClient must outlive the CopresenceManager.
  virtual audio_modem::WhispernetClient* GetWhispernetClient() = 0;

  // Clients may optionally provide a GCMDriver to receive messages from.
  // If no driver is available, this can return null.
  virtual gcm::GCMDriver* GetGCMDriver() = 0;

  // Get the copresence device ID for authenticated or anonymous calls,
  // as specified. If none exists, return the empty string.
  virtual std::string GetDeviceId(bool authenticated) = 0;

  // Save a copresence device ID for authenticated or anonymous calls.
  // If the device ID is empty, any stored ID should be deleted.
  virtual void SaveDeviceId(bool authenticated,
                            const std::string& device_id) = 0;

 protected:
  virtual ~CopresenceDelegate() {}
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_DELEGATE_H_
