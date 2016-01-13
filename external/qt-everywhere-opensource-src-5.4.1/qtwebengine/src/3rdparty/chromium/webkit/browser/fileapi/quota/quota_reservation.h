// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_QUOTA_QUOTA_RESERVATION_H_
#define WEBKIT_BROWSER_FILEAPI_QUOTA_QUOTA_RESERVATION_H_

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/quota/quota_reservation_manager.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_types.h"

class GURL;

namespace fileapi {

class QuotaReservationBuffer;
class OpenFileHandle;

// Represents a unit of quota reservation.
class WEBKIT_STORAGE_BROWSER_EXPORT QuotaReservation
    : public base::RefCounted<QuotaReservation> {
 public:
  typedef base::Callback<void(base::File::Error error)> StatusCallback;

  // Reclaims unused quota and reserves another |size| of quota.  So that the
  // resulting new |remaining_quota_| will be same as |size| as far as available
  // space is enough.  |remaining_quota_| may be less than |size| if there is
  // not enough space available.
  // Invokes |callback| upon completion.
  void RefreshReservation(int64 size, const StatusCallback& callback);

  // Associates |platform_path| to the QuotaReservation instance.
  // Returns an OpenFileHandle instance that represents a quota managed file.
  scoped_ptr<OpenFileHandle> GetOpenFileHandle(
      const base::FilePath& platform_path);

  // Should be called when the associated client is crashed.
  // This implies the client can no longer report its consumption of the
  // reserved quota.
  // QuotaReservation puts all remaining quota to the QuotaReservationBuffer, so
  // that the remaining quota will be reclaimed after all open files associated
  // to the origin and type.
  void OnClientCrash();

  // Consumes |size| of reserved quota for a associated file.
  // Consumed quota is sent to associated QuotaReservationBuffer for staging.
  void ConsumeReservation(int64 size);

  // Returns amount of unused reserved quota.
  int64 remaining_quota() const { return remaining_quota_; }

  QuotaReservationManager* reservation_manager();
  const GURL& origin() const;
  FileSystemType type() const;

 private:
  friend class QuotaReservationBuffer;

  // Use QuotaReservationManager as the entry point.
  explicit QuotaReservation(QuotaReservationBuffer* reservation_buffer);

  friend class base::RefCounted<QuotaReservation>;
  virtual ~QuotaReservation();

  static bool AdaptDidUpdateReservedQuota(
      const base::WeakPtr<QuotaReservation>& reservation,
      int64 previous_size,
      const StatusCallback& callback,
      base::File::Error error,
      int64 delta);
  bool DidUpdateReservedQuota(int64 previous_size,
                              const StatusCallback& callback,
                              base::File::Error error,
                              int64 delta);

  bool client_crashed_;
  bool running_refresh_request_;
  int64 remaining_quota_;

  scoped_refptr<QuotaReservationBuffer> reservation_buffer_;

  base::SequenceChecker sequence_checker_;
  base::WeakPtrFactory<QuotaReservation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservation);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_QUOTA_QUOTA_RESERVATION_H_
