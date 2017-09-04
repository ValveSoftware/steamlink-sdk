%{Cpp:LicenseTemplate}\

#ifndef %{GUARD}
#define %{GUARD}

@if '%{Base}' === 'QNode'
#include <Qt3DCore/qnode.h>
@elsif '%{Base}' === 'QComponent'
#include <Qt3DCore/qcomponent.h>
@elsif '%{Base}' === 'QEntity'
#include <Qt3DCore/qentity.h>
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
    explicit %{CN}(Qt3DCore::QNode *parent = nullptr);
@else
    %{CN}();
@endif
    ~%{CN}();

public Q_SLOTS:

Q_SIGNALS:

@if '%{Base}' === 'QNode' || '%{Base}' === 'QComponent' || '%{Base}' === 'QEntity'
protected:
    %{CN}(%{CN}Private &dd, Qt3DCore::QNode *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(%{CN})
    Qt3DCore::QNodeCreatedChangeBasePtr createNodeCreationChange() const Q_DECL_OVERRIDE;
@endif
};
%{JS: Cpp.closeNamespaces('%{Class}')}
QT_END_NAMESPACE

#endif // %{GUARD}\
