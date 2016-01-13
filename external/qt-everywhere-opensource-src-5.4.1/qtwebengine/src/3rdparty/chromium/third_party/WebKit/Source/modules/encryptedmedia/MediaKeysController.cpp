// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/MediaKeysController.h"

#include "modules/encryptedmedia/MediaKeysClient.h"
#include "public/platform/WebContentDecryptionModule.h"

namespace WebCore {

const char* MediaKeysController::supplementName()
{
    return "MediaKeysController";
}

MediaKeysController::MediaKeysController(MediaKeysClient* client)
    : m_client(client)
{
}

PassOwnPtr<blink::WebContentDecryptionModule> MediaKeysController::createContentDecryptionModule(ExecutionContext* context, const String& keySystem)
{
    return m_client->createContentDecryptionModule(context, keySystem);
}


void MediaKeysController::provideMediaKeysTo(Page& page, MediaKeysClient* client)
{
    MediaKeysController::provideTo(page, supplementName(), adoptPtrWillBeNoop(new MediaKeysController(client)));
}

} // namespace WebCore

