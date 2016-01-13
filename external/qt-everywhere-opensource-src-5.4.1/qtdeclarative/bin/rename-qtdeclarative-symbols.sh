#!/bin/sh
#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the QtQml module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL21$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia. For licensing terms and
## conditions see http://qt.digia.com/licensing. For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 or version 3 as published by the Free
## Software Foundation and appearing in the file LICENSE.LGPLv21 and
## LICENSE.LGPLv3 included in the packaging of this file. Please review the
## following information to ensure the GNU Lesser General Public License
## requirements will be met: https://www.gnu.org/licenses/lgpl.html and
## http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights. These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## $QT_END_LICENSE$
##
#############################################################################

# Replaces deprecated QDeclarative symbol names with their replacements
#
# Changes instances in all regular files under the specified directory;
# use on a clean source tree!

if [ "$#" -lt "1" ]
then
    echo "    Usage: $0 <directory>"
    exit 1;
fi

MODIFY_DIR="$1"

QML_SYMBOLS="\
    QDeclarativeAbstractBinding
    QDeclarativeAbstractBoundSignal
    QDeclarativeAbstractExpression
    QDeclarativeAccessible
    QDeclarativeAccessors
    QDeclarativeAccessorProperties
    QDeclarativeAnimationTimer
    QDeclarativeAssociationList
    QDeclarativeAttachedPropertiesFunc
    QDeclarativeBinding
    QDeclarativeBindingPrivate
    QDeclarativeBindingProfiler
    QDeclarativeBoundSignal
    QDeclarativeBoundSignalParameters
    QDeclarativeBoundSignalProxy
    QDeclarativeBuiltinFunctions
    QDeclarativeCleanup
    QDeclarativeColorValueType
    QDeclarativeCompiledData
    QDeclarativeCompiler
    QDeclarativeCompilerTypes
    QDeclarativeCompilingProfiler
    QDeclarativeComponent
    QDeclarativeComponentAttached
    QDeclarativeComponentExtension
    QDeclarativeComponentPrivate
    QDeclarativeComponent_setQmlParent
    QDeclarativeCompositeTypeData
    QDeclarativeConnectionsParser
    QDeclarativeContext
    QDeclarativeContextData
    QDeclarativeContextPrivate
    QDeclarativeCustomParser
    QDeclarativeCustomParserNode
    QDeclarativeCustomParserNodePrivate
    QDeclarativeCustomParserProperty
    QDeclarativeCustomParserPropertyPrivate
    QDeclarativeData
    QDeclarativeDataBlob
    QDeclarativeDataExtended
    QDeclarativeDataLoader
    QDeclarativeDataLoaderNetworkReplyProxy
    QDeclarativeDataLoaderThread
    QDeclarativeDateExtension
    QDeclarativeDataTest
    QDeclarativeDebug
    QDeclarativeDebugClient
    QDeclarativeDebugClientPrivate
    QDeclarativeDebugConnection
    QDeclarativeDebugConnectionPrivate
    QDeclarativeDebugContextReference
    QDeclarativeDebugData
    QDeclarativeDebugEngineReference
    QDeclarativeDebugEnginesQuery
    QDeclarativeDebugExpressionQuery
    QDeclarativeDebugFileReference
    QDeclarativeDebugger
    QDeclarativeDebuggingEnabler
    QDeclarativeDebugHelper
    QDeclarativeDebugObjectExpressionWatch
    QDeclarativeDebugObjectQuery
    QDeclarativeDebugObjectReference
    QDeclarativeDebugPropertyReference
    QDeclarativeDebugPropertyWatch
    QDeclarativeDebugQuery
    QDeclarativeDebugRootContextQuery
    QDeclarativeDebugServer
    QDeclarativeDebugServerConnection
    QDeclarativeDebugServerPrivate
    QDeclarativeDebugServerThread
    QDeclarativeDebugService
    QDeclarativeDebugServicePrivate
    QDeclarativeDebugStatesDelegate
    QDeclarativeDebugTrace
    QDeclarativeDebugWatch
    QDeclarativeDelayedError
    QDeclarativeDirComponents
    QDeclarativeDirParser
    QDeclarativeDirScripts
    QDeclarativeDOMNodeResource
    QDeclarativeEasingValueType
    QDeclarativeElement
    QDeclarativeEngine
    QDeclarativeEngineDebug
    QDeclarativeEngineDebugClient
    QDeclarativeEngineDebugService
    QDeclarativeEngineDebugPrivate
    QDeclarativeEnginePrivate
    QDeclarativeError
    QDeclarativeErrorPrivate
    QDeclarativeExpression
    QDeclarativeExpressionPrivate
    QDeclarativeExtensionInterface
    QDeclarativeExtensionPlugin
    QDeclarativeFontValueType
    QDeclarativeGraphics_DerivedObject
    QDeclarativeGuard
    QDeclarativeGuardedContextData
    QDeclarativeGuardImpl
    QDeclarativeHandlingSignalProfiler
    QDeclarativeImportDatabase
    QDeclarativeImportedNamespace
    QDeclarativeImports
    QDeclarativeImportsPrivate
    QDeclarativeIncubationController
    QDeclarativeIncubator
    QDeclarativeIncubatorController
    QDeclarativeIncubatorPrivate
    QDeclarativeIncubators
    QDeclarativeInfo
    QDeclarativeInfoPrivate
    QDeclarativeInspector
    QDeclarativeInspectorInterface
    QDeclarativeInspectorService
    QDeclarativeInstruction
    QDeclarativeInstructionData
    QDeclarativeInstructionMeta
    QDeclarativeIntegerCache
    QDeclarativeJavaScriptExpression
    QDeclarativeJavaScriptExpressionGuard
    QDeclarativeJS
    QDeclarativeJSGrammar
    QDeclarativeListProperty
    QDeclarativeListReference
    QDeclarativeListReferencePrivate
    QDeclarativeLocale
    QDeclarativeLocalStoragePlugin
    QDeclarativeMatrix4x4ValueType
    QDeclarativeMetaType
    QDeclarativeMetaTypeData
    QDeclarativeNetworkAccessManagerFactory
    QDeclarativeNotifier
    QDeclarativeNotifierEndpoint
    QDeclarativeNullableValue
    QDeclarativeNumberExtension
    QDeclarativeObjectCreatingProfiler
    QDeclarativeObjectData
    QDeclarativeObjectProperty
    QDeclarativeObserverMode
    QDeclarativeOpenMetaObject
    QDeclarativeOpenMetaObjectPrivate
    QDeclarativeOpenMetaObjectType
    QDeclarativeOpenMetaObjectTypePrivate
    QDeclarativeParser
    QDeclarativeParserStatus
    QDeclarativePointFValueType
    QDeclarativePointValueType
    QDeclarativePool
    QDeclarativePrivate
    QDeclarativeProfilerData
    QDeclarativeProfilerService
    QDeclarativeProperties
    QDeclarativeProperty
    QDeclarativePropertyCache
    QDeclarativePropertyCacheMethodArguments
    QDeclarativePropertyData
    QDeclarativePropertyMap
    QDeclarativePropertyMapMetaObject
    QDeclarativePropertyMapPrivate
    QDeclarativePropertyPrivate
    QDeclarativePropertyRawData
    QDeclarativePropertyValueInterceptor
    QDeclarativePropertyValueSource
    QDeclarativeProxyMetaObject
    QDeclarativeQmldirData
    QDeclarativeQtQuick1Module
    QDeclarativeQtQuick2Module
    QDeclarativeQtQuick2DebugStatesDelegate
    QDeclarativeQuaternionValueType
    QDeclarativeRectFValueType
    QDeclarativeRectValueType
    QDeclarativeRefCount
    QDeclarativeRefPointer
    QDeclarativeRegisterType
    QDeclarativeRewrite
    QDeclarativeScript
    QDeclarativeScriptBlob
    QDeclarativeScriptData
    QDeclarativeScriptPrivate
    QDeclarativeScriptString
    QDeclarativeScriptStringPrivate
    QDeclarativeSizeFValueType
    QDeclarativeSizeValueType
    QDeclarativeSqlDatabaseData
    QDeclarativeStringConverters
    QDeclarativeThread
    QDeclarativeThreadPrivate
    QDeclarativeTrace
    QDeclarativeType
    QDeclarativeTypeData
    QDeclarativeTypeInfo
    QDeclarativeTypeLoader
    QDeclarativeTypeModule
    QDeclarativeTypeModulePrivate
    QDeclarativeTypeModuleVersion
    QDeclarativeTypeNameCache
    QDeclarativeTypeNotAvailable
    QDeclarativeTypePrivate
    QDeclarativeTypesExtensionInterface
    QDeclarativeV8Function
    QDeclarativeV8Handle
    QDeclarativeValueType
    QDeclarativeValueTypeProxyBinding
    QDeclarativeValueTypeFactory
    QDeclarativeVector2DValueType
    QDeclarativeVector3DValueType
    QDeclarativeVector4DValueType
    QDeclarativeVME
    QDeclarativeVMEGuard
    QDeclarativeVMEMetaData
    QDeclarativeVMEMetaObject
    QDeclarativeVMEMetaObjectEndpoint
    QDeclarativeVMEVariant
    QDeclarativeVMETypes
    QDeclarativeWatcher
    QDeclarativeWatchProxy
    QDeclarativeXMLHttpRequest
    QDeclarativeXMLHttpRequestData
    QDeclarative_isFileCaseCorrect
    QDeclarative_setParent_noEvent
    QQuickProperties
    QQuickPropertyCacheMethodArguments
    QQuickPropertyData
"

QUICK_SYMBOLS="\
    QDeclarativeAbstractAnimation
    QDeclarativeAbstractAnimationAction
    QDeclarativeAbstractAnimationPrivate
    QDeclarativeAction
    QDeclarativeActionEvent
    QDeclarativeAnchors
    QDeclarativeAnimationController
    QDeclarativeAnimationControllerPrivate
    QDeclarativeAnimationGroup
    QDeclarativeAnimationGroupPrivate
    QDeclarativeAnimationPropertyUpdater
    QDeclarativeApplication
    QDeclarativeApplicationPrivate
    QDeclarativeBehavior
    QDeclarativeBehaviorPrivate
    QDeclarativeBind
    QDeclarativeBindPrivate
    QDeclarativeBulkValueAnimator
    QDeclarativeBulkValueUpdater
    QDeclarativeCachedBezier
    QDeclarativeChangeSet
    QDeclarativeColorAnimation
    QDeclarativeConnections
    QDeclarativeConnectionsPrivate
    QDeclarativeCurve
    QDeclarativeDefaultTextureFactory
    QDeclarativeFlick
    QDeclarativeFocusPanel
    QDeclarativeFolderListModel
    QDeclarativeFolderListModelPrivate
    QDeclarativeFontLoader
    QDeclarativeFontLoaderPrivate
    QDeclarativeFontObject
    QDeclarativeGestureArea
    QDeclarativeGestureAreaParser
    QDeclarativeGestureAreaPrivate
    QDeclarativeGraphics
    QDeclarativeImageProvider
    QDeclarativeImageProviderPrivate
    QDeclarativeItem
    QDeclarativeItemAccessor
    QDeclarativeItemChangeListener
    QDeclarativeItemKeyFilter
    QDeclarativeItemPrivate
    QDeclarativeListAccessor
    QDeclarativeListCompositor
    QDeclarativeListElement
    QDeclarativeListModel
    QDeclarativeListModelParser
    QDeclarativeListModelWorkerAgent
    QDeclarativeListView
    QDeclarativeNumberAnimation
    QDeclarativePackage
    QDeclarativePackageAttached
    QDeclarativePackagePrivate
    QDeclarativeParallelAnimation
    QDeclarativeParticle
    QDeclarativeParticleMotion
    QDeclarativeParticleMotionGravity
    QDeclarativeParticleMotionLinear
    QDeclarativeParticleMotionWander
    QDeclarativeParticles
    QDeclarativeParticlesPainter
    QDeclarativeParticlesPrivate
    QDeclarativePath
    QDeclarativePathArc
    QDeclarativePathAttribute
    QDeclarativePathCatmullRomCurve
    QDeclarativePathCubic
    QDeclarativePathCurve
    QDeclarativePathData
    QDeclarativePathElement
    QDeclarativePathInterpolator
    QDeclarativePathLine
    QDeclarativePathPercent
    QDeclarativePathPrivate
    QDeclarativePathQuad
    QDeclarativePathSvg
    QDeclarativePauseAnimation
    QDeclarativePauseAnimationPrivate
    QDeclarativePixmap
    QDeclarativePixmapData
    QDeclarativePixmapKey
    QDeclarativePixmapNull
    QDeclarativePixmapReader
    QDeclarativePixmapReaderThreadObject
    QDeclarativePixmapReply
    QDeclarativePixmapStore
    QDeclarativePropertyAction
    QDeclarativePropertyActionPrivate
    QDeclarativePropertyAnimation
    QDeclarativePropertyAnimationPrivate
    QDeclarativePropertyChanges
    QDeclarativePropertyChangesParser
    QDeclarativePropertyChangesPrivate
    QDeclarativeReplaceSignalHandler
    QDeclarativeRevertAction
    QDeclarativeRotationAnimation
    QDeclarativeRotationAnimationPrivate
    QDeclarativeSequentialAnimation
    QDeclarativeScriptAction
    QDeclarativeScriptActionPrivate
    QDeclarativeSetPropertyAnimationAction
    QDeclarativeSimpleAction
    QDeclarativeSmoothedAnimation
    QDeclarativeSmoothedAnimationPrivate
    QDeclarativeSpringAnimation
    QDeclarativeSpringAnimationPrivate
    QDeclarativeState
    QDeclarativeStateActions
    QDeclarativeStateChange
    QDeclarativeStateChangeScript
    QDeclarativeStateChangeScriptPrivate
    QDeclarativeStateGroup
    QDeclarativeStateGroupPrivate
    QDeclarativeStateOperation
    QDeclarativeStateOperationPrivate
    QDeclarativeStatePrivate
    QDeclarativeStyledText
    QDeclarativeStyledTextImgTag
    QDeclarativeStyledTextPrivate
    QDeclarativeSystemPalette
    QDeclarativeSystemPalettePrivate
    QDeclarativeTextureFactory
    QDeclarativeTimeLine
    QDeclarativeTimeLineCallback
    QDeclarativeTimeLineObject
    QDeclarativeTimeLinePrivate
    QDeclarativeTimeLineValue
    QDeclarativeTimeLineValueProxy
    QDeclarativeTimeLineValues
    QDeclarativeTimer
    QDeclarativeTimerPrivate
    QDeclarativeTransition
    QDeclarativeTransitionInstance
    QDeclarativeTransitionManager
    QDeclarativeTransitionManagerPrivate
    QDeclarativeTransitionPrivate
    QDeclarativeUtilModule
    QDeclarativeVector3dAnimation
    QDeclarativeView
    QDeclarativeViewInspector
    QDeclarativeViewInspectorPrivate
    QDeclarativeViewPrivate
    QDeclarativeWebView
    QDeclarativeXmlListModel
    QDeclarativeXmlListModelPrivate
    QDeclarativeXmlListModelRole
    QDeclarativeXmlListRange
    QDeclarativeXmlQueryEngine
    QDeclarativeXmlQueryResult
    QDeclarativeXmlQueryThreadObject
    QDeclarativeXmlRoleList
    QDeclarativeSvgParser
    QDeclarativeWorkerScript
    QDeclarativeWorkerScriptEngine
    QDeclarativeWorkerScriptEnginePrivate
"

QML_INCLUDE_FILES="\
    qdeclarativeaccessible.h
    qdeclarativeaccessors_p.h
    qdeclarativebinding_p.h
    qdeclarativebinding_p_p.h
    qdeclarativeboundsignal_p.h
    qdeclarativebuiltinfunctions_p.h
    qdeclarativecleanup_p.h
    qdeclarativecompiler_p.h
    qdeclarativecomponentattached_p.h
    qdeclarativecomponent.h
    qdeclarativecomponent_p.h
    qdeclarativecontext.h
    qdeclarativecontext_p.h
    qdeclarativecustomparser_p.h
    qdeclarativecustomparser_p_p.h
    qdeclarativedata_p.h
    qdeclarativedebugclient_p.h
    qdeclarativedebug.h
    qdeclarativedebughelper_p.h
    qdeclarativedebugserverconnection_p.h
    qdeclarativedebugserver_p.h
    qdeclarativedebugservice_p.h
    qdeclarativedebugservice_p_p.h
    qdeclarativedebugstatesdelegate_p.h
    qdeclarativedebugtrace_p.h
    qdeclarativedirparser_p.h
    qdeclarativeenginedebug_p.h
    qdeclarativeenginedebugservice_p.h
    qdeclarativeengine.h
    qdeclarativeengine_p.h
    qdeclarativeerror.h
    qdeclarativeexpression.h
    qdeclarativeexpression_p.h
    qdeclarativeextensioninterface.h
    qdeclarativeextensionplugin.h
    qdeclarativeglobal_p.h
    qdeclarativeguard_p.h
    qdeclarative.h
    qdeclarativeimageprovider.h
    qdeclarativeimport_p.h
    qdeclarativeincubator.h
    qdeclarativeincubator_p.h
    qdeclarativeinfo.h
    qdeclarativeinspectorinterface_p.h
    qdeclarativeinspectorprotocol.h
    qdeclarativeinspectorservice_p.h
    qdeclarativeinstruction_p.h
    qdeclarativeintegercache_p.h
    qdeclarativejsastfwd_p.h
    qdeclarativejsast_p.h
    qdeclarativejsastvisitor_p.h
    qdeclarativejsengine_p.h
    qdeclarativejsglobal_p.h
    qdeclarativejsgrammar_p.h
    qdeclarativejskeywords_p.h
    qdeclarativejslexer_p.h
    qdeclarativejsmemorypool_p.h
    qdeclarativejsparser_p.h
    qdeclarativelist.h
    qdeclarativelist_p.h
    qdeclarativelocale_p.h
    qdeclarativemetatype_p.h
    qdeclarativenetworkaccessmanagerfactory.h
    qdeclarativenotifier_p.h
    qdeclarativenullablevalue_p_p.h
    qdeclarativeopenmetaobject_p.h
    qdeclarativeparserstatus.h
    qdeclarativepool_p.h
    qdeclarativeprivate.h
    qdeclarativeprofilerservice_p.h
    qdeclarativepropertycache_p.h
    qdeclarativeproperty.h
    qdeclarativepropertymap.h
    qdeclarativeproperty_p.h
    qdeclarativepropertyvalueinterceptor_p.h
    qdeclarativepropertyvaluesource.h
    qdeclarativeproxymetaobject_p.h
    qdeclarativerefcount_p.h
    qdeclarativerewrite_p.h
    qdeclarativescript_p.h
    qdeclarativescriptstring.h
    qdeclarativescriptstring_p.h
    qdeclarativesqldatabase_p.h
    qdeclarativestringconverters_p.h
    qdeclarativethread_p.h
    qdeclarativetrace_p.h
    qdeclarativetypeloader_p.h
    qdeclarativetypenamecache_p.h
    qdeclarativetypenotavailable_p.h
    qdeclarativevaluetype_p.h
    qdeclarativevmemetaobject_p.h
    qdeclarativevme_p.h
    qdeclarativewatcher_p.h
    qdeclarativexmlhttprequest_p.h
    qdeclarativexmllistmodel_p.h
"

QUICK_INCLUDE_FILES="\
    qdeclarativeanimation_p.h
    qdeclarativeanimation_p_p.h
    qdeclarativeanimationcontroller_p.h
    qdeclarativeapplication_p.h
    qdeclarativebehavior_p.h
    qdeclarativebind_p.h
    qdeclarativechangeset_p.h
    qdeclarativeconnections_p.h
    qdeclarativefolderlistmodel.h
    qdeclarativefontloader_p.h
    qdeclarativelistaccessor_p.h
    qdeclarativelistcompositor_p.h
    qdeclarativelistmodel_p.h
    qdeclarativelistmodel_p_p.h
    qdeclarativelistmodelworkeragent_p.h
    qdeclarativepackage_p.h
    qdeclarativepathinterpolator_p.h
    qdeclarativepath_p.h
    qdeclarativepath_p_p.h
    qdeclarativepixmapcache_p.h
    qdeclarativepropertychanges_p.h
    qdeclarativesmoothedanimation_p.h
    qdeclarativesmoothedanimation_p_p.h
    qdeclarativespringanimation_p.h
    qdeclarativestategroup_p.h
    qdeclarativestateoperations_p.h
    qdeclarativestate_p.h
    qdeclarativestate_p_p.h
    qdeclarativestyledtext_p.h
    qdeclarativesvgparser_p.h
    qdeclarativesystempalette_p.h
    qdeclarativetimeline_p_p.h
    qdeclarativetimer_p.h
    qdeclarativetransitionmanager_p_p.h
    qdeclarativetransition_p.h
    qdeclarativeutilmodule_p.h
    qdeclarativeworkerscript_p.h
"

replaceMatch()
{
    SYMBOL="$1"
    REPLACEMENT="$2"
    echo "Replacing $SYMBOL with $REPLACEMENT:"

    CONTAINERS=$(find "$MODIFY_DIR" ! -path ".git" -type f -print0 | xargs -0 grep -l -I "$SYMBOL")
    for CONTAINER in $CONTAINERS
    do
        echo "    $CONTAINER"
        TMP_FILE="$CONTAINER.tmp"

        sed 's|'"$SYMBOL"'|'"$REPLACEMENT"'|g' <"$CONTAINER" >"$TMP_FILE"
        mv "$TMP_FILE" "$CONTAINER"
    done
    echo
}

for QML_SYMBOL in $QML_SYMBOLS
do
    QML_REPLACEMENT="QQml${QML_SYMBOL#QDeclarative}"
    replaceMatch "\bQtDeclarative/$QML_SYMBOL\b" "QtQml/$QML_REPLACEMENT"
    replaceMatch "\b$QML_SYMBOL\b" "$QML_REPLACEMENT"
done

for QUICK_SYMBOL in $QUICK_SYMBOLS
do
    QUICK_REPLACEMENT="QQuick${QUICK_SYMBOL#QDeclarative}"
    replaceMatch "\bQtDeclarative/$QUICK_SYMBOL\b" "QtQuick/$QUICK_REPLACEMENT"
    replaceMatch "\b$QUICK_SYMBOL\b" "$QUICK_REPLACEMENT"
done

for QML_INCLUDE_FILE in $QML_INCLUDE_FILES
do
    QML_INCLUDE_REPLACEMENT="qqml${QML_INCLUDE_FILE#qdeclarative}"
    replaceMatch "\b$QML_INCLUDE_FILE\b" "$QML_INCLUDE_REPLACEMENT"
done

for QUICK_INCLUDE_FILE in $QUICK_INCLUDE_FILES
do
    QUICK_INCLUDE_REPLACEMENT="qquick${QUICK_INCLUDE_FILE#qdeclarative}"
    replaceMatch "\b$QUICK_INCLUDE_FILE\b" "$QUICK_INCLUDE_REPLACEMENT"
done

# Various one-off replacements
replaceMatch "\bQtDeclarative\b" "QtQml"
replaceMatch "\basQDeclarativeContext\b" "asQQmlContext"
replaceMatch "\basQDeclarativeContextPrivate\b" "asQQmlContextPrivate"

# Replace any references to the 'declarative' module with 'qml'
echo "Replacing module declarative with qml:"
CONTAINERS=$(find "$MODIFY_DIR" \( -name \*\.pro -o -name \*\.pri \) -print0 | xargs -0 grep -l -I "\bdeclarative\b")
for CONTAINER in $CONTAINERS
do
    echo "    $CONTAINER"
    TMP_FILE="$CONTAINER.tmp"

    # We only want to replace standalone 'declarative' and 'declarative-private' tokens
    sed 's|\([[:space:]]\+\)declarative\([[:space:]]\+\)|\1qml\2|g' <"$CONTAINER" | sed 's|\([[:space:]]\+\)declarative$|\1qml|g' | sed 's|\([[:space:]]\+\)declarative-private\([[:space:]]\+\)|\1qml-private\2|g' | sed 's|\([[:space:]]\+\)declarative-private$|\1qml-private|g' >"$TMP_FILE"
    mv "$TMP_FILE" "$CONTAINER"
done
echo

echo "Replacements complete"
exit 0
