// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/CompressibleString.h"

#include "platform/Histogram.h"
#include "wtf/Assertions.h"
#include "wtf/WTFThreadData.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CompressibleStringTable {
    WTF_MAKE_NONCOPYABLE(CompressibleStringTable);
public:
    static CompressibleStringTable* create(WTFThreadData& data)
    {
        data.m_compressibleStringTable = new CompressibleStringTable;
        data.m_compressibleStringTableDestructor = CompressibleStringTable::destroy;
        return data.m_compressibleStringTable;
    }

    void add(CompressibleStringImpl* string)
    {
        ASSERT(!m_table.contains(string));
        m_table.add(string);
    }

    bool contains(CompressibleStringImpl* string) const
    {
        return m_table.contains(string);
    }

    void remove(CompressibleStringImpl* string)
    {
        ASSERT(m_table.contains(string));
        m_table.remove(string);
    }

    void compressAll()
    {
        HashSet<CompressibleStringImpl*>::iterator end = m_table.end();
        for (HashSet<CompressibleStringImpl*>::iterator iter = m_table.begin(); iter != end; ++iter) {
            CompressibleStringImpl* string = *iter;
            if (!string->isCompressed())
                string->compressString();
        }
    }

private:
    CompressibleStringTable() { }

    static void destroy(CompressibleStringTable* table)
    {
        delete table;
    }

    HashSet<CompressibleStringImpl*> m_table;
};

static inline CompressibleStringTable& compressibleStringTable()
{
    WTFThreadData& data = wtfThreadData();
    CompressibleStringTable* table = data.compressibleStringTable();
    if (UNLIKELY(!table))
        table = CompressibleStringTable::create(data);
    return *table;
}

static const unsigned CompressibleStringImplSizeThrehold = 100000;

void CompressibleStringImpl::compressAll()
{
    compressibleStringTable().compressAll();
}

CompressibleStringImpl::CompressibleStringImpl(PassRefPtr<StringImpl> impl)
    : m_string(impl)
    , m_isCompressed(false)
{
    if (originalContentSizeInBytes() > CompressibleStringImplSizeThrehold)
        compressibleStringTable().add(this);
}

CompressibleStringImpl::~CompressibleStringImpl()
{
    if (originalContentSizeInBytes() > CompressibleStringImplSizeThrehold)
        compressibleStringTable().remove(this);
}

enum CompressibleStringCountType {
    StringWasCompressedInBackgroundTab,
    StringWasDecompressed,
    CompressibleStringCountTypeMax = StringWasDecompressed,
};

static void recordCompressibleStringCount(CompressibleStringCountType type)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, sringTypeHistogram, new EnumerationHistogram("Memory.CompressibleStringCount", CompressibleStringCountTypeMax + 1));
    sringTypeHistogram.count(type);
}

// compressString does nothing but collect UMA so far.
// TODO(hajimehoshi): Implement this.
void CompressibleStringImpl::compressString()
{
    recordCompressibleStringCount(StringWasCompressedInBackgroundTab);
    ASSERT(!isCompressed());
    m_isCompressed = true;
}

// decompressString does nothing but collect UMA so far.
// TODO(hajimehoshi): Implement this.
void CompressibleStringImpl::decompressString()
{
    // TODO(hajimehoshi): We wanted to tell whether decompressing in a
    // background tab or a foreground tab, but this was impossible. For example,
    // one renderer process of a new tab page is used for multiple tabs.
    // Another example is that reloading a page will re-use the process with a
    // new Page object and updating a static variable along with reloading will
    // be complex. See also crbug/581266. We will revisit when the situation
    // changes.
    recordCompressibleStringCount(StringWasDecompressed);
    ASSERT(isCompressed());
    m_isCompressed = false;
}

} // namespace blink
