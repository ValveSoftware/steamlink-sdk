%{Cpp:LicenseTemplate}\
#include "%{PrivateHdrFileName}"

QT_BEGIN_NAMESPACE
%{JS: Cpp.openNamespaces('%{Class}')}
%{CN}::%{CN}()
{
}

void %{CN}::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
}

void %{CN}::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
}

%{JS: Cpp.closeNamespaces('%{Class}')}\

QT_END_NAMESPACE
