// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_SIGNED_CERTIFICATE_TIMESTAMP_LOG_PARAM_H_
#define NET_CERT_CT_SIGNED_CERTIFICATE_TIMESTAMP_LOG_PARAM_H_

#include "net/base/net_log.h"

namespace net {

namespace ct {
struct CTVerifyResult;
}

// Creates a dictionary of processed Signed Certificate Timestamps to be
// logged in the NetLog.
// See the documentation for SIGNED_CERTIFICATE_TIMESTAMPS_CHECKED
// in net/base/net_log_event_type_list.h
base::Value* NetLogSignedCertificateTimestampCallback(
    const ct::CTVerifyResult* ct_result, NetLog::LogLevel log_level);

// Creates a dictionary of raw Signed Certificate Timestamps to be logged
// in the NetLog.
// See the documentation for SIGNED_CERTIFICATE_TIMESTAMPS_RECEIVED
// in net/base/net_log_event_type_list.h
base::Value* NetLogRawSignedCertificateTimestampCallback(
    const std::string* embedded_scts,
    const std::string* sct_list_from_ocsp,
    const std::string* sct_list_from_tls_extension,
    NetLog::LogLevel log_level);

}  // namespace net

#endif  // NET_CERT_CT_SIGNED_CERTIFICATE_TIMESTAMP_LOG_PARAM_H_
