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
* $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
* $Revision: 827 $
* $Author: mnab $
*
************************************************************************/
/*********************************************************************************************
* FILE DESCR: Definitions for ActiveDTWClusterModel Class
*
* CONTENTS:
*
* AUTHOR: S Anand
* DATE:3-MAR-2009
* CHANGE HISTORY:
* Author       Date            Description of change
***********************************************************************************************/


#ifndef ACTIVEDTWCLUSTERMODEL_H
#define ACTIVEDTWCLUSTERMODEL_H

#include <iostream>
#include "LTKTypes.h"
#include "LTKInc.h"
#include "LTKMacros.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"

using namespace std;

typedef vector<double> doubleVector;
typedef vector<doubleVector> double2DVector;

/**
* @ingroup ActiveDTWShapeModel.h
* @brief The Header file for the ActiveDTWShapeModel
* @class ActiveDTWShapeModel
*<p> <p>
*/
class ActiveDTWClusterModel
{
private:
	int m_numberOfSamples;
	/** @brief Number of samples in the cluster 
	*     <p>
	*     Specifies the number of samples in the cluster 
	*     </p>
	*/
	
	doubleVector m_eigenValues;
	/** @brief Eigen Values
	*     <p>
	*     Eigen values of the cluster covariance matrix 
	*     </p>
	*/
	
	double2DVector m_eigenVectors;
	/** @brief Eigen Vectors
	*     <p>
	*     Eigen vectors of the cluster covariance matrix
	*     </p>
	*/
	
	doubleVector m_clusterMean;
	/** @brief Cluster mean
	*     <p>
	*     Mean of all the samples forming the cluster 
	*     </p>
	*/
	
public:
	
	/** @name Constructors and Destructor */
	ActiveDTWClusterModel();
	
	~ActiveDTWClusterModel();
	
	/**
	* Sets the number of samples in the cluster 
	* @param numSamples
	* @return SUCCESS : if the number of samples was set successfully
	* @exception EINVALID_SHAPEID
	*/
	int setNumSamples(int numSamples);
	
	/**
	* Sets the eigen values of the cluser
	* @param eigVal
	* @return SUCCESS : if the number of samples was set successfully
	* @exception EINVALID_SHAPEID
	*/
	void setEigenValues(const doubleVector& eigVal);
	
	/**
	* Sets the eigen vectors of the cluster
	* @param eigVec
	* @return SUCCESS : if the number of samples was set successfully
	* @exception EINVALID_SHAPEID
	*/
	void setEigenVectors(const double2DVector& eigVec);
	
	/**
	* Sets the mean of the cluster 
	* @param clusterMean
	* @return SUCCESS : if the number of samples was set successfully
	* @exception EINVALID_SHAPEID
	*/
	void setClusterMean(const doubleVector& clusterMean);
	
	/**
	* Returns the number of samples in the cluster 
	* @return number of samples
	* @exception EINVALID_SHAPEID
	*/
	int getNumSamples() const;
	
	/**
	* Returns the eigen values of the cluster
	* @return eigen values 
	* @exception EINVALID_SHAPEID
	*/
	const doubleVector& getEigenValues() const;
	
	/**
	* Returns the eigen vectors of the cluster
	* @return eigen vectors 
	* @exception EINVALID_SHAPEID
	*/
	const double2DVector& getEigenVectors() const;
	
	/**
	* Returns the mean of the cluster
	* @return cluster mean
	* @exception EINVALID_SHAPEID
	*/
	const doubleVector& getClusterMean() const;
	
	
};
#endif
