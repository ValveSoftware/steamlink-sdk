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
 * $LastChangedDate: 2008-02-20 10:03:51 +0530 (Wed, 20 Feb 2008) $
 * $Revision: 423 $
 * $Author: sharmnid $
 *
 ************************************************************************/
#include "LTKShapeFeature.h"



/** @ingroup L7ShapeFeatureExtractor
* @brief The feature representation class for L7 features
* This class represents X and Y coordinates, Normalized X and Y First Derivatives, Normalized X and Y Second Derivatives and the Curvature values of a point.
* @class L7ShapeFeature
*
*/
class L7ShapeFeature : public LTKShapeFeature
{
	/** @name private data members */
	//@{

private:
	/** @brief X value  */
	float m_x;					

	/** @brief Y value  */
	float m_y;
	
	/** @brief Normalized First Derivative w.r.t X  */
	float m_xFirstDerv;

	/** @brief Normalized First Derivative w.r.t Y  */
	float m_yFirstDerv;

    /** @brief Normalized Second Derivative w.r.t X  */
	float m_xSecondDerv;

	/** @brief Normalized Second Derivative w.r.t Y  */
	float m_ySecondDerv;

	/** @brief Curvature value */
	float m_curvature;
	
	/** @brief Pen-up information */
	bool m_penUp;

	/** @brief Delimiter character  */
    string m_data_delimiter;
	//@}
	
public:
	
	/** @name Constructors and Destructor */
	//@{

	/**
	* Default Constructor.
	*/
	L7ShapeFeature();

    /** parameterized constructor
        */
    L7ShapeFeature(float inX, float inY, float inXFirstDerv, float inYFirstDerv,
                   float inXSecondDerv, float inYSecondDerv, float inCurvature,
                   bool inPenUp);
    
	/**
	* Default destructor.
	*/
	~L7ShapeFeature();
	
	/**
	* Returns the value of the class data member L7ShapeFeature::m_x
	*/
	float getX() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_y
	*/
	float getY() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_xFirstDerv
	*/
	float getXFirstDerv() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_yFirstDerv
	*/
	float getYFirstDerv() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_xSecondDerv
	*/
	float getXSecondDerv() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_ySecondDerv
	*/
	float getYSecondDerv() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_curvature
	*/
	float getCurvature() const;

	/**
	* Returns the value of the class data member L7ShapeFeature::m_penUp
	*/
	bool isPenUp() const;

	/**
	* Sets the value of L7ShapeFeature::m_x
	*/	
	void setX(float x);

	/**
	* Sets the value of L7ShapeFeature::m_y
	*/	
	void setY(float y);

	/**
	* Sets the value of L7ShapeFeature::m_xFirstDerv
	*/	
	void setXFirstDerv(float xFirstDerv);

	/**
	* Sets the value of L7ShapeFeature::m_yFirstDerv
	*/	

	void setYFirstDerv(float yFirstDerv);
	/**
	* Sets the value of L7ShapeFeature::m_xSecondDerv
	*/	
	void setXSecondDerv(float xSecondDerv);

	/**
	* Sets the value of L7ShapeFeature::m_ySecondDerv
	*/	
	void setYSecondDerv(float ySecondDerv);

	/**
	* Sets the value of L7ShapeFeature::m_curvature
	*/	
	void setCurvature(float curvature);
	
	/**
	* Sets the value of L7ShapeFeature::m_penUp
	*/
	void setPenUp(bool penUp);

	
	/**
	* @brief Initializes an instance of L7ShapeFeature from the string passed as parameter.
	*
	* <b>Semantics</b>
	*
	*	- Tokenize the input string on L7ShapeFeature::m_data_delimiter using StringTokenizer::tokenizeString
	*	- Initialize the data members of the class with the tokens returned.
	*
	* @param initString : string& : Reference to the initialization string.
	*
	* @return FAILURE : If the initalization string contains more than or less than seven tokens (after tokenizing on L7ShapeFeature::m_data_delimiter)
	* @return SUCCESS : If initialization done without any errors.
	*	
	*/

    //see interface
	int initialize(const string& initString);

    int initialize(const floatVector& initFloatVector);

	/**
	* @brief  Gives the string representation of the L7ShapeFeature instance
	*/
    //see interface
	void toString(string& strFeat) const;
	
	/**
	* This method implements the clone pattern and returns a cloned instance of the invoking L7ShapeFeature object
	*/	
	LTKShapeFeaturePtr clone() const;
    
    
	/**
	* @brief Computes the Euclidean Distance between two L7ShapeFeature instances.
	* Computes  the square root of ( (m_x-passed_x)^2 + (m_y-passed_y)^2+ (m_xFirstDerv-passed_x1)^2 +(m_yFirstDerv-passed_y1)^2+(m_xSecondDerv-passed_x2)^2+(m_ySecondDerv-passed_y2)^2+(m_curvature-passed_curvature)^2).
	* @param f : LTKShapeFeature* : Base class pointer holding a pointer to L7ShapeFeature instance from which the distance to the invoking object is to be computed.
	*
	* @return float : Distance between two points in the LocalSeven feature representation
	*
	*/
	void getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const;
	
	/**
	* @brief Adds two L7ShapeFeature instances.
	* Computes the sum of the member variables of the passed LTKShapeFeature object and the invoking LTKShapeFeature object. The new member variables are stored in a newly created L7ShapeFeature object. Neither the invoking nor the passed object object are changed.
	* new_x=m_x+ (*secondFeature).m_x;
	* @param secondFeature : LTKShapeFeature* : Base class pointer holding a pointer to the L7ShapeFeature instance which is to be added to the invoking L7ShapeFeature object.
	*
	* @return LTKShapeFeature* : Base class pointer to the L7ShapeFeature instance holding the result of the addition operation.
	*	
	*/
	int addFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const;

	/**
	* @brief Subtracts two L7ShapeFeature instances.
	* Subtracts the member variables of the passed LTKShapeFeature object from the invoking LTKShapeFeature object. The new member variables are stored in a newly created L7ShapeFeature object, which is returned. Neither the invoking nor the passed object object are changed.
	* new_x=m_x-(*secondFeature).m_x;
	*
	* @param secondFeature : LTKShapeFeature* : Base class pointer holding a pointer to the L7ShapeFeature instance which is to be subtracted.
	*
	* @return LTKShapeFeature* : Base class pointer to the L7ShapeFeature instance holding the result of the subtraction operation.
	*	
	*/
	int subtractFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const;

	/**
	* @brief Scales the L7ShapeFeature feature by a scalar (float value).
	* Mulitplies all the member variables of the invoking L7ShapeFeature object by alpha. The new member variables are stored in a newly created L7ShapeFeature object, which is returned. The invoking LTKShapeFeature object is not changed.
	* new_x=alpha*m_x
	*
	* @param alpha : float: The float value by which all the member variables should be multiplied.
	* @return LTKShapeFeature* : Base class pointer to the L7ShapeFeature instance holding the result of the scaling operation .
	*	
	*/
	int scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const;

	/**
	* Converts the LocalSeven feature instance into a float vector. The elements of the float vector are m_x,m_y,m_xFirstDerv,m_yFirstDerv,m_xSecondDerv,m_ySecondDerv and m_curvature in this order.
	* @param floatVec: vector<float>&: The float vector which will contain the member values. 
	* @return int : returns SUCCESS or FAILURE
	*	
	*/
	int toFloatVector(vector<float>& floatVec);
	
	/**
	* Converts the LocalSeven feature instance into a integer vector. The elements of the int vector are m_x,m_y,m_xFirstDerv,m_yFirstDerv,m_xSecondDerv,m_ySecondDerv and m_curvature in this order.
	* @param intVec: vector<int>&: The int vector which will contain the member values. 
	* @return int : returns SUCCESS or FAILURE
	*	
	*/
	int toIntVector(vector<int>& intVec);

    int getFeatureDimension();
};
