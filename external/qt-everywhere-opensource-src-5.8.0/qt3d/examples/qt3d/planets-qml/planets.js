/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

var SUN = 0;
var MERCURY = 1;
var VENUS = 2;
var EARTH = 3;
var MARS = 4;
var JUPITER = 5;
var SATURN = 6;
var URANUS = 7;
var NEPTUNE = 8;
var NUM_SELECTABLE_PLANETS = 9;
var MOON = 9;
var SOLAR_SYSTEM = 100;

var planets = []; // Planet data info

var solarDistance = 2600000;
var saturnOuterRadius = 120.700;
var uranusOuterRadius = 40;

var auScale = 149597.870700; // AU in thousands of kilometers

function loadPlanetData() {

    // Planet Data
    // radius - planet radius
    // tilt - planet axis angle
    // N1/2 - longitude of the ascending node
    // i1/2 - inclination to the ecliptic (plane of the Earth's orbit)
    // w1/2 - argument of perihelion
    // a1/2 - semi-major axis, or mean distance from Sun
    // e1/2 - eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    // M1/2 - mean anomaly (0 at perihelion; increases uniformly with time)
    // period - sidereal rotation period
    // centerOfOrbit - the planet in the center of the orbit
    // (orbital elements based on http://www.stjarnhimlen.se/comp/ppcomp.html)

    var sun = { radius: 694.439, tilt: 63.87, period: 25.05, x: 0, y: 0, z: 0,
        roll: 0 };
    planets.push(sun);
    var mercury = {
        radius: 2.433722, tilt: 0.04, N1: 48.3313, N2: 0.0000324587,
        i1: 7.0047, i2: 0.0000000500, w1: 29.1241, w2: 0.0000101444,
        a1: 0.387098, a2: 0, e1: 0.205635, e2: 0.000000000559,
        M1: 168.6562, M2: 4.0923344368, period: 58.646, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(mercury);
    var venus = {
        radius: 6.046079, tilt: 177.36, N1: 76.6799, N2: 0.0000246590,
        i1: 3.3946, i2: 0.0000000275, w1: 54.8910, w2: 0.0000138374,
        a1: 0.723330, a2: 0, e1: 0.006773, e2: -0.000000001302,
        M1: 48.0052, M2: 1.6021302244, period: 243.0185, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(venus);
    var earth = {
        radius: 6.371, tilt: 25.44, N1: 174.873, N2: 0,
        i1: 0.00005, i2: 0, w1: 102.94719, w2: 0,
        a1: 1, a2: 0, e1: 0.01671022, e2: 0,
        M1: 357.529, M2: 0.985608, period: 0.997, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(earth);
    var mars = {
        radius: 3.389372, tilt: 25.19, N1: 49.5574, N2: 0.0000211081,
        i1: 1.8497, i2: -0.0000000178, w1: 286.5016, w2: 0.0000292961,
        a1: 1.523688, a2: 0, e1: 0.093405, e2: 0.000000002516,
        M1: 18.6021, M2: 0.5240207766, period: 1.025957, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(mars);
    var jupiter = {
        radius: 71.41254, tilt: 3.13, N1: 100.4542, N2: 0.0000276854,
        i1: 1.3030, i2: -0.0000001557, w1: 273.8777, w2: 0.0000164505,
        a1: 5.20256, a2: 0, e1: 0.048498, e2: 0.000000004469,
        M1: 19.8950, M2: 0.0830853001, period: 0.4135, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(jupiter);
    var saturn = {
        radius: 60.19958, tilt: 26.73, N1: 113.6634, N2: 0.0000238980,
        i1: 2.4886, i2: -0.0000001081, w1: 339.3939, w2: 0.0000297661,
        a1: 9.55475, a2: 0, e1: 0.055546, e2: -0.000000009499,
        M1: 316.9670, M2: 0.0334442282, period: 0.4395, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(saturn);
    var uranus = {
        radius: 25.5286, tilt: 97.77, N1: 74.0005, N2: 0.000013978,
        i1: 0.7733, i2: 0.000000019, w1: 96.6612, w2: 0.000030565,
        a1: 19.18171, a2: -0.0000000155, e1: 0.047318, e2: 0.00000000745,
        M1: 142.5905, M2: 0.011725806, period: 0.71833, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(uranus);
    var neptune = {
        radius: 24.73859, tilt: 28.32, N1: 131.7806, N2: 0.000030173,
        i1: 1.7700, i2: -0.000000255, w1: 272.8461, w2: 0.000006027,
        a1: 30.05826, a2: 0.00000003313, e1: 0.008606, e2: 0.00000000215,
        M1: 260.2471, M2: 0.005995147, period: 0.6713, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: SUN
    };
    planets.push(neptune);
    var moon = {
        radius: 1.5424, tilt: 28.32, N1: 125.1228, N2: -0.0529538083,
        i1: 5.1454, i2: 0, w1: 318.0634, w2: 0.1643573223,
        a1: 0.273, a2: 0, e1: 0.054900, e2: 0,
        M1: 115.3654, M2: 13.0649929509, period: 27.321582, x: 0, y: 0, z: 0,
        roll: 0, centerOfOrbit: EARTH
    };
    planets.push(moon);

    return planets;
}

function getOuterRadius(planet) {
    var outerRadius = solarDistance;
    if (planet !== SOLAR_SYSTEM) {
        outerRadius = planets[planet]["radius"];
        if (planet === SATURN) {
            outerRadius =+ saturnOuterRadius;
        } else if (planet === URANUS) {
            outerRadius =+ uranusOuterRadius;
        } else if (planet === SUN) {
            outerRadius = planets[planet]["radius"] / 100;
        }
    }

    return outerRadius;
}
