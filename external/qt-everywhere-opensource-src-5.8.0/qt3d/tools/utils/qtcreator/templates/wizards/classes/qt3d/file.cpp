%{Cpp:LicenseTemplate}\
#include "%{HdrFileName}"
#include "%{PrivateHdrFileName}"

QT_BEGIN_NAMESPACE
%{JS: Cpp.openNamespaces('%{Class}')}
@if '%{Base}' === 'QNode' || '%{Base}' === 'QComponent' || '%{Base}' === 'QEntity'
%{CN}Private::%{CN}Private()
    : Qt3DCore::%{Base}Private()
{
}

%{CN}::%{CN}(Qt3DCore::QNode *parent)
    : Qt3DCore::%{Base}(*new %{CN}Private, parent)
{
}

%{CN}::%{CN}(%{CN}Private &dd, Qt3DCore::QNode *parent)
    : Qt3DCore::%{Base}(dd, parent)
{
}

%{CN}::~%{CN}()
{
    QNode::cleanup();
}
@else
// TODO: Implement QBackendNode template
@endif

@if '%{Base}' === 'QNode' || '%{Base}' === 'QComponent' || '%{Base}' === 'QEntity'
void %{CN}::copy(const QNode *ref)
{
    %{Base}::copy(ref);
    const %{CN} *object = static_cast<const %{CN} *>(ref);

    // TODO: Copy the objects's members
}
@endif
%{JS: Cpp.closeNamespaces('%{Class}')}\

QT_END_NAMESPACE
