// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositedLayerMappingPtr_h
#define CompositedLayerMappingPtr_h

#include "wtf/Assertions.h"

namespace WebCore {

class CompositedLayerMapping;

class CompositedLayerMappingPtr {
public:
    CompositedLayerMappingPtr(CompositedLayerMapping* mapping)
        : m_mapping(mapping)
    {
    }

    CompositedLayerMapping& operator*() const
    {
        ASSERT(m_mapping);
        return *m_mapping;
    }

    CompositedLayerMapping* operator->() const
    {
        ASSERT(m_mapping);
        return m_mapping;
    }

private:
    CompositedLayerMapping* m_mapping;
};

} // namespace WebCore

#endif // CompositedLayerMappingPtr_h
