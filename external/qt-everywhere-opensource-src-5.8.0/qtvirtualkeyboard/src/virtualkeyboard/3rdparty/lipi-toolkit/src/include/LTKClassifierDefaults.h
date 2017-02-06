/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2009-11-24 17:23:35 +0530 (Tue, 24 Nov 2009) $
 * $Revision: 792 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of all classifier config variables (default) settings
 * CONTENTS:
 * AUTHOR:     Balaji MNA
 * DATE:       22-Nov-2008
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Balaji MNA  22-Nov-2008      [LIPTKT-405] Cleaning up LTKPreprocDefaults
 ************************************************************************/

#ifndef __CLASSIFIERDEFAULTS_H__
#define __CLASSIFIERDEFAULTS_H__

#define PREPROC_DEF_TRACE_DIMENSION 60
#define PREPROC_DEF_RESAMPLINGMETHOD "lengthbased"
#define PREPROC_DEF_SIZE_THRESHOLD 0.01f
#define PREPROC_DEF_PRESERVE_ASPECT_RATIO true
#define PREPROC_DEF_ASPECTRATIO_THRESHOLD 3.0f
#define PREPROC_DEF_PRESERVE_RELATIVE_Y_POSITION false
#define PREPROC_DEF_SMOOTHFILTER_LENGTH 3
#define NN_DEF_PREPROC_SEQ		"{CommonPreProc::normalizeSize,CommonPreProc::resampleTraceGroup,CommonPreProc::normalizeSize}"
#define NN_DEF_PROTOTYPESELECTION "hier-clustering"
#define NN_DEF_PROTOTYPEREDUCTIONFACTOR -1
#define NN_DEF_FEATURE_EXTRACTOR "PointFloatShapeFeatureExtractor"
#define NN_DEF_DTWEUCLIDEANFILTER -1
#define NN_DEF_REJECT_THRESHOLD 0.001
#define NN_DEF_NEARESTNEIGHBORS 1
#define NN_DEF_PROTOTYPEDISTANCE "dtw"
#define NN_DEF_BANDING 0.33

//ActiveDTW  parameters
#define ACTIVEDTW_DEF_PERCENTEIGENENERGY 90
#define ACTIVEDTW_DEF_EIGENSPREADVALUE 16
#define ACTIVEDTW_DEF_USESINGLETON true
#define ACTIVEDTW_DEF_DTWEUCLIDEANFILTER 100

#define NEURALNET_DEF_NORMALIZE_FACTOR	10.0
#define NEURALNET_DEF_RANDOM_NUMBER_SEED	426
#define NEURALNET_DEF_LEARNING_RATE		0.5
#define NEURALNET_DEF_MOMEMTUM_RATE		0.25
#define NEURALNET_DEF_TOTAL_ERROR		0.00001
#define NEURALNET_DEF_INDIVIDUAL_ERROR	0.00001
#define NEURALNET_DEF_HIDDEN_LAYERS_SIZE	1
#define	NEURALNET_DEF_HIDDEN_LAYERS_UNITS	25
#define NEURALNET_DEF_MAX_ITR	100
#endif //#ifdef __CLASSIFIERDEFAULTS_H__
