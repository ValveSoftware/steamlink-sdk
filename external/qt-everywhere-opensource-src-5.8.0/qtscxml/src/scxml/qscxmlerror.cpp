/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include "qscxmlerror.h"

QT_BEGIN_NAMESPACE

class QScxmlError::ScxmlErrorPrivate
{
public:
    ScxmlErrorPrivate()
        : line(-1)
        , column(-1)
    {}

    QString fileName;
    int line;
    int column;
    QString description;
};

/*!
 * \class QScxmlError
 * \brief The QScxmlError class describes the errors returned by the Qt SCXML
 * state machine when parsing an SCXML file.
 * \since 5.7
 * \inmodule QtScxml
 *
 * \sa QScxmlStateMachine QScxmlCompiler
 */

/*!
    \property QScxmlError::column
    \brief The column number in which the SCXML error occurred.
*/

/*!
    \property QScxmlError::description
    \brief A description of the SCXML error.
*/

/*!
    \property QScxmlError::fileName
    \brief The name of the file in which the SCXML error occurred.
*/

/*!
    \property QScxmlError::line
    \brief The line number on which the SCXML error occurred.
*/

/*!
    \property QScxmlError::valid
    \brief Whether the SCXML error is valid.
*/

/*!
 * Creates a new invalid SCXML error.
 */
QScxmlError::QScxmlError()
    : d(Q_NULLPTR)
{}

/*!
 * Creates a new valid SCXML error that contains the error message,
 * \a description, as well as the \a fileName, \a line, and \a column where the
 * error occurred.
 */
QScxmlError::QScxmlError(const QString &fileName, int line, int column, const QString &description)
    : d(new ScxmlErrorPrivate)
{
    d->fileName = fileName;
    d->line = line;
    d->column = column;
    d->description = description;
}

/*!
 * Constructs a copy of \a other.
 */
QScxmlError::QScxmlError(const QScxmlError &other)
    : d(Q_NULLPTR)
{
    *this = other;
}

/*!
 * Assigns \a other to this SCXML error and returns a reference to this SCXML
 * error.
 */
QScxmlError &QScxmlError::operator=(const QScxmlError &other)
{
    if (other.d) {
        if (!d)
            d = new ScxmlErrorPrivate;
        d->fileName = other.d->fileName;
        d->line = other.d->line;
        d->column = other.d->column;
        d->description = other.d->description;
    } else {
        delete d;
        d = Q_NULLPTR;
    }
    return *this;
}

/*!
 * Destroys the SCXML error.
 */
QScxmlError::~QScxmlError()
{
    delete d;
    d = Q_NULLPTR;
}

/*!
 * Returns \c true if the error is valid, \c false otherwise. An invalid error
 * can only be created by calling the default constructor or by assigning an
 * invalid error.
 */
bool QScxmlError::isValid() const
{
    return d != Q_NULLPTR;
}

/*!
 * Returns the name of the file in which the error occurred.
 */
QString QScxmlError::fileName() const
{
    return isValid() ? d->fileName : QString();
}

/*!
 * Returns the line on which the error occurred.
 */
int QScxmlError::line() const
{
    return isValid() ? d->line : -1;
}

/*!
 * Returns the column in which the error occurred.
 */
int QScxmlError::column() const
{
    return isValid() ? d->column : -1;
}

/*!
 * Returns the error message.
 */
QString QScxmlError::description() const
{
    return isValid() ? d->description : QString();
}

/*!
 * This convenience method converts an error to a string.
 * Returns the error message formatted as:
 * \e {"filename:line:column: error: description"}
 */
QString QScxmlError::toString() const
{
    QString str;
    if (!isValid())
        return str;

    if (d->fileName.isEmpty())
        str = QStringLiteral("<Unknown File>");
    else
        str = d->fileName;
    if (d->line != -1) {
        str += QStringLiteral(":%1").arg(d->line);
        if (d->column != -1)
            str += QStringLiteral(":%1").arg(d->column);
    }
    str += QStringLiteral(": error: ") + d->description;

    return str;
}

QT_END_NAMESPACE
