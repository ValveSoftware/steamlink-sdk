// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

// The operation could not be performed because the connection to the peer was
// lost.
HELIUM_ERROR(DISCONNECTED, 1)

// The remote end provided data which is not compliant with the protocol.
HELIUM_ERROR(PROTOCOL_ERROR, 2)
