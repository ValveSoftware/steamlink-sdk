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
 * $LastChangedDate: 2009-04-01 12:32:40 +0530 (Wed, 01 Apr 2009) $
 * $Revision: 751 $
 * $Author: royva $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of all factory preprocessor (default) settings
 *
 * CONTENTS:
 *
 * AUTHOR:     Thanigai Murugan
 *
 * DATE:       11-AUG-2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __PREPROCDEFAULTS_H__
#define __PREPROCDEFAULTS_H__

#define PREPROC_DEF_NORMALIZEDSIZE 10.0f
#define PREPROC_DEF_DOT_THRESHOLD 0.01f
#define PREPROC_DEF_LOOP_THRESHOLD 0.25f
#define PREPROC_DEF_HOOKLENGTH_THRESHOLD1 0.17f
#define PREPROC_DEF_HOOKLENGTH_THRESHOLD2 0.33f
#define PREPROC_DEF_HOOKANGLE_THRESHOLD 30
#define PREPROC_DEF_QUANTIZATIONSTEP 5
#define PREPROC_DEF_FILTER_LENGTH 3
#define NN_NUM_CLUST_INITIAL -2

#define PREPROC_DEF_INTERPOINT_DIST_FACTOR 15

// LVQ parameters
#define NN_DEF_LVQITERATIONSCALE 40
#define NN_DEF_LVQINITIALALPHA 0.3
#define NN_DEF_LVQDISTANCEMEASURE "eu"

#define NN_DEF_MDT_UPDATE_FREQ 5
#define NN_DEF_RECO_NUM_CHOICES 2

#define NN_MDT_OPEN_MODE_BINARY "binary"
#define NN_MDT_OPEN_MODE_ASCII "ascii"



//ADAPT parameters
#define ADAPT_DEF_MIN_NUMBER_SAMPLES_PER_CLASS 5
#define ADAPT_DEF_MAX_NUMBER_SAMPLES_PER_CLASS 10
#endif //#ifdef__PREPROCDEFAULTS_H__


