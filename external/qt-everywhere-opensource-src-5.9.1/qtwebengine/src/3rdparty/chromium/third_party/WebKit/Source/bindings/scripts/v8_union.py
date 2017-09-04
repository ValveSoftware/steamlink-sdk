# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import v8_types
import v8_utilities


UNION_CPP_INCLUDES = frozenset([
    'bindings/core/v8/ToV8.h',
])

UNION_H_INCLUDES = frozenset([
    'bindings/core/v8/Dictionary.h',
    'bindings/core/v8/ExceptionState.h',
    'bindings/core/v8/V8Binding.h',
    'platform/heap/Handle.h',
])

UNION_CPP_INCLUDES_BLACKLIST = frozenset([
    # This header defines static functions needed to implement event handler
    # attributes in interfaces that implement GlobalEventHandlers. They are not
    # needed or used by UnionTypes*.cpp, so including the header causes
    # compilation errors.
    # FIXME: We should solve this problem in a way that doesn't involve special-
    # casing a header like this.
    'core/dom/GlobalEventHandlers.h',
])


cpp_includes = set()
header_forward_decls = set()
header_includes = set()


def container_context(union_type, interfaces_info):
    cpp_includes.clear()
    header_forward_decls.clear()
    header_includes.clear()
    cpp_includes.update(UNION_CPP_INCLUDES)
    header_includes.update(UNION_H_INCLUDES)
    members = []

    # These variables refer to member contexts if the given union type has
    # corresponding types. They are used for V8 -> impl conversion.
    array_buffer_type = None
    array_buffer_view_type = None
    array_or_sequence_type = None
    boolean_type = None
    dictionary_type = None
    interface_types = []
    numeric_type = None
    object_type = None
    string_type = None
    for member in union_type.member_types:
        context = member_context(member, interfaces_info)
        members.append(context)
        if member.base_type == 'ArrayBuffer':
            if array_buffer_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            array_buffer_type = context
        elif member.base_type == 'ArrayBufferView':
            if array_buffer_view_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            array_buffer_view_type = context
        elif member.is_dictionary:
            if dictionary_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            dictionary_type = context
        elif member.is_array_or_sequence_type:
            if array_or_sequence_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            array_or_sequence_type = context
        # "Dictionary" is an object, rather than an IDL dictionary.
        elif member.base_type == 'Dictionary':
            if object_type:
                raise Exception('%s is ambiguous.' % union_type.name)
            object_type = context
        elif member.is_interface_type:
            interface_types.append(context)
        elif member is union_type.boolean_member_type:
            boolean_type = context
        elif member is union_type.numeric_member_type:
            numeric_type = context
        elif member is union_type.string_member_type:
            string_type = context
        else:
            raise Exception('%s is not supported as an union member.' % member.name)

    # Nullable restriction checks
    nullable_members = union_type.number_of_nullable_member_types
    if nullable_members > 1:
        raise Exception('%s contains more than one nullable members' % union_type.name)
    if dictionary_type and nullable_members == 1:
        raise Exception('%s has a dictionary and a nullable member' % union_type.name)

    cpp_class = union_type.cpp_type
    return {
        'array_buffer_type': array_buffer_type,
        'array_buffer_view_type': array_buffer_view_type,
        'array_or_sequence_type': array_or_sequence_type,
        'boolean_type': boolean_type,
        'cpp_class': cpp_class,
        'cpp_includes': sorted(cpp_includes - UNION_CPP_INCLUDES_BLACKLIST),
        'dictionary_type': dictionary_type,
        'header_includes': sorted(header_includes),
        'header_forward_decls': sorted(header_forward_decls),
        'includes_nullable_type': union_type.includes_nullable_type,
        'interface_types': interface_types,
        'members': members,
        'numeric_type': numeric_type,
        'object_type': object_type,
        'string_type': string_type,
        'type_string': str(union_type),
        'v8_class': v8_types.v8_type(cpp_class),
    }


def _update_includes_and_forward_decls(member, interface_info):
    if interface_info:
        cpp_includes.update(interface_info.get(
            'dependencies_include_paths', []))
        # We need complete types for IDL dictionaries in union containers.
        if member.is_dictionary:
            header_includes.update(member.includes_for_type())
        else:
            cpp_includes.update(member.includes_for_type())
            header_forward_decls.add(member.implemented_as)
    else:
        cpp_includes.update(member.includes_for_type())


def member_context(member, interfaces_info):
    interface_info = interfaces_info.get(member.name, None)
    _update_includes_and_forward_decls(member, interface_info)
    if member.is_nullable:
        member = member.inner_type
    return {
        'cpp_name': v8_utilities.uncapitalize(member.name),
        'cpp_type': member.cpp_type_args(used_in_cpp_sequence=True),
        'cpp_local_type': member.cpp_type,
        'cpp_value_to_v8_value': member.cpp_value_to_v8_value(
            cpp_value='impl.getAs%s()' % member.name, isolate='isolate',
            creation_context='creationContext'),
        'enum_values': member.enum_values,
        'is_array_buffer_or_view_type': member.is_array_buffer_or_view,
        'is_traceable': member.is_traceable,
        'rvalue_cpp_type': member.cpp_type_args(used_as_rvalue_type=True),
        'specific_type_enum': 'SpecificType' + member.name,
        'type_name': member.name,
        'v8_value_to_local_cpp_value': member.v8_value_to_local_cpp_value(
            {}, 'v8Value', 'cppValue', isolate='isolate',
            use_exception_state=True, restricted_float=True),
    }
