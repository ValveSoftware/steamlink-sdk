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

#include "qquickspinboxvalidator_p.h"

QT_BEGIN_NAMESPACE

QQuickSpinBoxValidator1::QQuickSpinBoxValidator1(QObject *parent)
    : QValidator(parent), m_value(0), m_step(1), m_initialized(false)
{
    m_validator.setTop(99);
    m_validator.setBottom(0);
    m_validator.setDecimals(0);
    m_validator.setNotation(QDoubleValidator::StandardNotation);

    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    setLocale(locale);

    connect(this, SIGNAL(valueChanged()), this, SIGNAL(textChanged()));
    connect(this, SIGNAL(minimumValueChanged()), this, SIGNAL(textChanged()));
    connect(this, SIGNAL(maximumValueChanged()), this, SIGNAL(textChanged()));
    connect(this, SIGNAL(decimalsChanged()), this, SIGNAL(textChanged()));
    connect(this, SIGNAL(prefixChanged()), this, SIGNAL(textChanged()));
    connect(this, SIGNAL(suffixChanged()), this, SIGNAL(textChanged()));
}

QQuickSpinBoxValidator1::~QQuickSpinBoxValidator1()
{
}

QString QQuickSpinBoxValidator1::text() const
{
    return textFromValue(m_value);
}

qreal QQuickSpinBoxValidator1::value() const
{
    return m_value;
}

void QQuickSpinBoxValidator1::setValue(qreal value)
{
    if (m_initialized) {
        value = qBound(minimumValue(), value, maximumValue());
        value = QString::number(value, 'f', m_validator.decimals()).toDouble();
    }

    if (m_value != value) {
        m_value = value;

        if (m_initialized)
            emit valueChanged();
    }
}

qreal QQuickSpinBoxValidator1::minimumValue() const
{
    return m_validator.bottom();
}

void QQuickSpinBoxValidator1::setMinimumValue(qreal min)
{
    if (min != m_validator.bottom()) {
        m_validator.setBottom(min);
        emit minimumValueChanged();
        if (m_initialized)
            setValue(m_value);
    }
}

qreal QQuickSpinBoxValidator1::maximumValue() const
{
    return m_validator.top();
}

void QQuickSpinBoxValidator1::setMaximumValue(qreal max)
{
    if (max != m_validator.top()) {
        m_validator.setTop(max);
        emit maximumValueChanged();
        if (m_initialized)
            setValue(m_value);
    }
}

int QQuickSpinBoxValidator1::decimals() const
{
    return m_validator.decimals();
}

void QQuickSpinBoxValidator1::setDecimals(int decimals)
{
    if (decimals != m_validator.decimals()) {
        m_validator.setDecimals(decimals);
        emit decimalsChanged();
        if (m_initialized)
            setValue(m_value);
    }
}

qreal QQuickSpinBoxValidator1::stepSize() const
{
    return m_step;
}

void QQuickSpinBoxValidator1::setStepSize(qreal step)
{
    if (m_step != step) {
        m_step = step;
        emit stepSizeChanged();
    }
}

QString QQuickSpinBoxValidator1::prefix() const
{
    return m_prefix;
}

void QQuickSpinBoxValidator1::setPrefix(const QString &prefix)
{
    if (m_prefix != prefix) {
        m_prefix = prefix;
        emit prefixChanged();
    }
}

QString QQuickSpinBoxValidator1::suffix() const
{
    return m_suffix;
}

void QQuickSpinBoxValidator1::setSuffix(const QString &suffix)
{
    if (m_suffix != suffix) {
        m_suffix = suffix;
        emit suffixChanged();
    }
}

void QQuickSpinBoxValidator1::fixup(QString &input) const
{
    input = textFromValue(m_value).remove(locale().groupSeparator());
}

QValidator::State QQuickSpinBoxValidator1::validate(QString &input, int &pos) const
{
    if (pos > 0 && pos < input.length()) {
        if (input.at(pos - 1) == locale().groupSeparator())
            return QValidator::Invalid;
        if (input.at(pos - 1) == locale().decimalPoint() && m_validator.decimals() == 0)
            return QValidator::Invalid;
    }

    if (!m_prefix.isEmpty() && !input.startsWith(m_prefix)) {
        input.prepend(m_prefix);
        pos += m_prefix.length();
    }

    if (!m_suffix.isEmpty() && !input.endsWith(m_suffix))
        input.append(m_suffix);

    QString value = input.mid(m_prefix.length(), input.length() - m_prefix.length() - m_suffix.length());
    int valuePos = pos - m_prefix.length();
    QValidator::State state = m_validator.validate(value, valuePos);
    input = m_prefix + value + m_suffix;
    pos = m_prefix.length() + valuePos;

    if (state == QValidator::Acceptable || state == QValidator::Intermediate) {
        bool ok = false;
        qreal val = locale().toDouble(value, &ok);
        if (ok) {
            if (state == QValidator::Acceptable ||
               (state == QValidator::Intermediate && val >= 0 && val <= m_validator.top()) ||
               (state == QValidator::Intermediate && val < 0 && val >= m_validator.bottom())) {
                const_cast<QQuickSpinBoxValidator1 *>(this)->setValue(val);
                if (input != textFromValue(val))
                    state = QValidator::Intermediate;
            } else if (val < m_validator.bottom() || val > m_validator.top()) {
                return QValidator::Invalid;
            }
        }
    }
    return state;
}

void QQuickSpinBoxValidator1::componentComplete()
{
    m_initialized = true;
    setValue(m_value);
}

void QQuickSpinBoxValidator1::increment()
{
    setValue(m_value + m_step);
}

void QQuickSpinBoxValidator1::decrement()
{
    setValue(m_value - m_step);
}

QString QQuickSpinBoxValidator1::textFromValue(qreal value) const
{
    return m_prefix + locale().toString(value, 'f', m_validator.decimals()) + m_suffix;
}

QT_END_NAMESPACE
