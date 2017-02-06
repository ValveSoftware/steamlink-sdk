# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'blink_core_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/core',
    'blink_platform_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/platform',
    'jinja_module_files': [
      # jinja2/__init__.py contains version string, so sufficient for package
      '<(DEPTH)/third_party/jinja2/__init__.py',
      '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
    ],
  },
  'targets': [
    {
      # GN version: //third_party/WebKit/Source/core/inspector:instrumentation_sources
      'target_name': 'instrumentation_sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generateInspectorInstrumentation',
          'inputs': [
            # The python script in action below.
            'CodeGeneratorInstrumentation.py',
            # Input file for the script.
            'InspectorInstrumentation.idl',
          ],
          'outputs': [
            '<(blink_core_output_dir)/InspectorInstrumentationInl.h',
            '<(blink_core_output_dir)/InspectorOverridesInl.h',
            '<(blink_core_output_dir)/InstrumentingAgents.h',
            '<(blink_core_output_dir)/InspectorInstrumentationImpl.cpp',
          ],
          'action': [
            'python',
            'CodeGeneratorInstrumentation.py',
            'InspectorInstrumentation.idl',
            '--output_dir', '<(blink_core_output_dir)',
          ],
          'message': 'Generating Inspector instrumentation code from InspectorInstrumentation.idl',
        }
      ]
    },
    {
      # GN version: //third_party/WebKit/Source/core/inspector:protocol_sources
      'target_name': 'protocol_sources',
      'type': 'none',
      'dependencies': ['protocol_version'],
      'actions': [
        {
          'action_name': 'generateInspectorProtocolBackendSources',
          'inputs': [
            '<@(jinja_module_files)',
            # The python script in action below.
            '../../platform/inspector_protocol/CodeGenerator.py',
            # Source code templates.
            '../../platform/inspector_protocol/TypeBuilder_h.template',
            '../../platform/inspector_protocol/TypeBuilder_cpp.template',
            # Protocol definition
            'browser_protocol.json',
            '../../platform/v8_inspector/js_protocol.json',
          ],
          'outputs': [
            '<(blink_core_output_dir)/inspector/protocol/Accessibility.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Accessibility.h',
            '<(blink_core_output_dir)/inspector/protocol/Animation.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Animation.h',
            '<(blink_core_output_dir)/inspector/protocol/ApplicationCache.cpp',
            '<(blink_core_output_dir)/inspector/protocol/ApplicationCache.h',
            '<(blink_core_output_dir)/inspector/protocol/CacheStorage.cpp',
            '<(blink_core_output_dir)/inspector/protocol/CacheStorage.h',
            '<(blink_core_output_dir)/inspector/protocol/CSS.cpp',
            '<(blink_core_output_dir)/inspector/protocol/CSS.h',
            '<(blink_core_output_dir)/inspector/protocol/Database.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Database.h',
            '<(blink_core_output_dir)/inspector/protocol/DeviceOrientation.cpp',
            '<(blink_core_output_dir)/inspector/protocol/DeviceOrientation.h',
            '<(blink_core_output_dir)/inspector/protocol/DOM.cpp',
            '<(blink_core_output_dir)/inspector/protocol/DOMDebugger.cpp',
            '<(blink_core_output_dir)/inspector/protocol/DOMDebugger.h',
            '<(blink_core_output_dir)/inspector/protocol/DOM.h',
            '<(blink_core_output_dir)/inspector/protocol/DOMStorage.cpp',
            '<(blink_core_output_dir)/inspector/protocol/DOMStorage.h',
            '<(blink_core_output_dir)/inspector/protocol/Emulation.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Emulation.h',
            '<(blink_core_output_dir)/inspector/protocol/IndexedDB.cpp',
            '<(blink_core_output_dir)/inspector/protocol/IndexedDB.h',
            '<(blink_core_output_dir)/inspector/protocol/Input.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Input.h',
            '<(blink_core_output_dir)/inspector/protocol/Inspector.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Inspector.h',
            '<(blink_core_output_dir)/inspector/protocol/IO.cpp',
            '<(blink_core_output_dir)/inspector/protocol/IO.h',
            '<(blink_core_output_dir)/inspector/protocol/LayerTree.cpp',
            '<(blink_core_output_dir)/inspector/protocol/LayerTree.h',
            '<(blink_core_output_dir)/inspector/protocol/Memory.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Memory.h',
            '<(blink_core_output_dir)/inspector/protocol/Network.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Network.h',
            '<(blink_core_output_dir)/inspector/protocol/Page.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Page.h',
            '<(blink_core_output_dir)/inspector/protocol/Rendering.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Rendering.h',
            '<(blink_core_output_dir)/inspector/protocol/Security.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Security.h',
            '<(blink_core_output_dir)/inspector/protocol/ServiceWorker.cpp',
            '<(blink_core_output_dir)/inspector/protocol/ServiceWorker.h',
            '<(blink_core_output_dir)/inspector/protocol/Storage.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Storage.h',
            '<(blink_core_output_dir)/inspector/protocol/Tracing.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Tracing.h',
            '<(blink_core_output_dir)/inspector/protocol/Worker.cpp',
            '<(blink_core_output_dir)/inspector/protocol/Worker.h',
          ],
          'action': [
            'python',
            '../../platform/inspector_protocol/CodeGenerator.py',
            '--protocol', 'browser_protocol.json',
            '--include', '../../platform/v8_inspector/js_protocol.json',
            '--string_type', 'String',
            '--export_macro', 'CORE_EXPORT',
            '--output_dir', '<(blink_core_output_dir)/inspector/protocol',
            '--output_package', 'core/inspector/protocol',
          ],
          'message': 'Generating Inspector protocol backend sources from json definitions',
        },
      ]
    },
    {
      # GN version: //third_party/WebKit/Source/core/inspector:protocol_version
      'target_name': 'protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateInspectorProtocolVersion',
          'inputs': [
            '../../platform/inspector_protocol/generate-inspector-protocol-version',
            'browser_protocol.json',
            '../../platform/v8_inspector/js_protocol.json',
          ],
          'outputs': [
            '<(blink_core_output_dir)/inspector/protocol.json',
          ],
          'action': [
            'python',
            '../../platform/inspector_protocol/generate-inspector-protocol-version',
            '--o',
            '<@(_outputs)',
            'browser_protocol.json',
            '../../platform/v8_inspector/js_protocol.json'
          ],
          'message': 'Validate inspector protocol for backwards compatibility and generate version file',
        },
      ]
    },
  ],  # targets
}
