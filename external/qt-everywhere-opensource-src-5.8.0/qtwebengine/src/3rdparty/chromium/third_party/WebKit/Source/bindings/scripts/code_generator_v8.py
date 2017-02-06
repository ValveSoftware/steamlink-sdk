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

# pylint: disable=import-error,print-statement,relative-import

"""Generate Blink V8 bindings (.h and .cpp files).

If run itself, caches Jinja templates (and creates dummy file for build,
since cache filenames are unpredictable and opaque).

This module is *not* concurrency-safe without care: bytecode caching creates
a race condition on cache *write* (crashes if one process tries to read a
partially-written cache). However, if you pre-cache the templates (by running
the module itself), then you can parallelize compiling individual files, since
cache *reading* is safe.

Input: An object of class IdlDefinitions, containing an IDL interface X
Output: V8X.h and V8X.cpp

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import os
import posixpath
import re
import sys

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
module_path, module_filename = os.path.split(os.path.realpath(__file__))
third_party_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, os.pardir, os.pardir))
templates_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, 'templates'))
# Make sure extension is .py, not .pyc or .pyo, so doesn't depend on caching
module_pyname = os.path.splitext(module_filename)[0] + '.py'

# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, third_party_dir)
import jinja2

from idl_definitions import Visitor
import idl_types
from idl_types import IdlType
from v8_attributes import attribute_filters
import v8_callback_interface
import v8_dictionary
from v8_globals import includes, interfaces
import v8_interface
from v8_methods import method_filters
import v8_types
import v8_union
from v8_utilities import capitalize, cpp_name, for_origin_trial_feature, unique_by
from utilities import idl_filename_to_component, is_valid_component_dependency, is_testing_target, shorten_union_name, abs


def normalize_and_sort_includes(include_paths):
    normalized_include_paths = []
    for include_path in include_paths:
        match = re.search(r'/gen/blink/(.*)$', posixpath.abspath(include_path))
        if match:
            include_path = match.group(1)
        normalized_include_paths.append(include_path)
    return sorted(normalized_include_paths)


def render_template(include_paths, header_template, cpp_template,
                    template_context, component=None):
    template_context['code_generator'] = module_pyname

    # Add includes for any dependencies
    template_context['header_includes'] = normalize_and_sort_includes(
        template_context['header_includes'])

    for include_path in include_paths:
        if component:
            dependency = idl_filename_to_component(include_path)
            assert is_valid_component_dependency(component, dependency)
        includes.add(include_path)

    template_context['cpp_includes'] = normalize_and_sort_includes(includes)

    header_text = header_template.render(template_context)
    cpp_text = cpp_template.render(template_context)
    return header_text, cpp_text


def set_global_type_info(info_provider):
    interfaces_info = info_provider.interfaces_info
    idl_types.set_ancestors(interfaces_info['ancestors'])
    IdlType.set_callback_interfaces(interfaces_info['callback_interfaces'])
    IdlType.set_dictionaries(interfaces_info['dictionaries'])
    IdlType.set_enums(info_provider.enumerations)
    IdlType.set_implemented_as_interfaces(interfaces_info['implemented_as_interfaces'])
    IdlType.set_garbage_collected_types(interfaces_info['garbage_collected_interfaces'])
    v8_types.set_component_dirs(interfaces_info['component_dirs'])


def should_generate_code(definitions):
    return definitions.interfaces or definitions.dictionaries


def depending_union_type(idl_type):
    """Returns the union type name if the given idl_type depends on a
    union type.
    """
    def find_base_type(current_type):
        if current_type.is_array_or_sequence_type:
            return find_base_type(current_type.element_type)
        if current_type.is_nullable:
            return find_base_type(current_type.inner_type)
        return current_type
    base_type = find_base_type(idl_type)
    if base_type.is_union_type:
        return base_type
    return None


class TypedefResolver(Visitor):
    def __init__(self, info_provider):
        self.info_provider = info_provider
        self.additional_header_includes = set()
        self.typedefs = {}

    def resolve(self, definitions, definition_name):
        """Traverse definitions and resolves typedefs with the actual types."""
        self.typedefs = {}
        for name, typedef in self.info_provider.typedefs.iteritems():
            self.typedefs[name] = typedef.idl_type
        self.additional_header_includes = set()
        definitions.accept(self)
        self._update_dependencies_include_paths(definition_name)

    def _update_dependencies_include_paths(self, definition_name):
        interface_info = self.info_provider.interfaces_info[definition_name]
        interface_info['additional_header_includes'] = set(
            self.additional_header_includes)

    def _resolve_typedefs(self, typed_object):
        """Resolve typedefs to actual types in the object."""
        for attribute_name in typed_object.idl_type_attributes:
            try:
                idl_type = getattr(typed_object, attribute_name)
            except AttributeError:
                continue
            if not idl_type:
                continue
            resolved_idl_type = idl_type.resolve_typedefs(self.typedefs)
            # TODO(bashi): Dependency resolution shouldn't happen here.
            # Move this into includes_for_type() families.
            union_type = depending_union_type(resolved_idl_type)
            if union_type:
                self.additional_header_includes.add(
                    self.info_provider.include_path_for_union_types(union_type))
            # Need to re-assign the attribute, not just mutate idl_type, since
            # type(idl_type) may change.
            setattr(typed_object, attribute_name, resolved_idl_type)

    def visit_typed_object(self, typed_object):
        self._resolve_typedefs(typed_object)


class CodeGeneratorBase(object):
    """Base class for v8 bindings generator and IDL dictionary impl generator"""

    def __init__(self, info_provider, cache_dir, output_dir):
        self.info_provider = info_provider
        self.jinja_env = initialize_jinja_env(cache_dir)
        self.output_dir = output_dir
        self.typedef_resolver = TypedefResolver(info_provider)
        set_global_type_info(info_provider)

    def generate_code(self, definitions, definition_name):
        """Returns .h/.cpp code as ((path, content)...)."""
        # Set local type info
        if not should_generate_code(definitions):
            return set()

        IdlType.set_callback_functions(definitions.callback_functions.keys())
        # Resolve typedefs
        self.typedef_resolver.resolve(definitions, definition_name)
        return self.generate_code_internal(definitions, definition_name)

    def generate_code_internal(self, definitions, definition_name):
        # This should be implemented in subclasses.
        raise NotImplementedError()


class CodeGeneratorV8(CodeGeneratorBase):
    def __init__(self, info_provider, cache_dir, output_dir):
        CodeGeneratorBase.__init__(self, info_provider, cache_dir, output_dir)

    def output_paths(self, definition_name):
        header_path = posixpath.join(self.output_dir,
                                     'V8%s.h' % definition_name)
        cpp_path = posixpath.join(self.output_dir, 'V8%s.cpp' % definition_name)
        return header_path, cpp_path

    def generate_code_internal(self, definitions, definition_name):
        if definition_name in definitions.interfaces:
            return self.generate_interface_code(
                definitions, definition_name,
                definitions.interfaces[definition_name])
        if definition_name in definitions.dictionaries:
            return self.generate_dictionary_code(
                definitions, definition_name,
                definitions.dictionaries[definition_name])
        raise ValueError('%s is not in IDL definitions' % definition_name)

    def generate_interface_code(self, definitions, interface_name, interface):
        # Store other interfaces for introspection
        interfaces.update(definitions.interfaces)

        interface_info = self.info_provider.interfaces_info[interface_name]
        full_path = interface_info.get('full_path')
        component = idl_filename_to_component(full_path)
        include_paths = interface_info.get('dependencies_include_paths')

        # Select appropriate Jinja template and contents function
        if interface.is_callback:
            header_template_filename = 'callback_interface.h'
            cpp_template_filename = 'callback_interface.cpp'
            interface_context = v8_callback_interface.callback_interface_context
        elif interface.is_partial:
            interface_context = v8_interface.interface_context
            header_template_filename = 'partial_interface.h'
            cpp_template_filename = 'partial_interface.cpp'
            interface_name += 'Partial'
            assert component == 'core'
            component = 'modules'
            include_paths = interface_info.get('dependencies_other_component_include_paths')
        else:
            header_template_filename = 'interface.h'
            cpp_template_filename = 'interface.cpp'
            interface_context = v8_interface.interface_context

        template_context = interface_context(interface)
        includes.update(interface_info.get('cpp_includes', {}).get(component, set()))
        if not interface.is_partial and not is_testing_target(full_path):
            template_context['header_includes'].add(self.info_provider.include_path_for_export)
            template_context['exported'] = self.info_provider.specifier_for_export
        # Add the include for interface itself
        if IdlType(interface_name).is_typed_array:
            template_context['header_includes'].add('core/dom/DOMTypedArray.h')
        elif interface_info['include_path']:
            template_context['header_includes'].add(interface_info['include_path'])
        template_context['header_includes'].update(
            interface_info.get('additional_header_includes', []))
        header_template = self.jinja_env.get_template(header_template_filename)
        cpp_template = self.jinja_env.get_template(cpp_template_filename)
        header_text, cpp_text = render_template(
            include_paths, header_template, cpp_template, template_context,
            component)
        header_path, cpp_path = self.output_paths(interface_name)
        return (
            (header_path, header_text),
            (cpp_path, cpp_text),
        )

    def generate_dictionary_code(self, definitions, dictionary_name,
                                 dictionary):
        # pylint: disable=unused-argument
        interfaces_info = self.info_provider.interfaces_info
        header_template = self.jinja_env.get_template('dictionary_v8.h')
        cpp_template = self.jinja_env.get_template('dictionary_v8.cpp')
        interface_info = interfaces_info[dictionary_name]
        template_context = v8_dictionary.dictionary_context(
            dictionary, interfaces_info)
        include_paths = interface_info.get('dependencies_include_paths')
        # Add the include for interface itself
        if interface_info['include_path']:
            template_context['header_includes'].add(interface_info['include_path'])
        if not is_testing_target(interface_info.get('full_path')):
            template_context['header_includes'].add(self.info_provider.include_path_for_export)
            template_context['exported'] = self.info_provider.specifier_for_export
        header_text, cpp_text = render_template(
            include_paths, header_template, cpp_template, template_context)
        header_path, cpp_path = self.output_paths(dictionary_name)
        return (
            (header_path, header_text),
            (cpp_path, cpp_text),
        )


class CodeGeneratorDictionaryImpl(CodeGeneratorBase):
    def __init__(self, info_provider, cache_dir, output_dir):
        CodeGeneratorBase.__init__(self, info_provider, cache_dir, output_dir)

    def output_paths(self, definition_name, interface_info):
        output_dir = posixpath.join(self.output_dir,
                                    interface_info['relative_dir'])
        header_path = posixpath.join(output_dir, '%s.h' % definition_name)
        cpp_path = posixpath.join(output_dir, '%s.cpp' % definition_name)
        return header_path, cpp_path

    def generate_code_internal(self, definitions, definition_name):
        if not definition_name in definitions.dictionaries:
            raise ValueError('%s is not an IDL dictionary')
        interfaces_info = self.info_provider.interfaces_info
        dictionary = definitions.dictionaries[definition_name]
        interface_info = interfaces_info[definition_name]
        header_template = self.jinja_env.get_template('dictionary_impl.h')
        cpp_template = self.jinja_env.get_template('dictionary_impl.cpp')
        template_context = v8_dictionary.dictionary_impl_context(
            dictionary, interfaces_info)
        include_paths = interface_info.get('dependencies_include_paths')
        if not is_testing_target(interface_info.get('full_path')):
            template_context['exported'] = self.info_provider.specifier_for_export
            template_context['header_includes'].add(self.info_provider.include_path_for_export)
        template_context['header_includes'].update(
            interface_info.get('additional_header_includes', []))
        header_text, cpp_text = render_template(
            include_paths, header_template, cpp_template, template_context)
        header_path, cpp_path = self.output_paths(
            cpp_name(dictionary), interface_info)
        return (
            (header_path, header_text),
            (cpp_path, cpp_text),
        )


class CodeGeneratorUnionType(object):
    """Generates union type container classes.
    This generator is different from CodeGeneratorV8 and
    CodeGeneratorDictionaryImpl. It assumes that all union types are already
    collected. It doesn't process idl files directly.
    """
    def __init__(self, info_provider, cache_dir, output_dir, target_component):
        self.info_provider = info_provider
        self.jinja_env = initialize_jinja_env(cache_dir)
        self.output_dir = output_dir
        self.target_component = target_component
        set_global_type_info(info_provider)

    def _generate_container_code(self, union_type):
        header_template = self.jinja_env.get_template('union_container.h')
        cpp_template = self.jinja_env.get_template('union_container.cpp')
        template_context = v8_union.container_context(
            union_type, self.info_provider.interfaces_info)
        template_context['header_includes'].append(
            self.info_provider.include_path_for_export)
        template_context['header_includes'] = normalize_and_sort_includes(
            template_context['header_includes'])
        template_context['code_generator'] = module_pyname
        template_context['exported'] = self.info_provider.specifier_for_export
        name = shorten_union_name(union_type)
        template_context['this_include_header_name'] = name
        header_text = header_template.render(template_context)
        cpp_text = cpp_template.render(template_context)
        header_path = posixpath.join(self.output_dir, '%s.h' % name)
        cpp_path = posixpath.join(self.output_dir, '%s.cpp' % name)
        return (
            (header_path, header_text),
            (cpp_path, cpp_text),
        )

    def _get_union_types_for_containers(self):
        union_types = self.info_provider.union_types
        if not union_types:
            return None
        # For container classes we strip nullable wrappers. For example,
        # both (A or B)? and (A? or B) will become AOrB. This should be OK
        # because container classes can handle null and it seems that
        # distinguishing (A or B)? and (A? or B) doesn't make sense.
        container_cpp_types = set()
        union_types_for_containers = set()
        for union_type in union_types:
            cpp_type = union_type.cpp_type
            if cpp_type not in container_cpp_types:
                union_types_for_containers.add(union_type)
                container_cpp_types.add(cpp_type)
        return union_types_for_containers

    def generate_code(self):
        union_types = self._get_union_types_for_containers()
        if not union_types:
            return ()
        outputs = set()
        for union_type in union_types:
            outputs.update(self._generate_container_code(union_type))
        return outputs


def initialize_jinja_env(cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        'blink_capitalize': capitalize,
        'exposed': exposed_if,
        'for_origin_trial_feature': for_origin_trial_feature,
        'runtime_enabled': runtime_enabled_if,
        'unique_by': unique_by,
        })
    jinja_env.filters.update(attribute_filters())
    jinja_env.filters.update(v8_interface.constant_filters())
    jinja_env.filters.update(method_filters())
    return jinja_env


def generate_indented_conditional(code, conditional):
    # Indent if statement to level of original code
    indent = re.match(' *', code).group(0)
    return ('%sif (%s) {\n' % (indent, conditional) +
            '    %s\n' % '\n    '.join(code.splitlines()) +
            '%s}\n' % indent)


# [Exposed]
def exposed_if(code, exposed_test):
    if not exposed_test:
        return code
    return generate_indented_conditional(code, 'executionContext && (%s)' % exposed_test)


# [RuntimeEnabled]
def runtime_enabled_if(code, runtime_enabled_function_name):
    if not runtime_enabled_function_name:
        return code
    return generate_indented_conditional(code, '%s()' % runtime_enabled_function_name)


################################################################################

def main(argv):
    # If file itself executed, cache templates
    try:
        cache_dir = abs(argv[1])
        dummy_filename = argv[2]
    except IndexError:
        print 'Usage: %s CACHE_DIR DUMMY_FILENAME' % argv[0]
        return 1

    # Cache templates
    jinja_env = initialize_jinja_env(cache_dir)
    template_filenames = [filename for filename in os.listdir(templates_dir)
                          # Skip .svn, directories, etc.
                          if filename.endswith(('.cpp', '.h'))]
    for template_filename in template_filenames:
        jinja_env.get_template(template_filename)

    # Create a dummy file as output for the build system,
    # since filenames of individual cache files are unpredictable and opaque
    # (they are hashes of the template path, which varies based on environment)
    with open(dummy_filename, 'w') as dummy_file:
        pass  # |open| creates or touches the file


if __name__ == '__main__':
    sys.exit(main(sys.argv))
