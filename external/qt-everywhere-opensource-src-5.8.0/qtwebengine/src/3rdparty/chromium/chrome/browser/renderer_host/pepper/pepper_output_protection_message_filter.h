// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/resource_message_filter.h"

namespace content {
class BrowserPpapiHost;
}  // namespace content

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

#if defined(OS_CHROMEOS)
namespace chromeos {
class OutputProtectionDelegate;
}
#endif

namespace chrome {

class PepperOutputProtectionMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperOutputProtectionMessageFilter(content::BrowserPpapiHost* host,
                                      PP_Instance instance);

 private:
  // ppapi::host::ResourceMessageFilter overrides.
  scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  ~PepperOutputProtectionMessageFilter() override;

  int32_t OnQueryStatus(ppapi::host::HostMessageContext* context);
  int32_t OnEnableProtection(ppapi::host::HostMessageContext* context,
                             uint32_t desired_method_mask);

  void OnQueryStatusComplete(ppapi::host::ReplyMessageContext reply_context,
                             bool success,
                             uint32_t link_mask,
                             uint32_t protection_mask);

  void OnEnableProtectionComplete(
      ppapi::host::ReplyMessageContext reply_context,
      bool success);

#if defined(OS_CHROMEOS)
  // Delegate. Should be deleted in UI thread.
  chromeos::OutputProtectionDelegate* delegate_;
#endif

  base::WeakPtrFactory<PepperOutputProtectionMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperOutputProtectionMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
