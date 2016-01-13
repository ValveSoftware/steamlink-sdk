// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards because this file
// is meant to be included inside a macro to generate enum values.

// This file contains a list of sync PopupItemTypes which describe the type
// and enabled state of a select popup item. List is used by Android when
// displaying a new select popup menu.

#ifndef DEFINE_POPUP_ITEM_TYPE
#error "Please define DEFINE_POPUP_ITEM_TYPE before including this file."
#endif

// Popup item is of type group
DEFINE_POPUP_ITEM_TYPE(GROUP, 0)

// Popup item is disabled
DEFINE_POPUP_ITEM_TYPE(DISABLED, 1)

// Popup item is enabled
DEFINE_POPUP_ITEM_TYPE(ENABLED, 2)
