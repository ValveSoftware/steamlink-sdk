// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_connection.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"

namespace printing {

namespace {

const int kTimeoutMs = 3000;

class DestinationEnumerator {
 public:
  DestinationEnumerator() {}

  static int cups_callback(void* user_data, unsigned flags, cups_dest_t* dest) {
    cups_dest_t* copied_dest;
    cupsCopyDest(dest, 0, &copied_dest);
    reinterpret_cast<DestinationEnumerator*>(user_data)->store_dest(
        copied_dest);

    //  keep going
    return 1;
  }

  void store_dest(cups_dest_t* dest) { dests_.emplace_back(dest); }

  // Returns the collected destinations.
  std::vector<std::unique_ptr<cups_dest_t, DestinationDeleter>>& get_dests() {
    return dests_;
  }

 private:
  std::vector<std::unique_ptr<cups_dest_t, DestinationDeleter>> dests_;

  DISALLOW_COPY_AND_ASSIGN(DestinationEnumerator);
};

}  // namespace

CupsConnection::CupsConnection(const GURL& print_server_url,
                               http_encryption_t encryption,
                               bool blocking)
    : print_server_url_(print_server_url),
      cups_encryption_(encryption),
      blocking_(blocking),
      cups_http_(nullptr) {}

CupsConnection::CupsConnection(CupsConnection&& connection)
    : print_server_url_(connection.print_server_url_),
      cups_encryption_(connection.cups_encryption_),
      blocking_(connection.blocking_),
      cups_http_(std::move(connection.cups_http_)) {}

CupsConnection::~CupsConnection() {}

bool CupsConnection::Connect() {
  if (cups_http_)
    return true;  // we're already connected

  std::string host;
  int port;

  if (!print_server_url_.is_empty()) {
    host = print_server_url_.host();
    port = print_server_url_.IntPort();
  } else {
    host = cupsServer();
    port = ippPort();
  }

  cups_http_.reset(httpConnect2(host.c_str(), port, nullptr, AF_UNSPEC,
                                cups_encryption_, blocking_ ? 1 : 0, kTimeoutMs,
                                nullptr));
  return !!cups_http_;
}

std::vector<CupsPrinter> CupsConnection::GetDests() {
  if (!Connect()) {
    LOG(WARNING) << "CUPS connection failed";
    return std::vector<CupsPrinter>();
  }

  DestinationEnumerator enumerator;
  int success =
      cupsEnumDests(CUPS_DEST_FLAGS_NONE, kTimeoutMs,
                    nullptr,               // no cancel signal
                    0,                     // all the printers
                    CUPS_PRINTER_SCANNER,  // except the scanners
                    &DestinationEnumerator::cups_callback, &enumerator);

  if (!success) {
    LOG(WARNING) << "Enumerating printers failed";
    return std::vector<CupsPrinter>();
  }

  auto dests = std::move(enumerator.get_dests());
  std::vector<CupsPrinter> printers;
  for (auto& dest : dests) {
    printers.emplace_back(cups_http_.get(), std::move(dest), nullptr);
  }

  return printers;
}

std::unique_ptr<CupsPrinter> CupsConnection::GetPrinter(
    const std::string& name) {
  if (!Connect())
    return nullptr;

  cups_dest_t* dest = cupsGetNamedDest(cups_http_.get(), name.c_str(), nullptr);
  if (!dest)
    return nullptr;

  cups_dinfo_t* info = cupsCopyDestInfo(cups_http_.get(), dest);
  return base::MakeUnique<CupsPrinter>(
      cups_http_.get(), std::unique_ptr<cups_dest_t, DestinationDeleter>(dest),
      std::unique_ptr<cups_dinfo_t, DestInfoDeleter>(info));
}

std::string CupsConnection::server_name() const {
  return print_server_url_.host();
}

int CupsConnection::last_error() const {
  return cupsLastError();
}

}  // namespace printing
