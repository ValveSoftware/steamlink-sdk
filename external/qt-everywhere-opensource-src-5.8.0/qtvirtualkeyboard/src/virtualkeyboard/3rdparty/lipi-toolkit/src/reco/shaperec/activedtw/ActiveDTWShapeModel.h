/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies 
* or substantial portions of the Software.
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
* $LastChangedDate: 2009-04-06 11:55:15 +0530 (Mon, 06 Apr 2009) $
* $Revision: 758 $
* $Author: royva $
*
************************************************************************/
/************************************************************************
* FILE DESCR: Definitions for ActiveDTW Shape Recognition module
*
* CONTENTS:
*
* AUTHOR:     
*
* DATE:       
* CHANGE HISTORY:
* Author		Date			Description of change	
************************************************************************/
#ifndef ACTIVEDTWSHAPEMODEL_H
#define ACTIVEDTWSHAPEMODEL_H
#include <iostream>
#include "LTKTypes.h"
#include "ActiveDTWClusterModel.h"
#include "LTKShapeFeatureMacros.h"
#include "LTKShapeFeature.h"
#include "LTKInc.h"

using namespace std;

typedef vector<LTKShapeFeaturePtr> shapeFeature;
typedef vector<shapeFeature> shapeMatrix;

/**
* @ingroup ActiveDTWShapeModel.h
* @brief The Header file for the ActiveDTWShapeModel
* @class ActiveDTWShapeModel
*<p> <p>
*/
class ActiveDTWShapeModel
{
private:
	int m_shapeId;
	/** @brief shape id of the class
	*     <p>
	*     It specifies a specific shape id to each shape model 
	*     </p>
	*/
	
	vector<ActiveDTWClusterModel> m_clusterModelVector;
	/**< @brief vector of cluster models
	*     <p>
	*     Contains the information of all the clusters of the class
	*     </p>
	*/
	
	shapeMatrix m_singletonVector;
	/**< @brief singletons /free samples of the class
	*     <p>
	*     Contains all the singleton vectors of the class
	*     </p>
	*/
	
public:
	
	/** @name Constructors and Destructor */
	ActiveDTWShapeModel();
	
	~ActiveDTWShapeModel(); 
	
	/**
	* Sets the shapeId of the class
	* @param shapeId 
	* @return SUCCESS : if the shapeId was set successfully
	* @exception EINVALID_SHAPEID
	*/
	int setShapeId(const int shapeId);
	
	/**
	* Sets the clusterModelVector of the class
	* @param clusterModelVector : vector<ActiveDTWClusterModel> 
	* @return NULL
	* @exception None
	*/
	void setClusterModelVector(const vector<ActiveDTWClusterModel>& clusterModelVector);
	
	/**
	* Sets the singleton vector of the class
	* @param singletonVector : shapeMatrix
	* @return NULL
	* @exception None 
	*/
	void setSingletonVector(const shapeMatrix& singletonVector);
	
	/**
	* Returns the shapeId of the class
	* @param None
	* @return shapeId
	* @exception None
	*/
	int getShapeId() const;
	
	/**
	* Returns the clusterModelVector of the class
	* @param None
	* @return vector<ActiveDTWClusterModel>
	* @exception None
	*/
	const vector<ActiveDTWClusterModel>& getClusterModelVector() const;
	
	/**
	* Returns the singletonVector of the class
	* @param None
	* @return shapeMatrix
	* @exception None
	*/
	const shapeMatrix& getSingletonVector() const ;
};

#endif
