// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

// See interfaces_ppb_private_no_permissions.h for other private interfaces.

PROXIED_API(PPB_X509Certificate_Private)

PROXIED_IFACE(PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1,
              PPB_X509Certificate_Private_0_1)

#if !defined(OS_NACL)
PROXIED_API(PPB_Broker)

PROXIED_IFACE(PPB_BROKER_TRUSTED_INTERFACE_0_2,
              PPB_BrokerTrusted_0_2)
PROXIED_IFACE(PPB_BROKER_TRUSTED_INTERFACE_0_3,
              PPB_BrokerTrusted_0_3)
PROXIED_IFACE(PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0,
              PPB_BrowserFont_Trusted_1_0)
PROXIED_IFACE(PPB_CONTENTDECRYPTOR_PRIVATE_INTERFACE_0_12,
              PPB_ContentDecryptor_Private_0_12)
PROXIED_IFACE(PPB_CHARSET_TRUSTED_INTERFACE_1_0,
              PPB_CharSet_Trusted_1_0)
PROXIED_IFACE(PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5,
              PPB_FileChooserTrusted_0_5)
PROXIED_IFACE(PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6,
              PPB_FileChooserTrusted_0_6)
PROXIED_IFACE(PPB_FILEREFPRIVATE_INTERFACE_0_1,
              PPB_FileRefPrivate_0_1)
PROXIED_IFACE(PPB_FIND_PRIVATE_INTERFACE_0_3,
              PPB_Find_Private_0_3)
PROXIED_IFACE(PPB_FLASHFULLSCREEN_INTERFACE_0_1,
              PPB_FlashFullscreen_0_1)
PROXIED_IFACE(PPB_FLASHFULLSCREEN_INTERFACE_1_0,
              PPB_FlashFullscreen_0_1)
PROXIED_IFACE(PPB_PDF_INTERFACE,
              PPB_PDF)
#if defined(OS_CHROMEOS)
PROXIED_IFACE(PPB_PLATFORMVERIFICATION_PRIVATE_INTERFACE_0_2,
              PPB_PlatformVerification_Private_0_2)
#endif
PROXIED_IFACE(PPB_TALK_PRIVATE_INTERFACE_1_0,
              PPB_Talk_Private_1_0)
PROXIED_IFACE(PPB_TALK_PRIVATE_INTERFACE_2_0,
              PPB_Talk_Private_2_0)

PROXIED_IFACE(PPB_URLLOADERTRUSTED_INTERFACE_0_3,
              PPB_URLLoaderTrusted_0_3)

PROXIED_IFACE(PPB_OUTPUTPROTECTION_PRIVATE_INTERFACE_0_1,
              PPB_OutputProtection_Private_0_1)

// Hack to keep font working. The Font 0.6 API is binary compatible with
// BrowserFont 1.0, so just map the string to the same thing.
// TODO(brettw) remove support for the old Font API.
PROXIED_IFACE(PPB_FONT_DEV_INTERFACE_0_6,
              PPB_BrowserFont_Trusted_1_0)
#endif  // !defined(OS_NACL)

PROXIED_IFACE(PPB_INPUTEVENT_PRIVATE_INTERFACE_0_1,
              PPB_InputEvent_Private_0_1)

#include "ppapi/thunk/interfaces_postamble.h"
