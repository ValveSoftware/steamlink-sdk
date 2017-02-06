/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QString>

#include "qcompressedwhitespace_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CompressedWhitespace::CharIdentifier CompressedWhitespace::toIdentifier(const QChar ch)
{
    switch(ch.unicode())
    {
        case ' ':
            return Space;
        case '\n':
            return LF;
        case '\r':
            return CR;
        case '\t':
            return Tab;
        default:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO,
                       "The caller must guarantee only whitespace is passed.");
            return Tab;
        }
    }
}

bool CompressedWhitespace::isEven(const int number)
{
    Q_ASSERT(number >= 0);
    return number % 2 == 0;
}

quint8 CompressedWhitespace::toCompressedChar(const QChar ch, const int len)
{
    Q_ASSERT(len > 0);
    Q_ASSERT(len <= MaxCharCount);

    return len + toIdentifier(ch);
}

QChar CompressedWhitespace::toChar(const CharIdentifier id)
{
    switch(id)
    {
        case Space: return QLatin1Char(' ');
        case CR:    return QLatin1Char('\r');
        case LF:    return QLatin1Char('\n');
        case Tab:   return QLatin1Char('\t');
        default:
                    {
                        Q_ASSERT_X(false, Q_FUNC_INFO, "Unexpected input");
                        return QChar();
                    }
    }
}

QString CompressedWhitespace::compress(const QStringRef &input)
{
    Q_ASSERT(!isEven(1) && isEven(0) && isEven(2));
    Q_ASSERT(!input.isEmpty());

    QString result;
    const int len = input.length();

    /* The amount of compressed characters. For instance, if input is
     * four spaces followed by one tab, compressedChars will be 2, and the resulting
     * QString will have a length of 1, two compressedChars stored in one QChar. */
    int compressedChars = 0;

    for(int i = 0; i < len; ++i)
    {
        const QChar c(input.at(i));

        int start = i;

        while(true)
        {
            if(i + 1 == input.length() || input.at(i + 1) != c)
                break;
            else
                ++i;
        }

        /* The length of subsequent whitespace characters in the input. */
        int wsLen = (i - start) + 1;

        /* We might get a sequence of whitespace that is so long, that we can't
         * store it in one unit/byte. In that case we chop it into as many subsequent
         * ones that is needed. */
        while(true)
        {
            const int unitLength = qMin(wsLen, int(MaxCharCount));
            wsLen -= unitLength;

            ushort resultCP = toCompressedChar(c, unitLength);

            if(isEven(compressedChars))
                result += QChar(resultCP);
            else
            {
                resultCP = resultCP << 8;
                resultCP |= result.at(result.size() - 1).unicode();
                result[result.size() - 1] = resultCP;
            }

            ++compressedChars;

            if(wsLen == 0)
                break;
        }
    }

    return result;
}

QString CompressedWhitespace::decompress(const QString &input)
{
    Q_ASSERT(!input.isEmpty());
    const int len = input.length() * 2;
    QString retval;

    for(int i = 0; i < len; ++i)
    {
        ushort cp = input.at(i / 2).unicode();

        if(isEven(i))
            cp &= Lower8Bits;
        else
        {
            cp = cp >> 8;

            if(cp == 0)
                return retval;
        }

        const quint8 wsLen = cp & Lower6Bits;
        const quint8 id = cp & UpperTwoBits;

        /* Resize retval, and fill in on the top. */
        const int oldSize = retval.size();
        const int newSize = retval.size() + wsLen;
        retval.resize(newSize);
        const QChar ch(toChar(CharIdentifier(id)));

        for(int f = oldSize; f < newSize; ++f)
            retval[f] = ch;
    }

    return retval;
}

QT_END_NAMESPACE

