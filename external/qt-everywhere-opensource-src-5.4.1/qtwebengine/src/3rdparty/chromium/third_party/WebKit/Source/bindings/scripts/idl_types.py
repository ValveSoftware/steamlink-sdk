# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""IDL type handling.

Classes:
IdlType
IdlUnionType
"""

from collections import defaultdict


################################################################################
# IDL types
################################################################################

INTEGER_TYPES = frozenset([
    # http://www.w3.org/TR/WebIDL/#dfn-integer-type
    'byte',
    'octet',
    'short',
    'unsigned short',
    # int and unsigned are not IDL types
    'long',
    'unsigned long',
    'long long',
    'unsigned long long',
])
NUMERIC_TYPES = (INTEGER_TYPES | frozenset([
    # http://www.w3.org/TR/WebIDL/#dfn-numeric-type
    'float',
    'unrestricted float',
    'double',
    'unrestricted double',
]))
# http://www.w3.org/TR/WebIDL/#dfn-primitive-type
PRIMITIVE_TYPES = (frozenset(['boolean']) | NUMERIC_TYPES)
BASIC_TYPES = (PRIMITIVE_TYPES | frozenset([
    # Built-in, non-composite, non-object data types
    # http://heycam.github.io/webidl/#idl-types
    'DOMString',
    'ByteString',
    'Date',
    # http://heycam.github.io/webidl/#es-type-mapping
    'void',
    # http://encoding.spec.whatwg.org/#type-scalarvaluestring
    'ScalarValueString',
]))
TYPE_NAMES = {
    # http://heycam.github.io/webidl/#dfn-type-name
    'any': 'Any',
    'boolean': 'Boolean',
    'byte': 'Byte',
    'octet': 'Octet',
    'short': 'Short',
    'unsigned short': 'UnsignedShort',
    'long': 'Long',
    'unsigned long': 'UnsignedLong',
    'long long': 'LongLong',
    'unsigned long long': 'UnsignedLongLong',
    'float': 'Float',
    'unrestricted float': 'UnrestrictedFloat',
    'double': 'Double',
    'unrestricted double': 'UnrestrictedDouble',
    'DOMString': 'String',
    'ByteString': 'ByteString',
    'ScalarValueString': 'ScalarValueString',
    'object': 'Object',
    'Date': 'Date',
}


################################################################################
# Inheritance
################################################################################

ancestors = defaultdict(list)  # interface_name -> ancestors

def inherits_interface(interface_name, ancestor_name):
    return (interface_name == ancestor_name or
            ancestor_name in ancestors[interface_name])


def set_ancestors(new_ancestors):
    ancestors.update(new_ancestors)


################################################################################
# IdlType
################################################################################

class IdlType(object):
    # FIXME: incorporate Nullable, etc.
    # FIXME: use nested types: IdlArrayType, IdlNullableType, IdlSequenceType
    # to support types like short?[] vs. short[]?, instead of treating these
    # as orthogonal properties (via flags).
    callback_functions = set()
    callback_interfaces = set()
    enums = {}  # name -> values

    def __init__(self, base_type, is_array=False, is_sequence=False, is_nullable=False, is_unrestricted=False):
        if is_array and is_sequence:
            raise ValueError('Array of Sequences are not allowed.')
        if is_unrestricted:
            self.base_type = 'unrestricted %s' % base_type
        else:
            self.base_type = base_type
        self.is_array = is_array
        self.is_sequence = is_sequence
        self.is_nullable = is_nullable

    def __str__(self):
        type_string = self.base_type
        if self.is_array:
            return type_string + '[]'
        if self.is_sequence:
            return 'sequence<%s>' % type_string
        if self.is_nullable:
            # FIXME: Dictionary::ConversionContext::setConversionType can't
            # handle the '?' in nullable types (passes nullability separately).
            # Update that function to handle nullability from the type name,
            # simplifying its signature.
            # return type_string + '?'
            return type_string
        return type_string

    # FIXME: rename to native_array_element_type and move to v8_types.py
    @property
    def array_or_sequence_type(self):
        return self.array_type or self.sequence_type

    # FIXME: rename to array_element_type
    @property
    def array_type(self):
        return self.is_array and IdlType(self.base_type)

    # FIXME: rename to sequence_element_type
    @property
    def sequence_type(self):
        return self.is_sequence and IdlType(self.base_type)

    @property
    def is_basic_type(self):
        return self.base_type in BASIC_TYPES and not self.array_or_sequence_type

    @property
    def is_callback_function(self):
        return self.base_type in IdlType.callback_functions

    @property
    def is_callback_interface(self):
        return self.base_type in IdlType.callback_interfaces

    @property
    def is_composite_type(self):
        return (self.name == 'Any' or
                self.array_type or
                self.sequence_type or
                self.is_union_type)

    @property
    def is_enum(self):
        # FIXME: add an IdlEnumType class and a resolve_enums step at end of
        # IdlDefinitions constructor
        return self.name in IdlType.enums

    @property
    def enum_values(self):
        return IdlType.enums[self.name]

    @property
    def is_integer_type(self):
        return self.base_type in INTEGER_TYPES and not self.array_or_sequence_type

    @property
    def is_numeric_type(self):
        return self.base_type in NUMERIC_TYPES and not self.array_or_sequence_type

    @property
    def is_primitive_type(self):
        return self.base_type in PRIMITIVE_TYPES and not self.array_or_sequence_type

    @property
    def is_interface_type(self):
        # Anything that is not another type is an interface type.
        # http://www.w3.org/TR/WebIDL/#idl-types
        # http://www.w3.org/TR/WebIDL/#idl-interface
        # In C++ these are RefPtr or PassRefPtr types.
        return not(self.is_basic_type or
                   self.is_composite_type or
                   self.is_callback_function or
                   self.is_enum or
                   self.name == 'Object' or
                   self.name == 'Promise')  # Promise will be basic in future

    @property
    def is_union_type(self):
        return isinstance(self, IdlUnionType)

    @property
    def name(self):
        """Return type name.

        http://heycam.github.io/webidl/#dfn-type-name
        """
        base_type = self.base_type
        base_type_name = TYPE_NAMES.get(base_type, base_type)
        if self.is_array:
            return base_type_name + 'Array'
        if self.is_sequence:
            return base_type_name + 'Sequence'
        if self.is_nullable:
            return base_type_name + 'OrNull'
        return base_type_name

    @classmethod
    def set_callback_functions(cls, new_callback_functions):
        cls.callback_functions.update(new_callback_functions)

    @classmethod
    def set_callback_interfaces(cls, new_callback_interfaces):
        cls.callback_interfaces.update(new_callback_interfaces)

    @classmethod
    def set_enums(cls, new_enums):
        cls.enums.update(new_enums)

    def resolve_typedefs(self, typedefs):
        if self.base_type not in typedefs:
            return self
        new_type = typedefs[self.base_type]
        if type(new_type) != type(self):
            # If type changes, need to return a different object,
            # since can't change type(self)
            return new_type
        # If type doesn't change, just mutate self to avoid a new object
        # FIXME: a bit ugly; use __init__ instead of setting flags
        self.base_type = new_type.base_type
        # handle array both in use and in typedef itself:
        # typedef Type TypeDef;
        # TypeDef[] ...
        # and:
        # typedef Type[] TypeArray
        # TypeArray ...
        self.is_array |= new_type.is_array
        self.is_sequence |= new_type.is_sequence
        return self


################################################################################
# IdlUnionType
################################################################################

class IdlUnionType(object):
    # http://heycam.github.io/webidl/#idl-union
    # FIXME: derive from IdlType, instead of stand-alone class, to reduce
    # duplication.
    def __init__(self, member_types, is_nullable=False):
        self.member_types = member_types
        self.is_nullable = is_nullable

    @property
    def array_or_sequence_type(self):
        return False

    @property
    def array_type(self):
        return False

    @property
    def is_array(self):
        # We do not support arrays of union types
        return False

    @property
    def base_type(self):
        return None

    @property
    def is_basic_type(self):
        return False

    @property
    def is_callback_function(self):
        return False

    @property
    def is_enum(self):
        return False

    @property
    def is_integer_type(self):
        return False

    @property
    def is_numeric_type(self):
        return False

    @property
    def is_primitivee_type(self):
        return False

    @property
    def is_sequence(self):
        # We do not support sequences of union types
        return False

    @property
    def is_union_type(self):
        return True

    @property
    def name(self):
        return 'Or'.join(member_type.name for member_type in self.member_types)

    def resolve_typedefs(self, typedefs):
        self.member_types = [
            typedefs.get(member_type, member_type)
            for member_type in self.member_types]
        return self
