/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
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

#ifndef QQUICKFONTLISTMODEL_P_H
#define QQUICKFONTLISTMODEL_P_H

#include <QtCore/qstringlist.h>
#include <QtCore/QAbstractListModel>
#include <QtGui/qpa/qplatformdialoghelper.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

class QModelIndex;

class QQuickFontListModelPrivate;

class QQuickFontListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString writingSystem READ writingSystem WRITE setWritingSystem NOTIFY writingSystemChanged)

    Q_PROPERTY(bool scalableFonts READ scalableFonts WRITE setScalableFonts NOTIFY scalableFontsChanged)
    Q_PROPERTY(bool nonScalableFonts READ nonScalableFonts WRITE setNonScalableFonts NOTIFY nonScalableFontsChanged)
    Q_PROPERTY(bool monospacedFonts READ monospacedFonts WRITE setMonospacedFonts NOTIFY monospacedFontsChanged)
    Q_PROPERTY(bool proportionalFonts READ proportionalFonts WRITE setProportionalFonts NOTIFY proportionalFontsChanged)

    Q_PROPERTY(int count READ count NOTIFY rowCountChanged)

public:
    QQuickFontListModel(QObject *parent = 0);
    ~QQuickFontListModel();

    enum Roles {
        FontFamilyRole = Qt::UserRole + 1
    };

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QHash<int, QByteArray> roleNames() const;

    int count() const { return rowCount(QModelIndex()); }

    QString writingSystem() const;
    void setWritingSystem(const QString& writingSystem);

    bool scalableFonts() const;
    bool nonScalableFonts() const;
    bool monospacedFonts() const;
    bool proportionalFonts() const;

    Q_INVOKABLE QJSValue get(int index) const;
    Q_INVOKABLE QJSValue pointSizes();

    virtual void classBegin();
    virtual void componentComplete();

public Q_SLOTS:
    void setScalableFonts(bool arg);
    void setNonScalableFonts(bool arg);
    void setMonospacedFonts(bool arg);
    void setProportionalFonts(bool arg);

Q_SIGNALS:
    void scalableFontsChanged();
    void nonScalableFontsChanged();
    void monospacedFontsChanged();
    void proportionalFontsChanged();
    void writingSystemChanged();
    void rowCountChanged() const;

protected:
    void updateFamilies();

private:
    Q_DISABLE_COPY(QQuickFontListModel)
    Q_DECLARE_PRIVATE(QQuickFontListModel)
    QScopedPointer<QQuickFontListModelPrivate> d_ptr;

};

QT_END_NAMESPACE

#endif // QQUICKFONTLISTMODEL_P_H
