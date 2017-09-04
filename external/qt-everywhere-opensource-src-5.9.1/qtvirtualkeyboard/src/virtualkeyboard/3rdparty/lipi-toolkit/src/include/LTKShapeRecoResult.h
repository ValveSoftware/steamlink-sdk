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
 * FILE DESCR: Definition of LTKShapeRecoResult which holds the recognition results of the 
 *			   shape recognition engine
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKSHAPERECORESULT_H
#define __LTKSHAPERECORESULT_H


/**
 * @class LTKShapeRecoResult
 * <p> This class is used to return recognition result back to the application program.
 * It contains a shapeId and the confidence that the shape with this Id is the true shape
 * of the input sample. </p>
 */
class LTKShapeRecoResult
{

private:

	int m_shapeId;			//	shape id of the recognition result

        float m_confidence;		//	corresponding confidence of recognition

public:

    /**
	 * @name Constructors and Destructor
	 */

	// @{

	/**
	 * Default Constructor
	 */

	LTKShapeRecoResult();

	/**
	 * This constrcutor takes two paramaters.
	 * @param shapeId shape Id of the shape
	 * @param confidence Confidence that this shape is the true shape of input sample 
	 */

	LTKShapeRecoResult(int shapeId, float confidence);

	/** Destructor */

	~LTKShapeRecoResult();

	// @}

	/**
	 * @name Getter Functions
	 */
	// @{

	/**
	 * This function returns the shape Id of the result
	 * @param void
	 *
	 * @return shape Id of the result.
	 */

	int getShapeId() const;

	/**
	 * This function returns the Confidence of the result
	 * @param void
	 *
	 * @return Confidence of the result.
	 */

	float getConfidence() const;

	// @}

	/**
	 * @name Setter Functions
	 */

	// @{

	/**
	 * This function sets shape Id of the result
	 * @param shapeId Identification tag of the shape
	 */

	int setShapeId(int shapeId);

	/**
	 * This function sets confidence of the recognized shape
	 * @param confidence Confidence value that the recognized shape is the true shape
	 */

	int setConfidence(float confidence);

	// @}

};

#endif

//#ifndef __LTKSHAPERECORESULT_H
//#define __LTKSHAPERECORESULT_H


