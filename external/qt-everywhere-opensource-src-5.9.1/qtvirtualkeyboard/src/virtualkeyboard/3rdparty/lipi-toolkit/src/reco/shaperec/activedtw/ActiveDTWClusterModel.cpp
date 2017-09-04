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
/*********************************************************************************************
* FILE DESCR: Implementation for ActiveDTW Cluster Model. Used to store cluster model information
*
* CONTENTS:
*
* AUTHOR: S Anand
*
w
* DATE:3-MAR-2009
* CHANGE HISTORY:
* Author       Date            Description of change
***********************************************************************************************/

#include "ActiveDTWClusterModel.h"

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: ActiveDTWClusterModel
* DESCRIPTION	        : Default Constructor 
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
ActiveDTWClusterModel::ActiveDTWClusterModel()
{
	
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setNumSamples
* DESCRIPTION	        : sets the number of samples in the cluster
* ARGUMENTS		: INPUT: numSamples
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWClusterModel::setNumSamples(int numSamples)
{
	if(numSamples > 0)
	{
		m_numberOfSamples = numSamples;
	}
	else
	{
		LTKReturnError(EINVALID_NUM_SAMPLES);
	}
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getNumSamples
* DESCRIPTION	        : returns the number of samples in a cluster 
* ARGUMENTS		: none
* RETURNS		: number of samples
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWClusterModel::getNumSamples() const
{
	return m_numberOfSamples;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setEigenValues
* DESCRIPTION	        : sets the eigen values of the cluster 
* ARGUMENTS		: INPUT: eigen values
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void ActiveDTWClusterModel::setEigenValues(const doubleVector& eigVal)
{
	m_eigenValues = eigVal;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setClusterMean
* DESCRIPTION	        : sets the cluster mean of the cluster
* ARGUMENTS		: INPUT: clusterMean
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void ActiveDTWClusterModel::setClusterMean(const doubleVector& clusterMean)
{
	m_clusterMean = clusterMean;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getEigenValues
* DESCRIPTION	        : returns the eigen values of the cluster 
* ARGUMENTS		: none
* RETURNS		: eigen values
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
const doubleVector& ActiveDTWClusterModel::getEigenValues() const 
{
	return m_eigenValues;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setEigenVectors
* DESCRIPTION	        : sets the eigen vectors of the cluster
* ARGUMENTS		: eigen vectors
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void ActiveDTWClusterModel::setEigenVectors(const double2DVector& eigVec)
{
	m_eigenVectors = eigVec;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getEigenVectors
* DESCRIPTION	        : returns the eigen vectors of the cluster
* ARGUMENTS		: none
* RETURNS		: eigen vectors
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
const double2DVector& ActiveDTWClusterModel::getEigenVectors() const
{
	return m_eigenVectors;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getClusterMean
* DESCRIPTION	        : returns the cluster mean of the cluster 
* ARGUMENTS		: none
* RETURNS		: cluster mean
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
const doubleVector& ActiveDTWClusterModel::getClusterMean() const
{
	return m_clusterMean;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: ~ActiveDTWClusterModel
* DESCRIPTION	        : Default Destructor
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
ActiveDTWClusterModel::~ActiveDTWClusterModel()
{
	
}

