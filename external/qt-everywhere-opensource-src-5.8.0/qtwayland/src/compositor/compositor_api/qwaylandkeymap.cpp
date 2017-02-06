/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandkeymap.h"
#include "qwaylandkeymap_p.h"

QT_BEGIN_NAMESPACE

QWaylandKeymap::QWaylandKeymap(const QString &layout, const QString &variant, const QString &options, const QString &model, const QString &rules, QObject *parent)
    : QObject(*new QWaylandKeymapPrivate(layout, variant, options, model, rules), parent)
{
}

QString QWaylandKeymap::layout() const {
    Q_D(const QWaylandKeymap);
    return d->m_layout;
}

void QWaylandKeymap::setLayout(const QString &layout)
{
    Q_D(QWaylandKeymap);
    if (d->m_layout == layout)
        return;
    d->m_layout = layout;
    emit layoutChanged();
}

QString QWaylandKeymap::variant() const
{
    Q_D(const QWaylandKeymap);
    return d->m_variant;
}

void QWaylandKeymap::setVariant(const QString &variant)
{
    Q_D(QWaylandKeymap);
    if (d->m_variant == variant)
        return;
    d->m_variant = variant;
    emit variantChanged();
}

QString QWaylandKeymap::options() const {
    Q_D(const QWaylandKeymap);
    return d->m_options;
}

void QWaylandKeymap::setOptions(const QString &options)
{
    Q_D(QWaylandKeymap);
    if (d->m_options == options)
        return;
    d->m_options = options;
    emit optionsChanged();
}

QString QWaylandKeymap::rules() const {
    Q_D(const QWaylandKeymap);
    return d->m_rules;
}

void QWaylandKeymap::setRules(const QString &rules)
{
    Q_D(QWaylandKeymap);
    if (d->m_rules == rules)
        return;
    d->m_rules = rules;
    emit rulesChanged();
}

QString QWaylandKeymap::model() const {
    Q_D(const QWaylandKeymap);
    return d->m_model;
}

void QWaylandKeymap::setModel(const QString &model)
{
    Q_D(QWaylandKeymap);
    if (d->m_model == model)
        return;
    d->m_model = model;
    emit modelChanged();
}

QWaylandKeymapPrivate::QWaylandKeymapPrivate(const QString &layout, const QString &variant,
                                             const QString &options, const QString &model,
                                             const QString &rules)
    : m_layout(layout)
    , m_variant(variant)
    , m_options(options)
    , m_rules(rules)
    , m_model(model)
{
}

QT_END_NAMESPACE
