#!/usr/bin/env python
# Copyright (c) 2013 Google Inc. All rights reserved.
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

import optparse
import re
import string
import sys

template_h = string.Template("""// Code generated from InspectorInstrumentation.idl

#ifndef ${file_name}_h
#define ${file_name}_h

${includes}

namespace blink {

${forward_declarations}

namespace InspectorInstrumentation {

$methods
} // namespace InspectorInstrumentation

} // namespace blink

#endif // !defined(${file_name}_h)
""")

template_cpp = string.Template("""// Code generated from InspectorInstrumentation.idl

${includes}

namespace blink {
${extra_definitions}

namespace InspectorInstrumentation {
$methods

} // namespace InspectorInstrumentation

} // namespace blink
""")

template_impl = string.Template("""
${return_type} ${name}(${params})
{
    InstrumentingAgents* agents = instrumentingAgentsFor(${first_param_name});
    if (!agents)
        ${default_return}${impl_lines}${maybe_default_return}
}""")

template_agent_call = string.Template("""
    if (agents->has${agent_class}s()) {
        for (${agent_class}* agent : agents->${agent_getter}s())
            ${maybe_return}agent->${name}(${params_agent});
    }""")

template_instrumenting_agents_h = string.Template("""// Code generated from InspectorInstrumentation.idl

#ifndef InstrumentingAgents_h
#define InstrumentingAgents_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"

namespace blink {

${forward_list}

class CORE_EXPORT InstrumentingAgents : public GarbageCollected<InstrumentingAgents> {
    WTF_MAKE_NONCOPYABLE(InstrumentingAgents);
public:
    InstrumentingAgents();
    DECLARE_TRACE();
${accessor_list}

private:
${member_list}
};

}

#endif // !defined(InstrumentingAgents_h)
""")

template_instrumenting_agent_accessor = string.Template("""
    bool has${class_name}s() const { return ${has_member_name}; }
    const HeapHashSet<Member<${class_name}>>& ${getter_name}s() const { return ${member_name}; }
    void add${class_name}(${class_name}* agent);
    void remove${class_name}(${class_name}* agent);""")

template_instrumenting_agent_impl = string.Template("""
void InstrumentingAgents::add${class_name}(${class_name}* agent)
{
    ${member_name}.add(agent);
    ${has_member_name} = true;
}

void InstrumentingAgents::remove${class_name}(${class_name}* agent)
{
    ${member_name}.remove(agent);
    ${has_member_name} = !${member_name}.isEmpty();
}
""")

template_instrumenting_agents_cpp = string.Template("""
InstrumentingAgents::InstrumentingAgents()
    : $init_list
{
}
${impl_list}
DEFINE_TRACE(InstrumentingAgents)
{
    $trace_list
}""")



def match_and_consume(pattern, source):
    match = re.match(pattern, source)
    if match:
        return match, source[len(match.group(0)):].strip()
    return None, source


def load_model_from_idl(source):
    source = re.sub("//.*", "", source)  # Remove line comments
    source = re.sub("/\*(.|\n)*?\*/", "", source, re.MULTILINE)  # Remove block comments
    source = re.sub("\]\s*?\n\s*", "] ", source)  # Merge the method annotation with the next line
    source = source.strip()

    model = []

    while len(source):
        match, source = match_and_consume("interface\s(\w*)\s?\{([^\{]*)\}", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source[:100])
            sys.exit(1)
        model.append(File(match.group(1), match.group(2)))

    return model


class File:
    def __init__(self, name, source):
        self.name = name
        self.header_name = self.name + "Inl"
        self.includes = [include_inspector_header("InspectorInstrumentation")]
        self.forward_declarations = []
        self.declarations = []
        for line in map(str.strip, source.split("\n")):
            line = re.sub("\s{2,}", " ", line).strip()  # Collapse whitespace
            if len(line) == 0:
                continue
            if line[0] == "#":
                self.includes.append(line)
            elif line.startswith("class "):
                self.forward_declarations.append(line)
            else:
                self.declarations.append(Method(line))
        self.includes.sort()
        self.forward_declarations.sort()

    def generate(self, cpp_lines, used_agents):
        header_lines = []
        for declaration in self.declarations:
            for agent in set(declaration.agents):
                used_agents.add(agent)
            declaration.generate_header(header_lines)
            declaration.generate_cpp(cpp_lines)

        return template_h.substitute(None,
                                     file_name=self.header_name,
                                     includes="\n".join(self.includes),
                                     forward_declarations="\n".join(self.forward_declarations),
                                     methods="\n".join(header_lines))


class Method:
    def __init__(self, source):
        match = re.match("(\[[\w|,|=|\s]*\])?\s?(\w*\*?) (\w*)\((.*)\)\s?;", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source)
            sys.exit(1)

        self.options = []
        if match.group(1):
            options_str = re.sub("\s", "", match.group(1)[1:-1])
            if len(options_str) != 0:
                self.options = options_str.split(",")

        self.return_type = match.group(2)

        self.name = match.group(3)

        # Splitting parameters by a comma, assuming that attribute lists contain no more than one attribute.
        self.params = map(Parameter, map(str.strip, match.group(4).split(",")))

        self.returns_value = self.return_type != "void"
        if self.return_type == "bool":
            self.default_return_value = "false"
        elif self.returns_value:
            sys.stderr.write("Can only return bool: %s\n" % self.name)
            sys.exit(1)

        self.params_agent = self.params
        if "Keep" not in self.params_agent[0].options:
            self.params_agent = self.params_agent[1:]

        self.agents = filter(lambda option: not "=" in option, self.options)

        if self.returns_value and len(self.agents) > 1:
            sys.stderr.write("Can only return value from a single agent: %s\n" % self.name)
            sys.exit(1)

    def generate_header(self, header_lines):
        header_lines.append("CORE_EXPORT %s %s(%s);" % (
            self.return_type, self.name, ", ".join(map(Parameter.to_str_class, self.params))))

    def generate_cpp(self, cpp_lines):
        default_return = "return;"
        maybe_default_return = ""
        if self.returns_value:
            default_return = "return %s;" % self.default_return_value
            maybe_default_return = "\n    " + default_return

        body_lines = map(self.generate_ref_ptr, self.params)
        body_lines += map(self.generate_agent_call, self.agents)

        cpp_lines.append(template_impl.substitute(
            None,
            name=self.name,
            return_type=self.return_type,
            params=", ".join(map(Parameter.to_str_class_and_name, self.params)),
            default_return=default_return,
            maybe_default_return=maybe_default_return,
            impl_lines="".join(body_lines),
            first_param_name=self.params[0].name))

    def generate_agent_call(self, agent):
        agent_class, agent_getter = agent_getter_signature(agent)

        maybe_return = ""
        if self.returns_value:
            maybe_return = "return "

        return template_agent_call.substitute(
            None,
            name=self.name,
            agent_class=agent_class,
            agent_getter=agent_getter,
            maybe_return=maybe_return,
            params_agent=", ".join(map(Parameter.to_str_value, self.params_agent)))

    def generate_ref_ptr(self, param):
        if param.is_prp:
            return "\n        RefPtr<%s> %s = %s;" % (param.inner_type, param.value, param.name)
        else:
            return ""

class Parameter:
    def __init__(self, source):
        self.options = []
        match, source = match_and_consume("\[(\w*)\]", source)
        if match:
            self.options.append(match.group(1))

        parts = map(str.strip, source.split("="))
        if len(parts) == 1:
            self.default_value = None
        else:
            self.default_value = parts[1]

        param_decl = parts[0]

        if re.match("(const|unsigned long) ", param_decl):
            min_type_tokens = 2
        else:
            min_type_tokens = 1

        if len(param_decl.split(" ")) > min_type_tokens:
            parts = param_decl.split(" ")
            self.type = " ".join(parts[:-1])
            self.name = parts[-1]
        else:
            self.type = param_decl
            self.name = generate_param_name(self.type)

        if re.match("PassRefPtr<", param_decl):
            self.is_prp = True
            self.value = self.name
            self.name = "prp" + self.name[0].upper() + self.name[1:]
            self.inner_type = re.match("PassRefPtr<(.+)>", param_decl).group(1)
        else:
            self.is_prp = False
            self.value = self.name


    def to_str_full(self):
        if self.default_value is None:
            return self.to_str_class_and_name()
        return "%s %s = %s" % (self.type, self.name, self.default_value)

    def to_str_class_and_name(self):
        return "%s %s" % (self.type, self.name)

    def to_str_class(self):
        return self.type

    def to_str_name(self):
        return self.name

    def to_str_value(self):
        return self.value


def generate_param_name(param_type):
    base_name = re.match("(const |PassRefPtr<)?(\w*)", param_type).group(2)
    return "param" + base_name


def agent_class_name(agent):
    if agent == "V8":
        return "InspectorSession"
    return "Inspector%sAgent" % agent


def agent_getter_signature(agent):
    agent_class = agent_class_name(agent)
    return agent_class, agent_class[0].lower() + agent_class[1:]


def include_header(name):
    return "#include \"%s.h\"" % name


def include_inspector_header(name):
    return include_header("core/inspector/" + name)


def generate_instrumenting_agents(used_agents):
    agents = list(used_agents)
    agents.sort()

    forward_list = []
    accessor_list = []
    member_list = []
    init_list = []
    trace_list = []
    impl_list = []

    for agent in agents:
        class_name, getter_name = agent_getter_signature(agent)
        member_name = "m_" + getter_name + "s"
        has_member_name = "m_has" + class_name + "s"

        forward_list.append("class %s;" % class_name)
        accessor_list.append(template_instrumenting_agent_accessor.substitute(
            None,
            class_name=class_name,
            getter_name=getter_name,
            member_name=member_name,
            has_member_name=has_member_name))
        member_list.append("    HeapHashSet<Member<%s>> %s;" % (class_name, member_name))
        member_list.append("    bool %s;" % has_member_name)
        init_list.append("%s(false)" % has_member_name)
        trace_list.append("visitor->trace(%s);" % member_name)
        impl_list.append(template_instrumenting_agent_impl.substitute(
            None,
            class_name=class_name,
            member_name=member_name,
            has_member_name=has_member_name))

    header_lines = template_instrumenting_agents_h.substitute(
        None,
        forward_list="\n".join(forward_list),
        accessor_list="\n".join(accessor_list),
        member_list="\n".join(member_list))

    cpp_lines = template_instrumenting_agents_cpp.substitute(
        None,
        init_list="\n    , ".join(init_list),
        impl_list="".join(impl_list),
        trace_list="\n    ".join(trace_list))

    return header_lines, cpp_lines


def generate(input_path, output_dir):
    fin = open(input_path, "r")
    files = load_model_from_idl(fin.read())
    fin.close()

    cpp_includes = []
    cpp_lines = []
    used_agents = set()
    for f in files:
        cpp_includes.append(include_header(f.header_name))

        fout = open(output_dir + "/" + f.header_name + ".h", "w")
        fout.write(f.generate(cpp_lines, used_agents))
        fout.close()

    for agent in used_agents:
        cpp_includes.append(include_inspector_header(agent_class_name(agent)))
    cpp_includes.append(include_header("InstrumentingAgents"))
    cpp_includes.append(include_header("core/CoreExport"))
    cpp_includes.sort()

    instrumenting_agents_header, instrumenting_agents_cpp = generate_instrumenting_agents(used_agents)

    fout = open(output_dir + "/" + "InstrumentingAgents.h", "w")
    fout.write(instrumenting_agents_header)
    fout.close()

    fout = open(output_dir + "/InspectorInstrumentationImpl.cpp", "w")
    fout.write(template_cpp.substitute(None,
                                       includes="\n".join(cpp_includes),
                                       extra_definitions=instrumenting_agents_cpp,
                                       methods="\n".join(cpp_lines)))
    fout.close()


cmdline_parser = optparse.OptionParser()
cmdline_parser.add_option("--output_dir")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    if (len(arg_values) != 1):
        raise Exception("Exactly one plain argument expected (found %s)" % len(arg_values))
    input_path = arg_values[0]
    output_dirpath = arg_options.output_dir
    if not output_dirpath:
        raise Exception("Output directory must be specified")
except Exception:
    # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
    exc = sys.exc_info()[1]
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
    sys.stderr.write("Usage: <script> --output_dir <output_dir> InspectorInstrumentation.idl\n")
    exit(1)

generate(input_path, output_dirpath)
