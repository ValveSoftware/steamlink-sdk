TEMPLATE = subdirs

qtConfig(private_tests) {
    SUBDIRS += \
        entity \
        renderqueue \
        renderpass \
        qgraphicsutils \
        shader \
        texture \
        renderviewutils \
        renderviews \
        material \
        vsyncframeadvanceservice \
        meshfunctors \
        qmaterial \
        qattribute \
        qbuffer \
        qgeometry \
        qgeometryrenderer \
        buffer \
        attribute \
        geometry \
        geometryrenderer \
        raycasting \
        qcameraselector \
        qclearbuffers \
        qframegraphnode \
        qlayerfilter \
        qabstractlight \
        qray3d \
        qrenderpassfilter \
        qrendertargetselector \
        qsortpolicy \
        qrenderstateset \
        qtechniquefilter \
        qtextureimagedata \
        qviewport \
        framegraphnode \
        qobjectpicker \
        objectpicker \
        picking \
#        qboundingvolumedebug \
#        boundingvolumedebug \
        qdefaultmeshes \
        trianglesextractor \
        triangleboundingvolume \
        ddstextures \
        shadercache \
        layerfiltering \
        filterentitybycomponent \
        genericlambdajob \
        qgraphicsapifilter \
        qrendersurfaceselector \
        sortpolicy \
        sceneloader \
        qsceneloader \
        qrendertargetoutput \
        qcameralens \
        qcomputecommand \
        loadscenejob \
        qrendercapture \
        uniform \
        graphicshelpergl3_3 \
        graphicshelpergl3_2 \
        graphicshelpergl2 \
        pickboundingvolumejob \
        sendrendercapturejob \
        textures \
        qparameter \
        parameter \
        qtextureloader \
        qtextureimage \
        qabstracttexture \
        qabstracttextureimage \
        qrendersettings \
        updatemeshtrianglelistjob \
        updateshaderdatatransformjob \
        texturedatamanager \
        rendertarget \
        transform \
        computecommand \
        qrendertarget \
        qdispatchcompute \
        qtechnique \
        qeffect \
        qrenderpass \
        qfilterkey \
        effect \
        filterkey \
        qmesh \
        technique \
        materialparametergathererjob

    !macos: SUBDIRS += graphicshelpergl4
}
