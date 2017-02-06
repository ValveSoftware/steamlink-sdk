// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_OPEN_PDF_IN_READER_PROMPT_CLIENT_H_
#define COMPONENTS_PDF_BROWSER_OPEN_PDF_IN_READER_PROMPT_CLIENT_H_

#include "base/strings/string16.h"

namespace content {
struct LoadCommittedDetails;
}

namespace pdf {

class OpenPDFInReaderPromptClient {
 public:
  virtual ~OpenPDFInReaderPromptClient() {}

  virtual base::string16 GetMessageText() const = 0;

  virtual base::string16 GetAcceptButtonText() const = 0;

  virtual base::string16 GetCancelButtonText() const = 0;

  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const = 0;

  virtual void Accept() = 0;

  virtual void Cancel() = 0;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_OPEN_PDF_IN_READER_PROMPT_CLIENT_H_
