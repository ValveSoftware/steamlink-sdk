/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

protected:
    QPlatformColorDialogHelper *m_dlgHelper;
    QSharedPointer<QColorDialogOptions> m_options;
    QColor m_color;
    QColor m_currentColor;

    Q_DISABLE_COPY(QQuickAbstractColorDialog)
};

QT_END_NAMESPACE

#endif // QQUICKABSTRACTCOLORDIALOG_P_H
