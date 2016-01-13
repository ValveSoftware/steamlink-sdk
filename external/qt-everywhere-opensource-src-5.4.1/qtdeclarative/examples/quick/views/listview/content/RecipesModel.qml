/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

import QtQuick 2.0

ListModel {
    ListElement {
        title: "Pancakes"
        picture: "content/pics/pancakes.jpg"
        ingredients: "<html>
                       <ul>
                        <li> 1 cup (150g) self-raising flour
                        <li> 1 tbs caster sugar
                        <li> 3/4 cup (185ml) milk
                        <li> 1 egg
                       </ul>
                      </html>"
        method: "<html>
                  <ol>
                   <li> Sift flour and sugar together into a bowl. Add a pinch of salt.
                   <li> Beat milk and egg together, then add to dry ingredients. Beat until smooth.
                   <li> Pour mixture into a pan on medium heat and cook until bubbles appear on the surface.
                   <li> Turn over and cook other side until golden.
                  </ol>
                 </html>"
    }
    ListElement {
        title: "Fruit Salad"
        picture: "content/pics/fruit-salad.jpg"
        ingredients: "* Seasonal Fruit"
        method: "* Chop fruit and place in a bowl."
    }
    ListElement {
        title: "Vegetable Soup"
        picture: "content/pics/vegetable-soup.jpg"
        ingredients: "<html>
                       <ul>
                        <li> 1 onion
                        <li> 1 turnip
                        <li> 1 potato
                        <li> 1 carrot
                        <li> 1 head of celery
                        <li> 1 1/2 litres of water
                       </ul>
                      </html>"
        method: "<html>
                  <ol>
                   <li> Chop vegetables.
                   <li> Boil in water until vegetables soften.
                   <li> Season with salt and pepper to taste.
                  </ol>
                 </html>"
    }
    ListElement {
        title: "Hamburger"
        picture: "content/pics/hamburger.jpg"
        ingredients: "<html>
                       <ul>
                        <li> 500g minced beef
                        <li> Seasoning
                        <li> lettuce, tomato, onion, cheese
                        <li> 1 hamburger bun for each burger
                       </ul>
                      </html>"
        method: "<html>
                  <ol>
                   <li> Mix the beef, together with seasoning, in a food processor.
                   <li> Shape the beef into burgers.
                   <li> Grill the burgers for about 5 mins on each side (until cooked through)
                   <li> Serve each burger on a bun with ketchup, cheese, lettuce, tomato and onion.
                  </ol>
                 </html>"
    }
    ListElement {
        title: "Lemonade"
        picture: "content/pics/lemonade.jpg"
        ingredients: "<html>
                       <ul>
                        <li> 1 cup Lemon Juice
                        <li> 1 cup Sugar
                        <li> 6 Cups of Water (2 cups warm water, 4 cups cold water)
                       </ul>
                      </html>"
        method: "<html>
                  <ol>
                   <li> Pour 2 cups of warm water into a pitcher and stir in sugar until it dissolves.
                   <li> Pour in lemon juice, stir again, and add 4 cups of cold water.
                   <li> Chill or serve over ice cubes.
                  </ol>
                 </html>"
    }
}
