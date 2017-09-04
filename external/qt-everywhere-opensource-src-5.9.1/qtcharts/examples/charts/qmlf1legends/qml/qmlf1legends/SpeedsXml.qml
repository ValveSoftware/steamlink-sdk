/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick.XmlListModel 2.0

//![1]
XmlListModel {
    // Hard-coded test data
    xml: "<results><row><speedTrap>0</speedTrap><driver>Fittipaldi</driver><speed>104.12</speed></row>"
        +"<row><speedTrap>0</speedTrap><driver>Stewart</driver><speed>106.12</speed></row>"
        +"<row><speedTrap>0</speedTrap><driver>Hunt</driver><speed>106.12</speed></row>"
//![1]
        +"<row><speedTrap>1</speedTrap><driver>Fittipaldi</driver><speed>115.12</speed></row>"
        +"<row><speedTrap>1</speedTrap><driver>Stewart</driver><speed>114.12</speed></row>"
        +"<row><speedTrap>1</speedTrap><driver>Hunt</driver><speed>115.12</speed></row>"
        +"<row><speedTrap>2</speedTrap><driver>Hunt</driver><speed>165.23</speed></row>"
        +"<row><speedTrap>2</speedTrap><driver>Fittipaldi</driver><speed>175.23</speed></row>"
        +"<row><speedTrap>2</speedTrap><driver>Stewart</driver><speed>168.23</speed></row>"
        +"<row><speedTrap>3</speedTrap><driver>Hunt</driver><speed>104.87</speed></row>"
        +"<row><speedTrap>3</speedTrap><driver>Fittipaldi</driver><speed>104.43</speed></row>"
        +"<row><speedTrap>3</speedTrap><driver>Stewart</driver><speed>94.83</speed></row>"
        +"<row><speedTrap>4</speedTrap><driver>Hunt</driver><speed>107.87</speed></row>"
        +"<row><speedTrap>4</speedTrap><driver>Fittipaldi</driver><speed>111.84</speed></row>"
        +"<row><speedTrap>4</speedTrap><driver>Stewart</driver><speed>106.84</speed></row>"
        +"<row><speedTrap>5</speedTrap><driver>Hunt</driver><speed>94.87</speed></row>"
        +"<row><speedTrap>5</speedTrap><driver>Stewart</driver><speed>92.37</speed></row>"
        +"<row><speedTrap>5</speedTrap><driver>Fittipaldi</driver><speed>99.37</speed></row>"
        +"<row><speedTrap>6</speedTrap><driver>Hunt</driver><speed>52.87</speed></row>"
        +"<row><speedTrap>6</speedTrap><driver>Fittipaldi</driver><speed>42.87</speed></row>"
        +"<row><speedTrap>6</speedTrap><driver>Stewart</driver><speed>55.87</speed></row>"
        +"<row><speedTrap>7</speedTrap><driver>Hunt</driver><speed>77.87</speed></row>"
        +"<row><speedTrap>7</speedTrap><driver>Fittipaldi</driver><speed>72.87</speed></row>"
        +"<row><speedTrap>7</speedTrap><driver>Stewart</driver><speed>87.87</speed></row>"
        +"<row><speedTrap>8</speedTrap><driver>Hunt</driver><speed>94.17</speed></row>"
        +"<row><speedTrap>8</speedTrap><driver>Fittipaldi</driver><speed>98.17</speed></row>"
        +"<row><speedTrap>8</speedTrap><driver>Stewart</driver><speed>84.17</speed></row>"
        +"<row><speedTrap>9</speedTrap><driver>Hunt</driver><speed>91.87</speed></row>"
        +"<row><speedTrap>9</speedTrap><driver>Fittipaldi</driver><speed>71.87</speed></row>"
        +"<row><speedTrap>9</speedTrap><driver>Stewart</driver><speed>81.87</speed></row>"
        +"<row><speedTrap>10</speedTrap><driver>Hunt</driver><speed>104.87</speed></row>"
        +"<row><speedTrap>10</speedTrap><driver>Fittipaldi</driver><speed>115.87</speed></row>"
        +"<row><speedTrap>10</speedTrap><driver>Stewart</driver><speed>119.87</speed></row>"
        +"<row><speedTrap>11</speedTrap><driver>Hunt</driver><speed>162.87</speed></row>"
        +"<row><speedTrap>11</speedTrap><driver>Fittipaldi</driver><speed>155.84</speed></row>"
        +"<row><speedTrap>11</speedTrap><driver>Stewart</driver><speed>152.84</speed></row>"
        +"<row><speedTrap>12</speedTrap><driver>Hunt</driver><speed>181.87</speed></row>"
        +"<row><speedTrap>12</speedTrap><driver>Fittipaldi</driver><speed>161.85</speed></row>"
        +"<row><speedTrap>12</speedTrap><driver>Stewart</driver><speed>167.85</speed></row>"
        +"<row><speedTrap>13</speedTrap><driver>Hunt</driver><speed>155.87</speed></row>"
        +"<row><speedTrap>13</speedTrap><driver>Fittipaldi</driver><speed>154.87</speed></row>"
        +"<row><speedTrap>13</speedTrap><driver>Stewart</driver><speed>164.87</speed></row>"
        +"<row><speedTrap>14</speedTrap><driver>Hunt</driver><speed>197.57</speed></row>"
        +"<row><speedTrap>14</speedTrap><driver>Fittipaldi</driver><speed>187.54</speed></row>"
        +"<row><speedTrap>14</speedTrap><driver>Stewart</driver><speed>180.54</speed></row>"
        +"<row><speedTrap>15</speedTrap><driver>Fittipaldi</driver><speed>216.87</speed></row>"
        +"<row><speedTrap>15</speedTrap><driver>Hunt</driver><speed>207.87</speed></row>"
        +"<row><speedTrap>15</speedTrap><driver>Stewart</driver><speed>197.87</speed></row>"
        +"<row><speedTrap>16</speedTrap><driver>Hunt</driver><speed>82.87</speed></row>"
        +"<row><speedTrap>16</speedTrap><driver>Fittipaldi</driver><speed>79.37</speed></row>"
        +"<row><speedTrap>16</speedTrap><driver>Stewart</driver><speed>85.37</speed></row>"
        +"<row><speedTrap>17</speedTrap><driver>Hunt</driver><speed>153.87</speed></row>"
        +"<row><speedTrap>17</speedTrap><driver>Fittipaldi</driver><speed>143.87</speed></row>"
        +"<row><speedTrap>17</speedTrap><driver>Stewart</driver><speed>133.87</speed></row>"
        +"<row><speedTrap>18</speedTrap><driver>Hunt</driver><speed>89.87</speed></row>"
        +"<row><speedTrap>18</speedTrap><driver>Fittipaldi</driver><speed>95.85</speed></row>"
        +"<row><speedTrap>18</speedTrap><driver>Stewart</driver><speed>98.85</speed></row>"
        +"<row><speedTrap>19</speedTrap><driver>Hunt</driver><speed>169.87</speed></row>"
        +"<row><speedTrap>19</speedTrap><driver>Stewart</driver><speed>167.87</speed></row>"
        +"<row><speedTrap>19</speedTrap><driver>Fittipaldi</driver><speed>154.87</speed></row>"
        +"</results>"
//![2]
    query: "/results/row"

    XmlRole { name: "speedTrap"; query: "speedTrap/string()" }
    XmlRole { name: "driver"; query: "driver/string()" }
    XmlRole { name: "speed"; query: "speed/string()" }
}
//![2]
