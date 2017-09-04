// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MultipartImageResourceParser_h
#define MultipartImageResourceParser_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/Vector.h"

namespace blink {

// A parser parsing mlutipart/x-mixed-replace resource.
class CORE_EXPORT MultipartImageResourceParser final
    : public GarbageCollectedFinalized<MultipartImageResourceParser> {
  WTF_MAKE_NONCOPYABLE(MultipartImageResourceParser);

 public:
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() = default;
    virtual void onePartInMultipartReceived(const ResourceResponse&) = 0;
    virtual void multipartDataReceived(const char* bytes, size_t) = 0;
    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  MultipartImageResourceParser(const ResourceResponse&,
                               const Vector<char>& boundary,
                               Client*);
  void appendData(const char* bytes, size_t);
  void finish();
  void cancel() { m_isCancelled = true; }

  DECLARE_TRACE();

  static size_t skippableLengthForTest(const Vector<char>& data, size_t size) {
    return skippableLength(data, size);
  }
  static size_t findBoundaryForTest(const Vector<char>& data,
                                    Vector<char>* boundary) {
    return findBoundary(data, boundary);
  }

 private:
  bool parseHeaders();
  bool isCancelled() const { return m_isCancelled; }
  static size_t skippableLength(const Vector<char>&, size_t);
  // This function updates |*boundary|.
  static size_t findBoundary(const Vector<char>& data, Vector<char>* boundary);

  const ResourceResponse m_originalResponse;
  Vector<char> m_boundary;
  Member<Client> m_client;

  Vector<char> m_data;
  bool m_isParsingTop = true;
  bool m_isParsingHeaders = false;
  bool m_sawLastBoundary = false;
  bool m_isCancelled = false;
};

}  // namespace blink

#endif  // MultipartImageResourceParser_h
