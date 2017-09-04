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
 * $LastChangedDate: 2007-06-01 11:16:10 +0530 (Fri, 01 Jun 2007) $
 * $Revision: 105 $
 * $Author: sharmnid $
 *
 ************************************************************************/

#include "NNShapeRecognizer.h"
#include "NNAdapt.h"
#include "LTKLoggerUtil.h"
#include "LTKConfigFileReader.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKPreprocDefaults.h"

#define ALPHA_MORPH -0.1

LTKAdapt* LTKAdapt::adaptInstance = NULL;
int LTKAdapt::m_count = 0;

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-Aug-2007
* NAME			: Constructor
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
LTKAdapt::LTKAdapt(NNShapeRecognizer* ptrNNShapeReco)
{
	m_nnShapeRecognizer = ptrNNShapeReco;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
			<< "Exit LTKAdapt::LTKAdapt()"<<endl;

	//Assign Default Values
	m_adaptSchemeName = NAME_ADD_LVQ_ADAPT_SCHEME;
}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-Aug-2007
* NAME			: Destructor
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
LTKAdapt::~LTKAdapt()
{

}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 8-Oct-2007
* NAME			: deleteInstance
* DESCRIPTION	: delete AdaptInstance
* ARGUMENTS		:
* RETURNS		: None
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void LTKAdapt::deleteInstance()
{
	m_count = 0;
	if(adaptInstance)
	{
		delete adaptInstance;
		adaptInstance = NULL;
	}
}
/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-Aug-2007
* NAME			: getInstance
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
LTKAdapt* LTKAdapt::getInstance(NNShapeRecognizer* ptrNNShapeReco)
{
	if(adaptInstance == NULL)
	{
		adaptInstance = new LTKAdapt(ptrNNShapeReco);
	}

	return adaptInstance;

}
/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-Aug-2007
* NAME			: Process
* DESCRIPTION	: Performs adaptation
* ARGUMENTS		:
* RETURNS		: Success : If completed successfully
*                 Failure : Error Code
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int LTKAdapt::adapt(int shapeId)
{
	int iErrorCode;
	if(m_count==0)
	{
		m_count = 1;

		iErrorCode = readAdaptConfig();
		if(iErrorCode !=0)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				<< "Error during LTKAdapt::readAdaptConfig()"<<endl;
			LTKReturnError(FAILURE);
		}
	}

	if(LTKSTRCMP(m_adaptSchemeName.c_str(),NAME_ADD_LVQ_ADAPT_SCHEME)==0)
	{
		iErrorCode = adaptAddLVQ(shapeId);
		if(iErrorCode!=SUCCESS)
		{
			LTKReturnError(iErrorCode);
		}
	}
	else
	{
		//Adapt Scheme not supported
		LOG( LTKLogger::LTK_LOGLEVEL_ERR)
						<<"AdaptScheme not supported: " <<m_adaptSchemeName
						<<endl;
		LTKReturnError(EADAPTSCHEME_NOT_SUPPORTED);
	}

	return(SUCCESS);
}

/******************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 07-06-2007
* NAME			: AdaptADDLVQ
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		: Success
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int LTKAdapt::adaptAddLVQ(int shapeId)
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)
						<<"Enter NNShapeRecognizer::adaptAddLVQ"
						<<endl;
	int errorCode;
	try
	{

		//Check if Cached variables are valid
		if(m_nnShapeRecognizer->m_neighborInfoVec.size()==0)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
							<<"DistanceIndexPair is empty"<<endl;

			LTKReturnError(ENEIGHBOR_INFO_VECTOR_EMPTY );
		}

		//Check if Cached variables are valid
		//Comment to allow addition/adaptation if Resultvec is empty as sample rejected
		/*if(m_nnShapeRecognizer->m_vecRecoResult.size()==0)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
							<<"Result Vector is empty"<<endl;

			LTKReturnError(ERECO_RESULT_EMPTY);
		}*/

		if(m_nnShapeRecognizer->m_cachedShapeSampleFeatures.getFeatureVector().size()>0)
		{
			m_nnShapeRecognizer->m_cachedShapeSampleFeatures.setClassID(shapeId);
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
							<<"Features of input TraceGroup is empty"<<endl;

			LTKReturnError(ESHAPE_SAMPLE_FEATURES_EMPTY);
		}


		//If mismatch,then add, else Morph with 0.1
		double alpha = ALPHA_MORPH;
		if(m_nnShapeRecognizer->m_vecRecoResult.size()==0 ||
			m_nnShapeRecognizer->m_vecRecoResult.at(0).getShapeId()
                        != shapeId ||
                        m_nnShapeRecognizer->m_shapeIDNumPrototypesMap[shapeId]
                        < m_minNumberSamplesPerClass)
		{
			m_nnShapeRecognizer->insertSampleToPrototypeSet(
										m_nnShapeRecognizer->m_cachedShapeSampleFeatures
										);
			//Update m_shapeIDNumPrototypesMap
			m_nnShapeRecognizer->m_shapeIDNumPrototypesMap[shapeId]=
				m_nnShapeRecognizer->m_shapeIDNumPrototypesMap[shapeId] + 1;
		}
		else
		{
			LTKShapeSample recognizedClassNearestSample;
			int nearestSampleIndex = 0;
			NNShapeRecognizer::NeighborInfo distindexPairObj;

			// Morph with Nearest Sample of Recognized Class
			for(int index =0;index <m_nnShapeRecognizer->m_neighborInfoVec.size();index++)
			{
				distindexPairObj = m_nnShapeRecognizer->m_neighborInfoVec.at(index);
				if(distindexPairObj.classId ==m_nnShapeRecognizer->m_vecRecoResult.at(0).getShapeId())
				{
					nearestSampleIndex = distindexPairObj.prototypeSetIndex;
					recognizedClassNearestSample = m_nnShapeRecognizer->m_prototypeSet.at(nearestSampleIndex);
					break;
				}
			}
			errorCode = m_nnShapeRecognizer->morphVector(
										m_nnShapeRecognizer->m_cachedShapeSampleFeatures,
										alpha,
										recognizedClassNearestSample);
			if(errorCode!=0)
			{
				return(errorCode);
			}

			//Update PrototypeSet with Morph Vector
			const vector<LTKShapeFeaturePtr>& tempFeatVec = recognizedClassNearestSample.getFeatureVector();
			m_nnShapeRecognizer->m_prototypeSet.at(nearestSampleIndex).setFeatureVector(tempFeatVec);
		}
		//Update MDT File
		m_nnShapeRecognizer->writePrototypeSetToMDTFile();

	}
	catch(...)
	{
//		return FALSE;
            return false;
	}

	LOG(LTKLogger::LTK_LOGLEVEL_INFO)
						<<"Exit NNShapeRecognizer::adaptAddLVQ"
						<<endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 11-06-2007
* NAME			: readAdaptConfig
* DESCRIPTION	:
*
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int LTKAdapt::readAdaptConfig()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
				<<"Enter Adapt::readAdaptConfig"
				<<endl;

	LTKConfigFileReader *adaptConfigReader = NULL;

	adaptConfigReader = new LTKConfigFileReader(m_nnShapeRecognizer->m_nnCfgFilePath);

	//Don't throw Error as ShapeRecognizer might not support ADAPT
	string tempStringVar = "";

	int errorCode = adaptConfigReader->getConfigValue(ADAPT_SCHEME,tempStringVar);
	if(errorCode == SUCCESS)
		m_adaptSchemeName = tempStringVar;
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)
			<< "AdaptScheme not specified. Assuming default(AddLVQ) " <<endl;
	}

        int tempIntegerVar = 0;

        errorCode =
            adaptConfigReader->getConfigValue(ADAPT_MIN_NUMBER_SAMPLES_PER_CLASS,tempStringVar);
        if(errorCode == SUCCESS)
        {
            if ( LTKStringUtil::isInteger(tempStringVar) )
            {
                tempIntegerVar = atoi((tempStringVar).c_str());
                if(tempIntegerVar > 0)
                {
                    m_minNumberSamplesPerClass = tempIntegerVar;
                }
                else
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<< "Error: " << ECONFIG_FILE_RANGE
                        <<ADAPT_MIN_NUMBER_SAMPLES_PER_CLASS << " is out of permitted range" 
                        << " LTKAdapt::readAdaptConfig()"<<endl;

                    delete adaptConfigReader;

                    LTKReturnError(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<< "Error: " << ECONFIG_FILE_RANGE
                    << ADAPT_MIN_NUMBER_SAMPLES_PER_CLASS << " is out of permitted range" 
                    << " LTKAdapt::readAdaptConfig()"<<endl;

                delete adaptConfigReader;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            m_minNumberSamplesPerClass = ADAPT_DEF_MIN_NUMBER_SAMPLES_PER_CLASS;
            LOG(LTKLogger::LTK_LOGLEVEL_INFO) << "Info: " <<
                "Using default value of MinimumNumerOfSamplesPerClass: "<<
                m_minNumberSamplesPerClass <<
                " LTKAdapt::readAdaptConfig()"<<endl;
        }

	if(adaptConfigReader)
			delete adaptConfigReader;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
							<<"Exit Adapt::readAdaptConfig"
							<<endl;

	return SUCCESS;
}

