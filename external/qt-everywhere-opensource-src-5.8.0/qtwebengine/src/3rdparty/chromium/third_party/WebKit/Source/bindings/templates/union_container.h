{% include 'copyright_block.txt' %}
#ifndef {{cpp_class}}_h
#define {{cpp_class}}_h

{% for filename in header_includes %}
#include "{{filename}}"
{% endfor %}

namespace blink {

{% for decl in header_forward_decls %}
class {{decl}};
{% endfor %}

class {{exported}}{{cpp_class}} final {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    {{cpp_class}}();
    bool isNull() const { return m_type == SpecificTypeNone; }

    {% for member in members %}
    bool is{{member.type_name}}() const { return m_type == {{member.specific_type_enum}}; }
    {{member.rvalue_cpp_type}} getAs{{member.type_name}}() const;
    void set{{member.type_name}}({{member.rvalue_cpp_type}});
    static {{cpp_class}} from{{member.type_name}}({{member.rvalue_cpp_type}});

    {% endfor %}
    {{cpp_class}}(const {{cpp_class}}&);
    ~{{cpp_class}}();
    {{cpp_class}}& operator=(const {{cpp_class}}&);
    DECLARE_TRACE();

private:
    enum SpecificTypes {
        SpecificTypeNone,
        {% for member in members %}
        {{member.specific_type_enum}},
        {% endfor %}
    };
    SpecificTypes m_type;

    {% for member in members %}
    {{member.cpp_type}} m_{{member.cpp_name}};
    {% endfor %}

    friend {{exported}}v8::Local<v8::Value> toV8(const {{cpp_class}}&, v8::Local<v8::Object>, v8::Isolate*);
};

class {{v8_class}} final {
public:
    {{exported}}static void toImpl(v8::Isolate*, v8::Local<v8::Value>, {{cpp_class}}&, UnionTypeConversionMode, ExceptionState&);
};

{{exported}}v8::Local<v8::Value> toV8(const {{cpp_class}}&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, {{cpp_class}}& impl)
{
    v8SetReturnValue(callbackInfo, toV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<{{cpp_class}}> {
    {{exported}}static {{cpp_class}} nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

} // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::{{cpp_class}});

#endif // {{cpp_class}}_h
