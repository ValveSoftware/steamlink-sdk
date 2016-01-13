// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

// This file defines the canonical axes mapping order for gamepad-like devices.

// TODO(SaurabhK): Consolidate with CanonicalAxisIndex enum in
// gamepad_standard_mappings.h, crbug.com/351558.
CANONICAL_AXIS_INDEX(AXIS_LEFT_STICK_X, 0)
CANONICAL_AXIS_INDEX(AXIS_LEFT_STICK_Y, 1)
CANONICAL_AXIS_INDEX(AXIS_RIGHT_STICK_X, 2)
CANONICAL_AXIS_INDEX(AXIS_RIGHT_STICK_Y, 3)
CANONICAL_AXIS_INDEX(NUM_CANONICAL_AXES, 4)
