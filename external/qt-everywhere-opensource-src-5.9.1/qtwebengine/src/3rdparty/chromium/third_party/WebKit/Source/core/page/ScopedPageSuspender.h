/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 */

#ifndef ScopedPageSuspender_h
#define ScopedPageSuspender_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Page;

class CORE_EXPORT ScopedPageSuspender final {
  WTF_MAKE_NONCOPYABLE(ScopedPageSuspender);
  USING_FAST_MALLOC(ScopedPageSuspender);

 public:
  explicit ScopedPageSuspender();
  ~ScopedPageSuspender();

 private:
  friend class Page;

  static void setSuspended(bool);
  static bool isActive();
};

}  // namespace blink

#endif  // ScopedPageSuspender_h
