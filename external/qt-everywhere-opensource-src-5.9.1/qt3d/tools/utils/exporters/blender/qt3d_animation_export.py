#############################################################################
##
## Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Qt3D module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

# Required Blender information.
bl_info = {
           "name": "Qt3D Animation Exporter",
           "author": "Sean Harmer <sean.harmer@kdab.com>, Paul Lemire <paul.lemire@kdab.com>",
           "version": (0, 4),
           "blender": (2, 72, 0),
           "location": "File > Export > Qt3D Animation (.json)",
           "description": "Export animations to json to use with Qt3D",
           "warning": "",
           "wiki_url": "",
           "tracker_url": "",
           "category": "Import-Export"
          }

import bpy
import os
import struct
import mathutils
import math
import json
from array import array
from bpy_extras.io_utils import ExportHelper
from bpy.props import (
        BoolProperty,
        FloatProperty,
        StringProperty,
        EnumProperty,
        )
from collections import defaultdict

def frameToTime(frame):
    # Get the fps to convert from frame number to time in seconds for x values
    fps = bpy.context.scene.render.fps

    # Calculate time, remembering that blender uses 1-based frame numbers
    return (frame - 1) / fps;

def findResolvingObject(rootId):
    if rootId == "OBJECT":
        return bpy.data.objects[0]
    elif rootId == "MATERIAL":
        return bpy.data.materials[0]
    return None

# Note that we swap the Y and Z components because blender uses Z-up
# whereas Qt 3D tends to use Y-Up convention.
def arrayIndexFromTypeAndIndex(dataPath, index):
    if dataPath.startswith("rotation"):
        # Swap Y and Z components of rotations
        if index == 2:
            return 3
        elif index == 3:
            return 2
    elif dataPath.startswith("location"):
        # Swap Y and Z components of locations
        if index == 1:
            return 2
        elif index == 2:
            return 1

    # Otherwise keep the original index
    return index

def componentSuffix(typeName, componentIndex):
    vectorComponents = ["X", "Y", "Z", "W"]
    quaternionComponents = ["W", "X", "Y", "Z"]
    colorComponents = ["R", "G", "B"]

    if typeName == "Vector":
        return vectorComponents[componentIndex]
    elif typeName == "Quaternion":
        return quaternionComponents[componentIndex]
    elif typeName == "Color":
        return colorComponents[componentIndex]
    return "Unknown"

def resolveDataType(object, dataPath):
    value = object.path_resolve(dataPath)
    if isinstance(value, mathutils.Vector):
        return "Vector"
    elif isinstance(value, mathutils.Quaternion):
        return "Quaternion"
    elif isinstance(value, mathutils.Euler):
        return "Euler" + value.order
    elif isinstance(value, mathutils.Color):
        return "Color"
    return "Unknown type"

class PropertyData:
    m_action = None
    m_name = ""
    m_resolverObject = None
    m_dataPath = ""
    m_fcurveIndices = []
    m_componentIndices = []
    m_dataType = ""
    m_outputDataType = ""
    m_outputChannelCount = 0
    m_outputChannelSuffixes = []

    def __init__(self):
        self.m_action = None
        self.m_resolverObject = None
        self.m_dataPath = ""
        self.m_fcurveIndices = []
        self.m_componentIndices = []
        self.m_dataType = ""
        self.m_outputDataType = ""
        self.m_outputChannelCount = 0
        self.m_outputChannelSuffixes = []

    def setDataPath(self, dataPath):
        self.m_dataPath = dataPath
        if dataPath.startswith("rotation"):
            self.m_name = "Rotation"
        else:
            self.m_name = dataPath.title()
        self.m_name = self.m_name.replace("_", " ")

    def print(self):
        print("Action = " + self.m_action.name \
            + "DataPath = " + self.m_dataPath \
            + "Name = " + self.m_name)
            # + "fcurve indices =" + self.m_componentIndices

    def generateKeyframesData(self, outputComponentIndex):
        outputKeyframes = []

        # Lookup fcurve index for this component
        fcurveIndex = self.m_fcurveIndices[outputComponentIndex]

        # Invert the sign of the 2nd component of quaternions for rotations
        # We already swap the Y and Z components in the componentSuffix function
        axisOrientationfactor = 1.0
        if self.m_name == "Rotation" and outputComponentIndex == 2:
            axisOrientationfactor = -1.0

        if self.m_dataType == self.m_outputDataType:
            # We can take easy route if no data type conversion is needed
            # Iterate over keyframes
            fcurve = self.m_action.fcurves[fcurveIndex]
            for keyframe in fcurve.keyframe_points:
                outputKeyframe = { \
                    "coords": [frameToTime(keyframe.co.x), axisOrientationfactor * keyframe.co.y], \
                    "leftHandle": [frameToTime(keyframe.handle_left.x), axisOrientationfactor * keyframe.handle_left.y], \
                    "rightHandle": [frameToTime(keyframe.handle_right.x), axisOrientationfactor * keyframe.handle_right.y] }
                outputKeyframes.append(outputKeyframe)
        else:
            # Iterate over keyframes - we assume that all channels were keyed at the same times
            # This is usually the case as blender doesn't support keying individual components
            # but a user could have tweaked the individual channels
            fcurve = self.m_action.fcurves[self.m_fcurveIndices[0]]
            for keyframeIndex, keyframe in enumerate(fcurve.keyframe_points):
                # Get data for this property
                if not self.m_dataType.startswith("Euler"):
                    print("Unhandled data type conversion")
                    return None

                # Convert the control point to a quaternion
                eulerOrder = self.m_dataType[-3:]
                time_co = frameToTime(self.m_action.fcurves[self.m_fcurveIndices[0]].keyframe_points[keyframeIndex].co.x)
                rotX = self.m_action.fcurves[self.m_fcurveIndices[0]].keyframe_points[keyframeIndex].co.y
                rotY = self.m_action.fcurves[self.m_fcurveIndices[1]].keyframe_points[keyframeIndex].co.y
                rotZ = self.m_action.fcurves[self.m_fcurveIndices[2]].keyframe_points[keyframeIndex].co.y
                euler = mathutils.Euler((rotX, rotY, rotZ), eulerOrder)
                q_co = euler.to_quaternion()

                # Convert the left handle to a quaternion
                time_hl = frameToTime(self.m_action.fcurves[self.m_fcurveIndices[0]].keyframe_points[keyframeIndex].handle_left.x)
                rotX = self.m_action.fcurves[self.m_fcurveIndices[0]].keyframe_points[keyframeIndex].handle_left.y
                rotY = self.m_action.fcurves[self.m_fcurveIndices[1]].keyframe_points[keyframeIndex].handle_left.y
                rotZ = self.m_action.fcurves[self.m_fcurveIndices[2]].keyframe_points[keyframeIndex].handle_left.y
                euler = mathutils.Euler((rotX, rotY, rotZ), eulerOrder)
                q_hl = euler.to_quaternion()

                # Convert the right handle to a quaternion
                time_hr = frameToTime(self.m_action.fcurves[self.m_fcurveIndices[0]].keyframe_points[keyframeIndex].handle_left.x)
                rotX = self.m_action.fcurves[self.m_fcurveIndices[0]].keyframe_points[keyframeIndex].handle_left.y
                rotY = self.m_action.fcurves[self.m_fcurveIndices[1]].keyframe_points[keyframeIndex].handle_left.y
                rotZ = self.m_action.fcurves[self.m_fcurveIndices[2]].keyframe_points[keyframeIndex].handle_left.y
                euler = mathutils.Euler((rotX, rotY, rotZ), eulerOrder)
                q_hr = euler.to_quaternion()

                # Extract the corresponding component
                co = []
                handle_left = []
                handle_right = []
                if outputComponentIndex == 0:
                    co = [time_co, axisOrientationfactor * q_co.w]
                    handle_left = [time_hl, axisOrientationfactor * q_hl.w]
                    handle_right = [time_hr, axisOrientationfactor * q_hr.w]
                elif outputComponentIndex == 1:
                    co = [time_co, axisOrientationfactor * q_co.x]
                    handle_left = [time_hl, axisOrientationfactor * q_hl.x]
                    handle_right = [time_hr, axisOrientationfactor * q_hr.x]
                elif outputComponentIndex == 2:
                    co = [time_co, axisOrientationfactor * q_co.y]
                    handle_left = [time_hl, axisOrientationfactor * q_hl.y]
                    handle_right = [time_hr, axisOrientationfactor * q_hr.y]
                elif outputComponentIndex == 3:
                    co = [time_co, axisOrientationfactor * q_co.z]
                    handle_left = [time_hl, axisOrientationfactor * q_hl.z]
                    handle_right = [time_hr, axisOrientationfactor * q_hr.z]

                outputKeyframe = { \
                    "coords": co, \
                    "leftHandle": handle_left, \
                    "rightHandle": handle_right }
                outputKeyframes.append(outputKeyframe)
        return outputKeyframes

    def generateChannelComponentsData(self):
        # First find the data type stored in the blender file
        self.m_dataType = resolveDataType(self.m_resolverObject, self.m_dataPath)

        # Convert this to an output data type - we force rotations as quaternions
        if self.m_dataType.startswith("Euler"):
            self.m_outputDataType = "Quaternion"
            self.m_outputChannelCount = 4
            for i in range(0, 4):
                index = arrayIndexFromTypeAndIndex(self.m_dataPath, i)
                suffix = componentSuffix(self.m_outputDataType, index)
                self.m_outputChannelSuffixes.append(suffix)
        else:
            self.m_outputDataType = self.m_dataType
            self.m_outputChannelCount = len(self.m_componentIndices)
            for i in self.m_componentIndices:
                index = arrayIndexFromTypeAndIndex(self.m_dataPath, i)
                suffix = componentSuffix(self.m_outputDataType, index)
                self.m_outputChannelSuffixes.append(suffix)

        outputChannels = []
        for i in range(0, self.m_outputChannelCount):
            outputChannel = { "channelComponentName": self.m_name + " " + self.m_outputChannelSuffixes[i], "keyFrames": [] }
            keyframes = self.generateKeyframesData(i)
            outputChannel["keyFrames"] = keyframes
            outputChannels.append(outputChannel)

        return outputChannels


class Qt3DAnimationConverter:
    def animationsToJson(self):
        propertyDataMap = defaultdict(list)

        # Pass 1 - collect data we need to produce the output in pass 2
        for action in bpy.data.actions:
            groupCount = len(action.groups)
            #print("    " + action.name + " for type " + action.id_root)

            # We need a datablock of the right type to be able to resolve an fcurve data path to a value.
            # We need the value to be able to determine the type and eventually the correct name for the
            # exported fcurve.
            resolverObject = findResolvingObject(action.id_root)

            fcurveCount = len(action.fcurves)
            #print("    " + action.name + " has " + str(fcurveCount) + " fcurves")

            if fcurveCount == 0:
                break

            lastTitle = ""
            property = PropertyData()
            for fcurveIndex,fcurve in enumerate(action.fcurves):
                title = fcurve.data_path.title()

                # For debugging
                groupName = "<NoGroup>"
                if fcurve.group != None:
                    groupName = fcurve.group.name
                dataPath = fcurve.data_path
                type = resolveDataType(resolverObject, dataPath)
                labelSuffix = componentSuffix("Vector", fcurve.array_index)
                print("        " + str(fcurveIndex) + ": Group: " + groupName \
                    + ", Title = " + title \
                    + ", Component:" + str(fcurve.array_index) \
                    + ", Data Path: " + dataPath \
                    + ", Data Type: " + type \
                    + ", Label: " + labelSuffix)

                # Create a new PropertyData if this fcurve is for a new property
                if title != lastTitle:
                    property = PropertyData()
                    property.m_action = action
                    property.setDataPath(fcurve.data_path)
                    property.m_resolverObject = resolverObject
                    arrayIndex = arrayIndexFromTypeAndIndex(fcurve.data_path, fcurve.array_index)
                    property.m_componentIndices.append(arrayIndex)
                    #property.m_componentIndices.append(fcurve.array_index)
                    property.m_fcurveIndices.append(fcurveIndex)
                    propertyDataMap[action.name].append(property)
                else:
                    property.m_componentIndices.append(fcurve.array_index)
                    property.m_fcurveIndices.append(fcurveIndex)

                lastTitle = title
            print("")

        # For debugging
        print("Pass 1 - Collected data for " + str(len(propertyDataMap)) + " actions")
        actionIndex = 0
        for key in propertyDataMap:
            print(str(actionIndex) + ": " + key + " has " + str(len(propertyDataMap[key])) + " properties")
            for propertyIndex, property in enumerate(propertyDataMap[key]):
                print("    " + str(propertyIndex) + ": " + property.m_name)
            actionIndex =  actionIndex + 1

        # Pass 2

        # The data structure that will be exported
        output = {"animations": []}

        actionIndex = 0
        for key in propertyDataMap:
            #print(str(actionIndex) + ": " + key)

            # Create an output action
            outputAction = { "animationName": key, "channels": []}
            for propertyIndex, property in enumerate(propertyDataMap[key]):
                #print("    " + str(propertyIndex) + ": " + property.m_name)

                # Create an output group and append it to the output action
                outputGroup = { "channelComponents": [], "channelName": property.m_name }

                # Populate the channels list from the property object
                outputChannels = property.generateChannelComponentsData()
                outputGroup["channelComponents"] = outputChannels

                outputAction["channels"].append(outputGroup)

            output["animations"].append(outputAction)
            actionIndex =  actionIndex + 1

        jsonData = json.dumps(output, indent=2, sort_keys=True, separators=(',', ': '))
        return jsonData


class Qt3DExporter(bpy.types.Operator, ExportHelper):
    """Qt3D Exporter"""
    bl_idname       = "export_scene.qt3d_exporter";
    bl_label        = "Qt3DExporter";
    bl_options      = {'PRESET'};

    filename_ext = ""
    use_filter_folder = True

    # TO DO: Handle properly
    use_mesh_modifiers = BoolProperty(
        name="Apply Modifiers",
        description="Apply modifiers (preview resolution)",
        default=True,
        )

    # TO DO: Handle properly
    use_selection_only = BoolProperty(
        name="Selection Only",
        description="Only export select objects",
        default=False,
        )

    def __init__(self):
        pass

    def execute(self, context):
        print("In Execute" + bpy.context.scene.name)

        self.userpath = self.properties.filepath

        # unselect all
        bpy.ops.object.select_all(action='DESELECT')

        converter = Qt3DAnimationConverter()
        fileContent = converter.animationsToJson()
        with open(self.userpath + ".json", '+w') as f:
            f.write(fileContent)

        return {'FINISHED'}

def createBlenderMenu(self, context):
    self.layout.operator(Qt3DExporter.bl_idname, text="Qt3D Animation(.json)")

# Register against Blender
def register():
    bpy.utils.register_class(Qt3DExporter)
    bpy.types.INFO_MT_file_export.append(createBlenderMenu)

def unregister():
    bpy.utils.unregister_class(Qt3DExporter)
    bpy.types.INFO_MT_file_export.remove(createBlenderMenu)

# Handle running the script from Blender's text editor.
if (__name__ == "__main__"):
   register();
   bpy.ops.export_scene.qt3d_exporter();
