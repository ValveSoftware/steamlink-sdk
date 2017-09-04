/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.5
import QtLocation 5.6
import QtPositioning 5.5

SearchOptionsForm {
    id: root
    property Plugin plugin
    property PlaceSearchModel model

    signal changeSearchSettings(bool orderByDistance,
                                bool orderByName,
                                string locales)
    signal closeForm()

    setButton.onClicked: changeSearchSettings(distanceOrderButton.checked,
                                              nameOrderButton.checked,
                                              locales.text)

    clearButton.onClicked: {
        locales.text = ""
        distanceOrderButton.checked = false
        nameOrderButton.checked = false
    }

    cancelButton.onClicked: {
        closeForm()
    }

    Component.onCompleted: {
          locales.visible = root.plugin != null && root.plugin.supportsPlaces(Plugin.LocalizedPlacesFeature);
          favoritesButton.visible = false;
//          favoritesButton.enabled = placeSearchModel.favoritesPlugin !== null)
//                        isFavoritesEnabled = true;
          locales.text = root.plugin.locales.join(Qt.locale().groupSeparator);
          distanceOrderButton.checked = model.relevanceHint == PlaceSearchModel.DistanceHint
          nameOrderButton.checked = model.relevanceHint == PlaceSearchModel.LexicalPlaceNameHint
    }
}
