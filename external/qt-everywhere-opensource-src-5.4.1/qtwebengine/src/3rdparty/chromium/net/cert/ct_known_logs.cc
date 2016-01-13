// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_known_logs.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/cert/ct_log_verifier.h"

namespace net {

namespace ct {

namespace {

// Oldest log - the "pilot" log.
const char kGooglePilotLogKey[] =
    "\x30\x59\x30\x13\x06\x07\x2a\x86\x48\xce\x3d\x02\x01\x06\x08\x2a\x86\x48"
    "\xce\x3d\x03\x01\x07\x03\x42\x00\x04\x7d\xa8\x4b\x12\x29\x80\xa3\x3d\xad"
    "\xd3\x5a\x77\xb8\xcc\xe2\x88\xb3\xa5\xfd\xf1\xd3\x0c\xcd\x18\x0c\xe8\x41"
    "\x46\xe8\x81\x01\x1b\x15\xe1\x4b\xf1\x1b\x62\xdd\x36\x0a\x08\x18\xba\xed"
    "\x0b\x35\x84\xd0\x9e\x40\x3c\x2d\x9e\x9b\x82\x65\xbd\x1f\x04\x10\x41\x4c"
    "\xa0";

const size_t kGooglePilotLogKeyLength = arraysize(kGooglePilotLogKey) - 1;

const char kGooglePilotLogName[] = "Google US1 CT";

// Newer log - the "aviator" log.
const char kGoogleAviatorLogKey[] =
    "\x30\x59\x30\x13\x06\x07\x2a\x86\x48\xce\x3d\x02\x01\x06\x08\x2a\x86\x48"
    "\xce\x3d\x03\x01\x07\x03\x42\x00\x04\xd7\xf4\xcc\x69\xb2\xe4\x0e\x90\xa3"
    "\x8a\xea\x5a\x70\x09\x4f\xef\x13\x62\xd0\x8d\x49\x60\xff\x1b\x40\x50\x07"
    "\x0c\x6d\x71\x86\xda\x25\x49\x8d\x65\xe1\x08\x0d\x47\x34\x6b\xbd\x27\xbc"
    "\x96\x21\x3e\x34\xf5\x87\x76\x31\xb1\x7f\x1d\xc9\x85\x3b\x0d\xf7\x1f\x3f"
    "\xe9";

const size_t kGoogleAviatorLogKeyLength = arraysize(kGoogleAviatorLogKey) - 1;

const char kGoogleAviatorLogName[] = "Google US2 CT";

// Latest log, not turned up yet, nicknamed "rocketeer"
const char kGoogleRocketeerLogKey[] =
    "\x30\x59\x30\x13\x06\x07\x2a\x86\x48\xce\x3d\x02\x01\x06\x08\x2a\x86\x48"
    "\xce\x3d\x03\x01\x07\x03\x42\x00\x04\xf3\x58\x9d\x31\x6e\x2f\xc8\x98\x46"
    "\x2b\x92\x08\x1f\x46\x98\x80\x55\xa9\x0d\x02\xe1\x39\xba\x9a\x90\xcf\x8b"
    "\xe0\x8a\x7e\x06\x72\xd6\x53\x48\xb3\x4a\xc3\x4d\x2f\x52\xa6\x21\xfc\xcc"
    "\x33\xcb\x92\x2b\x57\x95\x76\xf2\x07\xcd\x37\x56\x83\xbb\xfa\xea\xb6\xc4"
    "\xd8";

const size_t kGoogleRocketeerLogKeyLength =
    arraysize(kGoogleRocketeerLogKey) - 1;

const char kGoogleRocketeerLogName[] = "Google EU CT";

}  // namespace

scoped_ptr<CTLogVerifier> CreateGooglePilotLogVerifier() {
  base::StringPiece key(kGooglePilotLogKey, kGooglePilotLogKeyLength);

  return CTLogVerifier::Create(key, kGooglePilotLogName);
}

scoped_ptr<CTLogVerifier> CreateGoogleAviatorLogVerifier() {
  base::StringPiece key(kGoogleAviatorLogKey, kGoogleAviatorLogKeyLength);

  return CTLogVerifier::Create(key, kGoogleAviatorLogName);
}

scoped_ptr<CTLogVerifier> CreateGoogleRocketeerLogVerifier() {
  base::StringPiece key(kGoogleRocketeerLogKey, kGoogleRocketeerLogKeyLength);

  return CTLogVerifier::Create(key, kGoogleRocketeerLogName);
}

}  // namespace ct

}  // namespace net

