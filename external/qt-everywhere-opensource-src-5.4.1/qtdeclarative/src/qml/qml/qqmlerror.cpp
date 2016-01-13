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

#include "qqmlerror.h"
#include "qqmlglobal_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>
#include <QtCore/qstringlist.h>

#include <private/qv4errorobject_p.h>

QT_BEGIN_NAMESPACE

/*!
    \class QQmlError
    \since 5.0
    \inmodule QtQml
    \brief The QQmlError class encapsulates a QML error.

    QQmlError includes a textual description of the error, as well
    as location information (the file, line, and column). The toString()
    method creates a single-line, human-readable string containing all of
    this information, for example:
    \code
    file:///home/user/test.qml:7:8: Invalid property assignment: double expected
    \endcode

    You can use qDebug() or qWarning() to output errors to the console. This method
    will attempt to open the file indicated by the error
    and include additional contextual information.
    \code
    file:///home/user/test.qml:7:8: Invalid property assignment: double expected
            y: "hello"
               ^
    \endcode

    Note that the \l {Qt Quick 1} version is named QDeclarativeError

    \sa QQuickView::errors(), QQmlComponent::errors()
*/
class QQmlErrorPrivate
{
public:
    QQmlErrorPrivate();

    QUrl url;
    QString description;
    quint16 line;
    quint16 column;
    QObject *object;
};

QQmlErrorPrivate::QQmlErrorPrivate()
: line(0), column(0), object()
{
}

/*!
    Creates an empty error object.
*/
QQmlError::QQmlError()
: d(0)
{
}

/*!
    Creates a copy of \a other.
*/
QQmlError::QQmlError(const QQmlError &other)
: d(0)
{
    *this = other;
}

/*!
    Assigns \a other to this error object.
*/
QQmlError &QQmlError::operator=(const QQmlError &other)
{
    if (!other.d) {
        delete d;
        d = 0;
    } else {
        if (!d) d = new QQmlErrorPrivate;
        d->url = other.d->url;
        d->description = other.d->description;
        d->line = other.d->line;
        d->column = other.d->column;
        d->object = other.d->object;
    }
    return *this;
}

/*!
    \internal
*/
QQmlError::~QQmlError()
{
    delete d; d = 0;
}

/*!
    Returns true if this error is valid, otherwise false.
*/
bool QQmlError::isValid() const
{
    return d != 0;
}

/*!
    Returns the url for the file that caused this error.
*/
QUrl QQmlError::url() const
{
    if (d) return d->url;
    else return QUrl();
}

/*!
    Sets the \a url for the file that caused this error.
*/
void QQmlError::setUrl(const QUrl &url)
{
    if (!d) d = new QQmlErrorPrivate;
    d->url = url;
}

/*!
    Returns the error description.
*/
QString QQmlError::description() const
{
    if (d) return d->description;
    else return QString();
}

/*!
    Sets the error \a description.
*/
void QQmlError::setDescription(const QString &description)
{
    if (!d) d = new QQmlErrorPrivate;
    d->description = description;
}

/*!
    Returns the error line number.
*/
int QQmlError::line() const
{
    if (d) return qmlSourceCoordinate(d->line);
    else return -1;
}

/*!
    Sets the error \a line number.
*/
void QQmlError::setLine(int line)
{
    if (!d) d = new QQmlErrorPrivate;
    d->line = qmlSourceCoordinate(line);
}

/*!
    Returns the error column number.
*/
int QQmlError::column() const
{
    if (d) return qmlSourceCoordinate(d->column);
    else return -1;
}

/*!
    Sets the error \a column number.
*/
void QQmlError::setColumn(int column)
{
    if (!d) d = new QQmlErrorPrivate;
    d->column = qmlSourceCoordinate(column);
}

/*!
    Returns the nearest object where this error occurred.
    Exceptions in bound property expressions set this to the object
    to which the property belongs. It will be 0 for all
    other exceptions.
 */
QObject *QQmlError::object() const
{
    if (d) return d->object;
    else return 0;
}

/*!
    Sets the nearest \a object where this error occurred.
 */
void QQmlError::setObject(QObject *object)
{
    if (!d) d = new QQmlErrorPrivate;
    d->object = object;
}

/*!
    Returns the error as a human readable string.
*/
QString QQmlError::toString() const
{
    QString rv;

    QUrl u(url());
    int l(line());

    if (u.isEmpty() || (u.isLocalFile() && u.path().isEmpty()))
        rv = QLatin1String("<Unknown File>");
    else
        rv = u.toString();

    if (l != -1) {
        rv += QLatin1Char(':') + QString::number(l);

        int c(column());
        if (c != -1)
            rv += QLatin1Char(':') + QString::number(c);
    }

    rv += QLatin1String(": ") + description();

    return rv;
}

/*!
    \relates QQmlError
    \fn QDebug operator<<(QDebug debug, const QQmlError &error)

    Outputs a human readable version of \a error to \a debug.
*/

QDebug operator<<(QDebug debug, const QQmlError &error)
{
    debug << qPrintable(error.toString());

    QUrl url = error.url();

    if (error.line() > 0 && url.scheme() == QLatin1String("file")) {
        QString file = url.toLocalFile();
        QFile f(file);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            QTextStream stream(data, QIODevice::ReadOnly);
#ifndef QT_NO_TEXTCODEC
            stream.setCodec("UTF-8");
#endif
            const QString code = stream.readAll();
            const QStringList lines = code.split(QLatin1Char('\n'));

            if (lines.count() >= error.line()) {
                const QString &line = lines.at(error.line() - 1);
                debug << "\n    " << qPrintable(line);

                if(error.column() > 0) {
                    int column = qMax(0, error.column() - 1);
                    column = qMin(column, line.length());

                    QByteArray ind;
                    ind.reserve(column);
                    for (int i = 0; i < column; ++i) {
                        const QChar ch = line.at(i);
                        if (ch.isSpace())
                            ind.append(ch.unicode());
                        else
                            ind.append(' ');
                    }
                    ind.append('^');
                    debug << "\n    " << ind.constData();
                }
            }
        }
    }
    return debug;
}

QT_END_NAMESPACE
