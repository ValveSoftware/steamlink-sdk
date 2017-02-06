/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef VARIANTBARDATAMAPPING_H
#define VARIANTBARDATAMAPPING_H

#include <QtCore/QObject>
#include <QtCore/QStringList>

class VariantBarDataMapping : public QObject
{
    Q_OBJECT
    //! [0]
    Q_PROPERTY(int rowIndex READ rowIndex WRITE setRowIndex)
    Q_PROPERTY(int columnIndex READ columnIndex WRITE setColumnIndex)
    Q_PROPERTY(int valueIndex READ valueIndex WRITE setValueIndex)
    Q_PROPERTY(QStringList rowCategories READ rowCategories WRITE setRowCategories)
    Q_PROPERTY(QStringList columnCategories READ columnCategories WRITE setColumnCategories)
    //! [0]
public:
    explicit VariantBarDataMapping();
    VariantBarDataMapping(const VariantBarDataMapping &other);
    //! [1]
    VariantBarDataMapping(int rowIndex, int columnIndex, int valueIndex,
                           const QStringList &rowCategories,
                           const QStringList &columnCategories);
    //! [1]
    virtual ~VariantBarDataMapping();

    VariantBarDataMapping &operator=(const VariantBarDataMapping &other);

    void setRowIndex(int index);
    int rowIndex() const;
    void setColumnIndex(int index);
    int columnIndex() const;
    void setValueIndex(int index);
    int valueIndex() const;

    void setRowCategories(const QStringList &categories);
    const QStringList &rowCategories() const;
    void setColumnCategories(const QStringList &categories);
    const QStringList &columnCategories() const;

    //! [2]
    void remap(int rowIndex, int columnIndex, int valueIndex,
               const QStringList &rowCategories,
               const QStringList &columnCategories);
    //! [2]
Q_SIGNALS:
    //! [3]
    void mappingChanged();
    //! [3]

private:
    // Indexes of the mapped items in the VariantDataItem
    int m_rowIndex;
    int m_columnIndex;
    int m_valueIndex;

    // For row/column items, sort items into these categories. Other categories are ignored.
    QStringList m_rowCategories;
    QStringList m_columnCategories;
};

#endif
