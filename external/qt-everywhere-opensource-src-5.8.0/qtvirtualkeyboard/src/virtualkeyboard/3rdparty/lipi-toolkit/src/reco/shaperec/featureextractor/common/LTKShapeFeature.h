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
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: function prototype for the LTKShapeFeature class
 *
 * CONTENTS:
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       07-FEB-2007
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKSHAPEFEATURE_H
#define __LTKSHAPEFEATURE_H
#include "LTKTypes.h"
#include "LTKException.h"
#include "LTKErrorsList.h"
#include "LTKMacros.h"
#include "LTKErrors.h"
#include "LTKShapeFeatureMacros.h"

/**
 * \defgroup feature_extractor The feature extractor module
 * <p>
 * The feature extractor module consists of following directories
 *  - common
 *      - Builds as a static library called <b> featureextractorcommon.lib</b>
 *
 *  - Various feature extractors viz PointFloatShapeFeatureExtractor
 *      - These feature extractor are built as DLLs
 *
 * User can specify the feature extractor name in the classifier config file 
 * and the corresponding extractor is instantiated at run-time.
 *
 * </p>
 */

/** @ingroup feature_extractor
* @brief An abstract class that defines the interface for a class representing a feature.
* @class LTKShapeFeature
* The LTKShapeFeature is the abstract base class. Any class representing a shape feature
* is derived from the LTKShapeFeature class.
*/
class LTKShapeFeature
{
public:
	/** @name Constructors and Destructor */
	//@{

	/**
	* Default Constructor.
	*/
	
	LTKShapeFeature(){};

	/**
	* Virtual destructor.
	*/
	virtual ~LTKShapeFeature()
	{
	};
	//@}

	/** @Purely cirtual functions */
	//@{
	
	/**
	* <b>Responsibility</b><br>
	* This function initializes the instance of type LTKShapeFeature using the initialization string
	* passed as parameter.
	*
	* @param initString : string& : reference to the initialization string.
	*/

    virtual int initialize(const string& initString)=0;

    virtual int initialize(const floatVector& initFloatVector)=0;

    virtual int initialize(floatVector::const_pointer initFloatData, size_t dataSize) { (void)initFloatData; (void)dataSize; return SUCCESS; }

	/**
	* <b>Responsibility</b><br>
	* This function returns the character (char*) representation for the instance of type LTKShapeFeature.
	*
	* @param p1 : char* : Pointer to the character array.
	*/

    virtual void toString(string& strFeat) const =0;

	/**
	* <b>Responsibility</b><br>
	* Creates a new instance of type LTKShapeFeature and initializes it with the calling instance.
	*
	* @param none.
	*
	* @return LTKShapeFeature* Pointer to the LTKShapeFeature instance
	*/

	virtual LTKShapeFeaturePtr clone() const =0;

	/**
	* <b>Responsibility</b><br>
	* Returns the distance between two instances of LTKShapeFeature.
	*
	* @param shapeFeaturePtr : LTKShapeFeature* : Pointer to LTKShapeFeature
	*
	* @return distance (float)  Distance between two instances of LTKShapeFeature class.
	*/

    virtual void getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const = 0;

	//@}

	/**
	* <b>Responsibility</b><br>
	* Returns the representation of LTKShapeFeature instance as a vetor of floats.
	*
	* @param floatVec : vector<float>& : Reference to the vector of floats.
	*
	* @return FAILURE If no implementation is provided by the derived class
	*/
    virtual int toFloatVector(vector<float>& floatVec) { return FAILURE; }

	/**
	* <b>Responsibility</b><br>
	* Returns the representation of LTKShapeFeature instance as a vetor of integers.
	*
	*
	* @param intVec : vector<float>& : Reference to the vector of integers.
	*
	* @return FAILURE If no implementation is provided by the derived class
	*/
    virtual int toIntVector(vector<int>& intVec) { return FAILURE; }

	//ADAPTATION REQUIREMENTS
	
	/**
	* <b>Responsibility</b><br>
	* Adds two instances of type LTKShapeFeature.
	*
	* @param secondFeature : LTKShapeFeature* : Pointer to LTKShapeFeature
	*
	* @return LTKShapeFeature* : The pointer to LTKShapeFeature holding the result of adding
	*
	* @exception  LTKErrorsList::EFTR_RPRCLASS_NOIMPLEMENTATION   If no implementation is provided by the derived class
	*/

    virtual int addFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const
	{
		LTKReturnError(EFTR_RPRCLASS_NOIMPLEMENTATION);
	}

	/**
	* <b>Responsibility</b><br>
	* Subtract two instances of LTKShapeFeature.
	*
	*
	* @param secondFeature : LTKShapeFeature* : Pointer to LTKShapeFeature
	*
	* @return LTKShapeFeature* : The pointer to LTKShapeFeature holding the result of subtraction
	*
	* @exception  LTKErrorsList::EFTR_RPRCLASS_NOIMPLEMENTATION   If no implementation is provided by the derived class
	*/

    virtual int subtractFeature(const LTKShapeFeaturePtr& secondFeature, LTKShapeFeaturePtr& outResult ) const
	{
		LTKReturnError(EFTR_RPRCLASS_NOIMPLEMENTATION);
	}

	/**
	* <b>Responsibility</b><br>
	* Scales the instance of LTKShapeFeature by the floating point number alpha passed as parameter.
	*
	*
	* @param secondFeature : LTKShapeFeature* : Pointer to LTKShapeFeature
	*
	* @return LTKShapeFeature* : The pointer to scaled LTKShapeFeature
	*
	* @exception  LTKErrorsList::EFTR_RPRCLASS_NOIMPLEMENTATION   If no implementation is provided by the derived class
	*/

    virtual int scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const
	{
		LTKReturnError(EFTR_RPRCLASS_NOIMPLEMENTATION);
	}

    virtual int getFeatureDimension() = 0;
    
	virtual bool isPenUp() const 	
	{
		LTKReturnError(EFTR_RPRCLASS_NOIMPLEMENTATION);
	}

};


#endif
