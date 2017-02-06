%{Cpp:LicenseTemplate}\

#ifndef %{GUARD}
#define %{GUARD}

@if '%{Base}' === 'QNode'
#include <Qt3DCore/qnode.h>
@elsif '%{Base}' === 'QComponent'
#include <Qt3DCore/qcomponent.h>
@elsif '%{Base}' === 'QEntity'
#include <Qt3DCore/qentity.h>
@elsif '%{Base}' === 'QBackendNode'
#include <Qt3DCore/qbackendnode.h>
@endif

QT_BEGIN_NAMESPACE
%{JS: Cpp.openNamespaces('%{Class}')}
class %{CN}Private;

@if '%{Base}'
class %{CN} : public Qt3DCore::%{Base}
@else
class %{CN}
@endif
{
@if %{isQObject}
     Q_OBJECT
     // TODO: Add property declarations
@endif
public:
@if '%{Base}' === 'QNode' || '%{Base}' === 'QComponent' || '%{Base}' === 'QEntity'
    explicit %{CN}(Qt3DCore::QNode *parent = 0);
@else
    %{CN}();
@endif
    ~%{CN}();

public Q_SLOTS:

Q_SIGNALS:

@if '%{Base}' === 'QNode' || '%{Base}' === 'QComponent' || '%{Base}' === 'QEntity'
protected:
    Q_DECLARE_PRIVATE(%{CN})
    %{CN}(%{CN}Private &dd, Qt3DCore::QNode *parent = 0);
    void copy(const Qt3DCore::QNode *ref) Q_DECL_OVERRIDE;

private:
    QT3D_CLONEABLE(%{CN})
@endif
};
%{JS: Cpp.closeNamespaces('%{Class}')}
QT_END_NAMESPACE

#endif // %{GUARD}\
