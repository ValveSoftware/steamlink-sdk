/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SIMTEXTH_H
#define SIMTEXTH_H

const int textSimilarityThreshold = 190;

#include <QString>
#include <QList>

QT_BEGIN_NAMESPACE

class Translator;

struct Candidate
{
    Candidate() {}
    Candidate(const QString& source0, const QString &target0)
        : source(source0), target(target0)
    {}

    QString source;
    QString target;
};

inline bool operator==( const Candidate& c, const Candidate& d ) {
    return c.target == d.target && c.source == d.source;
}
inline bool operator!=( const Candidate& c, const Candidate& d ) {
    return !operator==( c, d );
}

typedef QList<Candidate> CandidateList;

struct CoMatrix
{
    CoMatrix(const QString &str);
    CoMatrix() {}

    /*
      The matrix has 20 * 20 = 400 entries.  This requires 50 bytes, or 13
      words.  Some operations are performed on words for more efficiency.
    */
    union {
        quint8 b[52];
        quint32 w[13];
    };
};

/**
 * This class is more efficient for searching through a large array of candidate strings, since we only
 * have to construct the CoMatrix for the \a stringToMatch once,
 * after that we just call getSimilarityScore(strCandidate).
 * \sa getSimilarityScore
 */
class StringSimilarityMatcher {
public:
    StringSimilarityMatcher(const QString &stringToMatch);
    int getSimilarityScore(const QString &strCandidate);

private:
    CoMatrix m_cm;
    int m_length;
};

/**
 * Checks how similar two strings are.
 * The return value is the score, and a higher score is more similar
 * than one with a low score.
 * Linguist considers a score over 190 to be a good match.
 * \sa StringSimilarityMatcher
 */
static inline int getSimilarityScore(const QString &str1, const QString &str2)
{
    return StringSimilarityMatcher(str1).getSimilarityScore(str2);
}

CandidateList similarTextHeuristicCandidates( const Translator *tor,
                                              const QString &text,
                                              int maxCandidates );

QT_END_NAMESPACE

#endif
