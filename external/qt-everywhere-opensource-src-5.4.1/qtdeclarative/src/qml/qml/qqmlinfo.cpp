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

#include "qqmlinfo.h"

#include "qqmldata_p.h"
#include "qqmlcontext.h"
#include "qqmlcontext_p.h"
#include "qqmlmetatype_p.h"
#include "qqmlengine_p.h"

#include <QCoreApplication>

QT_BEGIN_NAMESPACE

/*!
    \fn QQmlInfo qmlInfo(const QObject *object)
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
                QString typeName;
                QQmlType *type = QQmlMetaType::qmlType(object->metaObject());
                if (type) {
                    typeName = type->qmlTypeName();
                    int lastSlash = typeName.lastIndexOf(QLatin1Char('/'));
                    if (lastSlash != -1)
                        typeName = typeName.mid(lastSlash+1);
                } else {
                    typeName = QString::fromUtf8(object->metaObject()->className());
                    int marker = typeName.indexOf(QLatin1String("_QMLTYPE_"));
                    if (marker != -1)
                        typeName = typeName.left(marker);

                    marker = typeName.indexOf(QLatin1String("_QML_"));
                    if (marker != -1) {
                        typeName = typeName.left(marker);
                        typeName += QLatin1Char('*');
                        type = QQmlMetaType::qmlType(QMetaType::type(typeName.toLatin1()));
                        if (type) {
                            typeName = type->qmlTypeName();
                            int lastSlash = typeName.lastIndexOf(QLatin1Char('/'));
                            if (lastSlash != -1)
                                typeName = typeName.mid(lastSlash+1);
                        }
                    }
                }

                d->buffer.prepend(QLatin1String("QML ") + typeName + QLatin1String(": "));

                QQmlData *ddata = QQmlData::get(object, false);
                if (ddata && ddata->outerContext) {
                    error.setUrl(ddata->outerContext->url);
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
