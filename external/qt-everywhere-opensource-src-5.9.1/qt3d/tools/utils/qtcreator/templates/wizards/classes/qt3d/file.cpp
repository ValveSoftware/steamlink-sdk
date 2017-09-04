%{Cpp:LicenseTemplate}\
#include "%{HdrFileName}"
#include "%{PrivateHdrFileName}"

QT_BEGIN_NAMESPACE
%{JS: Cpp.openNamespaces('%{Class}')}
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
}

Qt3DCore::QNodeCreatedChangeBasePtr %{CN}::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<%{CN}Data>::create(this);
    auto &data = creationChange->data;
    Q_D(const %{CN});
    // TODO: Send data members in creation change
    return creationChange;
}
%{JS: Cpp.closeNamespaces('%{Class}')}\

QT_END_NAMESPACE
