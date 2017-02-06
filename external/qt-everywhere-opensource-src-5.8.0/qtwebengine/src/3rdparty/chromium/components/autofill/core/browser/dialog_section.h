// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DIALOG_SECTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DIALOG_SECTION_H_

namespace autofill {

// Sections of the dialog --- all fields that may be shown to the user fit under
// one of these sections.
enum DialogSection {
  // Lower boundary value for looping over all sections.
  SECTION_MIN,

  // The Autofill-backed dialog uses separate CC and billing sections.
  SECTION_CC = SECTION_MIN,
  SECTION_BILLING,
  SECTION_SHIPPING,

  // Upper boundary value for looping over all sections.
  SECTION_MAX = SECTION_SHIPPING,
};

}  // namespace autofill

#endif // COMPONENTS_AUTOFILL_CORE_BROWSER_DIALOG_SECTION_H_
