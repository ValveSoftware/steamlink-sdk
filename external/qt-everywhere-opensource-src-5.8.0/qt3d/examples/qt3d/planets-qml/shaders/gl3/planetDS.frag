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

#version 150 core

uniform mat4 viewMatrix;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;

uniform vec3 ka;            // Ambient reflectivity
uniform float shininess;    // Specular shininess factor
uniform float opacity;      // Alpha channel

uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;

in vec3 position;
in vec3 normal;
in vec2 texCoord;

out vec4 fragColor;

vec3 dsModel(const in vec2 flipYTexCoord)
{
    // Calculate the vector from the light to the fragment
    vec3 s = normalize(vec3(viewMatrix * vec4(lightPosition, 1.0)) - position);

    // Calculate the vector from the fragment to the eye position
    // (origin since this is in "eye" or "camera" space)
    vec3 v = normalize(-position);

    // Reflect the light beam using the normal at this fragment
    vec3 r = reflect(-s, normal);

    // Calculate the diffuse component
    float diffuse = max(dot(s, normal), 0.0);

    // Calculate the specular component
    float specular = 0.0;
    if (dot(s, normal) > 0.0)
        specular = (shininess / (8.0 * 3.14)) * pow(max(dot(r, v), 0.0), shininess);

    // Lookup diffuse and specular factors
    vec3 diffuseColor = texture(diffuseTexture, flipYTexCoord).rgb;
    vec3 specularColor = texture(specularTexture, flipYTexCoord).rgb;

    // Combine the ambient, diffuse and specular contributions
    return lightIntensity * ((ka + diffuse) * diffuseColor + specular * specularColor);
}

void main()
{
    vec2 flipYTexCoord = texCoord;
    flipYTexCoord.y = 1.0 - texCoord.y;

    vec3 result = lightIntensity * ka * texture(diffuseTexture, flipYTexCoord).rgb;
    result += dsModel(flipYTexCoord);

    fragColor = vec4(result, opacity * texture(diffuseTexture, flipYTexCoord).a);
}
