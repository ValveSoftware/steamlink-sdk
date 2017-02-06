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

#include "qqmlinfo.h"

#include "qqmldata_p.h"
#include "qqmlcontext.h"
#include "qqmlcontext_p.h"
#include "qqmlmetatype_p.h"
#include "qqmlengine_p.h"

#include <QCoreApplication>

QT_BEGIN_NAMESPACE

/*!
    \fn QQmlInfo QtQml::qmlInfo(const QObject *object)
    \relates QQmlEngine

    Prints warning messages that include the file and line number for the
    specified QML \a object.

    When QML types display warning messages, it improves traceability
    if they include the QML file and line number on which the
    particular instance was instantiated.

    To include the file and line number, an object must be passed.  If
    the file and line number is not available for that instance
    (either it was not instantiated by the QML engine or location
    information is disabled), "unknown location" will be used instead.

    For example,

    \code
    qmlInfo(object) << tr("component property is a write-once property");
    \endcode

    prints

    \code
    QML MyCustomType (unknown location): component property is a write-once property
    \endcode
*/

class QQmlInfoPrivate
{
public:
    QQmlInfoPrivate() : ref (1), object(0) {}

    int ref;
    const QObject *object;
    QString buffer;
    QList<QQmlError> errors;
};

QQmlInfo::QQmlInfo(QQmlInfoPrivate *p)
: QDebug(&p->buffer), d(p)
{
    nospace();
}

QQmlInfo::QQmlInfo(const QQmlInfo &other)
: QDebug(other), d(other.d)
{
    d->ref++;
}

QQmlInfo::~QQmlInfo()
{
    if (0 == --d->ref) {
        QList<QQmlError> errors = d->errors;

        QQmlEngine *engine = 0;

        if (!d->buffer.isEmpty()) {
            QQmlError error;

            QObject *object = const_cast<QObject *>(d->object);

            if (object) {
                engine = qmlEngine(d->object);

                d->buffer.prepend(QLatin1String("QML ") + QQmlMetaType::prettyTypeName(object) + QLatin1String(": "));

                QQmlData *ddata = QQmlData::get(object, false);
                if (ddata && ddata->outerContext) {
                    error.setUrl(ddata->outerContext->url());
                    error.setLine(ddata->lineNumber);
                    error.setColumn(ddata->columnNumber);
                }
            }

            error.setDescription(d->buffer);

            errors.prepend(error);
        }

        QQmlEnginePrivate::warning(engine, errors);

        delete d;
    }
}

namespace QtQml {

QQmlInfo qmlInfo(const QObject *me)
{
    QQmlInfoPrivate *d = new QQmlInfoPrivate;
    d->object = me;
    return QQmlInfo(d);
}

QQmlInfo qmlInfo(const QObject *me, const QQmlError &error)
{
    QQmlInfoPrivate *d = new QQmlInfoPrivate;
    d->object = me;
    d->errors << error;
    return QQmlInfo(d);
}

QQmlInfo qmlInfo(const QObject *me, const QList<QQmlError> &errors)
{
    QQmlInfoPrivate *d = new QQmlInfoPrivate;
    d->object = me;
    d->errors = errors;
    return QQmlInfo(d);
}

} // namespace QtQml

#if QT_DEPRECATED_SINCE(5, 1)

// Also define symbols outside namespace to keep binary compatibility with Qt 5.0

QQmlInfo qmlInfo(const QObject *me)
{
    return QtQml::qmlInfo(me);
}

QQmlInfo qmlInfo(const QObject *me, const QQmlError &error)
{
    return QtQml::qmlInfo(me, error);
}

QQmlInfo qmlInfo(const QObject *me, const QList<QQmlError> &errors)
{
    return QtQml::qmlInfo(me, errors);
}

#endif // QT_DEPRECATED_SINCE(5, 1)

QT_END_NAMESPACE
