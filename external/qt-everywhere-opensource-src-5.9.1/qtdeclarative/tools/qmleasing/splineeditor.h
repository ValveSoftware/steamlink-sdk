/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SPLINEEDITOR_H
#define SPLINEEDITOR_H

#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QScrollArea>

#include <QEasingCurve>
#include <QHash>

class SegmentProperties;

class SplineEditor : public QWidget
{
    Q_OBJECT

     Q_PROPERTY(QEasingCurve easingCurve READ easingCurve WRITE setEasingCurve NOTIFY easingCurveChanged);

public:
    explicit SplineEditor(QWidget *parent = 0);
    QString generateCode();
    QStringList presetNames() const;
    QWidget *pointListWidget();

    void setControlPoint(int index, const QPointF &point)
    {
        m_controlPoints[index] = point;
        update();
    }

    void setSmooth(int index, bool smooth)
    {
        m_smoothAction->setChecked(smooth);
        smoothPoint(index * 3 + 2);
        //update();
    }

signals:
    void easingCurveChanged();
    void easingCurveCodeChanged(const QString &code);


public slots:
    void setEasingCurve(const QEasingCurve &easingCurve);
    void setPreset(const QString &name);
    void setEasingCurve(const QString &code);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
#if QT_CONFIG(contextmenu)
    void contextMenuEvent(QContextMenuEvent *) override;
#endif // contextmenu

    void invalidate();
    void invalidateSmoothList();
    void invalidateSegmentProperties();

    QEasingCurve easingCurve() const
    { return m_easingCurve; }

    QHash<QString, QEasingCurve> presets() const;

private:
    int findControlPoint(const QPoint &point);
    bool isSmooth(int i) const;

    void smoothPoint( int index);
    void cornerPoint( int index);
    void deletePoint(int index);
    void addPoint(const QPointF point);

    void initPresets();

    void setupPointListWidget();

    bool isControlPointSmooth(int i) const;

    QEasingCurve m_easingCurve;
    QVector<QPointF> m_controlPoints;
    QVector<bool> m_smoothList;
    int m_numberOfSegments;
    int m_activeControlPoint;
    bool m_mouseDrag;
    QPoint m_mousePress;
    QHash<QString, QEasingCurve> m_presets;

    QMenu *m_pointContextMenu;
    QMenu *m_curveContextMenu;
    QAction *m_deleteAction;
    QAction *m_smoothAction;
    QAction *m_cornerAction;
    QAction *m_addPoint;

    QScrollArea *m_pointListWidget;

    QList<SegmentProperties*> m_segmentProperties;
    bool m_block;
};

#endif // SPLINEEDITOR_H
