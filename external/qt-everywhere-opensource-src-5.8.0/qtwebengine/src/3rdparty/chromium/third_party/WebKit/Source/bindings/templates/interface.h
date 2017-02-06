{% include 'copyright_block.txt' %}
#ifndef {{v8_class}}_h
#define {{v8_class}}_h

{% for filename in header_includes %}
#include "{{filename}}"
{% endfor %}

namespace blink {

{% if has_event_constructor %}
class Dictionary;
{% endif %}
{% if attributes|origin_trial_enabled_attributes %}
class ScriptState;
{% endif %}
{% if named_constructor %}
class {{v8_class}}Constructor {
    STATIC_ONLY({{v8_class}}Constructor);
public:
    static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*, const DOMWrapperWorld&);
    static const WrapperTypeInfo wrapperTypeInfo;
};

{% endif %}
class {{v8_class}} {
    STATIC_ONLY({{v8_class}});
public:
    {% if has_private_script %}
    class PrivateScript {
        STATIC_ONLY(PrivateScript);
    public:
        {% for method in methods if method.is_implemented_in_private_script %}
        static bool {{method.name}}Method({{method.argument_declarations_for_private_script | join(', ')}});
        {% endfor %}
        {% for attribute in attributes if attribute.is_implemented_in_private_script %}
        static bool {{attribute.name}}AttributeGetter(LocalFrame* frame, {{cpp_class}}* holderImpl, {{attribute.cpp_type}}* result);
        {% if not attribute.is_read_only %}
        static bool {{attribute.name}}AttributeSetter(LocalFrame* frame, {{cpp_class}}* holderImpl, {{attribute.argument_cpp_type}} cppValue);
        {% endif %}
        {% endfor %}
    };

    {% endif %}
    {% if is_array_buffer_or_view %}
    {{exported}}static {{cpp_class}}* toImpl(v8::Local<v8::Object> object);
    {% else %}
    {{exported}}static bool hasInstance(v8::Local<v8::Value>, v8::Isolate*);
    static v8::Local<v8::Object> findInstanceInPrototypeChain(v8::Local<v8::Value>, v8::Isolate*);
    {{exported}}static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*, const DOMWrapperWorld&);
    {% if has_named_properties_object %}
    {{exported}}static v8::Local<v8::FunctionTemplate> domTemplateForNamedPropertiesObject(v8::Isolate*, const DOMWrapperWorld&);
    {% endif %}
    static {{cpp_class}}* toImpl(v8::Local<v8::Object> object)
    {
        return toScriptWrappable(object)->toImpl<{{cpp_class}}>();
    }
    {% endif %}
    {{exported}}static {{cpp_class}}* toImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
    {% if has_partial_interface %}
    {{exported}}static WrapperTypeInfo wrapperTypeInfo;
    {% else %}
    {{exported}}static const WrapperTypeInfo wrapperTypeInfo;
    {% endif %}
    template<typename VisitorDispatcher>
    static void trace(VisitorDispatcher visitor, ScriptWrappable* scriptWrappable)
    {
        visitor->trace(scriptWrappable->toImpl<{{cpp_class}}>());
    }
    static void traceWrappers(WrapperVisitor* visitor, ScriptWrappable* scriptWrappable)
    {
        visitor->traceWrappers(scriptWrappable->toImpl<{{cpp_class}}>());
    }
    {% if has_visit_dom_wrapper %}
    static void visitDOMWrapper(v8::Isolate*, ScriptWrappable*, const v8::Persistent<v8::Object>&);
    {% endif %}
    {% if active_scriptwrappable %}
    static ActiveScriptWrappable* toActiveScriptWrappable(v8::Local<v8::Object>);
    {% endif %}
    {% for method in methods %}
    {% if method.is_custom %}
    static void {{method.name}}MethodCustom(const v8::FunctionCallbackInfo<v8::Value>&);
    {% endif %}
    {% if method.is_custom_call_prologue %}
    static void {{method.name}}MethodPrologueCustom(const v8::FunctionCallbackInfo<v8::Value>&, {{cpp_class}}*);
    {% endif %}
    {% if method.is_custom_call_epilogue %}
    static void {{method.name}}MethodEpilogueCustom(const v8::FunctionCallbackInfo<v8::Value>&, {{cpp_class}}*);
    {% endif %}
    {% endfor %}
    {% if constructors or has_custom_constructor or has_event_constructor %}
    static void constructorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    {% endif %}
    {% if has_custom_constructor %}
    static void constructorCustom(const v8::FunctionCallbackInfo<v8::Value>&);
    {% endif %}
    {% for attribute in attributes %}
    {% if attribute.has_custom_getter %}{# FIXME: and not attribute.implemented_by #}
    {% if attribute.is_data_type_property %}
    static void {{attribute.name}}AttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>&);
    {% else %}
    static void {{attribute.name}}AttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>&);
    {% endif %}
    {% endif %}
    {% if attribute.has_custom_setter %}{# FIXME: and not attribute.implemented_by #}
    {% if attribute.is_data_type_property %}
    static void {{attribute.name}}AttributeSetterCustom(v8::Local<v8::Value>, const v8::PropertyCallbackInfo<void>&);
    {% else %}
    static void {{attribute.name}}AttributeSetterCustom(v8::Local<v8::Value>, const v8::FunctionCallbackInfo<v8::Value>&);
    {% endif %}
    {% endif %}
    {% endfor %}
    {# Custom special operations #}
    {% if indexed_property_getter and indexed_property_getter.is_custom %}
    static void indexedPropertyGetterCustom(uint32_t, const v8::PropertyCallbackInfo<v8::Value>&);
    {% endif %}
    {% if indexed_property_setter and indexed_property_setter.is_custom %}
    static void indexedPropertySetterCustom(uint32_t, v8::Local<v8::Value>, const v8::PropertyCallbackInfo<v8::Value>&);
    {% endif %}
    {% if indexed_property_deleter and indexed_property_deleter.is_custom %}
    static void indexedPropertyDeleterCustom(uint32_t, const v8::PropertyCallbackInfo<v8::Boolean>&);
    {% endif %}
    {% if named_property_getter and named_property_getter.is_custom %}
    static void namedPropertyGetterCustom(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
    {% endif %}
    {% if named_property_setter and named_property_setter.is_custom %}
    static void namedPropertySetterCustom(v8::Local<v8::Name>, v8::Local<v8::Value>, const v8::PropertyCallbackInfo<v8::Value>&);
    {% endif %}
    {% if named_property_getter and
          named_property_getter.is_custom_property_query %}
    static void namedPropertyQueryCustom(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Integer>&);
    {% endif %}
    {% if named_property_deleter and named_property_deleter.is_custom %}
    static void namedPropertyDeleterCustom(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Boolean>&);
    {% endif %}
    {% if named_property_getter and
          named_property_getter.is_custom_property_enumerator %}
    static void namedPropertyEnumeratorCustom(const v8::PropertyCallbackInfo<v8::Array>&);
    {% endif %}
    {# END custom special operations #}
    {% if has_custom_legacy_call_as_function %}
    static void legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>&);
    {% endif %}
    {# Custom internal fields #}
    {% set custom_internal_field_counter = 0 %}
    {% if is_event_target and not is_node %}
    {# Event listeners on DOM nodes are explicitly supported in the GC controller. #}
    static const int eventListenerCacheIndex = v8DefaultWrapperInternalFieldCount + {{custom_internal_field_counter}};
    {% set custom_internal_field_counter = custom_internal_field_counter + 1 %}
    {% endif %}
    {# persistentHandleIndex must be the last field, if it is present.
       Detailed explanation: https://codereview.chromium.org/139173012
       FIXME: Remove this internal field, and share one field for either:
       * a persistent handle (if the object is in oilpan) or
       * a C++ pointer to the DOM object (if the object is not in oilpan) #}
    static const int internalFieldCount = v8DefaultWrapperInternalFieldCount + {{custom_internal_field_counter}};
    {# End custom internal fields #}
    {% if has_conditional_attributes %}
    static void installConditionallyEnabledProperties(v8::Local<v8::Object>, v8::Isolate*);
    {% endif %}
    {{exported}}static void preparePrototypeAndInterfaceObject(v8::Local<v8::Context>, const DOMWrapperWorld&, v8::Local<v8::Object> prototypeObject, v8::Local<v8::Function> interfaceObject, v8::Local<v8::FunctionTemplate> interfaceTemplate){% if unscopeables or has_conditional_attributes_on_prototype or methods | conditionally_exposed(is_partial) %};
    {% else %} { }
    {% endif %}
    {% if has_partial_interface %}
    {{exported}}static void updateWrapperTypeInfo(InstallTemplateFunction, PreparePrototypeAndInterfaceObjectFunction);
    {{exported}}static void install{{v8_class}}Template(v8::Isolate*, const DOMWrapperWorld&, v8::Local<v8::FunctionTemplate> interfaceTemplate);
    {% for method in methods if method.overloads and method.overloads.has_partial_overloads %}
    {{exported}}static void register{{method.name | blink_capitalize}}MethodForPartialInterface(void (*)(const v8::FunctionCallbackInfo<v8::Value>&));
    {% endfor %}
    {% endif %}
    {% for origin_trial_feature_name in origin_trial_feature_names %}{{newline}}
    static void install{{origin_trial_feature_name}}(ScriptState*, v8::Local<v8::Object> instance);
    {% endfor %}
    {% if has_partial_interface %}

private:
    static InstallTemplateFunction install{{v8_class}}TemplateFunction;
    {% endif %}
};

{% if has_event_constructor %}
{{exported}}bool initialize{{cpp_class}}({{cpp_class}}Init&, const Dictionary&, ExceptionState&, const v8::FunctionCallbackInfo<v8::Value>& info);

{% endif %}
template <>
struct V8TypeOf<{{cpp_class}}> {
    typedef {{v8_class}} Type;
};

} // namespace blink

#endif // {{v8_class}}_h
