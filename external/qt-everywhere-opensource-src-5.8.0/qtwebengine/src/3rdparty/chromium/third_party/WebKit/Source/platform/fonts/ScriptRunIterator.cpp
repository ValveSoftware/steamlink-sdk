// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ScriptRunIterator.h"

#include "platform/Logging.h"
#include "wtf/Threading.h"

#include <algorithm>

namespace blink {

typedef ScriptData::PairedBracketType PairedBracketType;

const int ScriptData::kMaxScriptCount = 20;

ScriptData::~ScriptData()
{
}

void ICUScriptData::getScripts(UChar32 ch, Vector<UScriptCode>& dst) const
{

    UErrorCode status = U_ZERO_ERROR;
    // Leave room to insert primary script. It's not strictly necessary but
    // it ensures that the result won't ever be greater than kMaxScriptCount,
    // which some client someday might expect.
    dst.resize(kMaxScriptCount - 1);
    // Note, ICU convention is to return the number of available items
    // regardless of the capacity passed to the call. So count can be greater
    // than dst->size(), if a later version of the unicode data has more
    // than kMaxScriptCount items.
    int count = uscript_getScriptExtensions(
        ch, &dst[0], dst.size(), &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        // Allow this, we'll just use what we have.
        DLOG(ERROR) << "Exceeded maximum script count of " << kMaxScriptCount << " for 0x" << std::hex << ch;
        count = dst.size();
        status = U_ZERO_ERROR;
    }
    UScriptCode primaryScript = uscript_getScript(ch, &status);

    if (U_FAILURE(status)) {
        DLOG(ERROR) << "Could not get icu script data: " << status << " for 0x" << std::hex << ch;
        dst.clear();
        return;
    }

    dst.resize(count);

    if (primaryScript == dst.at(0)) {
        // Only one script (might be common or inherited -- these are never in
        // the extensions unless they're the only script), or extensions are in
        // priority order already.
        return;
    }

    if (primaryScript != USCRIPT_INHERITED
        && primaryScript != USCRIPT_COMMON
        && primaryScript != USCRIPT_INVALID_CODE) {
        // Not common or primary, with extensions that are not in order. We know
        // the primary, so we insert it at the front and swap the previous front
        // to somewhere else in the list.
        auto it = std::find(dst.begin() + 1, dst.end(), primaryScript);
        if (it == dst.end()) {
            dst.append(primaryScript);
        }
        std::swap(*dst.begin(), *it);
        return;
    }

    if (primaryScript == USCRIPT_COMMON) {
        if (count == 1) {
            // Common with a preferred script. Keep common at head.
            dst.prepend(primaryScript);
            return;
        }

        // Ignore common. Find the preferred script of the multiple scripts that
        // remain, and ensure it is at the head. Just keep swapping them in,
        // there aren't likely to be many.
        for (size_t i = 1; i < dst.size(); ++i) {
            if (dst.at(0) == USCRIPT_LATIN || dst.at(i) < dst.at(0)) {
                std::swap(dst.at(0), dst.at(i));
            }
        }
        return;
    }

    // The primary is inherited, and there are other scripts. Put inherited at
    // the front, the true primary next, and then the others in random order.
    // TODO: Take into account the language of a document if available.
    // Otherwise, use Unicode block as a tie breaker. Comparing
    // ScriptCodes as integers is not meaningful because 'old' scripts are
    // just sorted in alphabetic order.
    dst.append(dst.at(0));
    dst.at(0) = primaryScript;
    for (size_t i = 2; i < dst.size(); ++i) {
        if (dst.at(1) == USCRIPT_LATIN || dst.at(i) < dst.at(1)) {
            std::swap(dst.at(1), dst.at(i));
        }
    }
}

UChar32 ICUScriptData::getPairedBracket(UChar32 ch) const
{
    return u_getBidiPairedBracket(ch);
}

PairedBracketType ICUScriptData::getPairedBracketType(UChar32 ch) const
{
    return static_cast<PairedBracketType>(
        u_getIntPropertyValue(ch, UCHAR_BIDI_PAIRED_BRACKET_TYPE));
}

const ICUScriptData* ICUScriptData::instance()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(const ICUScriptData, icuScriptDataInstance, (new ICUScriptData()));
    return &icuScriptDataInstance;
}

ScriptRunIterator::ScriptRunIterator(const UChar* text, size_t length, const ScriptData* data)
    : m_text(text)
    , m_length(length)
    , m_bracketsFixupDepth(0)
    // The initial value of m_aheadCharacter is not used.
    , m_aheadCharacter(0)
    , m_aheadPos(0)
    , m_commonPreferred(USCRIPT_COMMON)
    , m_scriptData(data)
{
    ASSERT(text);
    ASSERT(data);

    if (m_aheadPos < m_length) {
        m_currentSet.clear();
        // Priming the m_currentSet with USCRIPT_COMMON here so that the first
        // resolution between m_currentSet and m_nextSet in mergeSets() leads to
        // chosing the script of the first consumed character.
        m_currentSet.append(USCRIPT_COMMON);
        U16_NEXT(m_text, m_aheadPos, m_length, m_aheadCharacter);
        m_scriptData->getScripts(m_aheadCharacter, m_aheadSet);
    }
}

ScriptRunIterator::ScriptRunIterator(const UChar* text, size_t length)
    : ScriptRunIterator(text, length, ICUScriptData::instance())
{
}

bool ScriptRunIterator::consume(unsigned& limit, UScriptCode& script)
{
    if (m_currentSet.isEmpty()) {
        return false;
    }

    size_t pos;
    UChar32 ch;
    while (fetch(&pos, &ch)) {
        PairedBracketType pairedType = m_scriptData->getPairedBracketType(ch);
        switch (pairedType) {
        case PairedBracketType::BracketTypeOpen:
            openBracket(ch);
            break;
        case PairedBracketType::BracketTypeClose:
            closeBracket(ch);
            break;
        default:
            break;
        }
        if (!mergeSets()) {
            limit = pos;
            script = resolveCurrentScript();
            fixupStack(script);
            m_currentSet = m_nextSet;
            return true;
        }
    }

    limit = m_length;
    script = resolveCurrentScript();
    m_currentSet.clear();
    return true;
}

void ScriptRunIterator::openBracket(UChar32 ch)
{
    if (m_brackets.size() == kMaxBrackets) {
        m_brackets.removeFirst();
        if (m_bracketsFixupDepth == kMaxBrackets) {
            --m_bracketsFixupDepth;
        }
    }
    m_brackets.append(BracketRec({ ch, USCRIPT_COMMON }));
    ++m_bracketsFixupDepth;
}

void ScriptRunIterator::closeBracket(UChar32 ch)
{
    if (m_brackets.size() > 0) {
        UChar32 target = m_scriptData->getPairedBracket(ch);
        for (auto it = m_brackets.rbegin(); it != m_brackets.rend(); ++it) {
            if (it->ch == target) {
                // Have a match, use open paren's resolved script.
                UScriptCode script = it->script;
                m_nextSet.clear();
                m_nextSet.append(script);

                // And pop stack to this point.
                int numPopped = std::distance(m_brackets.rbegin(), it);
                // TODO: No resize operation in WTF::Deque?
                for (int i = 0; i < numPopped; ++i)
                    m_brackets.removeLast();
                m_bracketsFixupDepth = std::max(static_cast<size_t>(0),
                    m_bracketsFixupDepth - numPopped);
                return;
            }
        }
    }
    // leave stack alone, no match
}

// Keep items in m_currentSet that are in m_nextSet.
//
// If the sets are disjoint, return false and leave m_currentSet unchanged. Else
// return true and make current set the intersection. Make sure to maintain
// current priority script as priority if it remains, else retain next priority
// script if it remains.
//
// Also maintain a common preferred script.  If current and next are both
// common, and there is no common preferred script and next has a preferred
// script, set the common preferred script to that of next.
bool ScriptRunIterator::mergeSets()
{
    if (m_nextSet.isEmpty() || m_currentSet.isEmpty()) {
        return false;
    }

    auto currentSetIt = m_currentSet.begin();
    auto currentEnd = m_currentSet.end();
    // Most of the time, this is the only one.
    // Advance the current iterator, we won't need to check it again later.
    UScriptCode priorityScript = *currentSetIt++;

    // If next is common or inherited, the only thing that might change
    // is the common preferred script.
    if (m_nextSet.at(0) <= USCRIPT_INHERITED) {
        if (m_nextSet.size() == 2 && priorityScript <= USCRIPT_INHERITED && m_commonPreferred == USCRIPT_COMMON) {
            m_commonPreferred = m_nextSet.at(1);
        }
        return true;
    }

    // If current is common or inherited, use the next script set.
    if (priorityScript <= USCRIPT_INHERITED) {
        m_currentSet = m_nextSet;
        return true;
    }

    // Neither is common or inherited. If current is a singleton,
    // just see if it exists in the next set. This is the common case.
    auto next_it = m_nextSet.begin();
    auto next_end = m_nextSet.end();
    if (currentSetIt == currentEnd) {
        return std::find(next_it, next_end, priorityScript) != next_end;
    }

    // Establish the priority script, if we have one.
    // First try current priority script.
    bool havePriority = std::find(next_it, next_end, priorityScript)
        != next_end;
    if (!havePriority) {
        // So try next priority script.
        // Skip the first current script, we already know it's not there.
        // Advance the next iterator, later we won't need to check it again.
        priorityScript = *next_it++;
        havePriority = std::find(currentSetIt, currentEnd, priorityScript) != currentEnd;
    }

    // Note that we can never write more scripts into the current vector than
    // it already contains, so currentWriteIt won't ever exceed the size/capacity.
    auto currentWriteIt = m_currentSet.begin();
    if (havePriority) {
        // keep the priority script.
        *currentWriteIt++ = priorityScript;
    }

    if (next_it != next_end) {
        // Iterate over the remaining current scripts, and keep them if
        // they occur in the remaining next scripts.
        while (currentSetIt != currentEnd) {
            UScriptCode sc = *currentSetIt++;
            if (std::find(next_it, next_end, sc) != next_end) {
                *currentWriteIt++ = sc;
            }
        }
    }

    // Only change current if the run continues.
    int written = std::distance(m_currentSet.begin(), currentWriteIt);
    if (written > 0) {
        m_currentSet.resize(written);
        return true;
    }
    return false;
}

// When we hit the end of the run, and resolve the script, we now know the
// resolved script of any open bracket that was pushed on the stack since
// the start of the run. Fixup depth records how many of these there
// were. We've maintained this count during pushes, and taken care to
// adjust it if the stack got overfull and open brackets were pushed off
// the bottom. This sets the script of the fixup_depth topmost entries of the
// stack to the resolved script.
void ScriptRunIterator::fixupStack(UScriptCode resolvedScript)
{
    if (m_bracketsFixupDepth > 0) {
        if (m_bracketsFixupDepth > m_brackets.size()) {
            // Should never happen unless someone breaks the code.
            DLOG(ERROR) << "Brackets fixup depth exceeds size of bracket vector.";
            m_bracketsFixupDepth = m_brackets.size();
        }
        auto it = m_brackets.rbegin();
        for (size_t i = 0; i < m_bracketsFixupDepth; ++i) {
            it->script = resolvedScript;
            ++it;
        }
        m_bracketsFixupDepth = 0;
    }
}

bool ScriptRunIterator::fetch(size_t* pos, UChar32* ch)
{
    if (m_aheadPos > m_length) {
        return false;
    }
    *pos = m_aheadPos - (m_aheadCharacter >= 0x10000 ? 2 : 1);
    *ch = m_aheadCharacter;

    m_nextSet.swap(m_aheadSet);
    if (m_aheadPos == m_length) {
        // No more data to fetch, but last character still needs to be
        // processed. Advance m_aheadPos so that next time we will know
        // this has been done.
        m_aheadPos++;
        return true;
    }

    U16_NEXT(m_text, m_aheadPos, m_length, m_aheadCharacter);
    m_scriptData->getScripts(m_aheadCharacter, m_aheadSet);
    if (m_aheadSet.isEmpty()) {
        // No scripts for this character. This has already been logged, so
        // we just terminate processing this text.
        return false;
    }
    if (m_aheadSet[0] == USCRIPT_INHERITED && m_aheadSet.size() > 1) {
        if (m_nextSet[0] == USCRIPT_COMMON) {
            // Overwrite the next set with the non-inherited portion of the set.
            m_nextSet = m_aheadSet;
            m_nextSet.remove(0);
            // Discard the remaining values, we'll inherit.
            m_aheadSet.resize(1);
        } else {
            // Else, this applies to anything.
            m_aheadSet.resize(1);
        }
    }
    return true;
}

UScriptCode ScriptRunIterator::resolveCurrentScript() const
{
    UScriptCode result = m_currentSet.at(0);
    return result == USCRIPT_COMMON ? m_commonPreferred : result;
}

} // namespace blink
