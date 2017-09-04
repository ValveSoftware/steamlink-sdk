// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_DELETERS_H_
#define PRINTING_BACKEND_CUPS_DELETERS_H_

#include <cups/cups.h>

namespace printing {

struct HttpDeleter {
 public:
  void operator()(http_t* http) const;
};

struct DestinationDeleter {
 public:
  void operator()(cups_dest_t* dest) const;
};

struct DestInfoDeleter {
 public:
  void operator()(cups_dinfo_t* info) const;
};

struct OptionDeleter {
 public:
  void operator()(cups_option_t* option) const;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_DELETERS_H_
