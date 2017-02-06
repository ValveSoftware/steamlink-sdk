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

#ifndef QQUICKABSTRACTCOLORDIALOG_P_H
#define QQUICKABSTRACTCOLORDIALOG_P_H

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

#include <QQuickView>
#include <QtGui/qpa/qplatformdialoghelper.h>
#include <qpa/qplatformtheme.h>
#include "qquickabstractdialog_p.h"

QT_BEGIN_NAMESPACE

class QQuickAbstractColorDialog : public QQuickAbstractDialog
{
    Q_OBJECT
    Q_PROPERTY(bool showAlphaChannel READ showAlphaChannel WRITE setShowAlphaChannel NOTIFY showAlphaChannelChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor NOTIFY currentColorChanged)
    Q_PROPERTY(qreal currentHue READ currentHue NOTIFY currentColorChanged)
    Q_PROPERTY(qreal currentSaturation READ currentSaturation NOTIFY currentColorChanged)
    Q_PROPERTY(qreal currentLightness READ currentLightness NOTIFY currentColorChanged)
    Q_PROPERTY(qreal currentAlpha READ currentAlpha NOTIFY currentColorChanged)

public:
    QQuickAbstractColorDialog(QObject *parent = 0);
    virtual ~QQuickAbstractColorDialog();

    virtual QString title() const;
    bool showAlphaChannel() const;
    QColor color() const { return m_color; }
    QColor currentColor() const { return m_currentColor; }
    qreal currentHue() const { return m_currentColor.hslHueF(); }
    qreal currentSaturation() const { return m_currentColor.hslSaturationF(); }
    qreal currentLightness() const { return m_currentColor.lightnessF(); }
    qreal currentAlpha() const { return m_currentColor.alphaF(); }

public Q_SLOTS:
    void setVisible(bool v);
    void setModality(Qt::WindowModality m);
    void setTitle(const QString &t);
    void setColor(QColor arg);
    void setCurrentColor(QColor currentColor);
    void setShowAlphaChannel(bool arg);

Q_SIGNALS:
    void showAlphaChannelChanged();
    void colorChanged();
    void currentColorChanged();
    void selectionAccepted();

protected Q_SLOTS:
    virtual void accept();

protected:
    QPlatformColorDialogHelper *m_dlgHelper;
    QSharedPointer<QColorDialogOptions> m_options;
    QColor m_color;
    QColor m_currentColor;

    Q_DISABLE_COPY(QQuickAbstractColorDialog)
};

QT_END_NAMESPACE

#endif // QQUICKABSTRACTCOLORDIALOG_P_H
