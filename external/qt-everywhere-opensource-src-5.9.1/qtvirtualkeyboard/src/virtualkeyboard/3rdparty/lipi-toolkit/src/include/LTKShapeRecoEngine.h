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
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definition of LTKShapeRecoEngine which has the interface to the shape recognition engine
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKShapeRecoEngine_H
#define __LTKShapeRecoEngine_H

#include "LTKShapeRecognizer.h"

#include "LTKTraceGroup.h"

#include "LTKShapeRecoResult.h"

#include "LTKScreenContext.h"

#include "LTKCaptureDevice.h"

/**
 * @class LTKShapeRecoEngine
 * <p> This class provides the interface to the Shape Recognition Engine.
 * The required shape recognizer is instantiated dynamically and its corresponding 
 * recognize functions are called</p>
 */

class LTKShapeRecoEngine
{

private:

	LTKShapeRecognizer* m_shapeRecognizer;		//	a pointer to a derived class of LTKShapeRecognizer


public:

	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */

	LTKShapeRecoEngine();

	/**
	 * Destructor
	 */

	~LTKShapeRecoEngine();

	// @}


	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This method loads a shape recognizer with a particular profile for a particular 
	 * recognition problem by using the information in the configuration file and the lipi tool
	 * kit root path passed to it as arguments
	 * @param configFileName The configuration file to load a shape recognizer
	 * @param lipiRoot root path of the lipi tool kit
	 */

	int initializeShapeRecoEngine(const string& configFileName, string lipiRoot = "");

	/**
	* This method calls the loadTrainingData method of the instantiated shape recognizer.
	*/

	int loadTrainingData();

	/**
	* This method calls the recognize method of the instantiated shape recognizer.
	*
	* @param traceGroupObj The co-ordinates of the shape which is to be recognized
	* @param uiParams Contains information about the input field like whether it is boxed input
	*                 or continuous writing
	* @param deviceParams Contains information about the device like its sampling rate
	* @param shapeSubSet A subset of the entire set of shapes which is to be used for 
	*                        recognizing the input shape.
	* @param confThreshold A threshold on confidence value of the recognized shape. This is
	*                      used as Rejection Criteria
	* @param numOfChoices Number of top choices to be returned in the result structure
	* @param results The result of recognition
	*/

	int recognize(const LTKTraceGroup& traceGroup, const LTKScreenContext& screenContext,
		const LTKCaptureDevice& captureDevice, const vector<bool>& shapeSubSet, float confThreshold,
		int numChoices, vector<LTKShapeRecoResult>& results);

	/**
	* This method calls the recognize method of the derived shape recognizer.
	*
	* @param traceGroup The co-ordinates of the shape which is to be recognized
	* @param screenContext Contains information about the input field like whether it is boxed input
	*                 or continuous writing
	* @param captureDevice Contains information about the device like its sampling rate
	* @param shapeIds A subset of the entire shape space for which shape recongizer confidences
	*                 need to be computed
	* @param shapeRecoConfidences the confidence values calculated by the recognizer
	*/

	int getShapeRecoConfidences(const LTKTraceGroup& traceGroup, 
		const LTKScreenContext& screenContext,const LTKCaptureDevice& captureDevice, 
		const vector<int>& shapeIds, vector<float>& shapeRecoConfidences);

	/**
	* This method calls the learn method of the instantiated shape recognizer.
	*
	* @param prototype The sample to be used for adaptation
	* @param shapeId The shape id of the added prototype
	* @param uiParams Contains information about the input field like whether it is boxed input
	*                 or continuous writing
	* @param deviceParams Contains information about the device like its sampling rate
	*/

	int learn(const LTKTraceGroup& prototype, int shapeId, const LTKScreenContext& screenContext, 
		      const LTKCaptureDevice& captureDevice);

	/**
	* This method calls the train method of the instantiated shape recognizer.
	* @param trainingList The name of the file containing the listing of files to be used for
	*                    training each of the shapes.
	*/
	
	int train(const string& trainingList);

	// @}

};

#endif

//#ifndef __LTKShapeRecoEngine_H
//#define __LTKShapeRecoEngine_H


