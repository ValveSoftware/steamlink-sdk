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

#include "qdeclarativeinfo.h"

#include "private/qdeclarativedata_p.h"
#include "qdeclarativecontext.h"
#include "private/qdeclarativecontext_p.h"
#include "private/qdeclarativemetatype_p.h"
#include "private/qdeclarativeengine_p.h"

#include <QCoreApplication>

QT_BEGIN_NAMESPACE

/*!
    \fn QDeclarativeInfo qmlInfo(const QObject *object)
    \relates QDeclarativeEngine

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

class QDeclarativeInfoPrivate
{
public:
    QDeclarativeInfoPrivate() : ref (1), object(0) {}

    int ref;
    const QObject *object;
    QString buffer;
    QList<QDeclarativeError> errors;
};

QDeclarativeInfo::QDeclarativeInfo(QDeclarativeInfoPrivate *p)
: QDebug(&p->buffer), d(p)
{
    nospace();
}

QDeclarativeInfo::QDeclarativeInfo(const QDeclarativeInfo &other)
: QDebug(other), d(other.d)
{
    d->ref++;
}

QDeclarativeInfo::~QDeclarativeInfo()
{
    if (0 == --d->ref) {
        QList<QDeclarativeError> errors = d->errors;

        QDeclarativeEngine *engine = 0;

        if (!d->buffer.isEmpty()) {
            QDeclarativeError error;

            QObject *object = const_cast<QObject *>(d->object);

            if (object) {
                engine = qmlEngine(d->object);
                QString typeName;
                QDeclarativeType *type = QDeclarativeMetaType::qmlType(object->metaObject());
                if (type) {
                    typeName = QLatin1String(type->qmlTypeName());
                    int lastSlash = typeName.lastIndexOf(QLatin1Char('/'));
                    if (lastSlash != -1)
                        typeName = typeName.mid(lastSlash+1);
                } else {
                    typeName = QString::fromUtf8(object->metaObject()->className());
                    int marker = typeName.indexOf(QLatin1String("_QMLTYPE_"));
                    if (marker != -1)
                        typeName = typeName.left(marker);
                }

                d->buffer.prepend(QLatin1String("QML ") + typeName + QLatin1String(": "));

                QDeclarativeData *ddata = QDeclarativeData::get(object, false);
                if (ddata && ddata->outerContext) {
                    error.setUrl(ddata->outerContext->url);
                    error.setLine(ddata->lineNumber);
                    error.setColumn(ddata->columnNumber);
                }
            }

            error.setDescription(d->buffer);

            errors.prepend(error);
        }

        QDeclarativeEnginePrivate::warning(engine, errors);

        delete d;
    }
}

namespace QtDeclarative {

QDeclarativeInfo qmlInfo(const QObject *me)
{
    QDeclarativeInfoPrivate *d = new QDeclarativeInfoPrivate;
    d->object = me;
    return QDeclarativeInfo(d);
}

QDeclarativeInfo qmlInfo(const QObject *me, const QDeclarativeError &error)
{
    QDeclarativeInfoPrivate *d = new QDeclarativeInfoPrivate;
    d->object = me;
    d->errors << error;
    return QDeclarativeInfo(d);
}

QDeclarativeInfo qmlInfo(const QObject *me, const QList<QDeclarativeError> &errors)
{
    QDeclarativeInfoPrivate *d = new QDeclarativeInfoPrivate;
    d->object = me;
    d->errors = errors;
    return QDeclarativeInfo(d);
}

} // namespace QtDeclarative

#if QT_DEPRECATED_SINCE(5, 1)

// Also define symbols outside namespace to keep binary compatibility with 5.0

Q_DECLARATIVE_EXPORT QDeclarativeInfo qmlInfo(const QObject *me)
{
    return QtDeclarative::qmlInfo(me);
}

Q_DECLARATIVE_EXPORT QDeclarativeInfo qmlInfo(const QObject *me,
                                              const QDeclarativeError &error)
{
    return QtDeclarative::qmlInfo(me, error);
}

Q_DECLARATIVE_EXPORT QDeclarativeInfo qmlInfo(const QObject *me,
                                              const QList<QDeclarativeError> &errors)
{
    return QtDeclarative::qmlInfo(me, errors);
}

#endif // QT_DEPRECATED_SINCE(5, 1)

QT_END_NAMESPACE
