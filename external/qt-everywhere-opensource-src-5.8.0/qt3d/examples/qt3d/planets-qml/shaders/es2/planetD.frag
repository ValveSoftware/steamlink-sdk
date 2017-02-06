/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

uniform highp mat4 viewMatrix;

uniform highp vec3 lightPosition;
uniform highp vec3 lightIntensity;

uniform highp vec3 ka;            // Ambient reflectivity
uniform highp vec3 ks;            // Specular reflectivity
uniform highp float shininess;    // Specular shininess factor
uniform highp float opacity;      // Alpha channel

uniform sampler2D diffuseTexture;

varying highp vec4 positionInLightSpace;

varying highp vec3 position;
varying highp vec3 normal;
varying highp vec2 texCoord;

highp vec3 dModel(const highp vec2 flipYTexCoord)
{
    // Calculate the vector from the light to the fragment
    highp vec3 s = normalize(vec3(viewMatrix * vec4(lightPosition, 1.0)) - position);

    // Calculate the vector from the fragment to the eye position
    // (origin since this is in "eye" or "camera" space)
    highp vec3 v = normalize(-position);

    // Reflect the light beam using the normal at this fragment
    highp vec3 r = reflect(-s, normal);

    // Calculate the diffuse component
    highp float diffuse = max(dot(s, normal), 0.0);

    // Calculate the specular component
    highp float specular = 0.0;
    if (dot(s, normal) > 0.0)
        specular = (shininess / (8.0 * 3.14)) * pow(max(dot(r, v), 0.0), shininess);

    // Lookup diffuse and specular factors
    highp vec3 diffuseColor = texture2D(diffuseTexture, flipYTexCoord).rgb;

    // Combine the ambient, diffuse and specular contributions
    return lightIntensity * ((ka + diffuse) * diffuseColor + specular * ks);
}

void main()
{
    highp vec2 flipYTexCoord = texCoord;
    flipYTexCoord.y = 1.0 - texCoord.y;

    highp vec3 result = dModel(flipYTexCoord);

    highp float alpha = opacity * texture2D(diffuseTexture, flipYTexCoord).a;

    gl_FragColor = vec4(result, alpha);
}
