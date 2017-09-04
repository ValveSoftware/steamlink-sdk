// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_IFDE_TXTEDTDORECORD_H_
#define XFA_FDE_IFDE_TXTEDTDORECORD_H_

#include "core/fxcrt/fx_system.h"

class IFDE_TxtEdtDoRecord {
 public:
  virtual ~IFDE_TxtEdtDoRecord() {}

  virtual bool Redo() const = 0;
  virtual bool Undo() const = 0;
};

#endif  // XFA_FDE_IFDE_TXTEDTDORECORD_H_
