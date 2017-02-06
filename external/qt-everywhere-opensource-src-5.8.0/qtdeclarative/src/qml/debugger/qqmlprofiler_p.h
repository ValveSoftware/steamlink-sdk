/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLPROFILER_P_H
#define QQMLPROFILER_P_H

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

#include <private/qv4function_p.h>
#include <private/qqmlboundsignal_p.h>
#include <private/qfinitestack_p.h>
#include <private/qqmlbinding_p.h>
#include "qqmlprofilerdefinitions_p.h"
#include "qqmlabstractprofileradapter_p.h"

#include <QUrl>
#include <QString>

QT_BEGIN_NAMESPACE

#ifdef QT_NO_QML_DEBUGGER

#define Q_QML_PROFILE_IF_ENABLED(feature, profiler, Code)
#define Q_QML_PROFILE(feature, profiler, Method)
#define Q_QML_OC_PROFILE(member, Code)

struct QQmlProfiler {};

struct QQmlBindingProfiler
{
    QQmlBindingProfiler(quintptr, QQmlBinding *, QV4::FunctionObject *) {}
};

struct QQmlHandlingSignalProfiler
{
    QQmlHandlingSignalProfiler(quintptr, QQmlBoundSignalExpression *) {}
};

struct QQmlCompilingProfiler
{
    QQmlCompilingProfiler(quintptr, QQmlDataBlob *) {}
};

struct QQmlVmeProfiler {
    QQmlVmeProfiler() {}

    void init(quintptr, int) {}

    const QV4::CompiledData::Object *pop() { return nullptr; }
    void push(const QV4::CompiledData::Object *) {}

    static const quintptr profiler = 0;
};

struct QQmlObjectCreationProfiler
{
    QQmlObjectCreationProfiler(quintptr, const QV4::CompiledData::Object *) {}
    void update(QV4::CompiledData::CompilationUnit *, const QV4::CompiledData::Object *,
                const QString &, const QUrl &) {}
};

struct QQmlObjectCompletionProfiler
{
    QQmlObjectCompletionProfiler(QQmlVmeProfiler *) {}
};

#else

#define Q_QML_PROFILE_IF_ENABLED(feature, profiler, Code)\
    if (profiler && (profiler->featuresEnabled & (1 << feature))) {\
        Code;\
    } else\
        (void)0

#define Q_QML_PROFILE(feature, profiler, Method)\
    Q_QML_PROFILE_IF_ENABLED(feature, profiler, profiler->Method)

#define Q_QML_OC_PROFILE(member, Code)\
    Q_QML_PROFILE_IF_ENABLED(QQmlProfilerDefinitions::ProfileCreating, member.profiler, Code)

// This struct is somewhat dangerous to use:
// The messageType is a bit field. You can pack multiple messages into
// one object, e.g. RangeStart and RangeLocation. Each one will be read
// independently when converting to QByteArrays. Thus you can only pack
// messages if their data doesn't overlap. It's up to you to figure that
// out.
struct Q_AUTOTEST_EXPORT QQmlProfilerData : public QQmlProfilerDefinitions
{
    QQmlProfilerData(qint64 time = -1, int messageType = -1,
                     RangeType detailType = MaximumRangeType, quintptr locationId = 0) :
        time(time), locationId(locationId), messageType(messageType), detailType(detailType)
    {}

    qint64 time;
    quintptr locationId;

    int messageType;        //bit field of QQmlProfilerService::Message
    RangeType detailType;
};

Q_DECLARE_TYPEINFO(QQmlProfilerData, Q_MOVABLE_TYPE);

class Q_QML_PRIVATE_EXPORT QQmlProfiler : public QObject, public QQmlProfilerDefinitions {
    Q_OBJECT
public:

    class BindingRefCount : public QQmlRefCount {
    public:
        BindingRefCount(QQmlBinding *binding):
            m_binding(binding)
        {
            m_binding->ref.ref();
        }

        BindingRefCount(const BindingRefCount &other) :
            QQmlRefCount(other), m_binding(other.m_binding)
        {
            m_binding->ref.ref();
        }

        BindingRefCount &operator=(const BindingRefCount &other)
        {
            if (this != &other) {
                QQmlRefCount::operator=(other);
                other.m_binding->ref.ref();
                if (!m_binding->ref.deref())
                    delete m_binding;
                m_binding = other.m_binding;
            }
            return *this;
        }

        ~BindingRefCount()
        {
            if (!m_binding->ref.deref())
                delete m_binding;
        }

    private:
        QQmlBinding *m_binding;
    };

    struct Location {
        Location(const QQmlSourceLocation &location = QQmlSourceLocation(),
                 const QUrl &url = QUrl()) :
            location(location), url(url) {}
        QQmlSourceLocation location;
        QUrl url;
    };

    // Unfortunately we have to resolve the locations right away because the QML context might not
    // be available anymore when we send the data.
    struct RefLocation : public Location {
        RefLocation() : Location(), locationType(MaximumRangeType), ref(nullptr), sent(false)
        {}

        RefLocation(QQmlBinding *binding, QV4::FunctionObject *function) :
            Location(function->sourceLocation()), locationType(Binding),
            ref(new BindingRefCount(binding), QQmlRefPointer<QQmlRefCount>::Adopt), sent(false)
        {}

        RefLocation(QV4::CompiledData::CompilationUnit *ref, const QUrl &url, const QV4::CompiledData::Object *obj,
                    const QString &type) :
            Location(QQmlSourceLocation(type, obj->location.line, obj->location.column), url),
            locationType(Creating), ref(ref), sent(false)
        {}

        RefLocation(QQmlBoundSignalExpression *ref) :
            Location(ref->sourceLocation()), locationType(HandlingSignal), ref(ref), sent(false)
        {}

        RefLocation(QQmlDataBlob *ref) :
            Location(QQmlSourceLocation(), ref->url()), locationType(Compiling), ref(ref),
            sent(false)
        {}

        bool isValid() const
        {
            return locationType != MaximumRangeType;
        }

        RangeType locationType;
        QQmlRefPointer<QQmlRefCount> ref;
        bool sent;
    };

    typedef QHash<quintptr, Location> LocationHash;

    void startBinding(QQmlBinding *binding, QV4::FunctionObject *function)
    {
        quintptr locationId(id(binding));
        m_data.append(QQmlProfilerData(m_timer.nsecsElapsed(),
                                       (1 << RangeStart | 1 << RangeLocation), Binding,
                                       locationId));

        RefLocation &location = m_locations[locationId];
        if (!location.isValid())
            location = RefLocation(binding, function);
    }

    // Have toByteArrays() construct another RangeData event from the same QString later.
    // This is somewhat pointless but important for backwards compatibility.
    void startCompiling(QQmlDataBlob *blob)
    {
        quintptr locationId(id(blob));
        m_data.append(QQmlProfilerData(m_timer.nsecsElapsed(),
                                       (1 << RangeStart | 1 << RangeLocation | 1 << RangeData),
                                       Compiling, locationId));

        RefLocation &location = m_locations[locationId];
        if (!location.isValid())
            location = RefLocation(blob);
    }

    void startHandlingSignal(QQmlBoundSignalExpression *expression)
    {
        quintptr locationId(id(expression));
        m_data.append(QQmlProfilerData(m_timer.nsecsElapsed(),
                                       (1 << RangeStart | 1 << RangeLocation), HandlingSignal,
                                       locationId));

        RefLocation &location = m_locations[locationId];
        if (!location.isValid())
            location = RefLocation(expression);
    }

    void startCreating(const QV4::CompiledData::Object *obj)
    {
        m_data.append(QQmlProfilerData(m_timer.nsecsElapsed(),
                                       (1 << RangeStart | 1 << RangeLocation | 1 << RangeData),
                                       Creating, id(obj)));
    }

    void updateCreating(const QV4::CompiledData::Object *obj, QV4::CompiledData::CompilationUnit *ref,
                        const QUrl &url, const QString &type)
    {
        quintptr locationId(id(obj));
        RefLocation &location = m_locations[locationId];
        if (!location.isValid())
            location = RefLocation(ref, url, obj, type);
    }

    template<RangeType Range>
    void endRange()
    {
        m_data.append(QQmlProfilerData(m_timer.nsecsElapsed(), 1 << RangeEnd, Range));
    }

    QQmlProfiler();

    quint64 featuresEnabled;

    template<typename Object>
    static quintptr id(const Object *pointer)
    {
        return reinterpret_cast<quintptr>(pointer);
    }

    void startProfiling(quint64 features);
    void stopProfiling();
    void reportData(bool trackLocations);
    void setTimer(const QElapsedTimer &timer) { m_timer = timer; }

signals:
    void dataReady(const QVector<QQmlProfilerData> &, const QQmlProfiler::LocationHash &);

protected:
    QElapsedTimer m_timer;
    QHash<quintptr, RefLocation> m_locations;
    QVector<QQmlProfilerData> m_data;
};

//
// RAII helper structs
//

struct QQmlProfilerHelper : public QQmlProfilerDefinitions {
    QQmlProfiler *profiler;
    QQmlProfilerHelper(QQmlProfiler *profiler) : profiler(profiler) {}
};

struct QQmlBindingProfiler : public QQmlProfilerHelper {
    QQmlBindingProfiler(QQmlProfiler *profiler, QQmlBinding *binding,
                        QV4::FunctionObject *function) :
        QQmlProfilerHelper(profiler)
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileBinding, profiler,
                      startBinding(binding, function));
    }

    ~QQmlBindingProfiler()
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileBinding, profiler,
                      endRange<Binding>());
    }
};

struct QQmlHandlingSignalProfiler : public QQmlProfilerHelper {
    QQmlHandlingSignalProfiler(QQmlProfiler *profiler, QQmlBoundSignalExpression *expression) :
        QQmlProfilerHelper(profiler)
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileHandlingSignal, profiler,
                      startHandlingSignal(expression));
    }

    ~QQmlHandlingSignalProfiler()
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileHandlingSignal, profiler,
                      endRange<QQmlProfiler::HandlingSignal>());
    }
};

struct QQmlCompilingProfiler : public QQmlProfilerHelper {
    QQmlCompilingProfiler(QQmlProfiler *profiler, QQmlDataBlob *blob) :
        QQmlProfilerHelper(profiler)
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileCompiling, profiler, startCompiling(blob));
    }

    ~QQmlCompilingProfiler()
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileCompiling, profiler, endRange<Compiling>());
    }
};

struct QQmlVmeProfiler : public QQmlProfilerDefinitions {
public:

    QQmlVmeProfiler() : profiler(0) {}

    void init(QQmlProfiler *p, int maxDepth)
    {
        profiler = p;
        ranges.allocate(maxDepth);
    }

    const QV4::CompiledData::Object *pop()
    {
        if (ranges.count() > 0)
            return ranges.pop();
        else
            return nullptr;
    }

    void push(const QV4::CompiledData::Object *object)
    {
        if (ranges.capacity() > ranges.count())
            ranges.push(object);
    }

    QQmlProfiler *profiler;

private:
    QFiniteStack<const QV4::CompiledData::Object *> ranges;
};

class QQmlObjectCreationProfiler {
public:

    QQmlObjectCreationProfiler(QQmlProfiler *profiler, const QV4::CompiledData::Object *obj)
        : profiler(profiler)
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileCreating, profiler, startCreating(obj));
    }

    ~QQmlObjectCreationProfiler()
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileCreating, profiler, endRange<QQmlProfilerDefinitions::Creating>());
    }

    void update(QV4::CompiledData::CompilationUnit *ref, const QV4::CompiledData::Object *obj,
                const QString &typeName, const QUrl &url)
    {
        profiler->updateCreating(obj, ref, url, typeName);
    }

private:
    QQmlProfiler *profiler;
};

class QQmlObjectCompletionProfiler {
public:
    QQmlObjectCompletionProfiler(QQmlVmeProfiler *parent) :
        profiler(parent->profiler)
    {
        Q_QML_PROFILE_IF_ENABLED(QQmlProfilerDefinitions::ProfileCreating, profiler, {
            profiler->startCreating(parent->pop());
        });
    }

    ~QQmlObjectCompletionProfiler()
    {
        Q_QML_PROFILE(QQmlProfilerDefinitions::ProfileCreating, profiler,
                      endRange<QQmlProfilerDefinitions::Creating>());
    }
private:
    QQmlProfiler *profiler;
};

QT_END_NAMESPACE
Q_DECLARE_METATYPE(QVector<QQmlProfilerData>)
Q_DECLARE_METATYPE(QQmlProfiler::LocationHash)

#endif // QT_NO_QML_DEBUGGER

#endif // QQMLPROFILER_P_H
