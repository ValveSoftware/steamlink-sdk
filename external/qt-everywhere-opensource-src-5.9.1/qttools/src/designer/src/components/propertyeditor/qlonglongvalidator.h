/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QLONGLONGVALIDATOR_H
#define QLONGLONGVALIDATOR_H

#include <QtGui/QValidator>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

class QLongLongValidator : public QValidator
{
    Q_OBJECT
    Q_PROPERTY(qlonglong bottom READ bottom WRITE setBottom)
    Q_PROPERTY(qlonglong top READ top WRITE setTop)

public:
    explicit QLongLongValidator(QObject * parent);
    QLongLongValidator(qlonglong bottom, qlonglong top, QObject * parent);
    ~QLongLongValidator();

    QValidator::State validate(QString &, int &) const;

    void setBottom(qlonglong);
    void setTop(qlonglong);
    void setRange(qlonglong bottom, qlonglong top);

    qlonglong bottom() const { return b; }
    qlonglong top() const { return t; }

private:
    Q_DISABLE_COPY(QLongLongValidator)

    qlonglong b;
    qlonglong t;
};

// ----------------------------------------------------------------------------
class QULongLongValidator : public QValidator
{
    Q_OBJECT
    Q_PROPERTY(qulonglong bottom READ bottom WRITE setBottom)
    Q_PROPERTY(qulonglong top READ top WRITE setTop)

public:
    explicit QULongLongValidator(QObject * parent);
    QULongLongValidator(qulonglong bottom, qulonglong top, QObject * parent);
    ~QULongLongValidator();

    QValidator::State validate(QString &, int &) const;

    void setBottom(qulonglong);
    void setTop(qulonglong);
    void setRange(qulonglong bottom, qulonglong top);

    qulonglong bottom() const { return b; }
    qulonglong top() const { return t; }

private:
    Q_DISABLE_COPY(QULongLongValidator)

    qulonglong b;
    qulonglong t;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // QLONGLONGVALIDATOR_H
