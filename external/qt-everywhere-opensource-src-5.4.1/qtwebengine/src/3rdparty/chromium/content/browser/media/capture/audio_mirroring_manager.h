// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioMirroringManager is a singleton object that maintains a set of active
// audio mirroring destinations and auto-connects/disconnects audio streams
// to/from those destinations.  It is meant to be used exclusively on the IO
// BrowserThread.
//
// How it works:
//
//   1. AudioRendererHost gets a CreateStream message from the render process
//      and, among other things, creates an AudioOutputController to control the
//      audio data flow between the render and browser processes.
//   2. At some point, AudioRendererHost receives an "associate with render
//      view" message.  Among other actions, it registers the
//      AudioOutputController with AudioMirroringManager (as a Diverter).
//   3. A user request to mirror all the audio for a single RenderView is made.
//      A MirroringDestination is created, and StartMirroring() is called to
//      begin the mirroring session.  This causes AudioMirroringManager to
//      instruct any matching Diverters to divert their audio data to the
//      MirroringDestination.
//
// #2 and #3 above may occur in any order, as it is the job of
// AudioMirroringManager to realize when the players can be "matched up."

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "media/audio/audio_source_diverter.h"

namespace media {
class AudioOutputStream;
}

namespace content {

class CONTENT_EXPORT AudioMirroringManager {
 public:
  // Interface for diverting audio data to an alternative AudioOutputStream.
  typedef media::AudioSourceDiverter Diverter;

  // Interface to be implemented by audio mirroring destinations.  See comments
  // for StartMirroring() and StopMirroring() below.
  class MirroringDestination {
   public:
    // Create a consumer of audio data in the format specified by |params|, and
    // connect it as an input to mirroring.  When Close() is called on the
    // returned AudioOutputStream, the input is disconnected and the object
    // becomes invalid.
    virtual media::AudioOutputStream* AddInput(
        const media::AudioParameters& params) = 0;

   protected:
    virtual ~MirroringDestination() {}
  };

  AudioMirroringManager();

  virtual ~AudioMirroringManager();

  // Add/Remove a diverter for an audio stream with a known RenderView target
  // (represented by |render_process_id| + |render_view_id|).  Multiple
  // diverters may be added for the same target.  |diverter| must live until
  // after RemoveDiverter() is called.
  //
  // Re-entrancy warning: These methods should not be called by a Diverter
  // during a Start/StopDiverting() invocation.
  virtual void AddDiverter(int render_process_id, int render_view_id,
                           Diverter* diverter);
  virtual void RemoveDiverter(int render_process_id, int render_view_id,
                              Diverter* diverter);

  // Start/stop mirroring all audio output streams associated with a RenderView
  // target (represented by |render_process_id| + |render_view_id|) to
  // |destination|.  |destination| must live until after StopMirroring() is
  // called.
  virtual void StartMirroring(int render_process_id, int render_view_id,
                              MirroringDestination* destination);
  virtual void StopMirroring(int render_process_id, int render_view_id,
                             MirroringDestination* destination);

 private:
  // A mirroring target is a RenderView identified by a
  // <render_process_id, render_view_id> pair.
  typedef std::pair<int, int> Target;

  // Note: Objects in these maps are not owned.
  typedef std::multimap<Target, Diverter*> DiverterMap;
  typedef std::map<Target, MirroringDestination*> SessionMap;

  // Currently-active divertable audio streams.
  DiverterMap diverters_;

  // Currently-active mirroring sessions.
  SessionMap sessions_;

  DISALLOW_COPY_AND_ASSIGN(AudioMirroringManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_
