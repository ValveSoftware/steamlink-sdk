/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKSPINBOXVALIDATOR_P_H
#define QQUICKSPINBOXVALIDATOR_P_H

#include <QtGui/qvalidator.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickSpinBoxValidator1 : public QValidator, public QQmlParserStatus
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
    explicit QQuickSpinBoxValidator1(QObject *parent = 0);
    virtual ~QQuickSpinBoxValidator1();

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

    Q_DISABLE_COPY(QQuickSpinBoxValidator1)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickSpinBoxValidator1)

#endif // QQUICKSPINBOXVALIDATOR_P_H
