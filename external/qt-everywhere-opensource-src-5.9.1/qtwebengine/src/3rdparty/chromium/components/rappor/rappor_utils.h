// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_RAPPOR_UTILS_H_
#define COMPONENTS_RAPPOR_RAPPOR_UTILS_H_

#include <string>

#include "components/rappor/rappor_service.h"

class GURL;

namespace rappor {

class RapporService;

// Records a string to a Rappor metric.
// If |rappor_service| is NULL, this call does nothing.
void SampleString(RapporService* rappor_service,
                  const std::string& metric,
                  RapporType type,
                  const std::string& sample);

// Extract the domain and registry for a sample from a GURL.
// For file:// urls this will just return "file://" and for other special
// schemes like chrome-extension will return the scheme and host.
std::string GetDomainAndRegistrySampleFromGURL(const GURL& gurl);

// Records the domain and registry of a url to a Rappor metric.
// If |rappor_service| is NULL, this call does nothing.
void SampleDomainAndRegistryFromGURL(RapporService* rappor_service,
                                     const std::string& metric,
                                     const GURL& gurl);

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_RAPPOR_UTILS_H_
