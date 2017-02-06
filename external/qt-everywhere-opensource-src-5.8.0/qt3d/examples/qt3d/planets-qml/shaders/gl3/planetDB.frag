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
uniform vec3 ks;            // Specular reflectivity
uniform float shininess;    // Specular shininess factor
uniform float opacity;      // Alpha channel

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;

in vec3 lightDir;
in vec3 viewDir;
in vec2 texCoord;

out vec4 fragColor;

void dbModel(const in vec3 norm, const in vec2 flipYTexCoord, out vec3 ambientAndDiff, out vec3 spec)
{
    // Reflection of light direction about normal
    vec3 r = reflect(-lightDir, norm);

    vec3 diffuseColor = texture(diffuseTexture, flipYTexCoord).rgb;

    // Calculate the ambient contribution
    vec3 ambient = lightIntensity * ka * diffuseColor;

    // Calculate the diffuse contribution
    float sDotN = max(dot(lightDir, norm), 0.0);
    vec3 diffuse = lightIntensity * diffuseColor * sDotN;

    // Sum the ambient and diffuse contributions
    ambientAndDiff = ambient + diffuse;

    // Calculate the specular highlight contribution
    spec = vec3(0.0);
    if (sDotN > 0.0)
        spec = (lightIntensity * ks) * pow(max(dot(r, viewDir), 0.0), shininess);
}

void main()
{
    vec2 flipYTexCoord = texCoord;
    flipYTexCoord.y = 1.0 - texCoord.y;

    // Sample the textures at the interpolated texCoords
    vec4 normal = 2.0 * texture(normalTexture, flipYTexCoord) - vec4(1.0);

    vec3 result = lightIntensity * ka * texture(diffuseTexture, flipYTexCoord).rgb;

    // Calculate the lighting model, keeping the specular component separate
    vec3 ambientAndDiff, spec;
    dbModel(normalize(normal.xyz), flipYTexCoord, ambientAndDiff, spec);
    result = ambientAndDiff + spec;

    // Combine spec with ambient+diffuse for final fragment color
    fragColor = vec4(result, opacity);
}
