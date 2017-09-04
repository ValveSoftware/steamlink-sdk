/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
********************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definition of LTKScreenContext which holds the co-ordinates of 
 *             the writing area provided for the set of traces being sent for
 *             recognition
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji Raghavan.
 *
 * DATE:       Dec 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu      01-MAR-2005       Added hLines and vLines to the class
 *                              Also incorporated the accessor functions
 ************************************************************************/

#ifndef __LTKSCREENCONTEXT_H
#define __LTKSCREENCONTEXT_H

#include "LTKInc.h"
#include "LTKTypes.h"

 
 /**
 * @class LTKScreenContext
 * <p> This class contains the co-ordinates of the writing area. </p>
 */


class LTKScreenContext
{
private:

	float m_bboxLeft;			//	leftmost x-coord of the writing area

	float m_bboxBottom;			//	bottommost y-coord of the writing area

	float m_bboxRight;			//	rightmost x-coord of the writing area

	float m_bboxTop;			//	topmost y-coord of the writing area

	floatVector m_hLines;		//	Vector of horizontal reference lines

	floatVector m_vLines;		//	Vector of vertical reference lines


public:

	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */

	LTKScreenContext ();

	/**
	 * This constrcutor takes various paramaters about a writing area.
	 * @param bboxLeft Gets the left x co-ordinate of the writing area
	 * @param bboxBotton Gets the bottom y co-ordinate of the writing area
	 * @param bboxRight Gets the right x co-ordinate of the writing area
	 * @param bboxTop Gets the top y co-ordinate of the writing area
	 */

	LTKScreenContext(float bboxLeft, float bboxBotton, float bboxRight, 
		             float bboxTop);

	/** Destructor */

	~LTKScreenContext ();

	// @}

	/**
	 * @name Getter Functions
	 */
	// @{

	/**
	 * This function returns the bottom left x co-ordinate of the writing area
	 * @param void
	 *
	 * @return Left x co-ordinate of the writing area.
	 */

	float getBboxLeft() const;

	/**
	 * This function returns the bottom left y co-ordinate of the writing area
	 * @param void
	 *
	 * @return Bottom y co-ordinate of the writing area.
	 */

	float getBboxBottom() const;

	/**
	 * This function returns the top right x co-ordinate of the writing area
	 * @param void
	 *
	 * @return Right x co-ordinate of the writing area.
	 */

	float getBboxRight() const;

	/**
	 * This function returns the top right y co-ordinate of the writing area
	 * @param void
	 *
	 * @return Top y co-ordinate of the writing area.
	 */

	float getBboxTop() const;

	/**
	 * This function returns the horizontal lines
	 * @param void
	 *
	 * @return const reference to vector of ordinates of horizontal lines
	 */

	const floatVector& getAllHLines() const;

	/**
	 * This function returns the vertical lines
	 * @param void
	 *
	 * @return const reference to vector of abscissae of vertical lines
	 */

	const floatVector& getAllVLines() const;

	// @}

	/**
	 * @name Setter Functions
	 */
	// @{

	/**
	 * This function adds a horizontal line 
	 * in the screen context
	 * @param ordinate of the horizontal line 
	 *
	 * @return SUCCESS on successful set operation
	 */

	int addHLine(float ordinate);

	/**
	 * This function adds a vertical line 
	 * in the screen context
	 * @param abscissa of the horizontal line 
	 *
	 * @return SUCCESS on successful set operation
	 */

	int addVLine(float abscissa);
	
	/**
	 * This function sets the bottom left x co-ordinate of the writing area
	 * @param bboxLeft Left x co-ordinate of the writing area
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setBboxLeft(float bboxLeft);

	/**
	 * This function sets the bottom left y co-ordinate of the writing area
	 * @param bboxBottom Bottom y co-ordinate of the writing area
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setBboxBottom(float bboxBottom);

	/**
	 * This function sets the top right x co-ordinate of the writing area
	 * @param bboxRight Right x co-ordinate of the writing area
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setBboxRight(float bboxRight);

	/**
	 * This function sets the top right y co-ordinate of the writing area
	 * @param bboxTop Top y co-ordinate of the writing area
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setBboxTop(float bboxTop);

	// @}

};

#endif

//#ifndef __LTKSCREENCONTEXT_H
//#define __LTKSCREENCONTEXT_H



