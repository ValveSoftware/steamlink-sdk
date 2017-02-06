{% include 'copyright_block.txt' %}
#ifndef {{v8_class_or_partial}}_h
#define {{v8_class_or_partial}}_h

{% for filename in header_includes %}
#include "{{filename}}"
{% endfor %}

namespace blink {
{% if attributes|origin_trial_enabled_attributes %}
class ScriptState;
{% endif %}

class {{v8_class_or_partial}} {
    STATIC_ONLY({{v8_class_or_partial}});
public:
    static void initialize();
    {% for method in methods if method.is_custom %}
    static void {{method.name}}MethodCustom(const v8::FunctionCallbackInfo<v8::Value>&);
    {% endfor %}
    {% for attribute in attributes %}
    {% if attribute.has_custom_getter %}{# FIXME: and not attribute.implemented_by #}
    static void {{attribute.name}}AttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>&);
    {% endif %}
    {% if attribute.has_custom_setter %}{# FIXME: and not attribute.implemented_by #}
    static void {{attribute.name}}AttributeSetterCustom(v8::Local<v8::Value>, const v8::PropertyCallbackInfo<void>&);
    {% endif %}
    {% endfor %}
    {# Custom internal fields #}
    static void preparePrototypeAndInterfaceObject(v8::Local<v8::Context>, const DOMWrapperWorld&, v8::Local<v8::Object>, v8::Local<v8::Function>, v8::Local<v8::FunctionTemplate>);
    {% for origin_trial_feature_name in origin_trial_feature_names %}{{newline}}
    static void install{{origin_trial_feature_name}}(ScriptState*, v8::Local<v8::Object> instance);
    {% endfor %}
private:
    static void install{{v8_class}}Template(v8::Isolate*, const DOMWrapperWorld&, v8::Local<v8::FunctionTemplate> interfaceTemplate);
};
}
#endif // {{v8_class_or_partial}}_h
