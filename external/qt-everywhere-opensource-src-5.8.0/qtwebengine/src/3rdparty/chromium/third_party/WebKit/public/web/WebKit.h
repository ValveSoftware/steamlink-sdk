/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebKit_h
#define WebKit_h

#include "../platform/Platform.h"
#include <v8.h>

namespace blink {

// Initialize the entire Blink (wtf, platform, core, modules and web).
// If you just need wtf and platform, use Platform::initialize instead.
//
// Must be called on the thread that will be the main thread before
// using any other public APIs. The provided Platform; must be
// non-null and must remain valid until the current thread calls shutdown.
BLINK_EXPORT void initialize(Platform*);

// Get the V8 Isolate for the main thread.
// initialize must have been called first.
BLINK_EXPORT v8::Isolate* mainThreadIsolate();

// Once shutdown, the Platform passed to initialize will no longer
// be accessed. No other WebKit objects should be in use when this function is
// called. Any background threads created by WebKit are promised to be
// terminated by the time this function returns.
BLINK_EXPORT void shutdown();

// Alters the rendering of content to conform to a fixed set of rules.
BLINK_EXPORT void setLayoutTestMode(bool);
BLINK_EXPORT bool layoutTestMode();

// Enables or disables the use of the mock theme for layout tests. This function
// must be called only if setLayoutTestMode(true).
BLINK_EXPORT void setMockThemeEnabledForTest(bool);

// Alters the rendering of fonts for layout tests.
BLINK_EXPORT void setFontAntialiasingEnabledForTest(bool);
BLINK_EXPORT bool fontAntialiasingEnabledForTest();

// Forces the use of the complex text path for layout tests.
BLINK_EXPORT void setAlwaysUseComplexTextForTest(bool);
BLINK_EXPORT bool alwaysUseComplexTextForTest();

// Enables the named log channel. See WebCore/platform/Logging.h for details.
BLINK_EXPORT void enableLogChannel(const char*);

// Purge the plugin list cache. If |reloadPages| is true, any pages
// containing plugins will be reloaded after refreshing the plugin list.
BLINK_EXPORT void resetPluginCache(bool reloadPages = false);

// The embedder should call this periodically in an attempt to balance overall
// performance and memory usage.
BLINK_EXPORT void decommitFreeableMemory();

// Send memory pressure notification to worker thread isolate.
BLINK_EXPORT void MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel);

// Set the RAIL performance mode on all worker thread isolates.
BLINK_EXPORT void setRAILModeOnWorkerThreadIsolates(v8::RAILMode);

} // namespace blink

#endif
