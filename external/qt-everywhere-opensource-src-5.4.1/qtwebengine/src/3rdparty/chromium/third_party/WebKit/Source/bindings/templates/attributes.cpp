{##############################################################################}
{% macro attribute_getter(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetter{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
const v8::PropertyCallbackInfo<v8::Value>& info
{%- endif %})
{
    {% if attribute.is_reflect and not attribute.is_url and
          attribute.idl_type == 'DOMString' and is_node %}
    {% set cpp_class, v8_class = 'Element', 'V8Element' %}
    {% endif %}
    {# holder #}
    {% if attribute.is_unforgeable and interface_name != 'Window' %}
    {# perform lookup first #}
    {# FIXME: can we remove this lookup? #}
    v8::Handle<v8::Object> holder = {{v8_class}}::findInstanceInPrototypeChain(info.This(), info.GetIsolate());
    if (holder.IsEmpty())
        return;
    {% elif not attribute.is_static %}
    v8::Handle<v8::Object> holder = info.Holder();
    {% endif %}
    {# impl #}
    {% if attribute.cached_attribute_validation_method %}
    v8::Handle<v8::String> propertyName = v8AtomicString(info.GetIsolate(), "{{attribute.name}}");
    {{cpp_class}}* impl = {{v8_class}}::toNative(holder);
    if (!impl->{{attribute.cached_attribute_validation_method}}()) {
        v8::Handle<v8::Value> v8Value = V8HiddenValue::getHiddenValue(info.GetIsolate(), holder, propertyName);
        if (!v8Value.IsEmpty()) {
            v8SetReturnValue(info, v8Value);
            return;
        }
    }
    {% elif not attribute.is_static %}
    {{cpp_class}}* impl = {{v8_class}}::toNative(holder);
    {% endif %}
    {% if interface_name == 'Window' and attribute.idl_type == 'EventHandler' %}
    if (!impl->document())
        return;
    {% endif %}
    {# Local variables #}
    {% if attribute.is_call_with_execution_context %}
    ExecutionContext* executionContext = currentExecutionContext(info.GetIsolate());
    {% endif %}
    {% if attribute.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% if attribute.is_check_security_for_node or
          attribute.is_getter_raises_exception %}
    ExceptionState exceptionState(ExceptionState::GetterContext, "{{attribute.name}}", "{{interface_name}}", holder, info.GetIsolate());
    {% endif %}
    {% if attribute.is_nullable and not attribute.has_type_checking_nullable %}
    bool isNull = false;
    {% endif %}
    {# FIXME: consider always using a local variable for value #}
    {% if attribute.cached_attribute_validation_method or
          attribute.is_getter_raises_exception or
          attribute.is_nullable or
          attribute.reflect_only or
          attribute.idl_type == 'EventHandler' %}
    {{attribute.cpp_type}} {{attribute.cpp_value}} = {{attribute.cpp_value_original}};
    {% endif %}
    {# Checks #}
    {% if attribute.is_getter_raises_exception %}
    if (UNLIKELY(exceptionState.throwIfNeeded()))
        return;
    {% endif %}
    {% if attribute.is_check_security_for_node %}
    {# FIXME: use a local variable to not call getter twice #}
    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), {{attribute.cpp_value}}, exceptionState)) {
        v8SetReturnValueNull(info);
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if attribute.reflect_only %}
    {{release_only_check(attribute.reflect_only, attribute.reflect_missing,
                         attribute.reflect_invalid, attribute.reflect_empty)
      | indent}}
    {% endif %}
    {% if attribute.is_nullable %}
    {% if attribute.has_type_checking_nullable %}
    if (!{{attribute.cpp_value}}) {
    {% else %}
    if (isNull) {
    {% endif %}
        v8SetReturnValueNull(info);
        return;
    }
    {% endif %}
    {% if attribute.cached_attribute_validation_method %}
    V8HiddenValue::setHiddenValue(info.GetIsolate(), holder, propertyName, {{attribute.cpp_value_to_v8_value}});
    {% endif %}
    {# v8SetReturnValue #}
    {% if attribute.is_keep_alive_for_gc %}
    {# FIXME: merge local variable assignment with above #}
    {{attribute.cpp_type}} result({{attribute.cpp_value}});
    if (result && DOMDataStore::setReturnValueFromWrapper{{world_suffix}}<{{attribute.v8_type}}>(info.GetReturnValue(), result.get()))
        return;
    v8::Handle<v8::Value> wrapper = toV8(result.get(), holder, info.GetIsolate());
    if (!wrapper.IsEmpty()) {
        V8HiddenValue::setHiddenValue(info.GetIsolate(), holder, v8AtomicString(info.GetIsolate(), "{{attribute.name}}"), wrapper);
        {{attribute.v8_set_return_value}};
    }
    {% elif world_suffix %}
    {{attribute.v8_set_return_value_for_main_world}};
    {% else %}
    {{attribute.v8_set_return_value}};
    {% endif %}
}
{% endfilter %}
{% endmacro %}

{######################################}
{% macro release_only_check(reflect_only_values, reflect_missing,
                            reflect_invalid, reflect_empty) %}
{# Attribute is limited to only known values: check that the attribute value is
   one of those. If not, set it to the empty string.
   http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values #}
{% if reflect_empty %}
if (v8Value.isNull()) {
{% if reflect_missing %}
    v8Value = "{{reflect_missing}}";
{% else %}
    ;
{% endif %}
} else if (v8Value.isEmpty()) {
    v8Value = "{{reflect_empty}}";
{% else %}
if (v8Value.isEmpty()) {
{# FIXME: should use [ReflectEmpty] instead; need to change IDL files #}
{% if reflect_missing %}
    v8Value = "{{reflect_missing}}";
{% else %}
    ;
{% endif %}
{% endif %}
{% for value in reflect_only_values %}
} else if (equalIgnoringCase(v8Value, "{{value}}")) {
    v8Value = "{{value}}";
{% endfor %}
} else {
    v8Value = "{{reflect_invalid}}";
}
{% endmacro %}


{##############################################################################}
{% macro attribute_getter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetterCallback{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info
{%- endif %})
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {% if world_suffix in attribute.activity_logging_world_list_for_getter %}
    ScriptState* scriptState = ScriptState::from(info.GetIsolate()->GetCurrentContext());
    V8PerContextData* contextData = scriptState->perContextData();
    {% if attribute.activity_logging_world_check %}
    if (scriptState->world().isIsolatedWorld() && contextData && contextData->activityLogger())
    {% else %}
    if (contextData && contextData->activityLogger())
    {% endif %}
        contextData->activityLogger()->logGetter("{{interface_name}}.{{attribute.name}}");
    {% endif %}
    {% if attribute.has_custom_getter %}
    {{v8_class}}::{{attribute.name}}AttributeGetterCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::{{attribute.name}}AttributeGetter{{world_suffix}}(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro constructor_getter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}ConstructorGetterCallback{{world_suffix}}(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {{cpp_class}}V8Internal::{{cpp_class}}ConstructorGetter{{world_suffix}}(property, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_setter(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeSetter{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info
{%- endif %})
{
    {% if attribute.is_reflect and attribute.idl_type == 'DOMString' and
          is_node %}
    {% set cpp_class, v8_class = 'Element', 'V8Element' %}
    {% endif %}
    {# Local variables #}
    {% if not attribute.is_static %}
    v8::Handle<v8::Object> holder = info.Holder();
    {% endif %}
    {% if attribute.has_setter_exception_state %}
    ExceptionState exceptionState(ExceptionState::SetterContext, "{{attribute.name}}", "{{interface_name}}", holder, info.GetIsolate());
    {% endif %}
    {# Type checking #}
    {% if attribute.has_type_checking_interface %}
    {# Type checking for interface types (if interface not implemented, throw
       TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
    if ({% if attribute.is_nullable %}!isUndefinedOrNull(v8Value) && {% endif %}!V8{{attribute.idl_type}}::hasInstance(v8Value, info.GetIsolate())) {
        exceptionState.throwTypeError("The provided value is not of type '{{attribute.idl_type}}'.");
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {# impl #}
    {% if attribute.put_forwards %}
    {{cpp_class}}* proxyImpl = {{v8_class}}::toNative(holder);
    {{attribute.cpp_type}} impl = WTF::getPtr(proxyImpl->{{attribute.name}}());
    if (!impl)
        return;
    {% elif not attribute.is_static %}
    {{cpp_class}}* impl = {{v8_class}}::toNative(holder);
    {% endif %}
    {% if attribute.idl_type == 'EventHandler' and interface_name == 'Window' %}
    if (!impl->document())
        return;
    {% endif %}
    {# Convert JS value to C++ value #}
    {% if attribute.idl_type != 'EventHandler' %}
    {{attribute.v8_value_to_local_cpp_value}};
    {% elif not is_node %}{# EventHandler hack #}
    moveEventListenerToNewWrapper(holder, {{attribute.event_handler_getter_expression}}, v8Value, {{v8_class}}::eventListenerCacheIndex, info.GetIsolate());
    {% endif %}
    {# Type checking, possibly throw a TypeError, per:
       http://www.w3.org/TR/WebIDL/#es-type-mapping #}
    {% if attribute.has_type_checking_unrestricted %}
    {# Non-finite floating point values (NaN, +Infinity or −Infinity), per:
       http://heycam.github.io/webidl/#es-float
       http://heycam.github.io/webidl/#es-double #}
    if (!std::isfinite(cppValue)) {
        exceptionState.throwTypeError("The provided {{attribute.idl_type}} value is non-finite.");
        exceptionState.throwIfNeeded();
        return;
    }
    {% elif attribute.enum_validation_expression %}
    {# Setter ignores invalid enum values:
       http://www.w3.org/TR/WebIDL/#idl-enums #}
    String string = cppValue;
    if (!({{attribute.enum_validation_expression}}))
        return;
    {% endif %}
    {# Pre-set context #}
    {% if attribute.is_custom_element_callbacks or
          (attribute.is_reflect and
           not(attribute.idl_type == 'DOMString' and is_node)) %}
    {# Skip on compact node DOMString getters #}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {% if attribute.is_call_with_execution_context or
          attribute.is_setter_call_with_execution_context %}
    ExecutionContext* executionContext = currentExecutionContext(info.GetIsolate());
    {% endif %}
    {# Set #}
    {{attribute.cpp_setter}};
    {# Post-set #}
    {% if attribute.is_setter_raises_exception %}
    exceptionState.throwIfNeeded();
    {% endif %}
    {% if attribute.cached_attribute_validation_method %}
    V8HiddenValue::deleteHiddenValue(info.GetIsolate(), holder, v8AtomicString(info.GetIsolate(), "{{attribute.name}}")); // Invalidate the cached value.
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_setter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeSetterCallback{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
v8::Local<v8::String>, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info
{%- endif %})
{
    {% if attribute.is_expose_js_accessors %}
    v8::Local<v8::Value> v8Value = info[0];
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMSetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {% if world_suffix in attribute.activity_logging_world_list_for_setter %}
    ScriptState* scriptState = ScriptState::from(info.GetIsolate()->GetCurrentContext());
    V8PerContextData* contextData = scriptState->perContextData();
    {% if attribute.activity_logging_world_check %}
    if (scriptState->world().isIsolatedWorld() && contextData && contextData->activityLogger()) {
    {% else %}
    if (contextData && contextData->activityLogger()) {
    {% endif %}
        {% if attribute.activity_logging_include_old_value_for_setter %}
        {{cpp_class}}* impl = {{v8_class}}::toNative(info.Holder());
        {% if attribute.cpp_value_original %}
        {{attribute.cpp_type}} original = {{attribute.cpp_value_original}};
        {% else %}
        {{attribute.cpp_type}} original = {{attribute.cpp_value}};
        {% endif %}
        v8::Handle<v8::Value> originalValue = {{attribute.cpp_value_to_v8_value}};
        contextData->activityLogger()->logSetter("{{interface_name}}.{{attribute.name}}", v8Value, originalValue);
        {% else %}
        contextData->activityLogger()->logSetter("{{interface_name}}.{{attribute.name}}", v8Value);
        {% endif %}
    }
    {% endif %}
    {% if attribute.is_custom_element_callbacks or attribute.is_reflect %}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {% if attribute.has_custom_setter %}
    {{v8_class}}::{{attribute.name}}AttributeSetterCustom(v8Value, info);
    {% else %}
    {{cpp_class}}V8Internal::{{attribute.name}}AttributeSetter{{world_suffix}}(v8Value, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}
