// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_query_params.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns the amount of physical memory in GB, rounded to the nearest GB.
int GetPhysicalMemoryGB() {
  const double kOneGB = 1024 * 1024 * 1024;
  const int64_t phys_mem = base::SysInfo::AmountOfPhysicalMemory();
  return static_cast<int>(std::floor(0.5 + phys_mem / kOneGB));
}

// Produces an extension-like friendly id.
std::string HexStringToID(const std::string& hexstr) {
  std::string id;
  for (size_t i = 0; i < hexstr.size(); ++i) {
    int val = 0;
    if (base::HexStringToInt(
            base::StringPiece(hexstr.begin() + i, hexstr.begin() + i + 1),
            &val)) {
      id.append(1, val + 'a');
    } else {
      id.append(1, 'a');
    }
  }

  DCHECK(crx_file::id_util::IdIsValid(id));

  return id;
}

}  // namespace

std::string BuildProtocolRequest(const std::string& browser_version,
                                 const std::string& channel,
                                 const std::string& lang,
                                 const std::string& os_long_name,
                                 const std::string& download_preference,
                                 const std::string& request_body,
                                 const std::string& additional_attributes) {
  const std::string prod_id(
      UpdateQueryParams::GetProdIdString(UpdateQueryParams::CHROME));

  std::string request(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<request protocol=\"3.0\" ");

  if (!additional_attributes.empty())
    base::StringAppendF(&request, "%s ", additional_attributes.c_str());

  // Chrome version and platform information.
  base::StringAppendF(
      &request,
      "version=\"%s-%s\" prodversion=\"%s\" "
      "requestid=\"{%s}\" lang=\"%s\" updaterchannel=\"%s\" prodchannel=\"%s\" "
      "os=\"%s\" arch=\"%s\" nacl_arch=\"%s\"",
      prod_id.c_str(),
      browser_version.c_str(),            // "version"
      browser_version.c_str(),            // "prodversion"
      base::GenerateGUID().c_str(),       // "requestid"
      lang.c_str(),                       // "lang",
      channel.c_str(),                    // "updaterchannel"
      channel.c_str(),                    // "prodchannel"
      UpdateQueryParams::GetOS(),         // "os"
      UpdateQueryParams::GetArch(),       // "arch"
      UpdateQueryParams::GetNaclArch());  // "nacl_arch"
#if defined(OS_WIN)
  const bool is_wow64(base::win::OSInfo::GetInstance()->wow64_status() ==
                      base::win::OSInfo::WOW64_ENABLED);
  if (is_wow64)
    base::StringAppendF(&request, " wow64=\"1\"");
#endif
  if (!download_preference.empty())
    base::StringAppendF(&request, " dlpref=\"%s\"",
                        download_preference.c_str());
  base::StringAppendF(&request, ">");

  // HW platform information.
  base::StringAppendF(&request, "<hw physmemory=\"%d\"/>",
                      GetPhysicalMemoryGB());  // "physmem" in GB.

  // OS version and platform information.
  base::StringAppendF(
      &request, "<os platform=\"%s\" version=\"%s\" arch=\"%s\"/>",
      os_long_name.c_str(),                                    // "platform"
      base::SysInfo().OperatingSystemVersion().c_str(),        // "version"
      base::SysInfo().OperatingSystemArchitecture().c_str());  // "arch"

  // The actual payload of the request.
  base::StringAppendF(&request, "%s</request>", request_body.c_str());

  return request;
}

std::unique_ptr<net::URLFetcher> SendProtocolRequest(
    const GURL& url,
    const std::string& protocol_request,
    net::URLFetcherDelegate* url_fetcher_delegate,
    net::URLRequestContextGetter* url_request_context_getter) {
  std::unique_ptr<net::URLFetcher> url_fetcher = net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, url_fetcher_delegate);
  if (!url_fetcher.get())
    return url_fetcher;

  url_fetcher->SetUploadData("application/xml", protocol_request);
  url_fetcher->SetRequestContext(url_request_context_getter);
  url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SAVE_COOKIES |
                            net::LOAD_DISABLE_CACHE);
  url_fetcher->SetAutomaticallyRetryOn5xx(false);
  url_fetcher->Start();

  return url_fetcher;
}

bool FetchSuccess(const net::URLFetcher& fetcher) {
  return GetFetchError(fetcher) == 0;
}

int GetFetchError(const net::URLFetcher& fetcher) {
  const net::URLRequestStatus::Status status(fetcher.GetStatus().status());
  switch (status) {
    case net::URLRequestStatus::IO_PENDING:
    case net::URLRequestStatus::CANCELED:
      // Network status is a small positive number.
      return status;

    case net::URLRequestStatus::SUCCESS: {
      // Response codes are positive numbers, greater than 100.
      const int response_code(fetcher.GetResponseCode());
      if (response_code == 200)
        return 0;
      else
        return response_code ? response_code : -1;
    }

    case net::URLRequestStatus::FAILED: {
      // Network errors are small negative numbers.
      const int error = fetcher.GetStatus().error();
      return error ? error : -1;
    }

    default:
      return -1;
  }
}

bool HasDiffUpdate(const CrxUpdateItem* update_item) {
  return !update_item->crx_diffurls.empty();
}

bool IsHttpServerError(int status_code) {
  return 500 <= status_code && status_code < 600;
}

bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath) {
  if (!base::DeleteFile(filepath, false))
    return false;

  const base::FilePath dirname(filepath.DirName());
  if (!base::IsDirectoryEmpty(dirname))
    return true;

  return base::DeleteFile(dirname, false);
}

std::string GetCrxComponentID(const CrxComponent& component) {
  const size_t kCrxIdSize = 16;
  CHECK_GE(component.pk_hash.size(), kCrxIdSize);
  return HexStringToID(base::ToLowerASCII(
      base::HexEncode(&component.pk_hash[0], kCrxIdSize)));
}

bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::vector<uint8_t> expected_hash;
  if (!base::HexStringToBytes(expected_hash_str, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length) {
    return false;
  }

  base::MemoryMappedFile mmfile;
  if (!mmfile.Initialize(filepath))
    return false;

  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hasher->Update(mmfile.data(), mmfile.length());
  hasher->Finish(actual_hash, sizeof(actual_hash));

  return memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)) == 0;
}

bool IsValidBrand(const std::string& brand) {
  const size_t kMaxBrandSize = 4;
  if (!brand.empty() && brand.size() != kMaxBrandSize)
    return false;

  return std::find_if_not(brand.begin(), brand.end(), [](char ch) {
           return base::IsAsciiAlpha(ch);
         }) == brand.end();
}

// Helper function.
// Returns true if |part| matches the expression
// ^[<special_chars>a-zA-Z0-9]{min_length,max_length}$
bool IsValidInstallerAttributePart(const std::string& part,
                                   const std::string& special_chars,
                                   size_t min_length,
                                   size_t max_length) {
  if (part.size() < min_length || part.size() > max_length)
    return false;

  return std::find_if_not(part.begin(), part.end(), [&special_chars](char ch) {
           if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch))
             return true;

           for (auto c : special_chars) {
             if (c == ch)
               return true;
           }

           return false;
         }) == part.end();
}

// Returns true if the |name| parameter matches ^[-_a-zA-Z0-9]{1,256}$ .
bool IsValidInstallerAttributeName(const std::string& name) {
  return IsValidInstallerAttributePart(name, "-_", 1, 256);
}

// Returns true if the |value| parameter matches ^[-.,;+_=a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttributeValue(const std::string& value) {
  return IsValidInstallerAttributePart(value, "-.,;+_=", 0, 256);
}

bool IsValidInstallerAttribute(const InstallerAttribute& attr) {
  return IsValidInstallerAttributeName(attr.first) &&
         IsValidInstallerAttributeValue(attr.second);
}

void RemoveUnsecureUrls(std::vector<GURL>* urls) {
  DCHECK(urls);
  urls->erase(std::remove_if(
                  urls->begin(), urls->end(),
                  [](const GURL& url) { return !url.SchemeIsCryptographic(); }),
              urls->end());
}

}  // namespace update_client
