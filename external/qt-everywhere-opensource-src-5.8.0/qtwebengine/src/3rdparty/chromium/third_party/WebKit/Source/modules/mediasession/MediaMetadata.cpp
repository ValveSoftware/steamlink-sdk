// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadata.h"

#include "core/dom/ExecutionContext.h"
#include "modules/mediasession/MediaArtwork.h"
#include "modules/mediasession/MediaMetadataInit.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::create(ExecutionContext* context, const MediaMetadataInit& metadata)
{
    return new MediaMetadata(context, metadata);
}

MediaMetadata::MediaMetadata(ExecutionContext* context, const MediaMetadataInit& metadata)
{
    m_title = metadata.title();
    m_artist = metadata.artist();
    m_album = metadata.album();
    for (const auto &artwork : metadata.artwork())
        m_artwork.append(MediaArtwork::create(context, artwork));
}

String MediaMetadata::title() const
{
    return m_title;
}

String MediaMetadata::artist() const
{
    return m_artist;
}

String MediaMetadata::album() const
{
    return m_album;
}

const HeapVector<Member<MediaArtwork>>& MediaMetadata::artwork() const
{
    return m_artwork;
}

MediaMetadata::operator WebMediaMetadata() const
{
    WebMediaMetadata webMetadata;
    webMetadata.title = m_title;
    webMetadata.artist = m_artist;
    webMetadata.album = m_album;
    WebVector<WebMediaArtwork> webArtwork(m_artwork.size());
    for (size_t i = 0; i < m_artwork.size(); ++i) {
        webArtwork[i] = *m_artwork[i]->data();
    }
    webMetadata.artwork.swap(webArtwork);
    return webMetadata;
}

DEFINE_TRACE(MediaMetadata)
{
    visitor->trace(m_artwork);
}

} // namespace blink
