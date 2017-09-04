// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/context_provider_factory.h"

#include "cc/output/context_provider.h"

namespace ui {
namespace {

ContextProviderFactory* g_context_provider_factory = nullptr;

}  // namespace

// static
ContextProviderFactory* ContextProviderFactory::GetInstance() {
  return g_context_provider_factory;
}

// static
void ContextProviderFactory::SetInstance(
    ContextProviderFactory* context_provider_factory) {
  DCHECK(!g_context_provider_factory || !context_provider_factory);

  g_context_provider_factory = context_provider_factory;
}

}  // namespace ui
