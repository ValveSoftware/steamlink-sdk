// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_initializer.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/ime_bridge.h"
#elif defined(USE_AURA) && defined(OS_LINUX)
#include "base/logging.h"
#include "ui/base/ime/linux/fake_input_method_context_factory.h"
#endif

namespace {

#if !defined(OS_CHROMEOS) && defined(USE_AURA) && defined(OS_LINUX)
const ui::LinuxInputMethodContextFactory* g_linux_input_method_context_factory;
#endif

}  // namespace

namespace ui {

void InitializeInputMethod() {
#if defined(OS_CHROMEOS)
  chromeos::IMEBridge::Initialize();
#endif
}

void ShutdownInputMethod() {
#if defined(OS_CHROMEOS)
  chromeos::IMEBridge::Shutdown();
#endif
}

void InitializeInputMethodForTesting() {
#if defined(OS_CHROMEOS)
  chromeos::IMEBridge::Initialize();
#elif defined(USE_AURA) && defined(OS_LINUX)
  if (!g_linux_input_method_context_factory)
    g_linux_input_method_context_factory = new FakeInputMethodContextFactory();
  const LinuxInputMethodContextFactory* factory =
      LinuxInputMethodContextFactory::instance();
  CHECK(!factory || factory == g_linux_input_method_context_factory)
      << "LinuxInputMethodContextFactory was already initialized somewhere "
      << "else.";
  LinuxInputMethodContextFactory::SetInstance(
      g_linux_input_method_context_factory);
#endif
}

void ShutdownInputMethodForTesting() {
#if defined(OS_CHROMEOS)
  chromeos::IMEBridge::Shutdown();
#elif defined(USE_AURA) && defined(OS_LINUX)
  const LinuxInputMethodContextFactory* factory =
      LinuxInputMethodContextFactory::instance();
  CHECK(!factory || factory == g_linux_input_method_context_factory)
      << "An unknown LinuxInputMethodContextFactory was set.";
  LinuxInputMethodContextFactory::SetInstance(NULL);
  delete g_linux_input_method_context_factory;
  g_linux_input_method_context_factory = NULL;
#endif
}

}  // namespace ui
