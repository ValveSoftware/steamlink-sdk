/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of 
* this software and associated documentation files (the "Software"), to deal in 
* the Software without restriction, including without limitation the rights to use, 
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
* Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
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
 * $LastChangedDate: 2008-02-20 10:03:51 +0530 (Wed, 20 Feb 2008) $
 * $Revision: 423 $
 * $Author: sharmnid $
 *
 ************************************************************************/
#include "LTKShapeFeature.h"

#define DELIMITER_SIZE	5


/** @ingroup NPenShapeFeatureExtractor
* @brief The feature representation class for NPen features
* This class represents NPen++ features such as X, Y, cos/sin alpha, cos/sin beta, aspect, curliness, linearity, slope and pen-up/down bit
* @class NPenShapeFeature
*
*/
class NPenShapeFeature : public LTKShapeFeature
{
	/** @name private data members */
	//@{

private:
	/** @brief X value  */
	float m_x;					

	/** @brief Y value  */
	float m_y;
	
	
	float m_cosAlpha;

	float m_sinAlpha;
	
	float m_cosBeta;

	float m_sinBeta;

	float m_aspect;

	float m_curliness;

	float m_linearity;

	float m_slope;

	bool m_isPenUp;

	
	string m_data_delimiter;		
	//@}
	
public:
	
	/** @name Constructors and Destructor */
	//@{

	/**
	* Default Constructor.
	*/
	NPenShapeFeature();

    /** parameterized constructor
        */
    NPenShapeFeature(float inX, float inY, float cosAlpha, float inSinAlpha,
                   float inCosBeta, float inSinBeta, float inAspect, float inCurliness,
                   float inLinearity, float inSlope, bool isPenUp);
    
	/**
	* Default destructor.
	*/
	~NPenShapeFeature();
	
	/**
	* Returns the value of the class data member NPenShapeFeature::m_x
	*/
	float getX() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_y
	*/
	float getY() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_cosAlpha
	*/
	float getCosAlpha() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_sinAlpha
	*/
	float getSinAlpha() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_cosBeta
	*/
	float getCosBeta() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_sinBeta
	*/
	float getSinBeta() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_aspect
	*/
	float getAspect() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_curliness
	*/
	float getCurliness() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_linearity
	*/
	float getLinearity() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_slope
	*/
	float getSlope() const;

	/**
	* Returns the value of the class data member NPenShapeFeature::m_penUp
	*/
	bool isPenUp() const;

	/**
	* Sets the value of NPenShapeFeature::m_x
	*/	
	void setX(float x);

	/**
	* Sets the value of NPenShapeFeature::m_y
	*/	
	void setY(float y);

	/**
	* Sets the value of NPenShapeFeature::m_cosAlpha
	*/	
	void setCosAlpha(float cosAlpha);

	/**
	* Sets the value of NPenShapeFeature::m_sinAlpha
	*/	
	void setSinAlpha(float sinAlpha);

	/**
	* Sets the value of NPenShapeFeature::m_cosBeta
	*/	
	void setCosBeta(float cosBeta);

	/**
	* Sets the value of NPenShapeFeature::m_sinBeta
	*/	
	void setSinBeta(float sinBeta);

	/**
	* Sets the value of NPenShapeFeature::m_aspect
	*/	
	void setAspect(float aspect);

	/**
	* Sets the value of NPenShapeFeature::m_curliness
	*/	
	void setCurliness(float curliness);

	/**
	* Sets the value of NPenShapeFeature::m_linearity
	*/	
	void setLinearity(float linearity);

	/**
	* Sets the value of NPenShapeFeature::m_slope
	*/	
	void setSlope(float slope);
	
	/**
	* Sets the value of NPenShapeFeature::m_penUp
	*/
	void setPenUp(bool penUp);

	
	/**
	* @brief Initializes an instance of NPenShapeFeature from the string passed as parameter.
	*
	* <b>Semantics</b>
	*
	*	- Tokenize the input string on NPenShapeFeature::m_data_delimiter using StringTokenizer::tokenizeString
	*	- Initialize the data members of the class with the tokens returned.
	*
	* @param initString : string& : Reference to the initialization string.
	*
	* @return FAILURE : If the initalization string contains more than or less than seven tokens (after tokenizing on NPenShapeFeature::m_data_delimiter)
	* @return SUCCESS : If initialization done without any errors.
	*	
	*/

    //see interface
	int initialize(const string& initString);

	/**
	* @brief  Gives the string representation of the NPenShapeFeature instance
	*/
    //see interface
	void toString(string& strFeat) const;
	
	/**
	* This method implements the clone pattern and returns a cloned instance of the invoking NPenShapeFeature object
	*/	
	LTKShapeFeaturePtr clone() const;
    
    
	
	void getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const;
	
	/**
	* Converts the local feature instance into a float vector. The elements of the float vector are m_x, m_y, m_cosAlpha, m_sinAlpha, m_cosBeta, m_sinBeta, m_aspect, m_curliness, m_linearity, m_slope and m_isPenUp in the order.
	* @param floatVec: vector<float>&: The float vector which will contain the member values. 
	* @return int : returns SUCCESS or FAILURE
	*	
	*/
	int toFloatVector(vector<float>& floatVec);


	int getFeatureDimension();



	int initialize(const floatVector& initFloatVector);
	
	
};
