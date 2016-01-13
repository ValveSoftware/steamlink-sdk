// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PEPPER_FLASH_SETTINGS_HELPER_IMPL_H_
#define CONTENT_BROWSER_PEPPER_FLASH_SETTINGS_HELPER_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/pepper_flash_settings_helper.h"

namespace content {

class CONTENT_EXPORT PepperFlashSettingsHelperImpl
    : public PepperFlashSettingsHelper,
      NON_EXPORTED_BASE(public PpapiPluginProcessHost::BrokerClient) {
 public:
  PepperFlashSettingsHelperImpl();

  // PepperFlashSettingsHelper implementation.
  virtual void OpenChannelToBroker(
      const base::FilePath& path,
      const OpenChannelCallback& callback) OVERRIDE;

  // PpapiPluginProcessHost::BrokerClient implementation.
  virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                   int* renderer_id) OVERRIDE;
  virtual void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                                    base::ProcessId plugin_pid,
                                    int plugin_child_id) OVERRIDE;
  virtual bool OffTheRecord() OVERRIDE;

 protected:
  virtual ~PepperFlashSettingsHelperImpl();

 private:
  OpenChannelCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(PepperFlashSettingsHelperImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PEPPER_FLASH_SETTINGS_HELPER_IMPL_H_
