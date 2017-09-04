// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserver_h
#define ResizeObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class ResizeObserverCallback;
class ResizeObserverController;
class ResizeObservation;

// ResizeObserver represents ResizeObserver javascript api:
// https://github.com/WICG/ResizeObserver/
class CORE_EXPORT ResizeObserver final
    : public GarbageCollectedFinalized<ResizeObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ResizeObserver* create(Document&, ResizeObserverCallback*);

  virtual ~ResizeObserver(){};

  // API methods
  void observe(Element*);
  void unobserve(Element*);
  void disconnect();

  // Returns depth of shallowest observed node, kDepthLimit if none.
  size_t gatherObservations(size_t deeperThan);
  bool skippedObservations() { return m_skippedObservations; }
  void deliverObservations();
  void clearObservations();
  void elementSizeChanged();
  bool hasElementSizeChanged() { return m_elementSizeChanged; }
  DECLARE_TRACE();

 private:
  ResizeObserver(ResizeObserverCallback*, Document&);

  using ObservationList = HeapLinkedHashSet<WeakMember<ResizeObservation>>;

  Member<ResizeObserverCallback> m_callback;
  // List of elements we are observing
  ObservationList m_observations;
  // List of elements that have changes
  HeapVector<Member<ResizeObservation>> m_activeObservations;
  // True if observations were skipped gatherObservations
  bool m_skippedObservations;
  // True if any ResizeObservation reported size change
  bool m_elementSizeChanged;
  WeakMember<ResizeObserverController> m_controller;
};

}  // namespace blink

#endif  // ResizeObserver_h
