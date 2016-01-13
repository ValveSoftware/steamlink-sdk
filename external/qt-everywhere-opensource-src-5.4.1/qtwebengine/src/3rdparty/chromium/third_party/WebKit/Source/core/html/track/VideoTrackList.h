// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoTrackList_h
#define VideoTrackList_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/track/TrackListBase.h"
#include "core/html/track/VideoTrack.h"

namespace WebCore {

class VideoTrackList FINAL : public TrackListBase<VideoTrack>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<VideoTrackList> create(HTMLMediaElement&);

    virtual ~VideoTrackList();

    int selectedIndex() const;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;

    void trackSelected(blink::WebMediaPlayer::TrackId selectedTrackId);

private:
    explicit VideoTrackList(HTMLMediaElement&);
};

}

#endif
