/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "private/textwriter_p.h"

QT_QML_BEGIN_NAMESPACE

using namespace QDeclarativeJS;

TextWriter::TextWriter()
        :string(0), cursor(0)
{
}

static bool overlaps(int posA, int lengthA, int posB, int lengthB) {
    return (posA < posB + lengthB && posA + lengthA > posB + lengthB)
            || (posA < posB && posA + lengthA > posB);
}

bool TextWriter::hasOverlap(int pos, int length)
{
    {
        QListIterator<Replace> i(replaceList);
        while (i.hasNext()) {
            const Replace &cmd = i.next();
            if (overlaps(pos, length, cmd.pos, cmd.length))
                return true;
        }
    }
    {
        QListIterator<Move> i(moveList);
        while (i.hasNext()) {
            const Move &cmd = i.next();
            if (overlaps(pos, length, cmd.pos, cmd.length))
                return true;
        }
        return false;
    }
}

bool TextWriter::hasMoveInto(int pos, int length)
{
    QListIterator<Move> i(moveList);
    while (i.hasNext()) {
        const Move &cmd = i.next();
        if (cmd.to >= pos && cmd.to < pos + length)
            return true;
    }
    return false;
}

void TextWriter::replace(int pos, int length, const QString &replacement)
{
    Q_ASSERT(!hasOverlap(pos, length));
    Q_ASSERT(!hasMoveInto(pos, length));

    Replace cmd;
    cmd.pos = pos;
    cmd.length = length;
    cmd.replacement = replacement;
    replaceList += cmd;
}

void TextWriter::move(int pos, int length, int to)
{
    Q_ASSERT(!hasOverlap(pos, length));

    Move cmd;
    cmd.pos = pos;
    cmd.length = length;
    cmd.to = to;
    moveList += cmd;
}

void TextWriter::doReplace(const Replace &replace)
{
    int diff = replace.replacement.size() - replace.length;
    {
        QMutableListIterator<Replace> i(replaceList);
        while (i.hasNext()) {
            Replace &c = i.next();
            if (replace.pos < c.pos)
                c.pos += diff;
            else if (replace.pos + replace.length < c.pos + c.length)
                c.length += diff;
        }
    }
    {
        QMutableListIterator<Move> i(moveList);
        while (i.hasNext()) {
            Move &c = i.next();
            if (replace.pos < c.pos)
                c.pos += diff;
            else if (replace.pos + replace.length < c.pos + c.length)
                c.length += diff;

            if (replace.pos < c.to)
                c.to += diff;
        }
    }

    if (string) {
        string->replace(replace.pos, replace.length, replace.replacement);
    } else if (cursor) {
        cursor->setPosition(replace.pos);
        cursor->setPosition(replace.pos + replace.length, QTextCursor::KeepAnchor);
        cursor->insertText(replace.replacement);
    }
}

void TextWriter::doMove(const Move &move)
{
    QString text;
    if (string) {
        text = string->mid(move.pos, move.length);
    } else if (cursor) {
        cursor->setPosition(move.pos);
        cursor->setPosition(move.pos + move.length, QTextCursor::KeepAnchor);
        text = cursor->selectedText();
    }

    Replace cut;
    cut.pos = move.pos;
    cut.length = move.length;
    Replace paste;
    paste.pos = move.to;
    paste.length = 0;
    paste.replacement = text;

    replaceList.append(cut);
    replaceList.append(paste);

    Replace cmd;
    while (!replaceList.isEmpty()) {
        cmd = replaceList.first();
        replaceList.removeFirst();
        doReplace(cmd);
    }
}

void TextWriter::write(QString *s)
{
    string = s;
    write_helper();
    string = 0;
}

void TextWriter::write(QTextCursor *textCursor)
{
    cursor = textCursor;
    write_helper();
    cursor = 0;
}

void TextWriter::write_helper()
{
    if (cursor)
        cursor->beginEditBlock();
    {
        Replace cmd;
        while (!replaceList.isEmpty()) {
            cmd = replaceList.first();
            replaceList.removeFirst();
            doReplace(cmd);
        }
    }
    {
        Move cmd;
        while (!moveList.isEmpty()) {
            cmd = moveList.first();
            moveList.removeFirst();
            doMove(cmd);
        }
    }
    if (cursor)
        cursor->endEditBlock();
}

QT_QML_END_NAMESPACE
