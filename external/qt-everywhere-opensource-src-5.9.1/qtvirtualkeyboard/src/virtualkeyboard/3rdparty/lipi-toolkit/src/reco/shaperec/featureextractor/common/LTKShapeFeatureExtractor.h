/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Function prototype for feature extraction module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       07-FEB-2007
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKFEATUREEXTRACTOR_H
#define __LTKFEATUREEXTRACTOR_H


#include "LTKTypes.h"
#include "LTKShapeFeatureMacros.h"

// Forward class declarations
class LTKTraceGroup;
class LTKShapeFeature;

/**
* @ingroup feature_extractor
* @brief An abstract class which is extended by all the feature extractors.
* @class LTKShapeFeatureExtractor
* 
*/
class LTKShapeFeatureExtractor
{
public:
	/**
	* <b>Responsibility</b><br>
	* Extract features from the input TraceGroup passed a parameter.
	*
	* <b>Description</b>
	* <p>
	* Every feature representation class has a feature extractor associated with it. The feature extractor
	* extracts the features from the trace group passed as parameter.
	* </p>
	*
	* <p>This function is based on run-time polymorphism.
	* The pointer to the derived class instances are stored in a vector of base class (<i>LTKShapeFeature</i>) pointers.
	* </p>
	*
	* @return Pointer to the vector of LTKShapeFeature pointers.
	*/
	virtual int extractFeatures(const LTKTraceGroup& inTraceGroup,
	                              vector<LTKShapeFeaturePtr>& outFeatureVec)=0;

	/**
	* <b>Responsibility</b><br>
	* Returns the instance of the shape feature class associated with the feature extractor.
	*
	* @return Pointer to the shape feature class. 
	*
	*/
	virtual LTKShapeFeaturePtr getShapeFeatureInstance()=0;


	/**
	* <b>Responsibility</b><br>
	* Constructs traceGroup based on input LTKShapeFeaturePtr 
	*
	* @return Pointer to Trace Group. 
	*
	*/
	virtual int convertFeatVecToTraceGroup(
	                            const vector<LTKShapeFeaturePtr>& shapeFeature, 
	                            LTKTraceGroup& outTraceGroup);
};


#endif
