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
import "../helper.js" as Helper

PlaceDetailsForm {

    property variant place
    property real distanceToPlace

    signal searchForSimilar(variant place)
    signal showReviews(variant place)
    signal showEditorials(variant place)
    signal showImages(variant place)

    function placeAddress(place) {
        if (!place)
            return "";

        if (place.location.address.text.length > 0)
            return place.location.address.text;

        return place.location.address.street;
    }

    function categoryNames(categories) {
        var result = "";

        for (var i = 0; i < categories.length; ++i) {
            if (result == "") {
                result = categories[i].name;
            } else {
                result = result + ", " + categories[i].name;
            }
        }

        return result;
    }

    function additonalInformation(place) {
        var keys = place.extendedAttributes.keys();
        var result;

        for (var i = 0; i < keys.length; ++i) {
            var label = place.extendedAttributes[keys[i]].label;
            var text = place.extendedAttributes[keys[i]].text;
            if (label) {
                result += label + ": "
                if (text)
                    result += text
                result += "<br/>"
            }
        }

        if (!result)
            result = qsTr("No information")

        return result;
    }

    editorialsButton.onClicked: showEditorials(place)
    imagesButton.onClicked: showImages(place)
    reviewsButton.onClicked: showReviews(place)
    findSimilarButton.onClicked: searchForSimilar(place)

    Component.onCompleted: {
        placeName.text = place ? (place.favorite ? place.favorite.name : place.name) : ""
        placeIcon.source = place ? (place.favorite ? place.favorite.icon.url(Qt.size(40,40))
                                        : place.icon.url() == "" ?
                                          "../resources/marker.png"
                                        : place.icon.url(Qt.size(40,40))) : ""
        ratingView.rating = (place && place.ratings) ? place.ratings.average : 0
        distance.text = Helper.formatDistance(distanceToPlace)
        address.text = placeAddress(place)
        categories.text = place ? categoryNames(place.categories) : ""
        phone.text = place ? place.primaryPhone : ""
        fax.text = place ? place.primaryFax : ""
        email.text = place ? place.primaryEmail : ""
        website.text = place ? '<a href=\"' + place.primaryWebsite + '\">' + place.primaryWebsite + '</a>' : ""
        addInformation.text = place ? additonalInformation(place) : ""
        if (place) {
            editorialsButton.enabled = Qt.binding(function(){ return place && place.editorialModel.totalCount > 0 })
            reviewsButton.enabled = Qt.binding(function(){ return place && place.reviewModel.totalCount > 0 })
            imagesButton.enabled = Qt.binding(function(){ return place && place.imageModel.totalCount > 0 })
            findSimilarButton.enabled = true
        }
    }
}

