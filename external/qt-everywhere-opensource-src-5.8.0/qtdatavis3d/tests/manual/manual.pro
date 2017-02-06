TEMPLATE = subdirs

qtHaveModule(quick) {
    SUBDIRS += qmldynamicdata \
               qmlmultitest \
               qmlvolume \
               qmlperf
}

!android:!ios:!winrt {
    SUBDIRS += barstest \
               scattertest \
               surfacetest \
               multigraphs \
               directional \
               itemmodeltest \
               volumetrictest

    # For testing code snippets of minimal applications
    SUBDIRS += minimalbars \
               minimalscatter \
               minimalsurface

}
