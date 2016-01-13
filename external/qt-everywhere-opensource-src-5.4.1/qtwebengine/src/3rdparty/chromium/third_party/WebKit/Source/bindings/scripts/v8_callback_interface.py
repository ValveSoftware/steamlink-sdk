# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Generate template values for a callback interface.

Extends IdlType with property |callback_cpp_type|.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from idl_types import IdlType
from v8_globals import includes
import v8_types
import v8_utilities

CALLBACK_INTERFACE_H_INCLUDES = frozenset([
    'bindings/v8/ActiveDOMCallback.h',
    'bindings/v8/DOMWrapperWorld.h',
    'bindings/v8/ScopedPersistent.h',
])
CALLBACK_INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8Callback.h',
    'core/dom/ExecutionContext.h',
    'wtf/Assertions.h',
    'wtf/GetPtr.h',
    'wtf/RefPtr.h',
])


def cpp_type(idl_type):
    # FIXME: remove this function by making callback types consistent
    # (always use usual v8_types.cpp_type)
    idl_type_name = idl_type.name
    if idl_type_name == 'String':
        return 'const String&'
    if idl_type_name == 'void':
        return 'void'
    # Callbacks use raw pointers, so used_as_argument=True
    usual_cpp_type = idl_type.cpp_type_args(used_as_argument=True)
    if usual_cpp_type.startswith(('Vector', 'HeapVector', 'WillBeHeapVector')):
        return 'const %s&' % usual_cpp_type
    return usual_cpp_type

IdlType.callback_cpp_type = property(cpp_type)


def generate_callback_interface(callback_interface):
    includes.clear()
    includes.update(CALLBACK_INTERFACE_CPP_INCLUDES)
    return {
        'conditional_string': v8_utilities.conditional_string(callback_interface),
        'cpp_class': callback_interface.name,
        'v8_class': v8_utilities.v8_class_name(callback_interface),
        'header_includes': set(CALLBACK_INTERFACE_H_INCLUDES),
        'methods': [generate_method(operation)
                    for operation in callback_interface.operations],
    }


def add_includes_for_operation(operation):
    operation.idl_type.add_includes_for_type()
    for argument in operation.arguments:
        argument.idl_type.add_includes_for_type()


def generate_method(operation):
    extended_attributes = operation.extended_attributes
    idl_type = operation.idl_type
    idl_type_str = str(idl_type)
    if idl_type_str not in ['boolean', 'void']:
        raise Exception('We only support callbacks that return boolean or void values.')
    is_custom = 'Custom' in extended_attributes
    if not is_custom:
        add_includes_for_operation(operation)
    call_with = extended_attributes.get('CallWith')
    call_with_this_handle = v8_utilities.extended_attribute_value_contains(call_with, 'ThisValue')
    contents = {
        'call_with_this_handle': call_with_this_handle,
        'cpp_type': idl_type.callback_cpp_type,
        'custom': is_custom,
        'idl_type': idl_type_str,
        'name': operation.name,
    }
    contents.update(generate_arguments_contents(operation.arguments, call_with_this_handle))
    return contents


def generate_arguments_contents(arguments, call_with_this_handle):
    def generate_argument(argument):
        return {
            'handle': '%sHandle' % argument.name,
            # FIXME: setting creation_context=v8::Handle<v8::Object>() is
            # wrong, as toV8 then implicitly uses the current context, which
            # causes leaks between isolated worlds if a different context is
            # used.
            'cpp_value_to_v8_value': argument.idl_type.cpp_value_to_v8_value(
                argument.name, isolate='isolate',
                creation_context='m_scriptState->context()->Global()'),
        }

    argument_declarations = ['ScriptValue thisValue'] if call_with_this_handle else []
    argument_declarations.extend(
        '%s %s' % (argument.idl_type.callback_cpp_type, argument.name)
        for argument in arguments)
    return  {
        'argument_declarations': argument_declarations,
        'arguments': [generate_argument(argument) for argument in arguments],
    }
