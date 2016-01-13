// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_UTIL_INTERNAL_H_
#define CRYPTO_NSS_UTIL_INTERNAL_H_

#include <secmodt.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_nss_types.h"

namespace base {
class FilePath;
}

// These functions return a type defined in an NSS header, and so cannot be
// declared in nss_util.h.  Hence, they are declared here.

namespace crypto {

// Returns a reference to the default NSS key slot for storing
// public-key data only (e.g. server certs). Caller must release
// returned reference with PK11_FreeSlot.
CRYPTO_EXPORT PK11SlotInfo* GetPublicNSSKeySlot() WARN_UNUSED_RESULT;

// Returns a reference to the default slot for storing private-key and
// mixed private-key/public-key data.  Returns a hardware (TPM) NSS
// key slot if on ChromeOS and EnableTPMForNSS() has been called
// successfully. Caller must release returned reference with
// PK11_FreeSlot.
CRYPTO_EXPORT PK11SlotInfo* GetPrivateNSSKeySlot() WARN_UNUSED_RESULT;

// A helper class that acquires the SECMOD list read lock while the
// AutoSECMODListReadLock is in scope.
class CRYPTO_EXPORT AutoSECMODListReadLock {
 public:
  AutoSECMODListReadLock();
  ~AutoSECMODListReadLock();

 private:
  SECMODListLock* lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoSECMODListReadLock);
};

#if defined(OS_CHROMEOS)
// Prepare per-user NSS slot mapping. It is safe to call this function multiple
// times. Returns true if the user was added, or false if it already existed.
CRYPTO_EXPORT bool InitializeNSSForChromeOSUser(
    const std::string& email,
    const std::string& username_hash,
    bool is_primary_user,
    const base::FilePath& path) WARN_UNUSED_RESULT;

// Use TPM slot |slot_id| for user.  InitializeNSSForChromeOSUser must have been
// called first.
CRYPTO_EXPORT void InitializeTPMForChromeOSUser(
    const std::string& username_hash,
    CK_SLOT_ID slot_id);

// Use the software slot as the private slot for user.
// InitializeNSSForChromeOSUser must have been called first.
CRYPTO_EXPORT void InitializePrivateSoftwareSlotForChromeOSUser(
    const std::string& username_hash);

// Returns a reference to the public slot for user.
CRYPTO_EXPORT ScopedPK11Slot GetPublicSlotForChromeOSUser(
    const std::string& username_hash) WARN_UNUSED_RESULT;

// Returns the private slot for |username_hash| if it is loaded. If it is not
// loaded and |callback| is non-null, the |callback| will be run once the slot
// is loaded.
CRYPTO_EXPORT ScopedPK11Slot GetPrivateSlotForChromeOSUser(
    const std::string& username_hash,
    const base::Callback<void(ScopedPK11Slot)>& callback) WARN_UNUSED_RESULT;
#endif  // defined(OS_CHROMEOS)

}  // namespace crypto

#endif  // CRYPTO_NSS_UTIL_INTERNAL_H_
