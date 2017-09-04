/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "messagebubblewidget_p.h"

#include "api/qwebengineview.h"

#include <QBitmap>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QStyle>

namespace QtWebEngineWidgetUI {

Q_GLOBAL_STATIC(MessageBubbleWidget, bubbleInstance)

void MessageBubbleWidget::showBubble(QWebEngineView *view, const QRect &anchor, const QString &mainText, const QString &subText)
{
    hideBubble();
    if (mainText.isEmpty())
        return;

    bubbleInstance->createBubble(anchor.size().width(), mainText, subText);
    bubbleInstance->moveToAnchor(view, anchor);
}

void MessageBubbleWidget::hideBubble()
{
    bubbleInstance->hide();
}

void MessageBubbleWidget::moveBubble(QWebEngineView *view, const QRect &anchor)
{
    bubbleInstance->moveToAnchor(view, anchor);
}

MessageBubbleWidget::MessageBubbleWidget()
   : QWidget(0, Qt::ToolTip)
   , m_mainLabel(new QLabel)
   , m_subLabel(new QLabel)
{
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setAlignment(Qt::AlignTop);
    hLayout->setSizeConstraint(QLayout::SetFixedSize);
    hLayout->setMargin(3);

    const int iconSize = 18;
    QIcon si = style()->standardIcon(QStyle::SP_MessageBoxWarning);

    if (!si.isNull()) {
        QLabel *iconLabel = new QLabel(this);
        iconLabel->setPixmap(si.pixmap(iconSize, iconSize));
        iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        iconLabel->setMargin(2);
        hLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    }

    QVBoxLayout *vLayout = new QVBoxLayout;

    m_mainLabel->installEventFilter(this);
    m_mainLabel->setWordWrap(true);
    m_mainLabel->setTextFormat(Qt::PlainText);
    m_mainLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    vLayout->addWidget(m_mainLabel.data());

    QFont mainFont = m_mainLabel->font();
    mainFont.setPointSize(mainFont.pointSize() + 4);
    m_mainLabel->setFont(mainFont);

    m_subLabel->installEventFilter(this);
    m_subLabel->setWordWrap(true);
    m_subLabel->setTextFormat(Qt::PlainText);
    m_subLabel->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    vLayout->addWidget(m_subLabel.data());

    hLayout->addLayout(vLayout);
    setLayout(hLayout);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xff, 0xff, 0xe1));
    pal.setColor(QPalette::WindowText, Qt::black);
    setPalette(pal);
}

MessageBubbleWidget::~MessageBubbleWidget()
{
}

void MessageBubbleWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawPixmap(rect(), m_pixmap);
}

void MessageBubbleWidget::createBubble(const int maxWidth, const QString &mainText, const QString &subText)
{
    m_mainLabel->setText(mainText);
    m_mainLabel->setMaximumWidth(maxWidth);

    m_subLabel->setText(subText);
    m_subLabel->setMaximumWidth(maxWidth);
    m_subLabel->setVisible(!subText.isEmpty());

    int border = 1;
    int arrowHeight = 18;
    bool roundedCorners = true;

#if defined(QT_NO_XSHAPE) && defined(Q_WS_X11)
    // XShape is required for setting the mask, so we just
    // draw an ugly square when its not available
    arrowHeight = 0;
    roundedCorners = false;
#endif

    setContentsMargins(border + 3,  border + arrowHeight + 2, border + 3, border + 2);
    show(); // The widget should be visible for updateGeometry()
    updateGeometry();
    m_pixmap = QPixmap(sizeHint());

    QPainterPath path = drawBoxPath(QPoint(0, arrowHeight), border, roundedCorners);

    // Draw border and set background
    QPainter painter(&m_pixmap);
    painter.setPen(QPen(palette().color(QPalette::Window).darker(160), border));
    painter.setBrush(palette().color(QPalette::Window));
    painter.drawPath(path);
}

void MessageBubbleWidget::moveToAnchor(QWebEngineView *view, const QRect &anchor)
{
    QPoint topLeft = view->mapToGlobal(anchor.topLeft());
    move(topLeft.x(), topLeft.y() + anchor.height());
}

QPainterPath MessageBubbleWidget::drawBoxPath(const QPoint &pos, int border, bool roundedCorners)
{
    const int arrowHeight = pos.y();
    const int arrowOffset = 18;
    const int arrowWidth = 18;

    const int cornerRadius = roundedCorners ? 7 : 0;

    const int messageBoxLeft = pos.x();
    const int messageBoxTop = arrowHeight;
    const int messageBoxRight = m_pixmap.width() - 1;
    const int messageBoxBottom = m_pixmap.height() - 1;

    QPainterPath path;
    path.moveTo(messageBoxLeft + cornerRadius, messageBoxTop);

    if (arrowHeight) {
        path.lineTo(messageBoxLeft + arrowOffset, messageBoxTop);
        path.lineTo(messageBoxLeft + arrowOffset, messageBoxTop - arrowHeight);
        path.lineTo(messageBoxLeft + arrowOffset + arrowWidth, messageBoxTop);
    }

    if (roundedCorners) {
        path.lineTo(messageBoxRight - cornerRadius, messageBoxTop);
        path.quadTo(messageBoxRight, messageBoxTop, messageBoxRight, messageBoxTop + cornerRadius);
        path.lineTo(messageBoxRight, messageBoxBottom - cornerRadius);
        path.quadTo(messageBoxRight, messageBoxBottom, messageBoxRight - cornerRadius, messageBoxBottom);
        path.lineTo(messageBoxLeft + cornerRadius, messageBoxBottom);
        path.quadTo(messageBoxLeft, messageBoxBottom, messageBoxLeft, messageBoxBottom - cornerRadius);
        path.lineTo(messageBoxLeft, messageBoxTop + cornerRadius);
        path.quadTo(messageBoxLeft, messageBoxTop, messageBoxLeft + cornerRadius, messageBoxTop);
    } else {
        path.lineTo(messageBoxRight, messageBoxTop);
        path.lineTo(messageBoxRight, messageBoxBottom);
        path.lineTo(messageBoxLeft, messageBoxBottom);
        path.moveTo(messageBoxLeft, messageBoxTop);
    }

    // Set mask
    if (arrowHeight || roundedCorners) {
        QBitmap bitmap = QBitmap(sizeHint());
        bitmap.fill(Qt::color0);
        QPainter painter(&bitmap);
        painter.setPen(QPen(Qt::color1, border));
        painter.setBrush(QBrush(Qt::color1));
        painter.drawPath(path);
        setMask(bitmap);
    }

    return path;
}

} // namespace QtWebEngineWidgetUI
