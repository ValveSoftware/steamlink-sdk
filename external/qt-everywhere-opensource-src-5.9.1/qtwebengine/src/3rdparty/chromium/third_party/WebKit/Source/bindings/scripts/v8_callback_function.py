# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template values for a callback function.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from v8_globals import includes  # pylint: disable=W0403
import v8_utilities  # pylint: disable=W0403

CALLBACK_FUNCTION_H_INCLUDES = frozenset([
    'bindings/core/v8/ScriptWrappable.h',
    'bindings/core/v8/TraceWrapperV8Reference.h',
    'platform/heap/Handle.h',
    'wtf/text/WTFString.h',
])
CALLBACK_FUNCTION_CPP_INCLUDES = frozenset([
    'bindings/core/v8/ExceptionState.h',
    'bindings/core/v8/ScriptState.h',
    'bindings/core/v8/ToV8.h',
    'bindings/core/v8/V8Binding.h',
    'core/dom/ExecutionContext.h',
    'wtf/Assertions.h',
])


def callback_function_context(callback_function):
    includes.clear()
    includes.update(CALLBACK_FUNCTION_CPP_INCLUDES)
    idl_type = callback_function.idl_type
    idl_type_str = str(idl_type)
    forward_declarations = []
    for argument in callback_function.arguments:
        if argument.idl_type.is_interface_type:
            forward_declarations.append(argument.idl_type)
        argument.idl_type.add_includes_for_type(callback_function.extended_attributes)

    context = {
        'cpp_class': callback_function.name,
        'cpp_includes': sorted(includes),
        'forward_declarations': sorted(forward_declarations),
        'header_includes': sorted(CALLBACK_FUNCTION_H_INCLUDES),
        'idl_type': idl_type_str,
    }

    if idl_type_str != 'void':
        context.update({
            'return_cpp_type': idl_type.cpp_type + '&',
            'return_value': idl_type.v8_value_to_local_cpp_value(
                callback_function.extended_attributes,
                'v8ReturnValue', 'cppValue',
                isolate='m_scriptState->isolate()',
                bailout_return_value='false'),
        })

    context.update(arguments_context(callback_function.arguments, context.get('return_cpp_type')))
    return context


def arguments_context(arguments, return_cpp_type):
    def argument_context(argument):
        return {
            'argument_name': '%sArgument' % argument.name,
            'cpp_value_to_v8_value': argument.idl_type.cpp_value_to_v8_value(
                argument.name, isolate='m_scriptState->isolate()',
                creation_context='m_scriptState->context()->Global()'),
        }

    argument_declarations = [
        'ScriptWrappable* scriptWrappable',
    ]
    argument_declarations.extend(
        '%s %s' % (argument.idl_type.callback_cpp_type, argument.name)
        for argument in arguments)
    if return_cpp_type:
        argument_declarations.append('%s returnValue' % return_cpp_type)
    return {
        'argument_declarations': argument_declarations,
        'arguments': [argument_context(argument) for argument in arguments],
    }
