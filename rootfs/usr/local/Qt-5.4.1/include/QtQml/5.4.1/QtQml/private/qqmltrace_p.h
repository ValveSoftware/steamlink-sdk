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

#ifndef QQMLTRACE_P_H
#define QQMLTRACE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <private/qqmlpool_p.h>

// #define QML_ENABLE_TRACE

#if defined(QML_ENABLE_TRACE) && defined(Q_OS_MAC)
#include <mach/mach_time.h>
#endif

QT_BEGIN_NAMESPACE

class QUrl;
class QQmlTrace
{
public:
    inline QQmlTrace(const char *desc);
    inline ~QQmlTrace();

    inline void addDetail(const char *);
    inline void addDetail(const char *, int);
    inline void addDetail(const char *, const QString &);
    inline void addDetail(const char *, const QUrl &);

    inline void event(const char *desc);

#ifdef QML_ENABLE_TRACE

#ifdef Q_OS_MAC
    typedef uint64_t TimeType;
#else
    typedef timespec TimeType;
#endif

    struct Entry : public QQmlPool::POD {
        enum Type { Null, RangeStart, RangeEnd, Detail, IntDetail, StringDetail, UrlDetail, Event };
        inline Entry();
        inline Entry(Type);
        Type type;
        Entry *next;
    };
    struct RangeEnd : public Entry {
        inline RangeEnd();
        TimeType time;
    };
    struct RangeStart : public Entry {
        inline RangeStart();
        const char *description;
        TimeType time;
        QQmlTrace::RangeEnd *end;
    };
    struct Detail : public Entry {
        inline Detail();
        inline Detail(Type t);
        const char *description;
    };
    struct IntDetail : public Detail {
        inline IntDetail();
        int value;
    };
    struct StringDetail : public Detail {
        inline StringDetail();
        QString *value;
    };
    struct UrlDetail : public Detail {
        inline UrlDetail();
        QUrl *value;
    };
    struct Event : public Entry {
        inline Event();
        const char *description;
        TimeType time;
        QQmlTrace::RangeStart *start;
    };

    struct Pool : public QQmlPool {
        Pool();
        ~Pool();
    };

    static Pool logPool;
    static Entry *first;
    static Entry *last;

private:
    RangeStart *start;

    static TimeType gettime() {
#ifdef Q_OS_MAC
        return mach_absolute_time();
#else
        TimeType ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts;
#endif
    }
#endif
};

#ifdef QML_ENABLE_TRACE
QQmlTrace::Entry::Entry()
: type(Null), next(0)
{
}

QQmlTrace::Entry::Entry(Type type)
: type(type), next(0)
{
    QQmlTrace::last->next = this;
    QQmlTrace::last = this;
}

QQmlTrace::RangeEnd::RangeEnd()
: QQmlTrace::Entry(QQmlTrace::Entry::RangeEnd),
  time(gettime())
{
}

QQmlTrace::RangeStart::RangeStart()
: QQmlTrace::Entry(QQmlTrace::Entry::RangeStart),
  description(0), time(gettime())
{
}

QQmlTrace::Detail::Detail()
: QQmlTrace::Entry(QQmlTrace::Entry::Detail),
  description(0)
{
}

QQmlTrace::Detail::Detail(Type type)
: QQmlTrace::Entry(type), description(0)
{
}

QQmlTrace::IntDetail::IntDetail()
: QQmlTrace::Detail(QQmlTrace::Entry::IntDetail),
  value(0)
{
}

QQmlTrace::StringDetail::StringDetail()
: QQmlTrace::Detail(QQmlTrace::Entry::StringDetail),
  value(0)
{
}

QQmlTrace::UrlDetail::UrlDetail()
: QQmlTrace::Detail(QQmlTrace::Entry::UrlDetail),
  value(0)
{
}

QQmlTrace::Event::Event()
: QQmlTrace::Entry(QQmlTrace::Entry::Event),
  description(0), time(gettime()), start(0)
{
}
#endif

QQmlTrace::QQmlTrace(const char *desc)
{
#ifdef QML_ENABLE_TRACE
    RangeStart *e = logPool.New<RangeStart>();
    e->description = desc;
    e->end = 0;
    start = e;
#else
    Q_UNUSED(desc);
#endif
}

QQmlTrace::~QQmlTrace()
{
#ifdef QML_ENABLE_TRACE
    RangeEnd *e = logPool.New<RangeEnd>();
    start->end = e;
#endif
}

void QQmlTrace::addDetail(const char *desc)
{
#ifdef QML_ENABLE_TRACE
    Detail *e = logPool.New<Detail>();
    e->description = desc;
#else
    Q_UNUSED(desc);
#endif
}

void QQmlTrace::addDetail(const char *desc, int v)
{
#ifdef QML_ENABLE_TRACE
    IntDetail *e = logPool.New<IntDetail>();
    e->description = desc;
    e->value = v;
#else
    Q_UNUSED(desc);
    Q_UNUSED(v);
#endif
}

void QQmlTrace::addDetail(const char *desc, const QString &v)
{
#ifdef QML_ENABLE_TRACE
    StringDetail *e = logPool.New<StringDetail>();
    e->description = desc;
    e->value = logPool.NewString(v);
#else
    Q_UNUSED(desc);
    Q_UNUSED(v);
#endif
}

void QQmlTrace::addDetail(const char *desc, const QUrl &v)
{
#ifdef QML_ENABLE_TRACE
    UrlDetail *e = logPool.New<UrlDetail>();
    e->description = desc;
    e->value = logPool.NewUrl(v);
#else
    Q_UNUSED(desc);
    Q_UNUSED(v);
#endif
}

void QQmlTrace::event(const char *desc)
{
#ifdef QML_ENABLE_TRACE
    Event *e = logPool.New<Event>();
    e->start = start;
    e->description = desc;
#else
    Q_UNUSED(desc);
#endif
}

QT_END_NAMESPACE

#endif // QQMLTRACE_P_H
