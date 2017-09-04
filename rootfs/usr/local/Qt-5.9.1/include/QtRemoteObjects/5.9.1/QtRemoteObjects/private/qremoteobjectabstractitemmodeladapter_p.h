/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef QREMOTEOBJECTS_ABSTRACT_ITEM_ADAPTER_P_H
#define QREMOTEOBJECTS_ABSTRACT_ITEM_ADAPTER_P_H

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

#include "qremoteobjectabstractitemmodeltypes.h"
#include "qremoteobjectsource.h"

#include <QSize>

QT_BEGIN_NAMESPACE

class QAbstractItemModel;
class QItemSelectionModel;

class QAbstractItemModelSourceAdapter : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit QAbstractItemModelSourceAdapter(QAbstractItemModel *object, QItemSelectionModel *sel, const QVector<int> &roles = QVector<int>());
    Q_PROPERTY(QVector<int> availableRoles READ availableRoles WRITE setAvailableRoles NOTIFY availableRolesChanged)
    Q_PROPERTY(QIntHash roleNames READ roleNames)
    void registerTypes();
    QItemSelectionModel* selectionModel() const;

public Q_SLOTS:
    QVector<int> availableRoles() const { return m_availableRoles; }
    void setAvailableRoles(QVector<int> availableRoles)
    {
        if (availableRoles != m_availableRoles)
        {
            m_availableRoles = availableRoles;
            Q_EMIT availableRolesChanged();
        }
    }

    QIntHash roleNames() const {return m_model->roleNames();}

    QSize replicaSizeRequest(IndexList parentList);
    DataEntries replicaRowRequest(IndexList start, IndexList end, QVector<int> roles);
    QVariantList replicaHeaderRequest(QVector<Qt::Orientation> orientations, QVector<int> sections, QVector<int> roles);
    void replicaSetCurrentIndex(IndexList index, QItemSelectionModel::SelectionFlags command);
    void replicaSetData(const IndexList &index, const QVariant &value, int role);

    void sourceDataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & roles = QVector<int> ()) const;
    void sourceRowsInserted(const QModelIndex & parent, int start, int end);
    void sourceColumnsInserted(const QModelIndex & parent, int start, int end);
    void sourceRowsRemoved(const QModelIndex & parent, int start, int end);
    void sourceRowsMoved(const QModelIndex & sourceParent, int sourceRow, int count, const QModelIndex & destinationParent, int destinationChild) const;
    void sourceCurrentChanged(const QModelIndex & current, const QModelIndex & previous);

Q_SIGNALS:
    void availableRolesChanged();
    void dataChanged(IndexList topLeft, IndexList bottomRight, QVector<int> roles) const;
    void rowsInserted(IndexList parent, int start, int end) const;
    void rowsRemoved(IndexList parent, int start, int end) const;
    void rowsMoved(IndexList sourceParent, int sourceRow, int count, IndexList destinationParent, int destinationChild) const;
    void currentChanged(IndexList current, IndexList previous);
    void columnsInserted(IndexList parent, int start, int end) const;

private:
    QAbstractItemModelSourceAdapter();
    QAbstractItemModel *m_model;
    QItemSelectionModel *m_selectionModel;
    QVector<int> m_availableRoles;
};

template <class ObjectType, class AdapterType>
struct QAbstractItemAdapterSourceAPI : public SourceApiMap
{
    QAbstractItemAdapterSourceAPI(const QString &name)
        : SourceApiMap()
        , signalArgTypes{}
        , methodArgTypes{}
        , m_name(name)
    {
        _properties[0] = 2;
        _properties[1] = qtro_prop_index<AdapterType>(&AdapterType::availableRoles, static_cast<QVector<int> (QObject::*)()>(0),"availableRoles");
        _properties[2] = qtro_prop_index<AdapterType>(&AdapterType::roleNames, static_cast<QIntHash (QObject::*)()>(0),"roleNames");
        _signals[0] = 9;
        _signals[1] = qtro_signal_index<AdapterType>(&AdapterType::availableRolesChanged, static_cast<void (QObject::*)()>(0),signalArgCount+0,&signalArgTypes[0]);
        _signals[2] = qtro_signal_index<AdapterType>(&AdapterType::dataChanged, static_cast<void (QObject::*)(IndexList,IndexList,QVector<int>)>(0),signalArgCount+1,&signalArgTypes[1]);
        _signals[3] = qtro_signal_index<AdapterType>(&AdapterType::rowsInserted, static_cast<void (QObject::*)(IndexList,int,int)>(0),signalArgCount+2,&signalArgTypes[2]);
        _signals[4] = qtro_signal_index<AdapterType>(&AdapterType::rowsRemoved, static_cast<void (QObject::*)(IndexList,int,int)>(0),signalArgCount+3,&signalArgTypes[3]);
        _signals[5] = qtro_signal_index<AdapterType>(&AdapterType::rowsMoved, static_cast<void (QObject::*)(IndexList,int,int,IndexList,int)>(0),signalArgCount+4,&signalArgTypes[4]);
        _signals[6] = qtro_signal_index<AdapterType>(&AdapterType::currentChanged, static_cast<void (QObject::*)(IndexList,IndexList)>(0),signalArgCount+5,&signalArgTypes[5]);
        _signals[7] = qtro_signal_index<ObjectType>(&ObjectType::modelReset, static_cast<void (QObject::*)()>(0),signalArgCount+6,&signalArgTypes[6]);
        _signals[8] = qtro_signal_index<ObjectType>(&ObjectType::headerDataChanged, static_cast<void (QObject::*)(Qt::Orientation,int,int)>(0),signalArgCount+7,&signalArgTypes[7]);
        _signals[9] = qtro_signal_index<AdapterType>(&AdapterType::columnsInserted, static_cast<void (QObject::*)(IndexList,int,int)>(0),signalArgCount+8,&signalArgTypes[8]);
        _methods[0] = 5;
        _methods[1] = qtro_method_index<AdapterType>(&AdapterType::replicaSizeRequest, static_cast<void (QObject::*)(IndexList)>(0),"replicaSizeRequest(IndexList)",methodArgCount+0,&methodArgTypes[0]);
        _methods[2] = qtro_method_index<AdapterType>(&AdapterType::replicaRowRequest, static_cast<void (QObject::*)(IndexList,IndexList,QVector<int>)>(0),"replicaRowRequest(IndexList,IndexList,QVector<int>)",methodArgCount+1,&methodArgTypes[1]);
        _methods[3] = qtro_method_index<AdapterType>(&AdapterType::replicaHeaderRequest, static_cast<void (QObject::*)(QVector<Qt::Orientation>,QVector<int>,QVector<int>)>(0),"replicaHeaderRequest(QVector<Qt::Orientation>,QVector<int>,QVector<int>)",methodArgCount+2,&methodArgTypes[2]);
        _methods[4] = qtro_method_index<AdapterType>(&AdapterType::replicaSetCurrentIndex, static_cast<void (QObject::*)(IndexList,QItemSelectionModel::SelectionFlags)>(0),"replicaSetCurrentIndex(IndexList,QItemSelectionModel::SelectionFlags)",methodArgCount+3,&methodArgTypes[3]);
        _methods[5] = qtro_method_index<AdapterType>(&AdapterType::replicaSetData, static_cast<void (QObject::*)(IndexList,QVariant,int)>(0),"replicaSetData(IndexList,QVariant,int)",methodArgCount+4,&methodArgTypes[4]);
    }

    QString name() const override { return m_name; }
    QString typeName() const override { return QStringLiteral("QAbstractItemModelAdapter"); }
    int propertyCount() const override { return _properties[0]; }
    int signalCount() const override { return _signals[0]; }
    int methodCount() const override { return _methods[0]; }
    int sourcePropertyIndex(int index) const override
    {
        if (index < 0 || index >= _properties[0])
            return -1;
        return _properties[index+1];
    }
    int sourceSignalIndex(int index) const override
    {
        if (index < 0 || index >= _signals[0])
            return -1;
        return _signals[index+1];
    }
    int sourceMethodIndex(int index) const override
    {
        if (index < 0 || index >= _methods[0])
            return -1;
        return _methods[index+1];
    }
    int signalParameterCount(int index) const override { return signalArgCount[index]; }
    int signalParameterType(int sigIndex, int paramIndex) const override { return signalArgTypes[sigIndex][paramIndex]; }
    int methodParameterCount(int index) const override { return methodArgCount[index]; }
    int methodParameterType(int methodIndex, int paramIndex) const override { return methodArgTypes[methodIndex][paramIndex]; }
    int propertyIndexFromSignal(int index) const override
    {
        switch (index) {
        case 0: return _properties[1];
        case 1: return _properties[2];
        }
        return -1;
    }
    int propertyRawIndexFromSignal(int index) const override
    {
        switch (index) {
        case 0: return 0;
        case 1: return 1;
        }
        return -1;
    }
    const QByteArray signalSignature(int index) const override
    {
        switch (index) {
        case 0: return QByteArrayLiteral("availableRolesChanged()");
        case 1: return QByteArrayLiteral("dataChanged(IndexList,IndexList,QVector<int>)");
        case 2: return QByteArrayLiteral("rowsInserted(IndexList,int,int)");
        case 3: return QByteArrayLiteral("rowsRemoved(IndexList,int,int)");
        case 4: return QByteArrayLiteral("rowsMoved(IndexList,int,int,IndexList,int)");
        case 5: return QByteArrayLiteral("currentChanged(IndexList,IndexList)");
        case 6: return QByteArrayLiteral("resetModel()");
        case 7: return QByteArrayLiteral("headerDataChanged(Qt::Orientation,int,int)");
        case 8: return QByteArrayLiteral("columnsInserted(IndexList,int,int)");
        }
        return QByteArrayLiteral("");
    }
    const QByteArray methodSignature(int index) const override
    {
        switch (index) {
        case 0: return QByteArrayLiteral("replicaSizeRequest(IndexList)");
        case 1: return QByteArrayLiteral("replicaRowRequest(IndexList,IndexList,QVector<int>)");
        case 2: return QByteArrayLiteral("replicaHeaderRequest(QVector<Qt::Orientation>,QVector<int>,QVector<int>)");
        case 3: return QByteArrayLiteral("replicaSetCurrentIndex(IndexList,QItemSelectionModel::SelectionFlags)");
        case 4: return QByteArrayLiteral("replicaSetData(IndexList,QVariant,int)");
        }
        return QByteArrayLiteral("");
    }
    QMetaMethod::MethodType methodType(int) const override
    {
        return QMetaMethod::Slot;
    }
    const QByteArray typeName(int index) const override
    {
        switch (index) {
        case 0: return QByteArrayLiteral("QSize");
        case 1: return QByteArrayLiteral("DataEntries");
        case 2: return QByteArrayLiteral("QVariantList");
        case 3: return QByteArrayLiteral("");
        }
        return QByteArrayLiteral("");
    }
    QByteArray objectSignature() const override { return QByteArray{}; }
    bool isAdapterSignal(int index) const override
    {
        switch (index) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 8:
            return true;
        }
        return false;
    }
    bool isAdapterMethod(int index) const override
    {
        switch (index) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            return true;
        }
        return false;
    }
    bool isAdapterProperty(int index) const override
    {
        switch (index) {
        case 0:
        case 1:
            return true;
        }
        return false;
    }

    int _properties[3];
    int _signals[10];
    int _methods[6];
    int signalArgCount[9];
    const int* signalArgTypes[9];
    int methodArgCount[5];
    const int* methodArgTypes[5];
    QString m_name;
};

QT_END_NAMESPACE

#endif //QREMOTEOBJECTS_ABSTRACT_ITEM_ADAPTER_P_H
