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
 * $LastChangedDate: 2008-11-14 17:34:35 +0530 (Fri, 14 Nov 2008) $
 * $Revision: 702 $
 * $Author: royva $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of LTKShapeRecognizer which would be used as a place holder in LTKShapeRecognizer 
 *			   for anyone of the implemented shape recognizer like PCAShapeRecognizer which is derived from this class
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKShapeRecognizer_H
#define __LTKShapeRecognizer_H

#include "LTKInc.h"

#include "LTKTraceGroup.h"

#include "LTKShapeRecoResult.h"

#include "LTKScreenContext.h"

#include "LTKCaptureDevice.h"

#include "LTKShapeRecoConfig.h"

#include "LTKMacros.h"


#include "LTKShapeFeatureMacros.h"
/**
 * @class LTKShapeRecognizer
 * <p> This is an abstract class. This class will have to be derived by each of the shape recognizers
 * which are to be used for recognition. This class has some pure virtual functions which are
 * to be implemented by the derived class.</p>
 */

class LTKShapeRecognizer
{

protected:

	const string m_shapeRecognizerName;	//	name of the shape recognizer class deriving from the LTKShapeRecognizer class
	bool m_cancelRecognition;

public:

	/** @name Constructors and Destructor */
	//@{


	/**
	 * Default Constructor.
	 */

	LTKShapeRecognizer();

	/**
	 * Initialization Constructor. Initialzes the member m_shapeRecognizerName
	 */

	LTKShapeRecognizer(const string& shapeRecognizerName);

	/**
	 * Pure Virtual Destructor
	 */

	virtual ~LTKShapeRecognizer() = 0;

	//@}

	/**
	 * @name Methods
	 */

	//@{

	
	/**
	* This is a pure virtual method to be implemented by the derived class.This method loads the  
	* the Training Data of the derived class.
	*/

	virtual int loadModelData() =  0;

	/**
	* This is a pure virtual method to be implemented by the derived class.This method calls 
	* the recognize method of the derived shape recognizer.
	*
	* @param traceGroupObj The co-ordinates of the shape which is to be recognized
	* @param screenContext Contains information about the input field like whether it is boxed input
	*                 or continuous writing
	* @param captureDevice Contains information about the device like its sampling rate
	* @param shapeSubSet A subset of the entire shape space which is to be used for 
	*                        recognizing the input shape.
	* @param confThreshold A threshold on confidence value of the recognized shape. This is
	*                      used as Rejection Criteria
	* @param numOfChoices Number of top choices to be returned in the result structure
	* @param results The result of recognition
	*/

	virtual int recognize(const LTKTraceGroup& traceGroup,
			const LTKScreenContext& screenContext,
			const vector<int>& subSetOfClasses,
			float confThreshold,
			int  numChoices,
			vector<LTKShapeRecoResult>& resultVector) = 0;

	virtual int recognize(const vector<LTKShapeFeaturePtr>& shapeFeatureVector,
			const vector<int>& subSetOfClasses,
			float confThreshold,
			int  numChoices,
			vector<LTKShapeRecoResult>& resultVector) = 0;

	void requestCancelRecognition() { m_cancelRecognition = true; }
	
	/**
	* This is a pure virtual method to be implemented by the derived class.This method calls 
	* the recognize method of the derived shape recognizer.
	*
	* @param traceGroup The co-ordinates of the shape which is to be recognized
	* @param screenContext Contains information about the input field like whether it is boxed input
	*                 or continuous writing
	* @param captureDevice Contains information about the device like its sampling rate
	* @param shapeIds A subset of the entire shape space for which shape recongizer confidences
	*                 need to be computed
	* @param shapeRecoConfidences the confidence values calculated by the recognizer
	*/

	virtual int getShapeRecoConfidences(const LTKTraceGroup& traceGroup, 
		const LTKScreenContext& screenContext,
		const vector<int>& shapeIds, vector<float>& shapeRecoConfidences){(void)traceGroup;(void)screenContext;(void)shapeIds;(void)shapeRecoConfidences;return FAILURE;}
				 

	/**
	* This is a virtual method to be implemented by the derived class. This method calls the 
	* train method of the derived shape recognizer.
	*
	* @param trainingList The name of the file containing the listing of files to be used for
	*                    training each of the shapes.
	*/

	virtual int train(const string& trainingList,
                      const string& strModelDataHeaderInfoFile, 
                      const string &comment, 
                      const string &dataset, 
                      const string &inFileType=INK_FILE) = 0;	
	
	/**
	* This method unloads all the training data
	*/
	virtual int unloadModelData() = 0;

	/**
	* This method sets the device context for the recognition
	*
	* @param deviceInfo The parameter to be set
	*/

    	virtual int setDeviceContext(const LTKCaptureDevice& deviceinfo) = 0;

	/**
	* This is a pure virtual method to be implemented by the derived class.
	* This method will Add a new shape class
	* shapeID contains classID of new class added
	* Returns Sucess/Failure
	*/

    	virtual int addClass(const LTKTraceGroup& sampleTraceGroup,int& shapeID);

		
	/**
	* This is a pure virtual method to be implemented by the derived class.
	* This method will Add a new shape class for adapt
	* shapeID contains classID of new class added
	* Returns Sucess/Failure
	*/

		virtual int addSample(const LTKTraceGroup& sampleTraceGroup,int shapeID);
		
	/**
	* This is a pure virtual method to be implemented by the derived class.
	* This method will delete shape
	* Returns Sucess/Failure
	*/
	virtual int deleteClass(int shapeID );
	
	/**
	* This is a pure virtual method to be implemented by the derived class.
	* This method will adapt the recent sample recognized
	*  True ShapeID is passed as argument
	* Returns Sucess/Failure
	*/
	virtual int adapt(int shapeID );

	/**
	* This is a pure virtual method to be implemented by the derived class.
	* This method will adapt the sample provided
	*  True ShapeID is also passed as argument
	* Returns Sucess/Failure
	*/

    	virtual int adapt(const LTKTraceGroup& sampleTraceGroup, int shapeID );


};

#endif

//#ifndef __LTKShapeRecognizer_H
//#define __LTKShapeRecognizer_H

