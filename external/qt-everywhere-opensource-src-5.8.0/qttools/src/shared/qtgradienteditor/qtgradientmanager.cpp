/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "qtgradientmanager.h"
#include <QtGui/QPixmap>
#include <QtCore/QMetaEnum>

QT_BEGIN_NAMESPACE

QtGradientManager::QtGradientManager(QObject *parent)
    : QObject(parent)
{
}

QMap<QString, QGradient> QtGradientManager::gradients() const
{
    return m_idToGradient;
}

QString QtGradientManager::uniqueId(const QString &id) const
{
    if (!m_idToGradient.contains(id))
        return id;

    QString base = id;
    while (base.count() > 0 && base.at(base.count() - 1).isDigit())
        base = base.left(base.count() - 1);
    QString newId = base;
    int counter = 0;
    while (m_idToGradient.contains(newId)) {
        ++counter;
        newId = base + QString::number(counter);
    }
    return newId;
}

QString QtGradientManager::addGradient(const QString &id, const QGradient &gradient)
{
    QString newId = uniqueId(id);

    m_idToGradient[newId] = gradient;

    emit gradientAdded(newId, gradient);

    return newId;
}

void QtGradientManager::removeGradient(const QString &id)
{
    if (!m_idToGradient.contains(id))
        return;

    emit gradientRemoved(id);

    m_idToGradient.remove(id);
}

void QtGradientManager::renameGradient(const QString &id, const QString &newId)
{
    if (!m_idToGradient.contains(id))
        return;

    if (newId == id)
        return;

    QString changedId = uniqueId(newId);
    QGradient gradient = m_idToGradient.value(id);

    emit gradientRenamed(id, changedId);

    m_idToGradient.remove(id);
    m_idToGradient[changedId] = gradient;
}

void QtGradientManager::changeGradient(const QString &id, const QGradient &newGradient)
{
    if (!m_idToGradient.contains(id))
        return;

    if (m_idToGradient.value(id) == newGradient)
        return;

    emit gradientChanged(id, newGradient);

    m_idToGradient[id] = newGradient;
}

void QtGradientManager::clear()
{
    QMap<QString, QGradient> grads = gradients();
    QMapIterator<QString, QGradient> itGrad(grads);
    while (itGrad.hasNext()) {
        removeGradient(itGrad.next().key());
    }
}

QT_END_NAMESPACE
