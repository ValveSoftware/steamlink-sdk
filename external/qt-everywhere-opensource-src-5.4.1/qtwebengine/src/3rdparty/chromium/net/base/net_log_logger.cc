// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_logger.h"

#include <stdio.h>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/load_states.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"

namespace net {

// This should be incremented when significant changes are made that will
// invalidate the old loading code.
static const int kLogFormatVersion = 1;

NetLogLogger::NetLogLogger(FILE* file, const base::Value& constants)
    : file_(file), log_level_(NetLog::LOG_ALL_BUT_BYTES), added_events_(false) {
  DCHECK(file);

  // Write constants to the output file.  This allows loading files that have
  // different source and event types, as they may be added and removed
  // between Chrome versions.
  std::string json;
  base::JSONWriter::Write(&constants, &json);
  fprintf(file_.get(), "{\"constants\": %s,\n", json.c_str());
  fprintf(file_.get(), "\"events\": [\n");
}

NetLogLogger::~NetLogLogger() {
  if (file_.get())
    fprintf(file_.get(), "]}");
}

void NetLogLogger::set_log_level(net::NetLog::LogLevel log_level) {
  DCHECK(!net_log());
  log_level_ = log_level;
}

void NetLogLogger::StartObserving(net::NetLog* net_log) {
  net_log->AddThreadSafeObserver(this, log_level_);
}

void NetLogLogger::StopObserving() {
  net_log()->RemoveThreadSafeObserver(this);
}

void NetLogLogger::OnAddEntry(const net::NetLog::Entry& entry) {
  // Add a comma and newline for every event but the first.  Newlines are needed
  // so can load partial log files by just ignoring the last line.  For this to
  // work, lines cannot be pretty printed.
  scoped_ptr<base::Value> value(entry.ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), &json);
  fprintf(file_.get(), "%s%s",
          (added_events_ ? ",\n" : ""),
          json.c_str());
  added_events_ = true;
}

base::DictionaryValue* NetLogLogger::GetConstants() {
  base::DictionaryValue* constants_dict = new base::DictionaryValue();

  // Version of the file format.
  constants_dict->SetInteger("logFormatVersion", kLogFormatVersion);

  // Add a dictionary with information on the relationship between event type
  // enums and their symbolic names.
  constants_dict->Set("logEventTypes", net::NetLog::GetEventTypesAsValue());

  // Add a dictionary with information about the relationship between CertStatus
  // flags and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

#define CERT_STATUS_FLAG(label, value) dict->SetInteger(#label, value);
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG

    constants_dict->Set("certStatusFlag", dict);
  }

  // Add a dictionary with information about the relationship between load flag
  // enums and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

#define LOAD_FLAG(label, value) \
    dict->SetInteger(# label, static_cast<int>(value));
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG

    constants_dict->Set("loadFlag", dict);
  }

  // Add a dictionary with information about the relationship between load state
  // enums and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

#define LOAD_STATE(label) \
    dict->SetInteger(# label, net::LOAD_STATE_ ## label);
#include "net/base/load_states_list.h"
#undef LOAD_STATE

    constants_dict->Set("loadState", dict);
  }

  // Add information on the relationship between net error codes and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

#define NET_ERROR(label, value) \
    dict->SetInteger(# label, static_cast<int>(value));
#include "net/base/net_error_list.h"
#undef NET_ERROR

    constants_dict->Set("netError", dict);
  }

  // Add information on the relationship between QUIC error codes and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (net::QuicErrorCode error = net::QUIC_NO_ERROR;
         error < net::QUIC_LAST_ERROR;
         error = static_cast<net::QuicErrorCode>(error + 1)) {
      dict->SetInteger(net::QuicUtils::ErrorToString(error),
                       static_cast<int>(error));
    }

    constants_dict->Set("quicError", dict);
  }

  // Add information on the relationship between QUIC RST_STREAM error codes
  // and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (net::QuicRstStreamErrorCode error = net::QUIC_STREAM_NO_ERROR;
         error < net::QUIC_STREAM_LAST_ERROR;
         error = static_cast<net::QuicRstStreamErrorCode>(error + 1)) {
      dict->SetInteger(net::QuicUtils::StreamErrorToString(error),
                       static_cast<int>(error));
    }

    constants_dict->Set("quicRstStreamError", dict);
  }

  // Information about the relationship between event phase enums and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    dict->SetInteger("PHASE_BEGIN", net::NetLog::PHASE_BEGIN);
    dict->SetInteger("PHASE_END", net::NetLog::PHASE_END);
    dict->SetInteger("PHASE_NONE", net::NetLog::PHASE_NONE);

    constants_dict->Set("logEventPhase", dict);
  }

  // Information about the relationship between source type enums and
  // their symbolic names.
  constants_dict->Set("logSourceType", net::NetLog::GetSourceTypesAsValue());

  // Information about the relationship between LogLevel enums and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    dict->SetInteger("LOG_ALL", net::NetLog::LOG_ALL);
    dict->SetInteger("LOG_ALL_BUT_BYTES", net::NetLog::LOG_ALL_BUT_BYTES);
    dict->SetInteger("LOG_STRIP_PRIVATE_DATA",
                     net::NetLog::LOG_STRIP_PRIVATE_DATA);

    constants_dict->Set("logLevelType", dict);
  }

  // Information about the relationship between address family enums and
  // their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    dict->SetInteger("ADDRESS_FAMILY_UNSPECIFIED",
                     net::ADDRESS_FAMILY_UNSPECIFIED);
    dict->SetInteger("ADDRESS_FAMILY_IPV4",
                     net::ADDRESS_FAMILY_IPV4);
    dict->SetInteger("ADDRESS_FAMILY_IPV6",
                     net::ADDRESS_FAMILY_IPV6);

    constants_dict->Set("addressFamily", dict);
  }

  // Information about how the "time ticks" values we have given it relate to
  // actual system times. (We used time ticks throughout since they are stable
  // across system clock changes).
  {
    int64 cur_time_ms = (base::Time::Now() - base::Time()).InMilliseconds();

    int64 cur_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();

    // If we add this number to a time tick value, it gives the timestamp.
    int64 tick_to_time_ms = cur_time_ms - cur_time_ticks_ms;

    // Chrome on all platforms stores times using the Windows epoch
    // (Jan 1 1601), but the javascript wants a unix epoch.
    // TODO(eroman): Getting the timestamp relative to the unix epoch should
    //               be part of the time library.
    const int64 kUnixEpochMs = 11644473600000LL;
    int64 tick_to_unix_time_ms = tick_to_time_ms - kUnixEpochMs;

    // Pass it as a string, since it may be too large to fit in an integer.
    constants_dict->SetString("timeTickOffset",
                              base::Int64ToString(tick_to_unix_time_ms));
  }

  // "clientInfo" key is required for some NetLogLogger log readers.
  // Provide a default empty value for compatibility.
  constants_dict->Set("clientInfo", new base::DictionaryValue());

  return constants_dict;
}

}  // namespace net
