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
 * $LastChangedDate: 2009-07-01 20:12:22 +0530 (Wed, 01 Jul 2009) $
 * $Revision: 784 $
 * $Author: mnab $
 *
 ************************************************************************/
#ifndef __POINTFLOATSHAPEFEATURE_H
#define __POINTFLOATSHAPEFEATURE_H

#include "LTKShapeFeature.h"


/** @defgroup PointFloatShapeFeatureExtractor
*/

/** @ingroup PointFloatShapeFeatureExtractor
* @brief The feature representation class for X, Y features (float)
* @class PointFloatShapeFeature
*
*/
class PointFloatShapeFeature : public LTKShapeFeature
{
	/** @name private data members */
	//@{

private:
	/** @brief X value  */
	float m_x;					

	/** @brief Y value  */
	float m_y;			
	
	/** @brief sin theta value  */
	float m_sinTheta;

	/** @brief cos theta value  */
	float m_cosTheta;

	/** @brief Pen-up information */
	bool m_penUp;

	/** @brief Delimiter character  */
    static const string m_data_delimiter;
	//@}
	
public:
	
	/** @name Constructors and Destructor */
	//@{

	/**
	* Default Constructor.
	*/
	PointFloatShapeFeature();

    /** Parameterized Constructor */
    PointFloatShapeFeature(float inX, float inY, float inSinTheta,
                           float inCosTheta, bool inPenUp);
    
	/**
	* Default destructor.
	*/
	~PointFloatShapeFeature();
	
	/**
	* Returns the value of class data member PointFloatShapeFeature::m_x
	*/
	float getX() const;

	/**
	* Returns the value of PointFloatShapeFeature::m_y
	*/
	float getY() const;

	/**
	* Returns the value of PointFloatShapeFeature::m_sinTheta
	*/
	float getSinTheta() const;

	/**
	* Returns the value of PointFloatShapeFeature::m_cosTheta
	*/
	float getCosTheta() const;

	bool isPenUp() const;


	/**
	* Sets the value of PointFloatShapeFeature::m_x
	*/	

	void setX(float x);

	/**
	* Sets the value of PointFloatShapeFeature::m_y
	*/	
	void setY(float y);

	/**
	* Sets the value of PointFloatShapeFeature::m_sinTheta
	*/	

    //int setSinTheta(float y);
	void setSinTheta(float y);

	
	/**
	* Sets the value of PointFloatShapeFeature::m_cosTheta
	*/	
	//int setCosTheta(float y);
	void setCosTheta(float y);


	void setPenUp(bool penUp);
	/**
	* @brief Initializes the PointFloatShapeFeature from the string passed as parameter.
	*
	* <b>Semantics</b>
	*
	*	- Tokenize the input string on PointFloatShapeFeature::m_data_delimiter using StringTokenizer::tokenizeString
	*	- Initialize the data members of the class with the tokens returned.
	*
	* @param initString : string& : Reference to the initialization string.
	*
	* @return FAILURE : If the initalization string contains more than two tokens (after tokenizing on PointFloatShapeFeature::m_data_delimiter)
	* @return SUCCESS : If initialization done without any errors.
	*	
	*/
	int initialize(const string& initString);

    int initialize(const floatVector& initFloatVector);

    int initialize(floatVector::const_pointer initFloatData, size_t dataSize);

    int toFloatVector(vector<float>& floatVec);

	/**
	* @brief  Gives the string representation of the PointFloatShapeFeature instance
	*/	
	void toString(string& strFeat) const;
	/**
	* This method sets the value of x
	*/	

	LTKShapeFeaturePtr clone() const;

    void getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const;

    

	/**
	* @brief Adds two PoinFloatShapeFeature instances.
	*
	* @param secondFeature : LTKShapeFeature* : Base class pointer holding pointer to PointFloatShapeFeature instance
	*
	* @return LTKShapeFeature* : Pointer to PointFloatShapeFeature instance holding the result of addition.
	*	
	*/
	int addFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const ;

	/**
	* @brief Subtracts two PoinFloatShapeFeature instances.
	*
	* @param secondFeature : LTKShapeFeature* : Base class pointer holding pointer to PointFloatShapeFeature instance
	*
	* @return LTKShapeFeature* : Pointer to PointFloatShapeFeature instance holding the result of subtraction.
	*	
	*/
	int subtractFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const ;

	int scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const ;

    int getFeatureDimension();
    
};

#endif
//#ifndef __POINTFLOATSHAPEFEATURE_H
