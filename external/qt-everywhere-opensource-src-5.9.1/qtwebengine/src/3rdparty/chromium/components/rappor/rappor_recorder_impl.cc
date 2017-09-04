// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_recorder_impl.h"

#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace rappor {

RapporRecorderImpl::RapporRecorderImpl(RapporService* rappor_service)
    : rappor_service_(rappor_service) {}

RapporRecorderImpl::~RapporRecorderImpl() = default;

// static
void RapporRecorderImpl::Create(RapporService* rappor_service,
                                mojom::RapporRecorderRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<RapporRecorderImpl>(rappor_service),
                          std::move(request));
}

void RapporRecorderImpl::RecordRappor(const std::string& metric,
                                      const std::string& sample) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SampleString(rappor_service_, metric, ETLD_PLUS_ONE_RAPPOR_TYPE, sample);
}

void RapporRecorderImpl::RecordRapporURL(const std::string& metric,
                                         const GURL& sample) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SampleDomainAndRegistryFromGURL(rappor_service_, metric, sample);
}

}  // namespace rappor
