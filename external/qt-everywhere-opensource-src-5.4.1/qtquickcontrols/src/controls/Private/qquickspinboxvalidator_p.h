/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKSPINBOXVALIDATOR_P_H
#define QQUICKSPINBOXVALIDATOR_P_H

#include <QtGui/qvalidator.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickSpinBoxValidator : public QValidator, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(qreal minimumValue READ minimumValue WRITE setMinimumValue NOTIFY minimumValueChanged)
    Q_PROPERTY(qreal maximumValue READ maximumValue WRITE setMaximumValue NOTIFY maximumValueChanged)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals NOTIFY decimalsChanged)
    Q_PROPERTY(qreal stepSize READ stepSize WRITE setStepSize NOTIFY stepSizeChanged)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix NOTIFY prefixChanged)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix NOTIFY suffixChanged)

public:
    explicit QQuickSpinBoxValidator(QObject *parent = 0);
    virtual ~QQuickSpinBoxValidator();

    QString text() const;

    qreal value() const;
    void setValue(qreal value);

    qreal minimumValue() const;
    void setMinimumValue(qreal min);

    qreal maximumValue() const;
    void setMaximumValue(qreal max);

    int decimals() const;
    void setDecimals(int decimals);

    qreal stepSize() const;
    void setStepSize(qreal step);

    QString prefix() const;
    void setPrefix(const QString &prefix);

    QString suffix() const;
    void setSuffix(const QString &suffix);

    void fixup(QString &input) const;
    State validate(QString &input, int &pos) const;

    void classBegin() { }
    void componentComplete();

public Q_SLOTS:
    void increment();
    void decrement();

Q_SIGNALS:
    void valueChanged();
    void minimumValueChanged();
    void maximumValueChanged();
    void decimalsChanged();
    void stepSizeChanged();
    void prefixChanged();
    void suffixChanged();
    void textChanged();

protected:
    QString textFromValue(qreal value) const;

private:
    qreal m_value;
    qreal m_step;
    QString m_prefix;
    QString m_suffix;
    bool m_initialized;
    QDoubleValidator m_validator;

    Q_DISABLE_COPY(QQuickSpinBoxValidator)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickSpinBoxValidator)

#endif // QQUICKSPINBOXVALIDATOR_P_H
