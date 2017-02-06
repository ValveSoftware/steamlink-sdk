/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLBINDING_P_H
#define QQMLBINDING_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qqml.h"
#include "qqmlpropertyvaluesource.h"
#include "qqmlexpression.h"
#include "qqmlproperty.h"
#include "qqmlscriptstring.h"
#include "qqmlproperty_p.h"

#include <QtCore/QObject>
#include <QtCore/QMetaProperty>

#include <private/qqmlabstractbinding_p.h>
#include <private/qqmljavascriptexpression_p.h>

QT_BEGIN_NAMESPACE

class QQmlContext;
class Q_QML_PRIVATE_EXPORT QQmlBinding : public QQmlJavaScriptExpression,
                                         public QQmlAbstractBinding
{
    friend class QQmlAbstractBinding;
public:
    static QQmlBinding *create(const QQmlPropertyData *, const QString &, QObject *, QQmlContext *);
    static QQmlBinding *create(const QQmlPropertyData *, const QQmlScriptString &, QObject *, QQmlContext *);
    static QQmlBinding *create(const QQmlPropertyData *, const QString &, QObject *, QQmlContextData *);
    static QQmlBinding *create(const QQmlPropertyData *, const QString &, QObject *, QQmlContextData *,
                               const QString &url, quint16 lineNumber, quint16 columnNumber);
    static QQmlBinding *create(const QQmlPropertyData *, const QV4::Value &, QObject *, QQmlContextData *);
    ~QQmlBinding();

    void setTarget(const QQmlProperty &);
    void setTarget(QObject *, const QQmlPropertyData &, const QQmlPropertyData *valueType);

    void setNotifyOnValueChanged(bool);

    void refresh() Q_DECL_OVERRIDE;

    void setEnabled(bool, QQmlPropertyData::WriteFlags flags = QQmlPropertyData::DontRemoveBinding) Q_DECL_OVERRIDE;
    QString expression() const Q_DECL_OVERRIDE;
    void update(QQmlPropertyData::WriteFlags flags = QQmlPropertyData::DontRemoveBinding);

    typedef int Identifier;
    enum {
        Invalid = -1
    };

    QVariant evaluate();

    QString expressionIdentifier() Q_DECL_OVERRIDE;
    void expressionChanged() Q_DECL_OVERRIDE;

protected:
    virtual void doUpdate(const DeleteWatcher &watcher,
                          QQmlPropertyData::WriteFlags flags, QV4::Scope &scope,
                          const QV4::ScopedFunctionObject &f) = 0;

    void getPropertyData(QQmlPropertyData **propertyData, QQmlPropertyData *valueTypeData) const;
    int getPropertyType() const;

    bool slowWrite(const QQmlPropertyData &core, const QQmlPropertyData &valueTypeData,
                   const QV4::Value &result, bool isUndefined, QQmlPropertyData::WriteFlags flags);

private:
    inline bool updatingFlag() const;
    inline void setUpdatingFlag(bool);
    inline bool enabledFlag() const;
    inline void setEnabledFlag(bool);

    static QQmlBinding *newBinding(QQmlEnginePrivate *engine, const QQmlPropertyData *property);
};

bool QQmlBinding::updatingFlag() const
{
    return m_target.flag();
}

void QQmlBinding::setUpdatingFlag(bool v)
{
    m_target.setFlagValue(v);
}

bool QQmlBinding::enabledFlag() const
{
    return m_target.flag2();
}

void QQmlBinding::setEnabledFlag(bool v)
{
    m_target.setFlag2Value(v);
}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlBinding*)

#endif // QQMLBINDING_P_H
