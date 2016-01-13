/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Layouts module of the Qt Toolkit.
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

#include "qquickitem.h"
#include "qquickgridlayoutengine_p.h"
#include "qquicklayout_p.h"

QT_BEGIN_NAMESPACE

/*
  The layout engine assumes:
    1. minimum <= preferred <= maximum
    2. descent is within minimum and maximum bounds     (### verify)

    This function helps to ensure that by the following rules (in the following order):
    1. If minimum > maximum, set minimum = maximum
    2. Make sure preferred is not outside the [minimum,maximum] range.
    3. If descent > minimum, set descent = minimum      (### verify if this is correct, it might
                                                        need some refinements to multiline texts)

    If any values are "not set" (i.e. negative), they will be left untouched, so that we
    know which values needs to be fetched from the implicit hints (not user hints).
  */
static void normalizeHints(qreal &minimum, qreal &preferred, qreal &maximum, qreal &descent)
{
    if (minimum >= 0 && maximum >= 0 && minimum > maximum)
        minimum = maximum;

    if (preferred >= 0) {
        if (minimum >= 0 && preferred < minimum) {
            preferred = minimum;
        } else if (maximum >= 0 && preferred > maximum) {
            preferred = maximum;
        }
    }

    if (minimum >= 0 && descent > minimum)
        descent = minimum;
}

static void boundSize(QSizeF &result, const QSizeF &size)
{
    if (size.width() >= 0 && size.width() < result.width())
        result.setWidth(size.width());
    if (size.height() >= 0 && size.height() < result.height())
        result.setHeight(size.height());
}

static void expandSize(QSizeF &result, const QSizeF &size)
{
    if (size.width() >= 0 && size.width() > result.width())
        result.setWidth(size.width());
    if (size.height() >= 0 && size.height() > result.height())
        result.setHeight(size.height());
}

static inline void combineHints(qreal &current, qreal fallbackHint)
{
    if (current < 0)
        current = fallbackHint;
}

static inline void combineSize(QSizeF &result, const QSizeF &fallbackSize)
{
    combineHints(result.rwidth(), fallbackSize.width());
    combineHints(result.rheight(), fallbackSize.height());
}

static inline void combineImplicitHints(QQuickLayoutAttached *info, Qt::SizeHint which, QSizeF *size)
{
    if (!info) return;

    Q_ASSERT(which == Qt::MinimumSize || which == Qt::MaximumSize);

    const QSizeF constraint(which == Qt::MinimumSize
                            ? QSizeF(info->minimumWidth(), info->minimumHeight())
                            : QSizeF(info->maximumWidth(), info->maximumHeight()));

    if (!info->isExtentExplicitlySet(Qt::Horizontal, which))
        combineHints(size->rwidth(),  constraint.width());
    if (!info->isExtentExplicitlySet(Qt::Vertical, which))
        combineHints(size->rheight(), constraint.height());
}

/*!
    \internal
    Note: Can potentially return the attached QQuickLayoutAttached object through \a attachedInfo.

    It is like this is because it enables it to be reused.

    The goal of this function is to return the effective minimum, preferred and maximum size hints
    that the layout will use for this item.
    This function takes care of gathering all explicitly set size hints, normalizes them so
    that min < pref < max.
    Further, the hints _not_explicitly_ set will then be initialized with the implicit size hints,
    which is usually derived from the content of the layouts (or items).

    The following table illustrates the preference of the properties used for measuring layout
    items. If present, the USER properties will be preferred. If USER properties are not present,
    the HINT properties will be preferred. Finally, the FALLBACK properties will be used as an
    ultimate fallback.

    Note that one can query if the value of Layout.minimumWidth or Layout.maximumWidth has been
    explicitly or implicitly set with QQuickLayoutAttached::isExtentExplicitlySet(). This
    determines if it should be used as a USER or as a HINT value.

    Fractional size hints will be ceiled to the closest integer. This is in order to give some
    slack when the items are snapped to the pixel grid.

                 | *Minimum*            | *Preferred*           | *Maximum*                |
+----------------+----------------------+-----------------------+--------------------------+
|USER (explicit) | Layout.minimumWidth  | Layout.preferredWidth | Layout.maximumWidth      |
|HINT (implicit) | Layout.minimumWidth  | implicitWidth         | Layout.maximumWidth      |
|FALLBACK        | 0                    | width                 | Number.POSITIVE_INFINITY |
+----------------+----------------------+-----------------------+--------------------------+
 */
void QQuickGridLayoutItem::effectiveSizeHints_helper(QQuickItem *item, QSizeF *cachedSizeHints, QQuickLayoutAttached **attachedInfo, bool useFallbackToWidthOrHeight)
{
    for (int i = 0; i < Qt::NSizeHints; ++i)
        cachedSizeHints[i] = QSizeF();
    QQuickLayoutAttached *info = attachedLayoutObject(item, false);
    // First, retrieve the user-specified hints from the attached "Layout." properties
    if (info) {
        struct Getters {
            SizeGetter call[NSizes];
        };

        static Getters horGetters = {
            {&QQuickLayoutAttached::minimumWidth, &QQuickLayoutAttached::preferredWidth, &QQuickLayoutAttached::maximumWidth},
        };

        static Getters verGetters = {
            {&QQuickLayoutAttached::minimumHeight, &QQuickLayoutAttached::preferredHeight, &QQuickLayoutAttached::maximumHeight}
        };
        for (int i = 0; i < NSizes; ++i) {
            SizeGetter getter = horGetters.call[i];
            Q_ASSERT(getter);

            if (info->isExtentExplicitlySet(Qt::Horizontal, (Qt::SizeHint)i))
                cachedSizeHints[i].setWidth(qCeil((info->*getter)()));

            getter = verGetters.call[i];
            Q_ASSERT(getter);
            if (info->isExtentExplicitlySet(Qt::Vertical, (Qt::SizeHint)i))
                cachedSizeHints[i].setHeight(qCeil((info->*getter)()));
        }
    }

    QSizeF &minS = cachedSizeHints[Qt::MinimumSize];
    QSizeF &prefS = cachedSizeHints[Qt::PreferredSize];
    QSizeF &maxS = cachedSizeHints[Qt::MaximumSize];
    QSizeF &descentS = cachedSizeHints[Qt::MinimumDescent];

    // For instance, will normalize the following user-set hints
    // from: [10, 5, 60]
    // to:   [10, 10, 60]
    normalizeHints(minS.rwidth(), prefS.rwidth(), maxS.rwidth(), descentS.rwidth());
    normalizeHints(minS.rheight(), prefS.rheight(), maxS.rheight(), descentS.rheight());

    // All explicit values gathered, now continue to gather the implicit sizes

    //--- GATHER MAXIMUM SIZE HINTS ---
    combineImplicitHints(info, Qt::MaximumSize, &maxS);
    combineSize(maxS, QSizeF(std::numeric_limits<qreal>::infinity(), std::numeric_limits<qreal>::infinity()));
    // implicit max or min sizes should not limit an explicitly set preferred size
    expandSize(maxS, prefS);
    expandSize(maxS, minS);

    //--- GATHER MINIMUM SIZE HINTS ---
    combineImplicitHints(info, Qt::MinimumSize, &minS);
    expandSize(minS, QSizeF(0,0));
    boundSize(minS, prefS);
    boundSize(minS, maxS);

    //--- GATHER PREFERRED SIZE HINTS ---
    // First, from implicitWidth/Height
    qreal &prefWidth = prefS.rwidth();
    qreal &prefHeight = prefS.rheight();
    if (prefWidth < 0 && item->implicitWidth() > 0)
        prefWidth = qCeil(item->implicitWidth());
    if (prefHeight < 0 &&  item->implicitHeight() > 0)
        prefHeight = qCeil(item->implicitHeight());

    // If that fails, make an ultimate fallback to width/height

    if (!info && (prefWidth < 0 || prefHeight < 0))
        info = attachedLayoutObject(item);

    if (useFallbackToWidthOrHeight && info) {
        /* This block is a bit hacky, but if we want to support using width/height
           as preferred size hints in layouts, (which we think most people expect),
           we only want to use the initial width.
           This is because the width will change due to layout rearrangement, and the preferred
           width should return the same value, regardless of the current width.
           We therefore store the width in the implicitWidth attached property.
           Since the layout listens to changes of implicitWidth, (it will
           basically cause an invalidation of the layout), we have to disable that
           notification while we set the implicit width (and height).

           Only use this fallback the first time the size hint is queried. Otherwise, we might
           end up picking a width that is different than what was specified in the QML.
        */
        if (prefWidth < 0 || prefHeight < 0) {
            item->blockSignals(true);
            if (prefWidth < 0) {
                prefWidth = item->width();
                item->setImplicitWidth(prefWidth);
            }
            if (prefHeight < 0) {
                prefHeight = item->height();
                item->setImplicitHeight(prefHeight);
            }
            item->blockSignals(false);
        }
    }



    // Normalize again after the implicit hints have been gathered
    expandSize(prefS, minS);
    boundSize(prefS, maxS);

    //--- GATHER DESCENT
    // Minimum descent is only applicable for the effective minimum height,
    // so we gather the descent last.
    const qreal minimumDescent = minS.height() - item->baselineOffset();
    descentS.setHeight(minimumDescent);

    if (attachedInfo)
        *attachedInfo = info;
}

/*!
    \internal

    Assumes \a info is set (if the object has an attached property)
 */
QLayoutPolicy::Policy QQuickGridLayoutItem::effectiveSizePolicy_helper(QQuickItem *item, Qt::Orientation orientation, QQuickLayoutAttached *info)
{
    bool fillExtent = false;
    bool isSet = false;
    if (info) {
        if (orientation == Qt::Horizontal) {
            isSet = info->isFillWidthSet();
            if (isSet) fillExtent = info->fillWidth();
        } else {
            isSet = info->isFillHeightSet();
            if (isSet) fillExtent = info->fillHeight();
        }
    }
    if (!isSet && qobject_cast<QQuickLayout*>(item))
        fillExtent = true;
    return fillExtent ? QLayoutPolicy::Preferred : QLayoutPolicy::Fixed;

}

void QQuickGridLayoutEngine::setAlignment(QQuickItem *quickItem, Qt::Alignment alignment)
{
    if (QQuickGridLayoutItem *item = findLayoutItem(quickItem)) {
        item->setAlignment(alignment);
        invalidate();
    }
}

Qt::Alignment QQuickGridLayoutEngine::alignment(QQuickItem *quickItem) const
{
    if (QGridLayoutItem *item = findLayoutItem(quickItem))
        return item->alignment();
    return 0;
}

QT_END_NAMESPACE
