// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBUSB_WEBUSB_BROWSER_CLIENT_H_
#define COMPONENTS_WEBUSB_WEBUSB_BROWSER_CLIENT_H_

#include <string>

#include "base/strings/string16.h"

class GURL;

namespace webusb {

// Interface to allow the webusb module to make browser-process-specific
// calls.
class WebUsbBrowserClient {
 public:
  virtual ~WebUsbBrowserClient() {}

  virtual void OnDeviceAdded(const base::string16& product_name,
                             const GURL& landing_page,
                             const std::string& notification_id) = 0;

  virtual void OnDeviceRemoved(const std::string& notification_id) = 0;
};

}  // namespace webusb

#endif  // COMPONENTS_WEBUSB_WEBUSB_BROWSER_CLIENT_H_
