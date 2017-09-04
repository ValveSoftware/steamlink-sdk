/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef QWAYLANDKEYMAP_H
#define QWAYLANDKEYMAP_H

#include <QtCore/QObject>
#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>

QT_BEGIN_NAMESPACE

class QWaylandKeymapPrivate;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandKeymap : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandKeymap)
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(QString variant READ variant WRITE setVariant NOTIFY variantChanged)
    Q_PROPERTY(QString options READ options WRITE setOptions NOTIFY optionsChanged)
    Q_PROPERTY(QString rules READ rules WRITE setRules NOTIFY rulesChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
public:
    QWaylandKeymap(const QString &layout = QString(), const QString &variant = QString(), const QString &options = QString(),
                   const QString &model = QString(), const QString &rules = QString(), QObject *parent = nullptr);

    QString layout() const;
    void setLayout(const QString &layout);
    QString variant() const;
    void setVariant(const QString &variant);
    QString options() const;
    void setOptions(const QString &options);
    QString rules() const;
    void setRules(const QString &rules);
    QString model() const;
    void setModel(const QString &model);

Q_SIGNALS:
    void layoutChanged();
    void variantChanged();
    void optionsChanged();
    void rulesChanged();
    void modelChanged();
};

QT_END_NAMESPACE

#endif //QWAYLANDKEYMAP_H
