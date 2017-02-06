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
#include "LTKShapeFeatureExtractor.h"
/** @ingroup L7ShapeFeatureExtractor
* @brief The feature extractor for L7 features.
* @class L7ShapeFeatureExtractor
*
*/
#define FEATEXTR_L7_DEF_RADIUS 2


class L7ShapeFeatureExtractor : public LTKShapeFeatureExtractor
{
private:
	int m_radius;
    
public:
    /** @brief Constructor for the L7 feature extractor
	 * Gets the cfg file path from the contorInfo
	 * Reads the cfg variables and poputlates the member variables
	 * @param controlInfo: LTKControlInfo : The control information
	 * @return no return value as it is a constructor
	 */
	L7ShapeFeatureExtractor(const LTKControlInfo& controlInfo);
	
	/** @brief Extracts L7 features from an LTKTraceGroup
	 * The XY Coordinate values are extracted from the trace gruoup. 
	 * Using the coordinate values, normalized first derivatives are computed. 
	 * After computing the first derivatives, the normalized second derivatives and the curvature are computed. The second derivative and curvature computation occur simultaneously. 
	 * @param  inTraceGroup: LTKTraceGroup& : The trace group from which local seven features have to be extracted
	 * @return LTKShapeFeaturePtr* : A vector of pointers to LTKShapeFeature objects. The pointers point to instances of the L7ShapeFeature class. The vector contains local-seven features extracted from the trace group.
	 */
  
  	int extractFeatures(const LTKTraceGroup& inTraceGroup,
  	                      vector<LTKShapeFeaturePtr>& outFeatureVec);
 
	/** @brief Returns an instance of the L7ShapeFeature class
	* @return LTKShapeFeature*: The returned empty L7ShapeFeature instance.
	*/
	LTKShapeFeaturePtr getShapeFeatureInstance();
	
	/** @brief Converts a feature vector to trace group
	 * @param shapeFeature  : LTKShapeFeaturePtr* : The feature vector to be converted to a trace group. 
	 * @param outTraceGroup : LTKTraceGroup&			: The output trace group 
	 */
	 //see interface
	int convertFeatVecToTraceGroup(const vector<LTKShapeFeaturePtr>& shapeFeature, 
	                                      LTKTraceGroup& outTraceGroup);

    int getRadius();

private:

	/** @brief Computes the denominator for derivative calculation
	 * @param index : int: The index used in derivative computation 
	 * @return int	: The denominator used in derivative computation
	 */
	int computeDerivativeDenominator(int index);

	/** @brief Computes the derivative for two input sequences
	 * @param xVec	: vector<float>&: Input x vector.
	 * @param yVec	: vector<float>&: Input y vector. 
	 * @param dx	: vector<float>&: Derivative vector of x
	 * @param dy	: vector<float>&: Derivative vector of y
	 * @param index : int			: The window size used in derivative computation
	 * 
	 */
	void computeDerivative(const vector<float>& xVec, 
	                           const vector<float>& yVec,
	                           vector<float>& dx,
	                           vector<float>& dy,
	                           int index);
	
	/** @brief reads the cfg file and sets the member variables
	* @param cfgFilePath : const string&: The path of the cfg file to be opened
	* @return int : The sucess or failure of the function
	*/
	int readConfig(const string& cfgFilePath);
    
	/** @validates the value passed and sets the member variable
	* @param radius : const int: The value of the variable to be set
	* @return int : The sucess or failure of the function
	*/
	int setRadius(int radius);


};
