/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSCriptTools module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscriptobjectsnapshot_p.h"

#include <QtCore/qmap.h>
#include <QtCore/qset.h>
#include <QtCore/qvector.h>
#include <QtCore/qnumeric.h>
#include <QtScript/qscriptvalueiterator.h>

QT_BEGIN_NAMESPACE

QScriptObjectSnapshot::QScriptObjectSnapshot()
{
}

QScriptObjectSnapshot::~QScriptObjectSnapshot()
{
}

static bool _q_equal(const QScriptValue &v1, const QScriptValue &v2)
{
    if (v1.strictlyEquals(v2))
        return true;
    if (v1.isNumber() && v2.isNumber() && qIsNaN(v1.toNumber()) && qIsNaN(v2.toNumber()))
        return true;
    return false;
}

QScriptObjectSnapshot::Delta QScriptObjectSnapshot::capture(const QScriptValue &object)
{
    Delta result;
    QMap<QString, QScriptValueProperty> currProps;
    QHash<QString, int> propertyNameToIndex;
    {
        int i = 0;
        QScriptValueIterator it(object);
        while (it.hasNext()) {
            it.next();
            QScriptValueProperty prop(it.name(), it.value(), it.flags());
            currProps.insert(it.name(), prop);
            propertyNameToIndex.insert(it.name(), i);
            ++i;
        }
        if (object.prototype().isValid()) {
            QString __proto__ = QString::fromLatin1("__proto__");
            QScriptValueProperty protoProp(
                __proto__, object.prototype(),
                QScriptValue::Undeletable | QScriptValue::ReadOnly);
            currProps.insert(__proto__, protoProp);
            propertyNameToIndex.insert(__proto__, i);
            ++i;
        }
    }

    QSet<QString> prevSet;
    for (int i = 0; i < m_properties.size(); ++i)
        prevSet.insert(m_properties.at(i).name());
    QSet<QString> currSet = currProps.keys().toSet();
    QSet<QString> removedProperties = prevSet - currSet;
    QSet<QString> addedProperties = currSet - prevSet;
    QSet<QString> maybeChangedProperties = currSet & prevSet;

    {
        QMap<int, QScriptValueProperty> am;
        QSet<QString>::const_iterator it;
        for (it = addedProperties.constBegin(); it != addedProperties.constEnd(); ++it) {
            int idx = propertyNameToIndex[*it];
            am[idx] = currProps[*it];
        }
        result.addedProperties = am.values();
    }

    {
        QSet<QString>::const_iterator it;
        for (it = maybeChangedProperties.constBegin(); it != maybeChangedProperties.constEnd(); ++it) {
            const QScriptValueProperty &p1 = currProps[*it];
            const QScriptValueProperty &p2 = findProperty(*it);
            if (!_q_equal(p1.value(), p2.value())
                || (p1.flags() != p2.flags())) {
                result.changedProperties.append(p1);
            }
        }
    }

    result.removedProperties = removedProperties.toList();

    m_properties = currProps.values();

    return result;
}

QScriptValueProperty QScriptObjectSnapshot::findProperty(const QString &name) const
{
    for (int i = 0; i < m_properties.size(); ++i) {
        if (m_properties.at(i).name() == name)
            return m_properties.at(i);
    }
    return QScriptValueProperty();
}

QScriptValuePropertyList QScriptObjectSnapshot::properties() const
{
    return m_properties;
}

QT_END_NAMESPACE
