// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

// Interfaces go here.
PROXIED_IFACE(PPB_COMPOSITOR_INTERFACE_0_1, PPB_Compositor_0_1)
PROXIED_IFACE(PPB_COMPOSITORLAYER_INTERFACE_0_1, PPB_CompositorLayer_0_1)
PROXIED_IFACE(PPB_FILEMAPPING_INTERFACE_0_1, PPB_FileMapping_0_1)
PROXIED_IFACE(PPB_MESSAGING_INTERFACE_1_1, PPB_Messaging_1_1)
PROXIED_IFACE(PPB_VIDEODECODER_INTERFACE_0_1, PPB_VideoDecoder_0_1)

// Note, PPB_TraceEvent is special. We don't want to actually make it stable,
// but we want developers to be able to leverage it when running Chrome Dev or
// Chrome Canary.
PROXIED_IFACE(PPB_TRACE_EVENT_DEV_INTERFACE_0_1,
              PPB_Trace_Event_Dev_0_1)
PROXIED_IFACE(PPB_TRACE_EVENT_DEV_INTERFACE_0_2,
              PPB_Trace_Event_Dev_0_2)

#include "ppapi/thunk/interfaces_postamble.h"
