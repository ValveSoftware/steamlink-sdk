// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PLATFORM_VERIFICATION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PLATFORM_VERIFICATION_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/macros.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/resource_message_filter.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#endif

namespace content {
class BrowserPpapiHost;
}  // namespace content

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

namespace chrome {

// This filter handles messages for platform verification on the UI thread.
class PepperPlatformVerificationMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperPlatformVerificationMessageFilter(content::BrowserPpapiHost* host,
                                          PP_Instance instance);

 private:
  ~PepperPlatformVerificationMessageFilter() override;

  // ppapi::host::ResourceMessageFilter overrides.
  scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  int32_t OnChallengePlatform(ppapi::host::HostMessageContext* context,
                              const std::string& service_id,
                              const std::vector<uint8_t>& challenge);
#if defined(OS_CHROMEOS)
  // PlatformVerificationFlow callbacks.
  void ChallengePlatformCallback(
      ppapi::host::ReplyMessageContext reply_context,
      chromeos::attestation::PlatformVerificationFlow::Result challenge_result,
      const std::string& signed_data,
      const std::string& signature,
      const std::string& platform_key_certificate);
#endif

  // Used to lookup the WebContents associated with this PP_Instance.
  int render_process_id_;
  int render_frame_id_;

#if defined(OS_CHROMEOS)
  // Must only be accessed on the UI thread.
  scoped_refptr<chromeos::attestation::PlatformVerificationFlow> pv_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformVerificationMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PLATFORM_VERIFICATION_MESSAGE_FILTER_H_
