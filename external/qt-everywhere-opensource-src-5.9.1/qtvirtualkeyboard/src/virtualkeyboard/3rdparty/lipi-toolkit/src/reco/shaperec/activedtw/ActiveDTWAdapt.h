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
* $LastChangedDate: 2009-04-06 11:55:15 +0530 (Mon, 06 Apr 2009) $
* $Revision: 758 $
* $Author: royva $
*
************************************************************************/

/************************************************************************
* FILE DESCR: Implements ActiveDTWShapeRecognizer::Adapt
*
* CONTENTS:   
*
* AUTHOR:     S Anand
*
* DATE:       3-MAR-2009
* CHANGE HISTORY:
* Author		Date			Description 
************************************************************************/
#pragma once

#include "ActiveDTWShapeRecognizer.h"
#include "LTKLoggerUtil.h"
#include "LTKConfigFileReader.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKPreprocDefaults.h"

#define TRAINSINGLETONFACTOR 2

class ActiveDTWShapeRecognizer;

class LTKAdapt
{
private:
	/** @name Constructor */
	LTKAdapt(ActiveDTWShapeRecognizer* ptrActiveDTWShapeReco);
	
	
	static LTKAdapt* adaptInstance;
	
	static int m_count;
	/**< @brief 
	*     <p>
	*     Initially m_count = 0, when adapt is called on first sample
	*     it checks if m_count = 0, if yes it calls readAdaptConfig and then increments m_count to 1
	*     Thus readAdaptConfig is called only once
	*     </p>
	*/
	
public:	
	static LTKAdapt* getInstance(ActiveDTWShapeRecognizer* ptrActiveDTWShapeReco);
	
	/**
	* Adapts the shapeId
	*
	*	Semantics
	*	
	*	- Reads the internal classifier config information incase of first sample to be adapted
	
	  - Checks if the sample to be adapted was correctly classified or incorrectly classified
	  
		- Incase of correct classification
		--Check if the shape was closest to the cluster or singletons
		--If sample to be adapted was closest to the clusters, and cluster size does not exceed 
		m_maxClusterSize, then call adaptCluster
		--It sample to be adapted was closest to the singleton, call adaptSingleton
		
		  -Incase of incorrect classification
		  --If the sample to be adapted was closest to a cluster, call adaptCluster
		  --If the sample to be adapted was closest to a singleton, call adaptSingleton
		  *
		  * @param shapeId : shape to be adapted
		  * @return SUCCESS : if the shapeId was adapted successfully
		  *         ErrorCode: if some error occurs
		  * @exception ENEIGHBOR_INFO_VECTOR_EMPTY : ActiveDTWShapeRecognizer::m_neighbofInfoVec is empty
		  * @exception ESHAPE_SAMPLE_FEATURES_EMPTY : ActiveDTWShapeRecognizer::m_cachedShapeFeature is empty
	*/
	int adapt(int shapeId);
	
	/** @name Destructor */
	~LTKAdapt();
	
	void deleteInstance();
	
private:
	
/**< @brief Pointer to ActiveDTWShapeRecognizer
*	<p>
*
*	</p>
	*/
	ActiveDTWShapeRecognizer* m_activedtwShapeRecognizer;
	
	//the maximum number of samples in a cluster
	int m_maxClusterSize;
	/**< @brief Maximum Cluster Size
	*	<p>
	*       Specifies the maximum number of samples that can be present in a cluster 
	*       It must be >= the ActiveDTWShapeRecognizer::m_minClusterSize 
	*	</p>
	*/
	
	
	/**
	* This method reads Config variables related to Adapt from CFG
	*
	* Semantics
	*
	*
	* @param none
	*
	* @return SUCCESS:  
	*         FAILURE:  return ErrorCode
	* @exception none
	*/
	int readAdaptConfig();
	
	/**
	* Adapts the cluster with the new shapeFeature
	*
	*	Semantics
	*	
	*	- Recomputes the eigen values, eigen vectors and cluster mean, 
	*	  using the old values and the new shape feature 
	*
	* @param featureVecToAdapt : shapeFeature
	* @param clusterId : cluster to be adapted
	* @param shapeId : shape to be adapted
	* @return SUCCESS : if the shapeId was adapted successfully
	*         ErrorCode: if some error occurs
	* @exception EINVALID_SHAPEID
	* @exception EINVALID_CLUSTER_ID
	*/
	int adaptCluster(shapeFeature& featureVecToAdapt,int clusterId,int shapeId);
	
	/**
	* Adapts the set of singletons with the new shapeFeature
	*
	*	Semantics
	*	
	*	- Adds the new shapeFeature to the current set of singletons
	*	  
	*      - If the number of singletons exceeds a certain threshold train the singletons
	*
	* @param featureVecToAdapt : shapeFeature
	* @param shapeId : shape to be adapted
	* @return SUCCESS : if the shapeId was adapted successfully
	*         ErrorCode: if some error occurs
	* @exception EINVALID_SHAPEID
	*/
	int adaptSingleton(shapeFeature& featureVecToAdapt,int shapeId);
	
	/**
	* Performs training on the set of singletons
	*
	*	Semantics
	*	
	*	- performs clustering on the singletons, resulting in new clusters and singleton set
	*	  
	*      - cluster and singleton information are added to the shape model
	*
	* @param singletons : shapeMatrix
	* @param shapeId : shape to be adapted
	* @param index : index in ActiveDTWShapeRecognizer::m_prototypeShapes which holds the shapeModel information
	* @return SUCCESS : if the shapeId was adapted successfully
	*         ErrorCode: if some error occurs
	* @exception EINVALID_SHAPEID : shapeId specified is Invalid
	* @exception EPROTOYPESHAPE_INDEX_OUT_OF_BOUND ; index value specified is Invalid
	* @exception EEMPTY_EIGENVECTORS ; eigen vector dimension is < 0
	* @exception EINVALID_NUM_OF_EIGENVECTORS : number of eigen vectors < 0
	*/
	int trainSingletons(const shapeMatrix &singletons,int shapeId,int index);
};
