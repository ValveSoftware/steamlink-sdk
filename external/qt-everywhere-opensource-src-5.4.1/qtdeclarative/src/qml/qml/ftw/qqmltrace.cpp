/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmltrace_p.h"

#ifdef QML_ENABLE_TRACE
#include <stdio.h>
#include <unistd.h>
#endif

QT_BEGIN_NAMESPACE

#ifdef QML_ENABLE_TRACE

QQmlTrace::Pool QQmlTrace::logPool;
QQmlTrace::Entry *QQmlTrace::first = 0;
QQmlTrace::Entry *QQmlTrace::last = 0;

static qint64 toNsecs(QQmlTrace::TimeType time)
{
#ifdef Q_OS_MAC
    static mach_timebase_info_data_t info = {0,0};
    if (info.denom == 0)
        mach_timebase_info(&info);
    return time * info.numer / info.denom;
#else
    qint64 rv = time.tv_sec * 1000000000 + time.tv_nsec;
    return rv;
#endif
}

QQmlTrace::Pool::Pool()
{
    first = New<Entry>();
    last = first;
}

QQmlTrace::Pool::~Pool()
{
    char buffer[128];
    sprintf(buffer, "qml.%d.log", ::getpid());
    FILE *out = fopen(buffer, "w");
    if (!out) {
        fprintf (stderr, "QML Log: Could not open %s\n", buffer);
        return;
    } else {
        fprintf (stderr, "QML Log: Writing log to %s\n", buffer);
    }

    QQmlTrace::Entry *cur = QQmlTrace::first;
    QByteArray indent;
    int depth = -1;

    qint64 firstTime = -1;

    while (cur) {

        switch (cur->type) {
        case QQmlTrace::Entry::RangeStart: {
            RangeStart *rs = static_cast<QQmlTrace::RangeStart *>(cur);

            qint64 nsecs = toNsecs(rs->time);

            if (firstTime == -1)
                firstTime = nsecs;

            nsecs -= firstTime;

            depth++;
            indent = QByteArray(depth * 4, ' ');
            fprintf(out, "%s%s @%lld (%lld ns)\n", indent.constData(),
                    rs->description, nsecs, toNsecs(rs->end->time) - nsecs - firstTime);
            } break;
        case QQmlTrace::Entry::RangeEnd:
            depth--;
            indent = QByteArray(depth * 4, ' ');
            break;
        case QQmlTrace::Entry::Detail:
            fprintf(out, "%s  %s\n", indent.constData(),
                    static_cast<QQmlTrace::Detail *>(cur)->description);
            break;
        case QQmlTrace::Entry::IntDetail:
            fprintf(out, "%s  %s: %d\n", indent.constData(),
                    static_cast<QQmlTrace::Detail *>(cur)->description,
                    static_cast<QQmlTrace::IntDetail *>(cur)->value);
            break;
        case QQmlTrace::Entry::StringDetail: {
            QByteArray vLatin1 = static_cast<QQmlTrace::StringDetail *>(cur)->value->toLatin1();
            fprintf(out, "%s  %s: %s\n", indent.constData(),
                    static_cast<QQmlTrace::Detail *>(cur)->description,
                    vLatin1.constData());
            } break;
        case QQmlTrace::Entry::UrlDetail: {
            QByteArray vLatin1 = static_cast<QQmlTrace::UrlDetail *>(cur)->value->toString().toLatin1();
            fprintf(out, "%s  %s: %s\n", indent.constData(),
                    static_cast<QQmlTrace::Detail *>(cur)->description,
                    vLatin1.constData());
            } break;
        case QQmlTrace::Entry::Event: {
            Event *ev = static_cast<QQmlTrace::Event *>(cur);
            qint64 nsecs = toNsecs(ev->time) - firstTime;
            fprintf(out, "%s  + %s @%lld +%lld ns\n", indent.constData(),
                    ev->description, nsecs, nsecs - (toNsecs(ev->start->time) - firstTime));
            } break;
        case QQmlTrace::Entry::Null:
        default:
            break;
        }
        cur = cur->next;
    }
    fclose(out);
}

#endif

QT_END_NAMESPACE

