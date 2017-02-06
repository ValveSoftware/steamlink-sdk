/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Layouts module of the Qt Toolkit.
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

#ifndef QQUICKLAYOUT_P_H
#define QQUICKLAYOUT_P_H

#include <QPointer>
#include <QQuickItem>
#include <private/qquickitem_p.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>
#include <QtGui/private/qlayoutpolicy_p.h>

QT_BEGIN_NAMESPACE

class QQuickLayoutAttached;

#if 0 && !defined(QT_NO_DEBUG) && !defined(QT_NO_DEBUG_OUTPUT)
# define quickLayoutDebug QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).debug
#else
# define quickLayoutDebug QT_NO_QDEBUG_MACRO
#endif

class QQuickLayoutPrivate;
class QQuickLayout : public QQuickItem, public QQuickItemChangeListener

{
    Q_OBJECT
public:
    enum SizeHint {
        MinimumSize = 0,
        PreferredSize,
        MaximumSize,
        NSizes
    };

    explicit QQuickLayout(QQuickLayoutPrivate &dd, QQuickItem *parent = 0);
    ~QQuickLayout();

    static QQuickLayoutAttached *qmlAttachedProperties(QObject *object);


    void componentComplete() Q_DECL_OVERRIDE;
    virtual QSizeF sizeHint(Qt::SizeHint whichSizeHint) const = 0;
    virtual void setAlignment(QQuickItem *item, Qt::Alignment align) = 0;
    virtual void invalidate(QQuickItem * childItem = 0);
    virtual void updateLayoutItems() = 0;

    // iterator
    virtual QQuickItem *itemAt(int index) const = 0;
    virtual int itemCount() const = 0;

    virtual void rearrange(const QSizeF &);
    bool arrangementIsDirty() const { return m_dirty; }

    static void effectiveSizeHints_helper(QQuickItem *item, QSizeF *cachedSizeHints, QQuickLayoutAttached **info, bool useFallbackToWidthOrHeight);
    static QLayoutPolicy::Policy effectiveSizePolicy_helper(QQuickItem *item, Qt::Orientation orientation, QQuickLayoutAttached *info);
    bool shouldIgnoreItem(QQuickItem *child, QQuickLayoutAttached *&info, QSizeF *sizeHints) const;

    void itemChange(ItemChange change, const ItemChangeData &value) Q_DECL_OVERRIDE;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)  Q_DECL_OVERRIDE;
    bool isReady() const;


    /* QQuickItemChangeListener */
    void itemSiblingOrderChanged(QQuickItem *item) Q_DECL_OVERRIDE;
    void itemImplicitWidthChanged(QQuickItem *item) Q_DECL_OVERRIDE;
    void itemImplicitHeightChanged(QQuickItem *item) Q_DECL_OVERRIDE;
    void itemDestroyed(QQuickItem *item) Q_DECL_OVERRIDE;
    void itemVisibilityChanged(QQuickItem *item) Q_DECL_OVERRIDE;

protected:
    void updatePolish() Q_DECL_OVERRIDE;

    enum Orientation {
        Vertical = 0,
        Horizontal,
        NOrientations
    };

protected slots:
    void invalidateSenderItem();

private:
    bool m_dirty;

    Q_DECLARE_PRIVATE(QQuickLayout)

    friend class QQuickLayoutAttached;
};


class QQuickLayoutPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickLayout)
public:
    QQuickLayoutPrivate() : m_isReady(false), m_disableRearrange(true) {}

protected:
    unsigned m_isReady : 1;
    unsigned m_disableRearrange : 1;
    mutable QSet<QQuickItem *> m_ignoredItems;
};


class QQuickLayoutAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY minimumWidthChanged)
    Q_PROPERTY(qreal minimumHeight READ minimumHeight WRITE setMinimumHeight NOTIFY minimumHeightChanged)
    Q_PROPERTY(qreal preferredWidth READ preferredWidth WRITE setPreferredWidth NOTIFY preferredWidthChanged)
    Q_PROPERTY(qreal preferredHeight READ preferredHeight WRITE setPreferredHeight NOTIFY preferredHeightChanged)
    Q_PROPERTY(qreal maximumWidth READ maximumWidth WRITE setMaximumWidth NOTIFY maximumWidthChanged)
    Q_PROPERTY(qreal maximumHeight READ maximumHeight WRITE setMaximumHeight NOTIFY maximumHeightChanged)
    Q_PROPERTY(bool fillHeight READ fillHeight WRITE setFillHeight NOTIFY fillHeightChanged)
    Q_PROPERTY(bool fillWidth READ fillWidth WRITE setFillWidth NOTIFY fillWidthChanged)
    Q_PROPERTY(int row READ row WRITE setRow NOTIFY rowChanged)
    Q_PROPERTY(int column READ column WRITE setColumn NOTIFY columnChanged)
    Q_PROPERTY(int rowSpan READ rowSpan WRITE setRowSpan NOTIFY rowSpanChanged)
    Q_PROPERTY(int columnSpan READ columnSpan WRITE setColumnSpan NOTIFY columnSpanChanged)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)

    Q_PROPERTY(qreal margins READ margins WRITE setMargins NOTIFY marginsChanged)
    Q_PROPERTY(qreal leftMargin READ leftMargin WRITE setLeftMargin RESET resetLeftMargin NOTIFY leftMarginChanged)
    Q_PROPERTY(qreal topMargin READ topMargin WRITE setTopMargin RESET resetTopMargin NOTIFY topMarginChanged)
    Q_PROPERTY(qreal rightMargin READ rightMargin WRITE setRightMargin RESET resetRightMargin NOTIFY rightMarginChanged)
    Q_PROPERTY(qreal bottomMargin READ bottomMargin WRITE setBottomMargin RESET resetBottomMargin NOTIFY bottomMarginChanged)

public:
    QQuickLayoutAttached(QObject *object);

    qreal minimumWidth() const { return !m_isMinimumWidthSet ? sizeHint(Qt::MinimumSize, Qt::Horizontal) : m_minimumWidth; }
    void setMinimumWidth(qreal width);

    qreal minimumHeight() const { return !m_isMinimumHeightSet ? sizeHint(Qt::MinimumSize, Qt::Vertical) : m_minimumHeight; }
    void setMinimumHeight(qreal height);

    qreal preferredWidth() const { return m_preferredWidth; }
    void setPreferredWidth(qreal width);

    qreal preferredHeight() const { return m_preferredHeight; }
    void setPreferredHeight(qreal width);

    qreal maximumWidth() const { return !m_isMaximumWidthSet ? sizeHint(Qt::MaximumSize, Qt::Horizontal) : m_maximumWidth; }
    void setMaximumWidth(qreal width);

    qreal maximumHeight() const { return !m_isMaximumHeightSet ? sizeHint(Qt::MaximumSize, Qt::Vertical) : m_maximumHeight; }
    void setMaximumHeight(qreal height);

    void setMinimumImplicitSize(const QSizeF &sz);
    void setMaximumImplicitSize(const QSizeF &sz);

    bool fillWidth() const { return m_fillWidth; }
    void setFillWidth(bool fill);
    bool isFillWidthSet() const { return m_isFillWidthSet; }

    bool fillHeight() const { return m_fillHeight; }
    void setFillHeight(bool fill);
    bool isFillHeightSet() const { return m_isFillHeightSet; }

    int row() const { return qMax(m_row, 0); }
    void setRow(int row);
    bool isRowSet() const { return m_row >= 0; }
    int column() const { return qMax(m_column, 0); }
    void setColumn(int column);
    bool isColumnSet() const { return m_column >= 0; }

    int rowSpan() const { return m_rowSpan; }
    void setRowSpan(int span);
    int columnSpan() const { return m_columnSpan; }
    void setColumnSpan(int span);

    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment align);

    qreal margins() const { return m_defaultMargins; }
    void setMargins(qreal m);

    qreal leftMargin() const { return m_isLeftMarginSet ? m_margins.left() : m_defaultMargins; }
    void setLeftMargin(qreal m);
    void resetLeftMargin();

    qreal topMargin() const { return m_isTopMarginSet ? m_margins.top() : m_defaultMargins; }
    void setTopMargin(qreal m);
    void resetTopMargin();

    qreal rightMargin() const { return m_isRightMarginSet ? m_margins.right() : m_defaultMargins; }
    void setRightMargin(qreal m);
    void resetRightMargin();

    qreal bottomMargin() const { return m_isBottomMarginSet ? m_margins.bottom() : m_defaultMargins; }
    void setBottomMargin(qreal m);
    void resetBottomMargin();

    QMarginsF qMargins() const {
        return QMarginsF(leftMargin(), topMargin(), rightMargin(), bottomMargin());
    }

    bool setChangesNotificationEnabled(bool enabled)
    {
        const bool old = m_changesNotificationEnabled;
        m_changesNotificationEnabled = enabled;
        return old;
    }

    qreal sizeHint(Qt::SizeHint which, Qt::Orientation orientation) const;

    bool isExtentExplicitlySet(Qt::Orientation o, Qt::SizeHint whichSize) const
    {
        switch (whichSize) {
        case Qt::MinimumSize:
            return o == Qt::Horizontal ? m_isMinimumWidthSet : m_isMinimumHeightSet;
        case Qt::MaximumSize:
            return o == Qt::Horizontal ? m_isMaximumWidthSet : m_isMaximumHeightSet;
        case Qt::PreferredSize:
            return true;            // Layout.preferredWidth is always explicitly set
        case Qt::MinimumDescent:    // Not supported
        case Qt::NSizeHints:
            return false;
        }
        return false;
    }

signals:
    void minimumWidthChanged();
    void minimumHeightChanged();
    void preferredWidthChanged();
    void preferredHeightChanged();
    void maximumWidthChanged();
    void maximumHeightChanged();
    void fillWidthChanged();
    void fillHeightChanged();
    void leftMarginChanged();
    void topMarginChanged();
    void rightMarginChanged();
    void bottomMarginChanged();
    void marginsChanged();
    void rowChanged();
    void columnChanged();
    void rowSpanChanged();
    void columnSpanChanged();
    void alignmentChanged();

private:
    void invalidateItem();
    void repopulateLayout();
    QQuickLayout *parentLayout() const;
    QQuickItem *item() const;
private:
    qreal m_minimumWidth;
    qreal m_minimumHeight;
    qreal m_preferredWidth;
    qreal m_preferredHeight;
    qreal m_maximumWidth;
    qreal m_maximumHeight;

    qreal m_defaultMargins;
    QMarginsF m_margins;

    // GridLayout specific properties
    int m_row;
    int m_column;
    int m_rowSpan;
    int m_columnSpan;

    unsigned m_fillWidth : 1;
    unsigned m_fillHeight : 1;
    unsigned m_isFillWidthSet : 1;
    unsigned m_isFillHeightSet : 1;
    unsigned m_isMinimumWidthSet : 1;
    unsigned m_isMinimumHeightSet : 1;
    // preferredWidth and preferredHeight are always explicit, since
    // their implicit equivalent is implicitWidth and implicitHeight
    unsigned m_isMaximumWidthSet : 1;
    unsigned m_isMaximumHeightSet : 1;
    unsigned m_changesNotificationEnabled : 1;
    unsigned m_isLeftMarginSet : 1;
    unsigned m_isTopMarginSet : 1;
    unsigned m_isRightMarginSet : 1;
    unsigned m_isBottomMarginSet : 1;
    Qt::Alignment m_alignment;
    friend class QQuickLayout;
};

inline QQuickLayoutAttached *attachedLayoutObject(QQuickItem *item, bool create = true)
{
    return static_cast<QQuickLayoutAttached *>(qmlAttachedPropertiesObject<QQuickLayout>(item, create));
}

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickLayout)
QML_DECLARE_TYPEINFO(QQuickLayout, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKLAYOUT_P_H
