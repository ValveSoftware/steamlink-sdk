// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This a QtWebEngine specific stripped down replacement of Chromiums's Profile
// class, because it is used many places just for preference access.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_H_

#include "content/public/browser/browser_context.h"

class PrefService;

namespace content {
class WebUI;
}

class Profile : public content::BrowserContext {
 public:
  // Returns the profile corresponding to the given browser context.
  static Profile* FromBrowserContext(content::BrowserContext* browser_context);

  // Returns the profile corresponding to the given WebUI.
  static Profile* FromWebUI(content::WebUI* web_ui);

  // Retrieves a pointer to the PrefService that manages the
  // preferences for this user profile.
  virtual PrefService* GetPrefs() = 0;
  virtual const PrefService* GetPrefs() const = 0;

};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_H_
