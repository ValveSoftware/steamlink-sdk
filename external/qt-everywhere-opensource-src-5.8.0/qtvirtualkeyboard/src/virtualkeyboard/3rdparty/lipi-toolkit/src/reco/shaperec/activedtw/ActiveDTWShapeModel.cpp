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
* FILE DESCR: Implementation for ActiveDTW Shape Model. Used to store shape model information
*
* CONTENTS:
*
* AUTHOR: S Anand
*
* DATE:3-MAR-2009
* CHANGE HISTORY:
* Author       Date            Description of change
***********************************************************************************************/

#include "ActiveDTWShapeModel.h"

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: ActiveDTWShapeModel
* DESCRIPTION	        : Default Constructor 
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
ActiveDTWShapeModel::ActiveDTWShapeModel()
{
	
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setShapeId
* DESCRIPTION	        : sets the shape id for the shape model
* ARGUMENTS		: INPUT: shapeId
* RETURNS		: SUCCESS - on successfully setting the shape id
: ErrorCode - otherwise
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeModel::setShapeId(const int shapeId)
{
	if(shapeId >= 0)
	{
		m_shapeId = shapeId;
	}
	else
	{
		LTKReturnError(EINVALID_SHAPEID);
	}
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setClusterModelVector
* DESCRIPTION	        : sets the vector of clusters for the ActiveDTWShapeModel
* ARGUMENTS		: INPUT: clusterModelVector
* RETURNS		: NONE 
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void ActiveDTWShapeModel::setClusterModelVector(const vector<ActiveDTWClusterModel>& clusterModelVector)
{
	m_clusterModelVector = clusterModelVector;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setSingletonVector
* DESCRIPTION	        : sets the vector of singletons for the shape model
* ARGUMENTS		: INPUT: shapeId
* RETURNS		: NONE
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void ActiveDTWShapeModel::setSingletonVector(const shapeMatrix& singletonVector)
{
	m_singletonVector = singletonVector;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getShapeId
* DESCRIPTION	        : returns the shapeId of the model
* ARGUMENTS		: INPUT: NULL
* RETURNS		: shapeId
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeModel::getShapeId() const
{
	return m_shapeId;
}

/*************************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getClusterModelVector
* DESCRIPTION	        : returns the clusters model vector
* ARGUMENTS		: INPUT: NULL
* RETURNS		: clusterModelVector
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
const vector<ActiveDTWClusterModel>& ActiveDTWShapeModel::getClusterModelVector() const
{
	return m_clusterModelVector;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getSingletonVector
* DESCRIPTION	        : returns the set of singleton vectors
* ARGUMENTS		: INPUT: NULL
* RETURNS		: shapeMatrix
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
const shapeMatrix& ActiveDTWShapeModel::getSingletonVector() const
{
	return m_singletonVector;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: ~ActiveDTWShapeModel
* DESCRIPTION	        : Destructor
* ARGUMENTS		: NONE
* RETURNS		: NONE
* NOTES	        :
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
ActiveDTWShapeModel::~ActiveDTWShapeModel()
{
	
}
