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

#include "qquickspinboxvalidator_p.h"

QT_BEGIN_NAMESPACE

QQuickSpinBoxValidator::QQuickSpinBoxValidator(QObject *parent)
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

QQuickSpinBoxValidator::~QQuickSpinBoxValidator()
{
}

QString QQuickSpinBoxValidator::text() const
{
    return textFromValue(m_value);
}

qreal QQuickSpinBoxValidator::value() const
{
    return m_value;
}

void QQuickSpinBoxValidator::setValue(qreal value)
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

qreal QQuickSpinBoxValidator::minimumValue() const
{
    return m_validator.bottom();
}

void QQuickSpinBoxValidator::setMinimumValue(qreal min)
{
    if (min != m_validator.bottom()) {
        m_validator.setBottom(min);
        emit minimumValueChanged();
        if (m_initialized)
            setValue(m_value);
    }
}

qreal QQuickSpinBoxValidator::maximumValue() const
{
    return m_validator.top();
}

void QQuickSpinBoxValidator::setMaximumValue(qreal max)
{
    if (max != m_validator.top()) {
        m_validator.setTop(max);
        emit maximumValueChanged();
        if (m_initialized)
            setValue(m_value);
    }
}

int QQuickSpinBoxValidator::decimals() const
{
    return m_validator.decimals();
}

void QQuickSpinBoxValidator::setDecimals(int decimals)
{
    if (decimals != m_validator.decimals()) {
        m_validator.setDecimals(decimals);
        emit decimalsChanged();
        if (m_initialized)
            setValue(m_value);
    }
}

qreal QQuickSpinBoxValidator::stepSize() const
{
    return m_step;
}

void QQuickSpinBoxValidator::setStepSize(qreal step)
{
    if (m_step != step) {
        m_step = step;
        emit stepSizeChanged();
    }
}

QString QQuickSpinBoxValidator::prefix() const
{
    return m_prefix;
}

void QQuickSpinBoxValidator::setPrefix(const QString &prefix)
{
    if (m_prefix != prefix) {
        m_prefix = prefix;
        emit prefixChanged();
    }
}

QString QQuickSpinBoxValidator::suffix() const
{
    return m_suffix;
}

void QQuickSpinBoxValidator::setSuffix(const QString &suffix)
{
    if (m_suffix != suffix) {
        m_suffix = suffix;
        emit suffixChanged();
    }
}

void QQuickSpinBoxValidator::fixup(QString &input) const
{
    input = textFromValue(m_value).remove(locale().groupSeparator());
}

QValidator::State QQuickSpinBoxValidator::validate(QString &input, int &pos) const
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
                const_cast<QQuickSpinBoxValidator *>(this)->setValue(val);
                if (input != textFromValue(val))
                    state = QValidator::Intermediate;
            } else if (val < m_validator.bottom() || val > m_validator.top()) {
                return QValidator::Invalid;
            }
        }
    }
    return state;
}

void QQuickSpinBoxValidator::componentComplete()
{
    m_initialized = true;
    setValue(m_value);
}

void QQuickSpinBoxValidator::increment()
{
    setValue(m_value + m_step);
}

void QQuickSpinBoxValidator::decrement()
{
    setValue(m_value - m_step);
}

QString QQuickSpinBoxValidator::textFromValue(qreal value) const
{
    return m_prefix + locale().toString(value, 'f', m_validator.decimals()) + m_suffix;
}

QT_END_NAMESPACE
