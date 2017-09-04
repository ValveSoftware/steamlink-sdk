/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioListener_h
#define AudioListener_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/webaudio/AudioParam.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class HRTFDatabaseLoader;
class PannerHandler;

// AudioListener maintains the state of the listener in the audio scene as
// defined in the OpenAL specification.

class AudioListener : public GarbageCollectedFinalized<AudioListener>,
                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioListener* create(BaseAudioContext& context) {
    return new AudioListener(context);
  }
  virtual ~AudioListener();

  // Location of the listener
  AudioParam* positionX() const { return m_positionX; };
  AudioParam* positionY() const { return m_positionY; };
  AudioParam* positionZ() const { return m_positionZ; };

  // Forward direction vector of the listener
  AudioParam* forwardX() const { return m_forwardX; };
  AudioParam* forwardY() const { return m_forwardY; };
  AudioParam* forwardZ() const { return m_forwardZ; };

  // Up direction vector for the listener
  AudioParam* upX() const { return m_upX; };
  AudioParam* upY() const { return m_upY; };
  AudioParam* upZ() const { return m_upZ; };

  // True if any of AudioParams have automations.
  bool hasSampleAccurateValues() const;

  // Update the internal state of the listener, including updating the dirty
  // state of all PannerNodes if necessary.
  void updateState();

  bool isListenerDirty() const { return m_isListenerDirty; }

  const FloatPoint3D position() const {
    return FloatPoint3D(m_positionX->value(), m_positionY->value(),
                        m_positionZ->value());
  }
  const FloatPoint3D orientation() const {
    return FloatPoint3D(m_forwardX->value(), m_forwardY->value(),
                        m_forwardZ->value());
  }
  const FloatPoint3D upVector() const {
    return FloatPoint3D(m_upX->value(), m_upY->value(), m_upZ->value());
  }

  const float* getPositionXValues(size_t framesToProcess);
  const float* getPositionYValues(size_t framesToProcess);
  const float* getPositionZValues(size_t framesToProcess);

  const float* getForwardXValues(size_t framesToProcess);
  const float* getForwardYValues(size_t framesToProcess);
  const float* getForwardZValues(size_t framesToProcess);

  const float* getUpXValues(size_t framesToProcess);
  const float* getUpYValues(size_t framesToProcess);
  const float* getUpZValues(size_t framesToProcess);

  // Position
  void setPosition(float x, float y, float z) {
    setPosition(FloatPoint3D(x, y, z));
  }

  // Orientation and Up-vector
  void setOrientation(float x,
                      float y,
                      float z,
                      float upX,
                      float upY,
                      float upZ) {
    setOrientation(FloatPoint3D(x, y, z));
    setUpVector(FloatPoint3D(upX, upY, upZ));
  }

  Mutex& listenerLock() { return m_listenerLock; }
  void addPanner(PannerHandler&);
  void removePanner(PannerHandler&);

  // HRTF DB loader
  HRTFDatabaseLoader* hrtfDatabaseLoader() {
    return m_hrtfDatabaseLoader.get();
  }
  void createAndLoadHRTFDatabaseLoader(float);
  bool isHRTFDatabaseLoaded();
  void waitForHRTFDatabaseLoaderThreadCompletion();

  DECLARE_TRACE();

 private:
  AudioListener(BaseAudioContext&);

  void setPosition(const FloatPoint3D&);
  void setOrientation(const FloatPoint3D&);
  void setUpVector(const FloatPoint3D&);

  void markPannersAsDirty(unsigned);

  // Location of the listener
  Member<AudioParam> m_positionX;
  Member<AudioParam> m_positionY;
  Member<AudioParam> m_positionZ;

  // Forward direction vector of the listener
  Member<AudioParam> m_forwardX;
  Member<AudioParam> m_forwardY;
  Member<AudioParam> m_forwardZ;

  // Up direction vector for the listener
  Member<AudioParam> m_upX;
  Member<AudioParam> m_upY;
  Member<AudioParam> m_upZ;

  // The position, forward, and up vectors from the last rendering quantum.
  FloatPoint3D m_lastPosition;
  FloatPoint3D m_lastForward;
  FloatPoint3D m_lastUp;

  // Last time that the automations were updated.
  double m_lastUpdateTime;

  // Set every rendering quantum if the listener has moved in any way
  // (position, forward, or up).  This should only be read or written to from
  // the audio thread.
  bool m_isListenerDirty;

  void updateValuesIfNeeded(size_t framesToProcess);

  AudioFloatArray m_positionXValues;
  AudioFloatArray m_positionYValues;
  AudioFloatArray m_positionZValues;

  AudioFloatArray m_forwardXValues;
  AudioFloatArray m_forwardYValues;
  AudioFloatArray m_forwardZValues;

  AudioFloatArray m_upXValues;
  AudioFloatArray m_upYValues;
  AudioFloatArray m_upZValues;

  // Synchronize a panner's process() with setting of the state of the listener.
  mutable Mutex m_listenerLock;
  // List for pannerNodes in context. This is updated only in the main thread,
  // and can be referred in audio thread.
  // These raw pointers are safe because PannerHandler::uninitialize()
  // unregisters it from m_panners.
  HashSet<PannerHandler*> m_panners;
  // HRTF DB loader for panner node.
  RefPtr<HRTFDatabaseLoader> m_hrtfDatabaseLoader;
};

}  // namespace blink

#endif  // AudioListener_h
