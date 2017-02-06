{##############################################################################}
{% macro constant_getter_callback(constant) %}
static void {{constant.name}}ConstantGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% if constant.deprecate_as %}
    Deprecation::countDeprecationIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), UseCounter::{{constant.deprecate_as}});
    {% endif %}
    {% if constant.measure_as %}
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), UseCounter::{{constant.measure_as('ConstantGetter')}});
    {% endif %}
    {% if constant.idl_type in ('Double', 'Float') %}
    v8SetReturnValue(info, {{constant.value}});
    {% elif constant.idl_type == 'String' %}
    v8SetReturnValueString(info, "{{constant.value}}");
    {% else %}
    v8SetReturnValueInt(info, {{constant.value}});
    {% endif %}
}
{% endmacro %}


{######################################}
{% macro install_constants() %}
{% if constants | has_constant_configuration %}
{# Normal constants #}
const V8DOMConfiguration::ConstantConfiguration {{v8_class}}Constants[] = {
    {% for constant in constants | has_constant_configuration %}
    {{constant_configuration(constant)}},
    {% endfor %}
};
V8DOMConfiguration::installConstants(isolate, interfaceTemplate, prototypeTemplate, {{v8_class}}Constants, WTF_ARRAY_LENGTH({{v8_class}}Constants));
{% endif %}
{# Runtime-enabled constants #}
{% for group in constants | runtime_enabled_constants | groupby('runtime_feature_name') %}
{% filter runtime_enabled(group.list[0].runtime_enabled_function) %}
{% for constant in group.list %}
{% set constant_name = constant.name.title().replace('_', '') %}
const V8DOMConfiguration::ConstantConfiguration constant{{constant_name}}Configuration = {{constant_configuration(constant)}};
V8DOMConfiguration::installConstant(isolate, interfaceTemplate, prototypeTemplate, constant{{constant_name}}Configuration);
{% endfor %}
{% endfilter %}
{% endfor %}
{# Constants with [DeprecateAs] or [MeasureAs] #}
{% for constant in constants | has_special_getter %}
V8DOMConfiguration::installConstantWithGetter(isolate, interfaceTemplate, prototypeTemplate, "{{constant.name}}", {{cpp_class}}V8Internal::{{constant.name}}ConstantGetterCallback);
{% endfor %}
{# Check constants #}
{% if not do_not_check_constants %}
{% for constant in constants %}
{% if constant.idl_type not in ('Double', 'Float', 'String') %}
{% set constant_cpp_class = constant.cpp_class or cpp_class %}
static_assert({{constant.value}} == {{constant_cpp_class}}::{{constant.reflected_name}}, "the value of {{cpp_class}}_{{constant.reflected_name}} does not match with implementation");
{% endif %}
{% endfor %}
{% endif %}
{% endmacro %}


{######################################}
{%- macro constant_configuration(constant) %}
{% if constant.idl_type in ('Double', 'Float') %}
    {% set value = '0, %s' % constant.value %}
{% else %}
    {# 'Short', 'Long' etc. #}
    {% set value = '%s, 0' % constant.value %}
{% endif %}
{"{{constant.name}}", {{value}}, V8DOMConfiguration::ConstantType{{constant.idl_type}}}
{%- endmacro %}
