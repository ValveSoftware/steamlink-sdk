// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_CONTEXT_H_
#define MEDIA_BASE_CDM_CONTEXT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

class Decryptor;

// An interface representing the context that a media player needs from a
// content decryption module (CDM) to decrypt (and decode) encrypted buffers.
// This is used to pass the CDM to the media player (e.g. SetCdm()).
class MEDIA_EXPORT CdmContext {
 public:
  // Indicates an invalid CDM ID. See GetCdmId() for details.
  static const int kInvalidCdmId;

  virtual ~CdmContext();

  // Gets the Decryptor object associated with the CDM. Returns nullptr if the
  // CDM does not support a Decryptor (i.e. platform-based CDMs where decryption
  // occurs implicitly along with decoding). The returned object is only
  // guaranteed to be valid during the CDM's lifetime.
  virtual Decryptor* GetDecryptor() = 0;

  // Returns an ID that can be used to find a remote CDM, in which case this CDM
  // serves as a proxy to the remote one. Returns kInvalidCdmId when remote CDM
  // is not supported (e.g. this CDM is a local CDM).
  virtual int GetCdmId() const = 0;

  // Returns a unique class identifier. Some subclasses override and use this
  // method to provide safe down-casting to their type.
  virtual void* GetClassIdentifier() const;

 protected:
  CdmContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmContext);
};

// Callback to notify that the CdmContext has been completely attached to
// the media pipeline. Parameter indicates whether the operation succeeded.
typedef base::Callback<void(bool)> CdmAttachedCB;

// A dummy implementation of CdmAttachedCB.
MEDIA_EXPORT void IgnoreCdmAttached(bool success);

}  // namespace media

#endif  // MEDIA_BASE_CDM_CONTEXT_H_
