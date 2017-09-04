/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all 
* copies or substantial portions of the Software.
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
 * $LastChangedDate:  $
 * $Revision:  $
 * $Author:  $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for SubStrokeShapeFeature  module
 *
 * CONTENTS:
 *
 * AUTHOR:     Tanmay Mondal
 *
 * DATE:       February 2009
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/
#ifndef __SUBSTROKESHAPEFEATURE_H
#define __SUBSTROKESHAPEFEATURE_H

#include "LTKShapeFeature.h"

#define NUMBER_OF_SLOPE 5


class SubStrokeShapeFeature : public LTKShapeFeature
{
	/** @name private data members */
	//@{

private:

	/** @brief slope value 
	m_slopeVector slop(five angle)  extracted from one substrokes*/
	vector<float> m_slopeVector;  
	
	/** @brief x value
	m_xComponentOfCenterOfGravity, normalised x - value (by width) of the center of gravity of substrokes*/
	float m_xComponentOfCenterOfGravity;
	
	 /**@brief y value
	 m_yComponentOfCenterOfGravity, normalised y - value (by height) of the center of gravity of substrokes*/
	float m_yComponentOfCenterOfGravity;

	/**  @brief length value 
	m_subStrokeLength, this is the ratio of the total length of the substrokes and the normalised height of the character*/
	float m_subStrokeLength;

	/** @brief Delimiter character  */
    string m_data_delimiter;
	//@}
	
public:
	
	/** @name Constructors and Destructor */
	//@{

	/**
	* Default Constructor.
	*/
	SubStrokeShapeFeature();

    /** Parameterized Constructor */
    SubStrokeShapeFeature(vector<float>& inSlopeVector,float inCgX,float inCgY,float inLength);
    
	/**
	* Default destructor.
	*/
	~SubStrokeShapeFeature();
	
	/**
	* Returns the value of SubStrokeShapeFeature:: m_slopeVector
	*/
	void getSlopeVector(vector<float>& outSlopeVector) const;

	/**
	* Returns the value of SubStrokeShapeFeature::m_subStrokeLength
	*/
	float getSubStrokeLength() const;

	/**
	*	Returns the value of SubStrokeShapeFeature::m_xComponentOfCenterOfGravity
	*/
	float getXcomponentOfCenterOfGravity() const;

	/**
	* Returns the value of SubStrokeShapeFeature::m_yComponentOfCenterOfGravity
	*/
	float getYcomponentOfCenterOfGravity() const;

	/**
	* Sets the value of SubStrokeShapeFeature::m_slopeVector
	*/	
	void setSlopeVector(const vector<float>& inSlopeVector);
	
	/**
	* Set the value of SubStrokeShapeFeature::m_subStrokeLength
	*/
	void setSubStrokeLength(float inSubStrokeLength);

	/**
	* Set the value of SubStrokeShapeFeature::m_xComponentOfCenterOfGravity
	*/
	void setXcomponentOfCenterOfGravity(float inX);

	/**
	* Set the value of SubStrokeShapeFeature::m_yComponentOfCenterOfGravity
	*/
	void setYcomponentOfCenterOfGravity(float inY);


	/**
	* @brief Initializes the SubStrokeShapeFeature from the string passed as parameter.
	*
	* <b>Semantics</b>
	*
	*	- Tokenize the input string on SubStrokeShapeFeature::m_data_delimiter using StringTokenizer::tokenizeString
	*	- Initialize the data members of the class with the tokens returned.
	*
	* @param initString : string& : Reference to the initialization string.
	*
	* @return FAILURE : If the initalization string contains more than two tokens (after tokenizing on AngleShapeFeature::m_data_delimiter)
	* @return SUCCESS : If initialization done without any errors.
	*	
	*/
	int initialize(const string& initString);

	int initialize(const floatVector& initFloatVector);

	/**
	* @brief  Gives the floating point representation of the SubStrokeShapeFeature instance
	*/	
	int toFloatVector(vector<float>& outFloatVec);

    /**
	* @brief  Gives the string representation of the SubStrokeShapeFeature instance
	*/	
	void toString(string& outStrFeat) const;
	

	LTKShapeFeaturePtr clone() const;

    /**
	* @brief  Gives the distance between two SubStrokeShapeFeature instance
	*/	
	void getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const;

		/**
	* @brief Adds two SubStrokeShapeFeature instances.
	*
	* @param secondFeature : LTKShapeFeature* : Base class pointer holding pointer to SubStrokeShapeFeature instance
	*
	* @return LTKShapeFeature* : Pointer to SubStrokeShapeFeature instance holding the result of addition.
	*	
	*/
	int addFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const ;

	/**
	* @brief Subtracts two SubStrokeShapeFeature instances.
	*
	* @param secondFeature : LTKShapeFeature* : Base class pointer holding pointer to SubStrokeShapeFeature instance
	*
	* @return LTKShapeFeature* : Pointer to SubStrokeShapeFeature instance holding the result of subtraction.
	*	
	*/
	int subtractFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const ;

	int scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const ;

	int getFeatureDimension();

	/**
	* Returns the value of the class data member SubStrokeShapeFeature::m_penUp
	*/
	bool isPenUp() const;
};

#endif //#ifndef __SUBSTROKESHAPEFEATURE_H
