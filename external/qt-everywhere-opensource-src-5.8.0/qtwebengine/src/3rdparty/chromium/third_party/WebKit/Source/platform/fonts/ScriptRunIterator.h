// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptRunIterator_h
#define ScriptRunIterator_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/dtoa/utils.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace blink {

class ScriptData;

class PLATFORM_EXPORT ScriptRunIterator {
    USING_FAST_MALLOC(ScriptRunIterator);
    WTF_MAKE_NONCOPYABLE(ScriptRunIterator);
public:
    ScriptRunIterator(const UChar* text, size_t length);

    // This maintains a reference to data. It must exist for the lifetime of
    // this object. Typically data is a singleton that exists for the life of
    // the process.
    ScriptRunIterator(const UChar* text, size_t length, const ScriptData*);

    bool consume(unsigned& limit, UScriptCode&);

private:
    struct BracketRec {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        UChar32 ch;
        UScriptCode script;
    };
    void openBracket(UChar32);
    void closeBracket(UChar32);
    bool mergeSets();
    void fixupStack(UScriptCode resolvedScript);
    bool fetch(size_t* pos, UChar32*);

    UScriptCode resolveCurrentScript() const;

    const UChar* m_text;
    const size_t m_length;

    Deque<BracketRec> m_brackets;
    size_t m_bracketsFixupDepth;
    // Limit max brackets so that the bracket tracking buffer does not grow
    // excessively large when processing long runs of text.
    static const int kMaxBrackets = 32;

    Vector<UScriptCode> m_currentSet;
    Vector<UScriptCode> m_nextSet;
    Vector<UScriptCode> m_aheadSet;

    UChar32 m_aheadCharacter;
    size_t m_aheadPos;

    UScriptCode m_commonPreferred;

    const ScriptData* m_scriptData;
};

// ScriptData is a wrapper which returns a set of scripts for a particular
// character retrieved from the character's primary script and script extensions,
// as per ICU / Unicode data. ScriptData maintains a certain priority order of
// the returned values, which are essential for mergeSets method to work
// correctly.
class PLATFORM_EXPORT ScriptData {
    USING_FAST_MALLOC(ScriptData);
    WTF_MAKE_NONCOPYABLE(ScriptData);
protected:
    ScriptData() = default;

public:
    virtual ~ScriptData();

    enum PairedBracketType {
        BracketTypeNone,
        BracketTypeOpen,
        BracketTypeClose,
        BracketTypeCount
    };

    static const int kMaxScriptCount;

    virtual void getScripts(UChar32, Vector<UScriptCode>& dst) const = 0;

    virtual UChar32 getPairedBracket(UChar32) const = 0;

    virtual PairedBracketType getPairedBracketType(UChar32) const = 0;
};

class PLATFORM_EXPORT ICUScriptData : public ScriptData {
public:
    ~ICUScriptData() override
    {
    }

    static const ICUScriptData* instance();

    void getScripts(UChar32, Vector<UScriptCode>& dst) const override;

    UChar32 getPairedBracket(UChar32) const override;

    PairedBracketType getPairedBracketType(UChar32) const override;
};
} // namespace blink

#endif
