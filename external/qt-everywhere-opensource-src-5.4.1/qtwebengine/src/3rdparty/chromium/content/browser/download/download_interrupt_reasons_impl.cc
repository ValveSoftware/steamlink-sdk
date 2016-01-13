// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_interrupt_reasons_impl.h"

#include "base/logging.h"

namespace content {

DownloadInterruptReason ConvertNetErrorToInterruptReason(
    net::Error net_error, DownloadInterruptSource source) {
  switch (net_error) {
    case net::OK:
      return DOWNLOAD_INTERRUPT_REASON_NONE;

    // File errors.

    // The file is too large.
    case net::ERR_FILE_TOO_BIG:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE;

    // Permission to access a resource, other than the network, was denied.
    case net::ERR_ACCESS_DENIED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // There were not enough resources to complete the operation.
    case net::ERR_INSUFFICIENT_RESOURCES:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    // Memory allocation failed.
    case net::ERR_OUT_OF_MEMORY:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    // The path or file name is too long.
    case net::ERR_FILE_PATH_TOO_LONG:
      return DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;

    // Not enough room left on the disk.
    case net::ERR_FILE_NO_SPACE:
      return DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE;

    // The file has a virus.
    case net::ERR_FILE_VIRUS_INFECTED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED;

    // The file was blocked by local policy.
    case net::ERR_BLOCKED_BY_CLIENT:
      return DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED;

    // Network errors.

    // The network operation timed out.
    case net::ERR_TIMED_OUT:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT;

    // The network connection has been lost.
    case net::ERR_INTERNET_DISCONNECTED:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED;

    // The server has gone down.
    case net::ERR_CONNECTION_FAILED:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN;

    // Server responses.

    // The server does not support range requests.
    case net::ERR_REQUEST_RANGE_NOT_SATISFIABLE:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;

    default: break;
  }

  // Handle errors that don't have mappings, depending on the source.
  switch (source) {
    case DOWNLOAD_INTERRUPT_FROM_DISK:
      return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
    case DOWNLOAD_INTERRUPT_FROM_NETWORK:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED;
    case DOWNLOAD_INTERRUPT_FROM_SERVER:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
    default:
      break;
  }

  NOTREACHED();

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

std::string DownloadInterruptReasonToString(DownloadInterruptReason error) {

#define INTERRUPT_REASON(name, value)  \
    case DOWNLOAD_INTERRUPT_REASON_##name: return #name;

  switch (error) {
    INTERRUPT_REASON(NONE, 0)

#include "content/public/browser/download_interrupt_reason_values.h"

    default:
      break;
  }

#undef INTERRUPT_REASON

  return "Unknown error";
}

}  // namespace content
