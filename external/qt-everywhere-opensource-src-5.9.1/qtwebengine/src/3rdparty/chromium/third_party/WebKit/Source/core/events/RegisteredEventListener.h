/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RegisteredEventListener_h
#define RegisteredEventListener_h

#include "core/events/AddEventListenerOptionsResolved.h"
#include "core/events/EventListener.h"
#include "wtf/RefPtr.h"

namespace blink {

class RegisteredEventListener {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  RegisteredEventListener()
      : m_useCapture(false),
        m_passive(false),
        m_once(false),
        m_blockedEventWarningEmitted(false),
        m_passiveForcedForDocumentTarget(false),
        m_passiveSpecified(false) {}

  RegisteredEventListener(EventListener* listener,
                          const AddEventListenerOptionsResolved& options)
      : m_listener(listener),
        m_useCapture(options.capture()),
        m_passive(options.passive()),
        m_once(options.once()),
        m_blockedEventWarningEmitted(false),
        m_passiveForcedForDocumentTarget(
            options.passiveForcedForDocumentTarget()),
        m_passiveSpecified(options.passiveSpecified()) {}

  DEFINE_INLINE_TRACE() { visitor->trace(m_listener); }

  AddEventListenerOptionsResolved options() const {
    AddEventListenerOptionsResolved result;
    result.setCapture(m_useCapture);
    result.setPassive(m_passive);
    result.setPassiveForcedForDocumentTarget(m_passiveForcedForDocumentTarget);
    result.setOnce(m_once);
    result.setPassiveSpecified(m_passiveSpecified);
    return result;
  }

  const EventListener* listener() const { return m_listener; }

  EventListener* listener() { return m_listener; }

  bool passive() const { return m_passive; }

  bool once() const { return m_once; }

  bool capture() const { return m_useCapture; }

  bool blockedEventWarningEmitted() const {
    return m_blockedEventWarningEmitted;
  }

  bool passiveForcedForDocumentTarget() const {
    return m_passiveForcedForDocumentTarget;
  }

  bool passiveSpecified() const { return m_passiveSpecified; }

  void setBlockedEventWarningEmitted() { m_blockedEventWarningEmitted = true; }

  bool matches(const EventListener* listener,
               const EventListenerOptions& options) const {
    // Equality is soley based on the listener and useCapture flags.
    DCHECK(m_listener);
    DCHECK(listener);
    return *m_listener == *listener &&
           static_cast<bool>(m_useCapture) == options.capture();
  }

  bool operator==(const RegisteredEventListener& other) const {
    // Equality is soley based on the listener and useCapture flags.
    DCHECK(m_listener);
    DCHECK(other.m_listener);
    return *m_listener == *other.m_listener &&
           m_useCapture == other.m_useCapture;
  }

 private:
  Member<EventListener> m_listener;
  unsigned m_useCapture : 1;
  unsigned m_passive : 1;
  unsigned m_once : 1;
  unsigned m_blockedEventWarningEmitted : 1;
  unsigned m_passiveForcedForDocumentTarget : 1;
  unsigned m_passiveSpecified : 1;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::RegisteredEventListener);

#endif  // RegisteredEventListener_h
