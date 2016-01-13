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

"""Functions for type handling and type conversion (Blink/C++ <-> V8/JS).

Extends IdlType and IdlUnionType with V8-specific properties, methods, and
class methods.

Spec:
http://www.w3.org/TR/WebIDL/#es-type-mapping

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import posixpath

from idl_types import IdlType, IdlUnionType
import v8_attributes  # for IdlType.constructor_type_name
from v8_globals import includes


################################################################################
# V8-specific handling of IDL types
################################################################################

NON_WRAPPER_TYPES = frozenset([
    'CompareHow',
    'Dictionary',
    'EventHandler',
    'EventListener',
    'MediaQueryListListener',
    'NodeFilter',
    'SerializedScriptValue',
])
TYPED_ARRAYS = {
    # (cpp_type, v8_type), used by constructor templates
    'ArrayBuffer': None,
    'ArrayBufferView': None,
    'Float32Array': ('float', 'v8::kExternalFloatArray'),
    'Float64Array': ('double', 'v8::kExternalDoubleArray'),
    'Int8Array': ('signed char', 'v8::kExternalByteArray'),
    'Int16Array': ('short', 'v8::kExternalShortArray'),
    'Int32Array': ('int', 'v8::kExternalIntArray'),
    'Uint8Array': ('unsigned char', 'v8::kExternalUnsignedByteArray'),
    'Uint8ClampedArray': ('unsigned char', 'v8::kExternalPixelArray'),
    'Uint16Array': ('unsigned short', 'v8::kExternalUnsignedShortArray'),
    'Uint32Array': ('unsigned int', 'v8::kExternalUnsignedIntArray'),
}

IdlType.is_typed_array_type = property(
    lambda self: self.base_type in TYPED_ARRAYS)


IdlType.is_wrapper_type = property(
    lambda self: (self.is_interface_type and
                  self.base_type not in NON_WRAPPER_TYPES))


################################################################################
# C++ types
################################################################################

CPP_TYPE_SAME_AS_IDL_TYPE = set([
    'double',
    'float',
    'long long',
    'unsigned long long',
])
CPP_INT_TYPES = set([
    'byte',
    'long',
    'short',
])
CPP_UNSIGNED_TYPES = set([
    'octet',
    'unsigned int',
    'unsigned long',
    'unsigned short',
])
CPP_SPECIAL_CONVERSION_RULES = {
    'CompareHow': 'Range::CompareHow',
    'Date': 'double',
    'Dictionary': 'Dictionary',
    'EventHandler': 'EventListener*',
    'MediaQueryListListener': 'RefPtrWillBeRawPtr<MediaQueryListListener>',
    'NodeFilter': 'RefPtrWillBeRawPtr<NodeFilter>',
    'Promise': 'ScriptPromise',
    'ScriptValue': 'ScriptValue',
    # FIXME: Eliminate custom bindings for XPathNSResolver  http://crbug.com/345529
    'XPathNSResolver': 'RefPtrWillBeRawPtr<XPathNSResolver>',
    'boolean': 'bool',
    'unrestricted double': 'double',
    'unrestricted float': 'float',
}


def cpp_type(idl_type, extended_attributes=None, used_as_argument=False, used_as_variadic_argument=False, used_in_cpp_sequence=False):
    """Returns C++ type corresponding to IDL type.

    |idl_type| argument is of type IdlType, while return value is a string

    Args:
        idl_type:
            IdlType
        used_as_argument:
            bool, True if idl_type's raw/primitive C++ type should be returned.
        used_in_cpp_sequence:
            bool, True if the C++ type is used as an element of an array or sequence.
    """
    def string_mode():
        # FIXME: the Web IDL spec requires 'EmptyString', not 'NullString',
        # but we use NullString for performance.
        if extended_attributes.get('TreatNullAs') != 'NullString':
            return ''
        if extended_attributes.get('TreatUndefinedAs') != 'NullString':
            return 'WithNullCheck'
        return 'WithUndefinedOrNullCheck'

    extended_attributes = extended_attributes or {}
    idl_type = idl_type.preprocessed_type

    # Composite types
    if used_as_variadic_argument:
        array_or_sequence_type = idl_type
    else:
        array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        vector_type = cpp_ptr_type('Vector', 'HeapVector', array_or_sequence_type.gc_type)
        return cpp_template_type(vector_type, array_or_sequence_type.cpp_type_args(used_in_cpp_sequence=True))

    # Simple types
    base_idl_type = idl_type.base_type

    if base_idl_type in CPP_TYPE_SAME_AS_IDL_TYPE:
        return base_idl_type
    if base_idl_type in CPP_INT_TYPES:
        return 'int'
    if base_idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if base_idl_type in CPP_SPECIAL_CONVERSION_RULES:
        return CPP_SPECIAL_CONVERSION_RULES[base_idl_type]

    if base_idl_type in NON_WRAPPER_TYPES:
        return 'RefPtr<%s>' % base_idl_type
    if base_idl_type in ('DOMString', 'ByteString', 'ScalarValueString'):
        if not used_as_argument:
            return 'String'
        return 'V8StringResource<%s>' % string_mode()

    if idl_type.is_typed_array_type and used_as_argument:
        return base_idl_type + '*'
    if idl_type.is_interface_type:
        implemented_as_class = idl_type.implemented_as
        if used_as_argument:
            return implemented_as_class + '*'
        new_type = 'Member' if used_in_cpp_sequence else 'RawPtr'
        ptr_type = cpp_ptr_type('RefPtr', new_type, idl_type.gc_type)
        return cpp_template_type(ptr_type, implemented_as_class)
    # Default, assume native type is a pointer with same type name as idl type
    return base_idl_type + '*'


def cpp_type_union(idl_type, extended_attributes=None, used_as_argument=False):
    return (member_type.cpp_type for member_type in idl_type.member_types)


# Allow access as idl_type.cpp_type if no arguments
IdlType.cpp_type = property(cpp_type)
IdlUnionType.cpp_type = property(cpp_type_union)
IdlType.cpp_type_args = cpp_type
IdlUnionType.cpp_type_args = cpp_type_union


def cpp_template_type(template, inner_type):
    """Returns C++ template specialized to type, with space added if needed."""
    if inner_type.endswith('>'):
        format_string = '{template}<{inner_type} >'
    else:
        format_string = '{template}<{inner_type}>'
    return format_string.format(template=template, inner_type=inner_type)


def cpp_ptr_type(old_type, new_type, gc_type):
    if gc_type == 'GarbageCollectedObject':
        return new_type
    if gc_type == 'WillBeGarbageCollectedObject':
        if old_type == 'Vector':
            return 'WillBe' + new_type
        return old_type + 'WillBe' + new_type
    return old_type


def v8_type(interface_name):
    return 'V8' + interface_name


# [ImplementedAs]
# This handles [ImplementedAs] on interface types, not [ImplementedAs] in the
# interface being generated. e.g., given:
#   Foo.idl: interface Foo {attribute Bar bar};
#   Bar.idl: [ImplementedAs=Zork] interface Bar {};
# when generating bindings for Foo, the [ImplementedAs] on Bar is needed.
# This data is external to Foo.idl, and hence computed as global information in
# compute_interfaces_info.py to avoid having to parse IDLs of all used interfaces.
IdlType.implemented_as_interfaces = {}


def implemented_as(idl_type):
    base_idl_type = idl_type.base_type
    if base_idl_type in IdlType.implemented_as_interfaces:
        return IdlType.implemented_as_interfaces[base_idl_type]
    return base_idl_type


IdlType.implemented_as = property(implemented_as)

IdlType.set_implemented_as_interfaces = classmethod(
    lambda cls, new_implemented_as_interfaces:
        cls.implemented_as_interfaces.update(new_implemented_as_interfaces))


# [GarbageCollected]
IdlType.garbage_collected_types = set()

IdlType.is_garbage_collected = property(
    lambda self: self.base_type in IdlType.garbage_collected_types)

IdlType.set_garbage_collected_types = classmethod(
    lambda cls, new_garbage_collected_types:
        cls.garbage_collected_types.update(new_garbage_collected_types))


# [WillBeGarbageCollected]
IdlType.will_be_garbage_collected_types = set()

IdlType.is_will_be_garbage_collected = property(
    lambda self: self.base_type in IdlType.will_be_garbage_collected_types)

IdlType.set_will_be_garbage_collected_types = classmethod(
    lambda cls, new_will_be_garbage_collected_types:
        cls.will_be_garbage_collected_types.update(new_will_be_garbage_collected_types))


def gc_type(idl_type):
    if idl_type.is_garbage_collected:
        return 'GarbageCollectedObject'
    if idl_type.is_will_be_garbage_collected:
        return 'WillBeGarbageCollectedObject'
    return 'RefCountedObject'

IdlType.gc_type = property(gc_type)


################################################################################
# Includes
################################################################################

def includes_for_cpp_class(class_name, relative_dir_posix):
    return set([posixpath.join('bindings', relative_dir_posix, class_name + '.h')])


INCLUDES_FOR_TYPE = {
    'object': set(),
    'CompareHow': set(),
    'Dictionary': set(['bindings/v8/Dictionary.h']),
    'EventHandler': set(['bindings/v8/V8AbstractEventListener.h',
                         'bindings/v8/V8EventListenerList.h']),
    'EventListener': set(['bindings/v8/BindingSecurity.h',
                          'bindings/v8/V8EventListenerList.h',
                          'core/frame/LocalDOMWindow.h']),
    'HTMLCollection': set(['bindings/core/v8/V8HTMLCollection.h',
                           'core/dom/ClassCollection.h',
                           'core/dom/TagCollection.h',
                           'core/html/HTMLCollection.h',
                           'core/html/HTMLFormControlsCollection.h',
                           'core/html/HTMLTableRowsCollection.h']),
    'MediaQueryListListener': set(['core/css/MediaQueryListListener.h']),
    'NodeList': set(['bindings/core/v8/V8NodeList.h',
                     'core/dom/NameNodeList.h',
                     'core/dom/NodeList.h',
                     'core/dom/StaticNodeList.h',
                     'core/html/LabelsNodeList.h']),
    'Promise': set(['bindings/v8/ScriptPromise.h']),
    'SerializedScriptValue': set(['bindings/v8/SerializedScriptValue.h']),
    'ScriptValue': set(['bindings/v8/ScriptValue.h']),
}


def includes_for_type(idl_type):
    idl_type = idl_type.preprocessed_type

    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        return includes_for_type(array_or_sequence_type)

    # Simple types
    base_idl_type = idl_type.base_type
    if base_idl_type in INCLUDES_FOR_TYPE:
        return INCLUDES_FOR_TYPE[base_idl_type]
    if idl_type.is_basic_type:
        return set()
    if idl_type.is_typed_array_type:
        return set(['bindings/v8/custom/V8%sCustom.h' % base_idl_type])
    if base_idl_type.endswith('ConstructorConstructor'):
        # FIXME: rename to NamedConstructor
        # FIXME: replace with a [NamedConstructorAttribute] extended attribute
        # Ending with 'ConstructorConstructor' indicates a named constructor,
        # and these do not have header files, as they are part of the generated
        # bindings for the interface
        return set()
    if base_idl_type.endswith('Constructor'):
        # FIXME: replace with a [ConstructorAttribute] extended attribute
        base_idl_type = idl_type.constructor_type_name
    return set(['bindings/%s/v8/V8%s.h' % (component_dir[base_idl_type],
                                           base_idl_type)])

IdlType.includes_for_type = property(includes_for_type)
IdlUnionType.includes_for_type = property(
    lambda self: set.union(*[includes_for_type(member_type)
                             for member_type in self.member_types]))


def add_includes_for_type(idl_type):
    includes.update(idl_type.includes_for_type)

IdlType.add_includes_for_type = add_includes_for_type
IdlUnionType.add_includes_for_type = add_includes_for_type


def includes_for_interface(interface_name):
    return IdlType(interface_name).includes_for_type


def add_includes_for_interface(interface_name):
    includes.update(includes_for_interface(interface_name))

component_dir = {}


def set_component_dirs(new_component_dirs):
        component_dir.update(new_component_dirs)


################################################################################
# V8 -> C++
################################################################################

V8_VALUE_TO_CPP_VALUE = {
    # Basic
    'Date': 'toCoreDate({v8_value})',
    'DOMString': '{v8_value}',
    'ByteString': 'toByteString({arguments})',
    'ScalarValueString': 'toScalarValueString({arguments})',
    'boolean': '{v8_value}->BooleanValue()',
    'float': 'static_cast<float>({v8_value}->NumberValue())',
    'unrestricted float': 'static_cast<float>({v8_value}->NumberValue())',
    'double': 'static_cast<double>({v8_value}->NumberValue())',
    'unrestricted double': 'static_cast<double>({v8_value}->NumberValue())',
    'byte': 'toInt8({arguments})',
    'octet': 'toUInt8({arguments})',
    'short': 'toInt16({arguments})',
    'unsigned short': 'toUInt16({arguments})',
    'long': 'toInt32({arguments})',
    'unsigned long': 'toUInt32({arguments})',
    'long long': 'toInt64({arguments})',
    'unsigned long long': 'toUInt64({arguments})',
    # Interface types
    'CompareHow': 'static_cast<Range::CompareHow>({v8_value}->Int32Value())',
    'Dictionary': 'Dictionary({v8_value}, info.GetIsolate())',
    'EventTarget': 'V8DOMWrapper::isDOMWrapper({v8_value}) ? toWrapperTypeInfo(v8::Handle<v8::Object>::Cast({v8_value}))->toEventTarget(v8::Handle<v8::Object>::Cast({v8_value})) : 0',
    'MediaQueryListListener': 'MediaQueryListListener::create(ScriptState::current(info.GetIsolate()), ScriptValue(ScriptState::current(info.GetIsolate()), {v8_value}))',
    'NodeFilter': 'toNodeFilter({v8_value}, info.Holder(), ScriptState::current(info.GetIsolate()))',
    'Promise': 'ScriptPromise::cast(ScriptState::current(info.GetIsolate()), {v8_value})',
    'SerializedScriptValue': 'SerializedScriptValue::create({v8_value}, info.GetIsolate())',
    'ScriptValue': 'ScriptValue(ScriptState::current(info.GetIsolate()), {v8_value})',
    'Window': 'toDOMWindow({v8_value}, info.GetIsolate())',
    'XPathNSResolver': 'toXPathNSResolver({v8_value}, info.GetIsolate())',
}


def v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, index):
    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        return v8_value_to_cpp_value_array_or_sequence(array_or_sequence_type, v8_value, index)

    # Simple types
    idl_type = idl_type.preprocessed_type
    add_includes_for_type(idl_type)
    base_idl_type = idl_type.base_type

    if 'EnforceRange' in extended_attributes:
        arguments = ', '.join([v8_value, 'EnforceRange', 'exceptionState'])
    elif (idl_type.is_integer_type or  # NormalConversion
          idl_type.name in ('ByteString', 'ScalarValueString')):
        arguments = ', '.join([v8_value, 'exceptionState'])
    else:
        arguments = v8_value

    if base_idl_type in V8_VALUE_TO_CPP_VALUE:
        cpp_expression_format = V8_VALUE_TO_CPP_VALUE[base_idl_type]
    elif idl_type.is_typed_array_type:
        cpp_expression_format = (
            '{v8_value}->Is{idl_type}() ? '
            'V8{idl_type}::toNative(v8::Handle<v8::{idl_type}>::Cast({v8_value})) : 0')
    else:
        cpp_expression_format = (
            'V8{idl_type}::toNativeWithTypeCheck(info.GetIsolate(), {v8_value})')

    return cpp_expression_format.format(arguments=arguments, idl_type=base_idl_type, v8_value=v8_value)


def v8_value_to_cpp_value_array_or_sequence(array_or_sequence_type, v8_value, index):
    # Index is None for setters, index (starting at 0) for method arguments,
    # and is used to provide a human-readable exception message
    if index is None:
        index = 0  # special case, meaning "setter"
    else:
        index += 1  # human-readable index
    if (array_or_sequence_type.is_interface_type and
        array_or_sequence_type.name != 'Dictionary'):
        this_cpp_type = None
        ref_ptr_type = cpp_ptr_type('RefPtr', 'Member', array_or_sequence_type.gc_type)
        expression_format = '(to{ref_ptr_type}NativeArray<{array_or_sequence_type}, V8{array_or_sequence_type}>({v8_value}, {index}, info.GetIsolate()))'
        add_includes_for_type(array_or_sequence_type)
    else:
        ref_ptr_type = None
        this_cpp_type = array_or_sequence_type.cpp_type
        expression_format = 'toNativeArray<{cpp_type}>({v8_value}, {index}, info.GetIsolate())'
    expression = expression_format.format(array_or_sequence_type=array_or_sequence_type.name, cpp_type=this_cpp_type, index=index, ref_ptr_type=ref_ptr_type, v8_value=v8_value)
    return expression


def v8_value_to_local_cpp_value(idl_type, extended_attributes, v8_value, variable_name, index=None, declare_variable=True):
    """Returns an expression that converts a V8 value to a C++ value and stores it as a local value."""
    this_cpp_type = idl_type.cpp_type_args(extended_attributes=extended_attributes, used_as_argument=True)

    idl_type = idl_type.preprocessed_type
    cpp_value = v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, index)
    args = [variable_name, cpp_value]
    if idl_type.base_type == 'DOMString' and not idl_type.array_or_sequence_type:
        macro = 'TOSTRING_VOID'
    elif (idl_type.is_integer_type or
          idl_type.name in ('ByteString', 'ScalarValueString')):
        macro = 'TONATIVE_VOID_EXCEPTIONSTATE'
        args.append('exceptionState')
    else:
        macro = 'TONATIVE_VOID'

    # Macros come in several variants, to minimize expensive creation of
    # v8::TryCatch.
    suffix = ''

    if declare_variable:
        args.insert(0, this_cpp_type)
    else:
        suffix += '_INTERNAL'

    return '%s(%s)' % (macro + suffix, ', '.join(args))


IdlType.v8_value_to_local_cpp_value = v8_value_to_local_cpp_value
IdlUnionType.v8_value_to_local_cpp_value = v8_value_to_local_cpp_value


################################################################################
# C++ -> V8
################################################################################

def preprocess_idl_type(idl_type):
    if idl_type.is_enum:
        # Enumerations are internally DOMStrings
        return IdlType('DOMString')
    if (idl_type.name == 'Any' or idl_type.is_callback_function):
        return IdlType('ScriptValue')
    return idl_type

IdlType.preprocessed_type = property(preprocess_idl_type)
IdlUnionType.preprocessed_type = property(preprocess_idl_type)


def preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes):
    """Returns IDL type and value, with preliminary type conversions applied."""
    idl_type = idl_type.preprocessed_type
    if idl_type.name == 'Promise':
        idl_type = IdlType('ScriptValue')
    if idl_type.base_type in ['long long', 'unsigned long long']:
        # long long and unsigned long long are not representable in ECMAScript;
        # we represent them as doubles.
        idl_type = IdlType('double', is_nullable=idl_type.is_nullable)
        cpp_value = 'static_cast<double>(%s)' % cpp_value
    # HTML5 says that unsigned reflected attributes should be in the range
    # [0, 2^31). When a value isn't in this range, a default value (or 0)
    # should be returned instead.
    extended_attributes = extended_attributes or {}
    if ('Reflect' in extended_attributes and
        idl_type.base_type in ['unsigned long', 'unsigned short']):
        cpp_value = cpp_value.replace('getUnsignedIntegralAttribute',
                                      'getIntegralAttribute')
        cpp_value = 'std::max(0, %s)' % cpp_value
    return idl_type, cpp_value


def v8_conversion_type(idl_type, extended_attributes):
    """Returns V8 conversion type, adding any additional includes.

    The V8 conversion type is used to select the C++ -> V8 conversion function
    or v8SetReturnValue* function; it can be an idl_type, a cpp_type, or a
    separate name for the type of conversion (e.g., 'DOMWrapper').
    """
    extended_attributes = extended_attributes or {}

    # Composite types
    array_or_sequence_type = idl_type.array_or_sequence_type
    if array_or_sequence_type:
        if array_or_sequence_type.is_interface_type:
            add_includes_for_type(array_or_sequence_type)
        return 'array'

    # Simple types
    base_idl_type = idl_type.base_type
    # Basic types, without additional includes
    if base_idl_type in CPP_INT_TYPES:
        return 'int'
    if base_idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if base_idl_type in ('DOMString', 'ByteString', 'ScalarValueString'):
        if 'TreatReturnedNullStringAs' not in extended_attributes:
            return base_idl_type
        treat_returned_null_string_as = extended_attributes['TreatReturnedNullStringAs']
        if treat_returned_null_string_as == 'Null':
            return 'StringOrNull'
        if treat_returned_null_string_as == 'Undefined':
            return 'StringOrUndefined'
        raise 'Unrecognized TreatReturnNullStringAs value: "%s"' % treat_returned_null_string_as
    if idl_type.is_basic_type or base_idl_type == 'ScriptValue':
        return base_idl_type

    # Data type with potential additional includes
    add_includes_for_type(idl_type)
    if base_idl_type in V8_SET_RETURN_VALUE:  # Special v8SetReturnValue treatment
        return base_idl_type

    # Pointer type
    return 'DOMWrapper'

IdlType.v8_conversion_type = v8_conversion_type


V8_SET_RETURN_VALUE = {
    'boolean': 'v8SetReturnValueBool(info, {cpp_value})',
    'int': 'v8SetReturnValueInt(info, {cpp_value})',
    'unsigned': 'v8SetReturnValueUnsigned(info, {cpp_value})',
    'DOMString': 'v8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    'ByteString': 'v8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    'ScalarValueString': 'v8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    # [TreatNullReturnValueAs]
    'StringOrNull': 'v8SetReturnValueStringOrNull(info, {cpp_value}, info.GetIsolate())',
    'StringOrUndefined': 'v8SetReturnValueStringOrUndefined(info, {cpp_value}, info.GetIsolate())',
    'void': '',
    # No special v8SetReturnValue* function (set value directly)
    'float': 'v8SetReturnValue(info, {cpp_value})',
    'unrestricted float': 'v8SetReturnValue(info, {cpp_value})',
    'double': 'v8SetReturnValue(info, {cpp_value})',
    'unrestricted double': 'v8SetReturnValue(info, {cpp_value})',
    # No special v8SetReturnValue* function, but instead convert value to V8
    # and then use general v8SetReturnValue.
    'array': 'v8SetReturnValue(info, {cpp_value})',
    'Date': 'v8SetReturnValue(info, {cpp_value})',
    'EventHandler': 'v8SetReturnValue(info, {cpp_value})',
    'ScriptValue': 'v8SetReturnValue(info, {cpp_value})',
    'SerializedScriptValue': 'v8SetReturnValue(info, {cpp_value})',
    # DOMWrapper
    'DOMWrapperForMainWorld': 'v8SetReturnValueForMainWorld(info, WTF::getPtr({cpp_value}))',
    'DOMWrapperFast': 'v8SetReturnValueFast(info, WTF::getPtr({cpp_value}), {script_wrappable})',
    'DOMWrapperDefault': 'v8SetReturnValue(info, {cpp_value})',
}


def v8_set_return_value(idl_type, cpp_value, extended_attributes=None, script_wrappable='', release=False, for_main_world=False):
    """Returns a statement that converts a C++ value to a V8 value and sets it as a return value.

    """
    def dom_wrapper_conversion_type():
        if not script_wrappable:
            return 'DOMWrapperDefault'
        if for_main_world:
            return 'DOMWrapperForMainWorld'
        return 'DOMWrapperFast'

    idl_type, cpp_value = preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = idl_type.v8_conversion_type(extended_attributes)
    # SetReturn-specific overrides
    if this_v8_conversion_type in ['Date', 'EventHandler', 'ScriptValue', 'SerializedScriptValue', 'array']:
        # Convert value to V8 and then use general v8SetReturnValue
        cpp_value = idl_type.cpp_value_to_v8_value(cpp_value, extended_attributes=extended_attributes)
    if this_v8_conversion_type == 'DOMWrapper':
        this_v8_conversion_type = dom_wrapper_conversion_type()

    format_string = V8_SET_RETURN_VALUE[this_v8_conversion_type]
    # FIXME: oilpan: Remove .release() once we remove all RefPtrs from generated code.
    if release:
        cpp_value = '%s.release()' % cpp_value
    statement = format_string.format(cpp_value=cpp_value, script_wrappable=script_wrappable)
    return statement


def v8_set_return_value_union(idl_type, cpp_value, extended_attributes=None, script_wrappable='', release=False, for_main_world=False):
    """
    release: can be either False (False for all member types) or
             a sequence (list or tuple) of booleans (if specified individually).
    """

    return [
        member_type.v8_set_return_value(cpp_value + str(i),
                                        extended_attributes,
                                        script_wrappable,
                                        release and release[i],
                                        for_main_world)
            for i, member_type in
            enumerate(idl_type.member_types)]

IdlType.v8_set_return_value = v8_set_return_value
IdlUnionType.v8_set_return_value = v8_set_return_value_union

IdlType.release = property(lambda self: self.is_interface_type)
IdlUnionType.release = property(
    lambda self: [member_type.is_interface_type
                  for member_type in self.member_types])


CPP_VALUE_TO_V8_VALUE = {
    # Built-in types
    'Date': 'v8DateOrNaN({cpp_value}, {isolate})',
    'DOMString': 'v8String({isolate}, {cpp_value})',
    'ByteString': 'v8String({isolate}, {cpp_value})',
    'ScalarValueString': 'v8String({isolate}, {cpp_value})',
    'boolean': 'v8Boolean({cpp_value}, {isolate})',
    'int': 'v8::Integer::New({isolate}, {cpp_value})',
    'unsigned': 'v8::Integer::NewFromUnsigned({isolate}, {cpp_value})',
    'float': 'v8::Number::New({isolate}, {cpp_value})',
    'unrestricted float': 'v8::Number::New({isolate}, {cpp_value})',
    'double': 'v8::Number::New({isolate}, {cpp_value})',
    'unrestricted double': 'v8::Number::New({isolate}, {cpp_value})',
    'void': 'v8Undefined()',
    # Special cases
    'EventHandler': '{cpp_value} ? v8::Handle<v8::Value>(V8AbstractEventListener::cast({cpp_value})->getListenerObject(impl->executionContext())) : v8::Handle<v8::Value>(v8::Null({isolate}))',
    'ScriptValue': '{cpp_value}.v8Value()',
    'SerializedScriptValue': '{cpp_value} ? {cpp_value}->deserialize() : v8::Handle<v8::Value>(v8::Null({isolate}))',
    # General
    'array': 'v8Array({cpp_value}, {creation_context}, {isolate})',
    'DOMWrapper': 'toV8({cpp_value}, {creation_context}, {isolate})',
}


def cpp_value_to_v8_value(idl_type, cpp_value, isolate='info.GetIsolate()', creation_context='info.Holder()', extended_attributes=None):
    """Returns an expression that converts a C++ value to a V8 value."""
    # the isolate parameter is needed for callback interfaces
    idl_type, cpp_value = preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = idl_type.v8_conversion_type(extended_attributes)
    format_string = CPP_VALUE_TO_V8_VALUE[this_v8_conversion_type]
    statement = format_string.format(cpp_value=cpp_value, isolate=isolate, creation_context=creation_context)
    return statement

IdlType.cpp_value_to_v8_value = cpp_value_to_v8_value
