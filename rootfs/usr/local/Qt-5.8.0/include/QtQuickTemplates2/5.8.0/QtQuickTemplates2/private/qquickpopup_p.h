/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#ifndef QQUICKPOPUP_P_H
#define QQUICKPOPUP_P_H

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

#include <QtCore/qobject.h>
#include <QtCore/qmargins.h>
#include <QtGui/qevent.h>
#include <QtCore/qlocale.h>
#include <QtGui/qfont.h>
#include <QtQuickTemplates2/private/qtquicktemplates2global_p.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmllist.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQuick/qquickitem.h>

#ifndef QT_NO_ACCESSIBILITY
#include <QtGui/qaccessible.h>
#endif

QT_BEGIN_NAMESPACE

class QQuickWindow;
class QQuickPopupPrivate;
class QQuickTransition;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickPopup : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY xChanged FINAL)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY yChanged FINAL)
    Q_PROPERTY(qreal z READ z WRITE setZ NOTIFY zChanged FINAL)
    Q_PROPERTY(qreal width READ width WRITE setWidth RESET resetWidth NOTIFY widthChanged FINAL)
    Q_PROPERTY(qreal height READ height WRITE setHeight RESET resetHeight NOTIFY heightChanged FINAL)
    Q_PROPERTY(qreal implicitWidth READ implicitWidth WRITE setImplicitWidth NOTIFY implicitWidthChanged FINAL)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight WRITE setImplicitHeight NOTIFY implicitHeightChanged FINAL)
    Q_PROPERTY(qreal contentWidth READ contentWidth WRITE setContentWidth NOTIFY contentWidthChanged FINAL)
    Q_PROPERTY(qreal contentHeight READ contentHeight WRITE setContentHeight NOTIFY contentHeightChanged FINAL)
    Q_PROPERTY(qreal availableWidth READ availableWidth NOTIFY availableWidthChanged FINAL)
    Q_PROPERTY(qreal availableHeight READ availableHeight NOTIFY availableHeightChanged FINAL)
    Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing RESET resetSpacing NOTIFY spacingChanged FINAL REVISION 1)
    Q_PROPERTY(qreal margins READ margins WRITE setMargins RESET resetMargins NOTIFY marginsChanged FINAL)
    Q_PROPERTY(qreal topMargin READ topMargin WRITE setTopMargin RESET resetTopMargin NOTIFY topMarginChanged FINAL)
    Q_PROPERTY(qreal leftMargin READ leftMargin WRITE setLeftMargin RESET resetLeftMargin NOTIFY leftMarginChanged FINAL)
    Q_PROPERTY(qreal rightMargin READ rightMargin WRITE setRightMargin RESET resetRightMargin NOTIFY rightMarginChanged FINAL)
    Q_PROPERTY(qreal bottomMargin READ bottomMargin WRITE setBottomMargin RESET resetBottomMargin NOTIFY bottomMarginChanged FINAL)
    Q_PROPERTY(qreal padding READ padding WRITE setPadding RESET resetPadding NOTIFY paddingChanged FINAL)
    Q_PROPERTY(qreal topPadding READ topPadding WRITE setTopPadding RESET resetTopPadding NOTIFY topPaddingChanged FINAL)
    Q_PROPERTY(qreal leftPadding READ leftPadding WRITE setLeftPadding RESET resetLeftPadding NOTIFY leftPaddingChanged FINAL)
    Q_PROPERTY(qreal rightPadding READ rightPadding WRITE setRightPadding RESET resetRightPadding NOTIFY rightPaddingChanged FINAL)
    Q_PROPERTY(qreal bottomPadding READ bottomPadding WRITE setBottomPadding RESET resetBottomPadding NOTIFY bottomPaddingChanged FINAL)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale RESET resetLocale NOTIFY localeChanged FINAL)
    Q_PROPERTY(QFont font READ font WRITE setFont RESET resetFont NOTIFY fontChanged FINAL)
    Q_PROPERTY(QQuickItem *parent READ parentItem WRITE setParentItem NOTIFY parentChanged FINAL)
    Q_PROPERTY(QQuickItem *background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)
    Q_PROPERTY(QQuickItem *contentItem READ contentItem WRITE setContentItem NOTIFY contentItemChanged FINAL)
    Q_PROPERTY(QQmlListProperty<QObject> contentData READ contentData FINAL)
    Q_PROPERTY(QQmlListProperty<QQuickItem> contentChildren READ contentChildren NOTIFY contentChildrenChanged FINAL)
    Q_PROPERTY(bool clip READ clip WRITE setClip NOTIFY clipChanged FINAL)
    Q_PROPERTY(bool focus READ hasFocus WRITE setFocus NOTIFY focusChanged FINAL)
    Q_PROPERTY(bool activeFocus READ hasActiveFocus NOTIFY activeFocusChanged FINAL)
    Q_PROPERTY(bool modal READ isModal WRITE setModal NOTIFY modalChanged FINAL)
    Q_PROPERTY(bool dim READ dim WRITE setDim RESET resetDim NOTIFY dimChanged FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged FINAL)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged FINAL)
    Q_PROPERTY(ClosePolicy closePolicy READ closePolicy WRITE setClosePolicy NOTIFY closePolicyChanged FINAL)
    Q_PROPERTY(TransformOrigin transformOrigin READ transformOrigin WRITE setTransformOrigin)
    Q_PROPERTY(QQuickTransition *enter READ enter WRITE setEnter NOTIFY enterChanged FINAL)
    Q_PROPERTY(QQuickTransition *exit READ exit WRITE setExit NOTIFY exitChanged FINAL)
    Q_CLASSINFO("DefaultProperty", "contentData")

public:
    explicit QQuickPopup(QObject *parent = nullptr);
    ~QQuickPopup();

    qreal x() const;
    void setX(qreal x);

    qreal y() const;
    void setY(qreal y);

    QPointF position() const;
    void setPosition(const QPointF &pos);

    qreal z() const;
    void setZ(qreal z);

    qreal width() const;
    void setWidth(qreal width);
    void resetWidth();

    qreal height() const;
    void setHeight(qreal height);
    void resetHeight();

    qreal implicitWidth() const;
    void setImplicitWidth(qreal width);

    qreal implicitHeight() const;
    void setImplicitHeight(qreal height);

    qreal contentWidth() const;
    void setContentWidth(qreal width);

    qreal contentHeight() const;
    void setContentHeight(qreal height);

    qreal availableWidth() const;
    qreal availableHeight() const;

    qreal spacing() const;
    void setSpacing(qreal spacing);
    void resetSpacing();

    qreal margins() const;
    void setMargins(qreal margins);
    void resetMargins();

    qreal topMargin() const;
    void setTopMargin(qreal margin);
    void resetTopMargin();

    qreal leftMargin() const;
    void setLeftMargin(qreal margin);
    void resetLeftMargin();

    qreal rightMargin() const;
    void setRightMargin(qreal margin);
    void resetRightMargin();

    qreal bottomMargin() const;
    void setBottomMargin(qreal margin);
    void resetBottomMargin();

    qreal padding() const;
    void setPadding(qreal padding);
    void resetPadding();

    qreal topPadding() const;
    void setTopPadding(qreal padding);
    void resetTopPadding();

    qreal leftPadding() const;
    void setLeftPadding(qreal padding);
    void resetLeftPadding();

    qreal rightPadding() const;
    void setRightPadding(qreal padding);
    void resetRightPadding();

    qreal bottomPadding() const;
    void setBottomPadding(qreal padding);
    void resetBottomPadding();

    QLocale locale() const;
    void setLocale(const QLocale &locale);
    void resetLocale();

    QFont font() const;
    void setFont(const QFont &font);
    void resetFont();

    QQuickWindow *window() const;
    QQuickItem *popupItem() const;

    QQuickItem *parentItem() const;
    void setParentItem(QQuickItem *parent);

    QQuickItem *background() const;
    void setBackground(QQuickItem *background);

    QQuickItem *contentItem() const;
    void setContentItem(QQuickItem *item);

    QQmlListProperty<QObject> contentData();
    QQmlListProperty<QQuickItem> contentChildren();

    bool clip() const;
    void setClip(bool clip);

    bool hasFocus() const;
    void setFocus(bool focus);

    bool hasActiveFocus() const;

    bool isModal() const;
    void setModal(bool modal);

    bool dim() const;
    void setDim(bool dim);
    void resetDim();

    bool isVisible() const;
    virtual void setVisible(bool visible);

    qreal opacity() const;
    void setOpacity(qreal opacity);

    qreal scale() const;
    void setScale(qreal scale);

    enum ClosePolicyFlag {
        NoAutoClose = 0x00,
        CloseOnPressOutside = 0x01,
        CloseOnPressOutsideParent = 0x02,
        CloseOnReleaseOutside = 0x04,
        CloseOnReleaseOutsideParent = 0x08,
        CloseOnEscape = 0x10
    };
    Q_DECLARE_FLAGS(ClosePolicy, ClosePolicyFlag)
    Q_FLAG(ClosePolicy)

    ClosePolicy closePolicy() const;
    void setClosePolicy(ClosePolicy policy);

    // keep in sync with Item.TransformOrigin
    enum TransformOrigin {
        TopLeft, Top, TopRight,
        Left, Center, Right,
        BottomLeft, Bottom, BottomRight
    };
    Q_ENUM(TransformOrigin)

    TransformOrigin transformOrigin() const;
    void setTransformOrigin(TransformOrigin);

    QQuickTransition *enter() const;
    void setEnter(QQuickTransition *transition);

    QQuickTransition *exit() const;
    void setExit(QQuickTransition *transition);

    bool filtersChildMouseEvents() const;
    void setFiltersChildMouseEvents(bool filter);

    Q_INVOKABLE void forceActiveFocus(Qt::FocusReason reason = Qt::OtherFocusReason);

public Q_SLOTS:
    void open();
    void close();

Q_SIGNALS:
    void xChanged();
    void yChanged();
    void zChanged();
    void widthChanged();
    void heightChanged();
    void implicitWidthChanged();
    void implicitHeightChanged();
    void contentWidthChanged();
    void contentHeightChanged();
    void availableWidthChanged();
    void availableHeightChanged();
    Q_REVISION(1) void spacingChanged();
    void marginsChanged();
    void topMarginChanged();
    void leftMarginChanged();
    void rightMarginChanged();
    void bottomMarginChanged();
    void paddingChanged();
    void topPaddingChanged();
    void leftPaddingChanged();
    void rightPaddingChanged();
    void bottomPaddingChanged();
    void fontChanged();
    void localeChanged();
    void parentChanged();
    void backgroundChanged();
    void contentItemChanged();
    void contentChildrenChanged();
    void clipChanged();
    void focusChanged();
    void activeFocusChanged();
    void modalChanged();
    void dimChanged();
    void visibleChanged();
    void opacityChanged();
    void scaleChanged();
    void closePolicyChanged();
    void enterChanged();
    void exitChanged();
    void windowChanged(QQuickWindow *window);

    void aboutToShow();
    void aboutToHide();
    void opened();
    void closed();

protected:
    QQuickPopup(QQuickPopupPrivate &dd, QObject *parent);

    void classBegin() override;
    void componentComplete() override;
    bool isComponentComplete() const;

    virtual bool childMouseEventFilter(QQuickItem *child, QEvent *event);
    virtual void focusInEvent(QFocusEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void mouseUngrabEvent();
    virtual bool overlayEvent(QQuickItem *item, QEvent *event);
    virtual void wheelEvent(QWheelEvent *event);

    virtual void contentItemChange(QQuickItem *newItem, QQuickItem *oldItem);
    virtual void fontChange(const QFont &newFont, const QFont &oldFont);
    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);
    virtual void localeChange(const QLocale &newLocale, const QLocale &oldLocale);
    virtual void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data);
    virtual void marginsChange(const QMarginsF &newMargins, const QMarginsF &oldMargins);
    virtual void paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding);
    virtual void spacingChange(qreal newSpacing, qreal oldSpacing);

    virtual QFont defaultFont() const;

#ifndef QT_NO_ACCESSIBILITY
    virtual QAccessible::Role accessibleRole() const;
    virtual void accessibilityActiveChanged(bool active);
#endif

    QString accessibleName() const;
    void setAccessibleName(const QString &name);

    QVariant accessibleProperty(const char *propertyName);
    bool setAccessibleProperty(const char *propertyName, const QVariant &value);

private:
    Q_DISABLE_COPY(QQuickPopup)
    Q_DECLARE_PRIVATE(QQuickPopup)
    friend class QQuickPopupItem;
    friend class QQuickOverlay;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPopup::ClosePolicy)

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPopup)

#endif // QQUICKPOPUP_P_H
