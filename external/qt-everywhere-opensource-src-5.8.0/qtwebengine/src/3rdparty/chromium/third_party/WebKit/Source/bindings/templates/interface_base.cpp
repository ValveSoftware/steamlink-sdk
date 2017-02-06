{% include 'copyright_block.txt' %}
#include "{{v8_class_or_partial}}.h"

{% for filename in cpp_includes if filename != '%s.h' % cpp_class_or_partial %}
#include "{{filename}}"
{% endfor %}

namespace blink {
{% set to_active_scriptwrappable = '%s::toActiveScriptWrappable' % v8_class
                                   if active_scriptwrappable else '0' %}
{% set visit_dom_wrapper = '%s::visitDOMWrapper' % v8_class
                           if has_visit_dom_wrapper else '0' %}
{% set parent_wrapper_type_info = '&V8%s::wrapperTypeInfo' % parent_interface
                                  if parent_interface else '0' %}
{% set wrapper_type_prototype = 'WrapperTypeExceptionPrototype' if is_exception else
                                'WrapperTypeObjectPrototype' %}
{% set dom_template = '%s::domTemplate' % v8_class if not is_array_buffer_or_view else '0' %}

{% set wrapper_type_info_const = '' if has_partial_interface else 'const ' %}
{% if not is_partial %}
// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
{{wrapper_type_info_const}}WrapperTypeInfo {{v8_class}}::wrapperTypeInfo = { gin::kEmbedderBlink, {{dom_template}}, {{v8_class}}::trace, {{v8_class}}::traceWrappers, {{to_active_scriptwrappable}}, {{visit_dom_wrapper}}, {{v8_class}}::preparePrototypeAndInterfaceObject,{% if has_conditional_attributes %} {{v8_class}}::installConditionallyEnabledProperties{% else %} nullptr{% endif %}, "{{interface_name}}", {{parent_wrapper_type_info}}, WrapperTypeInfo::{{wrapper_type_prototype}}, WrapperTypeInfo::{{wrapper_class_id}}, WrapperTypeInfo::{{event_target_inheritance}}, WrapperTypeInfo::{{lifetime}} };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in {{cpp_class}}.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// bindings/core/v8/ScriptWrappable.h.
{% if not is_typed_array_type %}
const WrapperTypeInfo& {{cpp_class}}::s_wrapperTypeInfo = {{v8_class}}::wrapperTypeInfo;
{% endif %}

{% endif %}
{% if not is_array_buffer_or_view %}
namespace {{cpp_class_or_partial}}V8Internal {
{% if has_partial_interface %}
{% for method in methods if method.overloads and method.overloads.has_partial_overloads %}
static void (*{{method.name}}MethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
{% endfor %}
{% endif %}

{# Constants #}
{% from 'constants.cpp' import constant_getter_callback
       with context %}
{% for constant in constants | has_special_getter %}
{{constant_getter_callback(constant)}}
{% endfor %}
{# Attributes #}
{##############################################################################}
{% from 'attributes.cpp' import constructor_getter_callback,
       attribute_getter, attribute_getter_callback,
       attribute_setter, attribute_setter_callback,
       attribute_getter_implemented_in_private_script,
       attribute_setter_implemented_in_private_script
       with context %}
{% for attribute in attributes if attribute.should_be_exposed_to_script %}
{% for world_suffix in attribute.world_suffixes %}
{% if not attribute.has_custom_getter and not attribute.constructor_type %}
{{attribute_getter(attribute, world_suffix)}}
{% endif %}
{% if not attribute.constructor_type %}
{{attribute_getter_callback(attribute, world_suffix)}}
{% elif attribute.needs_constructor_getter_callback %}
{{constructor_getter_callback(attribute, world_suffix)}}
{% endif %}
{% if attribute.has_setter %}
{% if not attribute.has_custom_setter %}
{{attribute_setter(attribute, world_suffix)}}
{% endif %}
{{attribute_setter_callback(attribute, world_suffix)}}
{% endif %}
{% endfor %}
{% endfor %}
{##############################################################################}
{% block security_check_functions %}
{% if has_access_check_callbacks and not is_partial %}
bool securityCheck(v8::Local<v8::Context> accessingContext, v8::Local<v8::Object> accessedObject, v8::Local<v8::Value> data)
{
    {% if interface_name == 'Window' %}
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Object> window = V8Window::findInstanceInPrototypeChain(accessedObject, isolate);
    if (window.IsEmpty())
        return false; // the frame is gone.

    const DOMWindow* targetWindow = V8Window::toImpl(window);
    return BindingSecurity::shouldAllowAccessTo(isolate, toLocalDOMWindow(toDOMWindow(accessingContext)), targetWindow, DoNotReportSecurityError);
    {% else %}{# if interface_name == 'Window' #}
    {# Not 'Window' means it\'s Location. #}
    {{cpp_class}}* impl = {{v8_class}}::toImpl(accessedObject);
    return BindingSecurity::shouldAllowAccessTo(v8::Isolate::GetCurrent(), toLocalDOMWindow(toDOMWindow(accessingContext)), impl, DoNotReportSecurityError);
    {% endif %}{# if interface_name == 'Window' #}
}

{% endif %}
{% endblock %}
{##############################################################################}
{# Methods #}
{% from 'methods.cpp' import generate_method, overload_resolution_method,
       method_callback, origin_safe_method_getter, generate_constructor,
       method_implemented_in_private_script, generate_post_message_impl,
       runtime_determined_length_method, runtime_determined_maxarg_method
       with context %}
{% for method in methods if method.should_be_exposed_to_script %}
{% for world_suffix in method.world_suffixes %}
{% if not method.is_custom and not method.is_post_message and method.visible %}
{{generate_method(method, world_suffix)}}
{% endif %}
{% if method.is_post_message %}
{{generate_post_message_impl()}}
{% endif %}
{% if method.overloads and method.overloads.visible %}
{% if method.overloads.runtime_determined_lengths %}
{{runtime_determined_length_method(method.overloads)}}
{% endif %}
{% if method.overloads.runtime_determined_maxargs %}
{{runtime_determined_maxarg_method(method.overloads)}}
{% endif %}
{{overload_resolution_method(method.overloads, world_suffix)}}
{% endif %}
{% if not method.overload_index or method.overloads %}
{# Document about the following condition: #}
{# https://docs.google.com/document/d/1qBC7Therp437Jbt_QYAtNYMZs6zQ_7_tnMkNUG_ACqs/edit?usp=sharing #}
{% if (method.overloads and method.overloads.visible and
        (not method.overloads.has_partial_overloads or not is_partial)) or
      (not method.overloads and method.visible) %}
{# A single callback is generated for overloaded methods #}
{# with considering partial overloads #}
{{method_callback(method, world_suffix)}}
{% endif %}
{% endif %}
{% if method.is_do_not_check_security and method.visible %}
{{origin_safe_method_getter(method, world_suffix)}}
{% endif %}
{% endfor %}
{% endfor %}
{% if iterator_method %}
{{generate_method(iterator_method)}}
{{method_callback(iterator_method)}}
{% endif %}
{% block origin_safe_method_setter %}{% endblock %}
{# Constructors #}
{% for constructor in constructors %}
{{generate_constructor(constructor)}}
{% endfor %}
{% block overloaded_constructor %}{% endblock %}
{% block event_constructor %}{% endblock %}
{# Special operations (methods) #}
{% block indexed_property_getter %}{% endblock %}
{% block indexed_property_getter_callback %}{% endblock %}
{% block indexed_property_setter %}{% endblock %}
{% block indexed_property_setter_callback %}{% endblock %}
{% block indexed_property_deleter %}{% endblock %}
{% block indexed_property_deleter_callback %}{% endblock %}
{% block named_property_getter %}{% endblock %}
{% block named_property_getter_callback %}{% endblock %}
{% block named_property_setter %}{% endblock %}
{% block named_property_setter_callback %}{% endblock %}
{% block named_property_query %}{% endblock %}
{% block named_property_query_callback %}{% endblock %}
{% block named_property_deleter %}{% endblock %}
{% block named_property_deleter_callback %}{% endblock %}
{% block named_property_enumerator %}{% endblock %}
{% block named_property_enumerator_callback %}{% endblock %}
} // namespace {{cpp_class_or_partial}}V8Internal

{% block visit_dom_wrapper %}{% endblock %}
{##############################################################################}
{% block install_attributes %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% if attributes | has_attribute_configuration %}
// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const V8DOMConfiguration::AttributeConfiguration {{v8_class}}Attributes[] = {
    {% for attribute in attributes | has_attribute_configuration %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

{% endif %}
{% endblock %}
{##############################################################################}
{% block install_accessors %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% if attributes | has_accessor_configuration %}
const V8DOMConfiguration::AccessorConfiguration {{v8_class}}Accessors[] = {
    {% for attribute in attributes | has_accessor_configuration %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};

{% endif %}
{% endblock %}
{##############################################################################}
{% block install_methods %}
{% from 'methods.cpp' import method_configuration with context %}
{% if methods | has_method_configuration(is_partial) %}
const V8DOMConfiguration::MethodConfiguration {{v8_class}}Methods[] = {
    {% for method in methods | has_method_configuration(is_partial) %}
    {{method_configuration(method)}},
    {% endfor %}
};

{% endif %}
{% endblock %}
{% endif %}{# not is_array_buffer_or_view #}
{##############################################################################}
{% block named_constructor %}{% endblock %}
{% block constructor_callback %}{% endblock %}
{##############################################################################}
{% block install_dom_template %}
{% if not is_array_buffer_or_view %}
{% from 'methods.cpp' import install_custom_signature with context %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% from 'constants.cpp' import install_constants with context %}
{% from 'methods.cpp' import method_configuration with context %}
{% if has_partial_interface or is_partial %}
void {{v8_class_or_partial}}::install{{v8_class}}Template(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate)
{% else %}
static void install{{v8_class}}Template(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate)
{% endif %}
{
    {% set newline = '' %}
    // Initialize the interface object's template.
    {% if is_partial %}
    {{v8_class}}::install{{v8_class}}Template(isolate, world, interfaceTemplate);
    {% else %}
    {% set parent_interface_template =
           '%s::domTemplateForNamedPropertiesObject(isolate, world)' % v8_class
           if has_named_properties_object else
           'V8%s::domTemplate(isolate, world)' % parent_interface
           if parent_interface else
           'v8::Local<v8::FunctionTemplate>()' %}
    V8DOMConfiguration::initializeDOMInterfaceTemplate(isolate, interfaceTemplate, {{v8_class}}::wrapperTypeInfo.interfaceName, {{parent_interface_template}}, {{v8_class}}::internalFieldCount);
    {% if constructors or has_custom_constructor or has_event_constructor %}
    interfaceTemplate->SetCallHandler({{v8_class}}::constructorCallback);
    interfaceTemplate->SetLength({{interface_length}});
    {% endif %}
    {% endif %}{# is_partial #}
    v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
    ALLOW_UNUSED_LOCAL(signature);
    v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
    ALLOW_UNUSED_LOCAL(instanceTemplate);
    v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
    ALLOW_UNUSED_LOCAL(prototypeTemplate);

    {%- if interface_name == 'Window' and not is_partial %}{{newline}}
    prototypeTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    {% endif %}

    // Register DOM constants, attributes and operations.
    {% filter runtime_enabled(runtime_enabled_function) %}
    {% if constants %}
    {{install_constants() | indent}}
    {% endif %}
    {% if attributes | has_attribute_configuration %}
    V8DOMConfiguration::installAttributes(isolate, world, instanceTemplate, prototypeTemplate, {{'%sAttributes' % v8_class}}, {{'WTF_ARRAY_LENGTH(%sAttributes)' % v8_class}});
    {% endif %}
    {% if attributes | has_accessor_configuration %}
    V8DOMConfiguration::installAccessors(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, {{'%sAccessors' % v8_class}}, {{'WTF_ARRAY_LENGTH(%sAccessors)' % v8_class}});
    {% endif %}
    {% if methods | has_method_configuration(is_partial) %}
    V8DOMConfiguration::installMethods(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, {{'%sMethods' % v8_class}}, {{'WTF_ARRAY_LENGTH(%sMethods)' % v8_class}});
    {% endif %}
    {% endfilter %}
    {%- if has_access_check_callbacks and not is_partial %}{{newline}}
    // Cross-origin access check
    instanceTemplate->SetAccessCheckCallback({{cpp_class}}V8Internal::securityCheck, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&{{v8_class}}::wrapperTypeInfo)));
    {% endif %}

    {%- if has_array_iterator and not is_partial and not is_global %}{{newline}}
    // Array iterator
    prototypeTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);
    {% endif %}

    {%- for group in attributes | runtime_enabled_attributes | groupby('runtime_feature_name') %}{{newline}}
    if ({{group.list[0].runtime_enabled_function}}()) {
        {% for attribute in group.list | unique_by('name') | sort %}
        {% if attribute.is_data_type_property %}
        const V8DOMConfiguration::AttributeConfiguration attribute{{attribute.name}}Configuration = \
        {{attribute_configuration(attribute)}};
        V8DOMConfiguration::installAttribute(isolate, world, instanceTemplate, prototypeTemplate, attribute{{attribute.name}}Configuration);
        {% else %}
        const V8DOMConfiguration::AccessorConfiguration accessor{{attribute.name}}Configuration = \
        {{attribute_configuration(attribute)}};
        V8DOMConfiguration::installAccessor(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, accessor{{attribute.name}}Configuration);
        {% endif %}
        {% endfor %}
    }
    {% endfor %}

    {%- if indexed_property_getter and not is_partial %}{{newline}}
    // Indexed properties
    {{install_indexed_property_handler('instanceTemplate') | indent}}
    {% endif %}
    {% if named_property_getter and not is_partial and not has_named_properties_object %}
    // Named properties
    {{install_named_property_handler('instanceTemplate') | indent}}
    {% endif %}

    {%- if iterator_method %}{{newline}}
    {% filter exposed(iterator_method.exposed_test) %}
    {% filter runtime_enabled(iterator_method.runtime_enabled_function) %}
    // Iterator (@@iterator)
    const V8DOMConfiguration::SymbolKeyedMethodConfiguration symbolKeyedIteratorConfiguration = { v8::Symbol::GetIterator, {{cpp_class_or_partial}}V8Internal::iteratorMethodCallback, 0, v8::DontDelete, V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnPrototype };
    V8DOMConfiguration::installMethod(isolate, world, prototypeTemplate, signature, symbolKeyedIteratorConfiguration);
    {% endfilter %}
    {% endfilter %}
    {% endif %}

    {%- if has_custom_legacy_call_as_function and not is_partial %}{{newline}}
    instanceTemplate->SetCallAsFunctionHandler({{v8_class}}::legacyCallCustom);
    {% endif %}

    {%- if interface_name == 'HTMLAllCollection' and not is_partial %}{{newline}}
    // Needed for legacy support of document.all
    instanceTemplate->MarkAsUndetectable();
    {% endif %}

    {%- if methods | custom_registration(is_partial) %}{{newline}}
    {% for method in methods | custom_registration(is_partial) %}
    {# install_custom_signature #}
    {% filter exposed(method.overloads.exposed_test_all
                      if method.overloads else
                      method.exposed_test) %}
    {% filter runtime_enabled(method.overloads.runtime_enabled_function_all
                              if method.overloads else
                              method.runtime_enabled_function) %}
    {% if method.is_do_not_check_security %}
    {{install_do_not_check_security_method(method, '', 'instanceTemplate', 'prototypeTemplate') | indent}}
    {% else %}
    {{install_custom_signature(method, 'instanceTemplate', 'prototypeTemplate', 'interfaceTemplate', 'signature') | indent}}
    {% endif %}
    {% endfilter %}
    {% endfilter %}
    {% endfor %}
    {% endif %}
}

{% endif %}{# not is_array_buffer_or_view #}
{% endblock %}
{##############################################################################}
{% block origin_trials %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% from 'constants.cpp' import constant_configuration with context %}
{% from 'methods.cpp' import method_configuration with context %}
{% for origin_trial_feature_name in origin_trial_feature_names %}{{newline}}
void {{v8_class_or_partial}}::install{{origin_trial_feature_name}}(ScriptState* scriptState, v8::Local<v8::Object> instance)
{
    v8::Local<v8::Object> prototype = instance->GetPrototype()->ToObject(scriptState->isolate());

    {# Origin-Trial-enabled attributes #}
    {% if attributes | for_origin_trial_feature(origin_trial_feature_name) or
          methods | method_for_origin_trial_feature(origin_trial_feature_name, is_partial) %}
    V8PerIsolateData* perIsolateData = V8PerIsolateData::from(scriptState->isolate());
    v8::Local<v8::FunctionTemplate> interfaceTemplate = perIsolateData->findInterfaceTemplate(scriptState->world(), &{{v8_class}}::wrapperTypeInfo);
    v8::Local<v8::Signature> signature = v8::Signature::New(scriptState->isolate(), interfaceTemplate);
    ALLOW_UNUSED_LOCAL(signature);
    {% endif %}
    {% if constants | for_origin_trial_feature(origin_trial_feature_name) or
          methods | method_for_origin_trial_feature(origin_trial_feature_name, is_partial) %}
    V8PerContextData* perContextData = V8PerContextData::from(scriptState->context());
    v8::Local<v8::Function> interface = perContextData->constructorForType(&{{v8_class}}::wrapperTypeInfo);
    {% endif %}
    {% for attribute in attributes | for_origin_trial_feature(origin_trial_feature_name) | unique_by('name') | sort %}
    {% if attribute.is_data_type_property %}
    const V8DOMConfiguration::AttributeConfiguration attribute{{attribute.name}}Configuration = \
        {{attribute_configuration(attribute)}};
    V8DOMConfiguration::installAttribute(scriptState->isolate(), scriptState->world(), instance, prototype, attribute{{attribute.name}}Configuration);
    {% else %}
    const V8DOMConfiguration::AccessorConfiguration accessor{{attribute.name}}Configuration = \
        {{attribute_configuration(attribute)}};
    V8DOMConfiguration::installAccessor(scriptState->isolate(), scriptState->world(), instance, prototype, v8::Local<v8::Function>(), signature, accessor{{attribute.name}}Configuration);
    {% endif %}
    {% endfor %}
    {# Origin-Trial-enabled constants #}
    {% for constant in constants | for_origin_trial_feature(origin_trial_feature_name) | unique_by('name') | sort %}
    {% set constant_name = constant.name.title().replace('_', '') %}
    const V8DOMConfiguration::ConstantConfiguration constant{{constant_name}}Configuration = {{constant_configuration(constant)}};
    V8DOMConfiguration::installConstant(scriptState->isolate(), interface, prototype, constant{{constant_name}}Configuration);
    {% endfor %}
    {# Origin-Trial-enabled methods (no overloads) #}
    {% for method in methods | method_for_origin_trial_feature(origin_trial_feature_name, is_partial) | unique_by('name') | sort %}
    {% set method_name = method.name.title().replace('_', '') %}
    const V8DOMConfiguration::MethodConfiguration method{{method_name}}Configuration = {{method_configuration(method)}};
    V8DOMConfiguration::installMethod(scriptState->isolate(), scriptState->world(), instance, prototype, interface, signature, method{{method_name}}Configuration);
    {% endfor %}
}
{% endfor %}
{% endblock %}
{##############################################################################}
{% block get_dom_template %}{% endblock %}
{% block get_dom_template_for_named_properties_object %}{% endblock %}
{% block has_instance %}{% endblock %}
{% block to_impl %}{% endblock %}
{% block to_impl_with_type_check %}{% endblock %}
{% block install_conditional_attributes %}{% endblock %}
{##############################################################################}
{% block prepare_prototype_and_interface_object %}{% endblock %}
{##############################################################################}
{% block to_active_scriptwrappable %}{% endblock %}
{% for method in methods if method.is_implemented_in_private_script and method.visible %}
{{method_implemented_in_private_script(method)}}
{% endfor %}
{% for attribute in attributes if attribute.is_implemented_in_private_script %}
{{attribute_getter_implemented_in_private_script(attribute)}}
{% if attribute.has_setter %}
{{attribute_setter_implemented_in_private_script(attribute)}}
{% endif %}
{% endfor %}
{% block partial_interface %}{% endblock %}
} // namespace blink
