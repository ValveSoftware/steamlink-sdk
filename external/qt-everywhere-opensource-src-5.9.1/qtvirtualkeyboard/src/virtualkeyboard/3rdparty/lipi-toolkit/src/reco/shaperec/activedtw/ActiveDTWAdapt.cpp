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
* FILE DESCR: Definitions for ActiveDTW Adaptation module
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

#include "ActiveDTWShapeRecognizer.h"
#include "ActiveDTWAdapt.h"
#include "LTKLoggerUtil.h"
#include "LTKConfigFileReader.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKPreprocDefaults.h"

LTKAdapt* LTKAdapt::adaptInstance = NULL;
int LTKAdapt::m_count = 0;

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: Constructor
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
LTKAdapt::LTKAdapt(ActiveDTWShapeRecognizer* ptrActiveDTWShapeReco)
{
	m_activedtwShapeRecognizer = ptrActiveDTWShapeReco;
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<< "Exit LTKAdapt::LTKAdapt()"<<endl;
	
	//Assign Default Values
	
	m_maxClusterSize = ADAPT_DEF_MAX_NUMBER_SAMPLES_PER_CLASS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
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
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
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
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getInstance
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
LTKAdapt* LTKAdapt::getInstance(ActiveDTWShapeRecognizer* ptrActiveDTWShapeReco)
{
	if(adaptInstance == NULL)
	{
		adaptInstance = new LTKAdapt(ptrActiveDTWShapeReco);
	}
	
	return adaptInstance;
	
}
/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
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
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<< "Enter LTKAdapt::adapt()"<<endl;
	
	int iErrorCode;
	
	//read config file values when first adapt sample is encountered
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
	
	
	//Check if Cached variables are valid
	if(m_activedtwShapeRecognizer->m_neighborInfoVec.size()==0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"DistanceIndexPair is empty"<<endl;
		
		LTKReturnError(ENEIGHBOR_INFO_VECTOR_EMPTY );
	}
	
	//check if test featureVector is empty
	if(m_activedtwShapeRecognizer->m_cachedShapeFeature.size() <= 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Features of input TraceGroup is empty"<<endl;
		
		LTKReturnError(ESHAPE_SAMPLE_FEATURES_EMPTY);
	}
	
	//find out whether the test sample is close to a singleton or cluster
	
	//case of incorrect classification
	if(m_activedtwShapeRecognizer->m_vecRecoResult.size() == 0 || 
		m_activedtwShapeRecognizer->m_vecRecoResult.at(0).getShapeId() != shapeId)
	{
		
		int index = 0;
		//iterating through neighborInfoVec to retrieve information about the true
		//shape model of class 
		while(shapeId != m_activedtwShapeRecognizer->m_neighborInfoVec[index].classId )
			index++;
		
		if(m_activedtwShapeRecognizer->m_neighborInfoVec[index].typeId == CLUSTER)
		{
			int clusterId = m_activedtwShapeRecognizer->m_neighborInfoVec[index].sampleId;
			
			//adapting the cluster	
			iErrorCode = adaptCluster(m_activedtwShapeRecognizer->m_cachedShapeFeature,clusterId,shapeId);
			
			if(iErrorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<iErrorCode<<
					" LTKAdapt::adapt" << endl;
				LTKReturnError(iErrorCode);
			}	
		}
		else
		{
			//adapting the singleton set 
			iErrorCode = adaptSingleton(m_activedtwShapeRecognizer->m_cachedShapeFeature,shapeId);
			if(iErrorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<iErrorCode<<
					" LTKAdapt::adapt" << endl;
				LTKReturnError(iErrorCode);
			}
		}
	}
	else
	{
		//case of correct classification
		if(m_activedtwShapeRecognizer->m_neighborInfoVec[0].typeId == CLUSTER)
		{
			int clusterId = m_activedtwShapeRecognizer->m_neighborInfoVec[0].sampleId;
			int iterator = 0;
			while(m_activedtwShapeRecognizer->m_prototypeShapes[iterator].getShapeId() != shapeId)
				iterator++;
			
			ActiveDTWShapeModel shapeModelToAdapt = m_activedtwShapeRecognizer->m_prototypeShapes[iterator];
			
			vector<ActiveDTWClusterModel> currentClusterModelVector = shapeModelToAdapt.getClusterModelVector();
			
			//adapt the model only if number of samples seen by the model is less than the threshold m_maxClustersize	
			if(currentClusterModelVector[clusterId].getNumSamples() < m_maxClusterSize)
			{
				iErrorCode = adaptCluster(m_activedtwShapeRecognizer->m_cachedShapeFeature,clusterId,shapeId);
				
				if(iErrorCode != SUCCESS)
				{
					currentClusterModelVector.clear();
					
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<iErrorCode<<
						" LTKAdapt::adapt" << endl;
					LTKReturnError(iErrorCode);
				}
			}
			currentClusterModelVector.clear();
		}
		else
		{
			//adapt singleton set
			iErrorCode = adaptSingleton(m_activedtwShapeRecognizer->m_cachedShapeFeature,shapeId);
			if(iErrorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<iErrorCode<<
					" LTKAdapt::adapt" << endl;
				LTKReturnError(iErrorCode);
			}
		}
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<< "Exit LTKAdapt::adapt()"<<endl;
	
	return(SUCCESS);
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: readAdaptConfig
* DESCRIPTION	        : Reads configuration info for adaptation
* ARGUMENTS		: NONE
* RETURNS		: NONE
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
	
	adaptConfigReader = new LTKConfigFileReader(m_activedtwShapeRecognizer->m_activedtwCfgFilePath);
	
	
	//Don't throw Error as ShapeRecognizer might not support ADAPT
	string tempStringVar = "";
	int tempIntegerVar;
	
	int errorCode = adaptConfigReader->getConfigValue(MAXCLUSTERSIZE,tempStringVar);
	
	if(errorCode == SUCCESS)
	{
		if(LTKStringUtil::isInteger(tempStringVar))
		{
			tempIntegerVar = atoi((tempStringVar).c_str());
			
			if(tempIntegerVar > 1)
			{
				if(tempIntegerVar < m_activedtwShapeRecognizer->m_minClusterSize)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
						"Error: "<< ECONFIG_FILE_RANGE << m_maxClusterSize <<
						" is out of permitted range MAXCUSTERSIZE < MINCLUSTERSIZE" <<
						" LTKAdapt::readAdaptConfig"<<endl;
					LTKReturnError(ECONFIG_FILE_RANGE);
				}
				else
				{
					m_maxClusterSize = tempIntegerVar;
					LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
						MAXCLUSTERSIZE << " = " <<m_maxClusterSize<<endl;
				}
			}
			else
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
					"Error: "<< ECONFIG_FILE_RANGE << m_maxClusterSize <<
					" is out of permitted range" <<
					" LTKAdapt::readAdaptConfig"<<endl;
				LTKReturnError(ECONFIG_FILE_RANGE);
			}
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
				"Error: "<< ECONFIG_FILE_RANGE << MAXCLUSTERSIZE <<
				" is out of permitted range" <<
				" LTKAdapt::readAdaptConfig"<<endl;
			
			LTKReturnError(ECONFIG_FILE_RANGE);
		}
	}
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
			"Using default value for " << MAXCLUSTERSIZE << " : " << m_maxClusterSize << endl;
	}
	
	
	if(adaptConfigReader)
		delete adaptConfigReader;
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exit LTKAdapt::readAdaptConfig"
		<<endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: adaptCluster
* DESCRIPTION	        : This method adapts the cluster with the new featureVector. Implementation of method described in paper:
"Incremental eigenanalysis for classifiation, published in British Machine Vision Conference-2008" 
* 				
* ARGUMENTS		: INPUT
featureVecToAdapt shapeFeature 
clusterId int 
shapeId int 
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int LTKAdapt::adaptCluster(shapeFeature& featureVecToAdapt,int clusterId,int shapeId)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "LTKAdapt::adaptCluster()" << endl;
	
	int errorCode;
	
	//validating input arguments
	if(m_activedtwShapeRecognizer->m_shapeIDNumPrototypesMap.find(shapeId) == m_activedtwShapeRecognizer->m_shapeIDNumPrototypesMap.end())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_SHAPEID << " " <<
			" LTKAdapt::adaptCluster()" << endl;
		LTKReturnError(EINVALID_SHAPEID);
	}
	
	//finding the prototypeShape
	int index = 0;
	
	int i = 0;
	int j = 0;
	
	while(m_activedtwShapeRecognizer->m_prototypeShapes[index].getShapeId() != shapeId)
		index++;
	
	ActiveDTWShapeModel shapeModelToAdapt = m_activedtwShapeRecognizer->m_prototypeShapes[index];
	
	vector<ActiveDTWClusterModel> currentClusterModelVector = shapeModelToAdapt.getClusterModelVector();
	
	if(clusterId < 0 || clusterId >= currentClusterModelVector.size())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_CLUSTER_ID << " " <<
			" LTKAdapt::adaptCluster()" << endl;
		
		LTKReturnError(EINVALID_CLUSTER_ID);
	}
	
	
	ActiveDTWClusterModel clusterToAdapt = currentClusterModelVector[clusterId];
	
	//obtaining data of cluster
	doubleVector oldEigenValues = clusterToAdapt.getEigenValues();
	
	double2DVector oldEigenVectors = clusterToAdapt.getEigenVectors();
	
	doubleVector oldClusterMean = clusterToAdapt.getClusterMean();
	
	int numClusterSamples = clusterToAdapt.getNumSamples();
	
	//convert the shapefeature to double vector
	floatVector floatFeatureVector;
	
	doubleVector doubleFeatureVector;
	
	errorCode = m_activedtwShapeRecognizer->m_shapeRecUtil.shapeFeatureVectorToFloatVector(featureVecToAdapt,floatFeatureVector);
	
	if( errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
			"LTKAdapt::adaptCluster" <<endl;
		LTKReturnError(errorCode);
	}
	
	int featureSize = floatFeatureVector.size();
	
	for(i = 0; i < featureSize; i++)
		doubleFeatureVector.push_back(floatFeatureVector[i]);
	
	floatFeatureVector.clear();
	
	//the difference vector 
	//refers to y - x(mean)
	doubleVector diffVector;
	
	//refers to g in the paper (Eqn 2)
	//g = unp*(y - x(mean))
	doubleVector projectedTestSample;
	
	//refers to h in the paper  (Eqn 3)
	//h = (y - x(mean)) - (unp * g)
	doubleVector residueVector;
	
	//refers to unp*g
	doubleVector reverseProjection;
	
	int numEigenVectors = oldEigenVectors.size();
	
	//calculating diffVector
	for(i = 0; i < featureSize; i++)
		diffVector.push_back(doubleFeatureVector[i] - oldClusterMean[i]);
	
	//constructing the projected test sample i.e g
	for(i = 0; i < numEigenVectors; i++)
	{
		double tempValue = 0;
		for(j = 0; j < featureSize; j++)
		{
			tempValue += (oldEigenVectors[i][j] * diffVector[j]);
		}
		projectedTestSample.push_back(tempValue);
	}
	
	//constructing reverse projection
	for(i = 0; i < featureSize; i++)
	{
		double tempValue = 0;
		for(j = 0; j < numEigenVectors;j++)
		{
			tempValue += oldEigenVectors[j][i] * projectedTestSample[j];
		}
		reverseProjection.push_back(tempValue);
	}
	
	//construct residue vector
	for(i = 0; i < featureSize; i++)
		residueVector.push_back(diffVector[i] - reverseProjection[i]);
	
	//magnitude of residue vector
	double residueMagnitude = 0;
	
	for(i = 0; i < featureSize; i++)
		residueMagnitude = residueMagnitude + (residueVector[i] * residueVector[i]); 	
	
	residueMagnitude = sqrt(residueMagnitude);
	
	//determining the recomputed eigenValues and eigenVectors
	//case 1 residueMagnitude is 0
	if(residueMagnitude == 0)
	{
		//construct the matrix whose eigen values and eigen vectors are to be calculated
		doubleVector tempVector;
		
		//corresponds to matrix in eqn 10
		double2DVector coeff1;
		
		tempVector.assign(numEigenVectors,0.0);
		
		coeff1.assign(numEigenVectors,tempVector);
		
		for(i = 0; i < numEigenVectors; i++)	
		{
			for(j = 0; j < numEigenVectors; j++)
			{
				if(i == j)
					coeff1[i][j] = oldEigenValues[j];
			}
		}
		
		for(i = 0; i < numEigenVectors; i++)
		{
			for(j = 0; j < numEigenVectors; j++)
				coeff1[i][j] = (coeff1[i][j] * numClusterSamples)/(numClusterSamples + 1);
		}
		
		//refers to eqn 11 in paper
		double2DVector coeff2;
		
		coeff2.assign(numEigenVectors,tempVector);
		
		//constructing g*g(transpose)
		for(i = 0; i < numEigenVectors; i++)
		{
			for(j = 0; j < numEigenVectors; j++)
				coeff2[i][j] = projectedTestSample[i] * projectedTestSample[j];
		}
		
		for(i = 0; i < numEigenVectors; i++)
		{
			for(j = 0; j < numEigenVectors; j++)
				coeff2[i][j] = (coeff2[i][j] * numClusterSamples) / ((numClusterSamples + 1) * (numClusterSamples + 1));
		}
		
		//final matrix
		for(i = 0; i < numEigenVectors; i++)
		{
			for(j = 0; j < numEigenVectors; j++)
				coeff1[i][j] += coeff2[i][j];
		}
		
		//solving the intermediate eigen value problem
		//refers to eigenValue problem in eqn 12
		double2DVector intermediateEigenVectors;
		doubleVector eigenValues;
		int nrot = 0;
		
		intermediateEigenVectors.assign(numEigenVectors,tempVector);
		
		errorCode = m_activedtwShapeRecognizer->computeEigenVectors(coeff1,coeff1.size(),eigenValues,intermediateEigenVectors,nrot);
		
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
				" LTKAdapt::adaptCluster()" << endl;
			LTKReturnError(errorCode);
		}
		
		//the new eigen vectors
		double2DVector eigenVectors;
		
		eigenVectors.assign(featureSize,tempVector);
		
		tempVector.clear();
		
		//calculating the new eigen vectors from the intermediateEigenVectors and oldEigenVectors
		//refers to eqn 8
		for( i = 0; i < featureSize; i++)
		{
			for(j = 0; j < numEigenVectors; j++)
			{
				for(int k = 0; k < numEigenVectors;k++)
				{
					eigenVectors[i][j] += (oldEigenVectors[k][i] * intermediateEigenVectors[k][j]);
				}
			}
		}
		
		//now converting the eigen vectors to row format
		//this makes it easy to write the eigen vectors to the mdt file and read from it
		double2DVector newEigenVectors; 
		
		tempVector.assign(featureSize,0.0);
		
		newEigenVectors.assign(numEigenVectors,tempVector);
		
		for(i = 0; i < numEigenVectors; i++)
		{
			for(j = 0; j < featureSize; j++)
				newEigenVectors[i][j] = eigenVectors[j][i];
		}
		
		numEigenVectors = 0;
		double eigenEnergy = 0;
		for( i = 0; i < eigenValues.size(); i++)
		{
			eigenEnergy += eigenValues[i];
		}
		
		double tempEigenEnergy = 0;
		
		while(tempEigenEnergy <= ((m_activedtwShapeRecognizer->m_percentEigenEnergy * eigenEnergy)/100))
			tempEigenEnergy += eigenValues[numEigenVectors++];
		
		doubleVector selectedEigenValues;
		double2DVector selectedEigenVectors;
		for( i = 0; i < numEigenVectors; i++)
		{
			selectedEigenValues.push_back(eigenValues[i]);
			selectedEigenVectors.push_back(newEigenVectors[i]);
		}
		
		doubleVector newClusterMean;
		
		for( i = 0; i < featureSize; i++)
		{
			double tempValue = ((numClusterSamples * oldClusterMean[i]) + doubleFeatureVector[i])/(numClusterSamples + 1);
			newClusterMean.push_back(tempValue);
		}
		
		//updating the cluster model
		errorCode = clusterToAdapt.setNumSamples(numClusterSamples + 1);
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
				" LTKAdapt::adaptCluster()" << endl;
			
			LTKReturnError(errorCode);
		}
		
		
		clusterToAdapt.setEigenValues(selectedEigenValues);
		selectedEigenValues.clear();
		
		clusterToAdapt.setEigenVectors(selectedEigenVectors);
		selectedEigenVectors.clear();
		
		clusterToAdapt.setClusterMean(newClusterMean);
		
		currentClusterModelVector[clusterId] = clusterToAdapt;
		
		shapeModelToAdapt.setClusterModelVector(currentClusterModelVector);
		
		m_activedtwShapeRecognizer->m_prototypeShapes[index] = shapeModelToAdapt;
		
		//clearing vectors
		tempVector.clear();
		coeff1.clear();
		coeff2.clear();
		eigenValues.clear();
		newEigenVectors.clear();
		intermediateEigenVectors.clear();
		newClusterMean.clear();
		eigenVectors.clear();
	}
	else
	{
		//case residueMagnitude is not 0;
		doubleVector unitResidueVector;
		
		for(i = 0; i < featureSize; i++)
			unitResidueVector.push_back(residueVector[i]/residueMagnitude);
		
		//construct the matrix whose eigen values and eigen vectors are to be calculated
		doubleVector tempVector;
		
		//refer to eqn 10
		double2DVector coeff1;
		
		tempVector.assign(numEigenVectors + 1,0.0);
		
		coeff1.assign(numEigenVectors + 1,tempVector);
		
		for( i = 0; i < numEigenVectors; i++)	
		{
			for(j = 0; j < numEigenVectors; j++)
			{
				if(i == j)
				{
					coeff1[i][j] = oldEigenValues[j];
				}
			}
		}
		
		for( i = 0; i < (numEigenVectors + 1 ); i++)
		{
			for(j = 0; j < (numEigenVectors + 1); j++)
				coeff1[i][j] = (coeff1[i][j] * numClusterSamples)/(numClusterSamples + 1);
		}
		
		//refers to unith(transpose)*diffVector
		double gamma = 0;
		
		for( i = 0; i < featureSize; i++)
			gamma = gamma + (unitResidueVector[i] * diffVector[i]);	
		
		//refers to eqn 11
		double2DVector coeff2;
		
		coeff2.assign(numEigenVectors + 1,tempVector);
		
		//constructing g*g(transpose)
		for( i = 0; i < numEigenVectors; i++)
		{
			for(j = 0; j < numEigenVectors; j++)
				coeff2[i][j] = projectedTestSample[i] * projectedTestSample[j];
		}
		
		//calculating gamma * projectedTestSample i.e gamma * g
		doubleVector gammaProjTestSample;	
		
		for( i = 0; i < numEigenVectors; i++)
			gammaProjTestSample.push_back(projectedTestSample[i]*gamma);
		
		for( i = 0; i < numEigenVectors; i++)
		{
			coeff2[i][numEigenVectors] = gammaProjTestSample[i];
			coeff2[numEigenVectors][i] = gammaProjTestSample[i];
		}
		
		coeff2[numEigenVectors][numEigenVectors] = gamma * gamma;
		
		for( i = 0; i < numEigenVectors + 1; i++)
		{
			for(j = 0; j < numEigenVectors + 1; j++)
				coeff2[i][j] = (coeff2[i][j] * numClusterSamples) / ((numClusterSamples + 1) * (numClusterSamples + 1));
		}
		
		//final matrix
		for( i = 0; i < numEigenVectors + 1; i++)
		{
			for(j = 0; j < numEigenVectors + 1; j++)
				coeff1[i][j] = coeff1[i][j] + coeff2[i][j];
		}
		
		//solving the intermediate eigen value problem
		//refers to eqn 12
		double2DVector intermediateEigenVectors;
		doubleVector eigenValues;
		int nrot = 0;
		
		intermediateEigenVectors.assign(numEigenVectors + 1,tempVector);
		
		errorCode = m_activedtwShapeRecognizer->computeEigenVectors(coeff1,coeff1.size(),eigenValues,intermediateEigenVectors,nrot);
		
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
				" LTKAdapt::adaptCluster()" << endl;
			LTKReturnError(errorCode);
		}
		
		//the new eigen vectors
		double2DVector eigenVectors;
		
		//adding unith to the old set of eigen vectors
		oldEigenVectors.push_back(unitResidueVector);
		
		eigenVectors.assign(featureSize,tempVector);
		
		tempVector.clear();
		
		//calculating the new eigen vectors from the oldEigenVectors
		//refers to eqn 8
		for( i = 0; i < featureSize; i++)
		{
			for(j = 0; j < (numEigenVectors + 1); j++)
			{
				for(int k = 0; k < (numEigenVectors + 1);k++)
				{
					eigenVectors[i][j] = eigenVectors[i][j] + (oldEigenVectors[k][i] * intermediateEigenVectors[k][j]);
				}
			}
		}
		
		//now converting the eigen vectors to row format
		//this makes it easy to write the eigen vectors to the mdt file and read from it
		double2DVector newEigenVectors; 
		
		tempVector.assign(featureSize,0.0);
		
		newEigenVectors.assign(numEigenVectors + 1,tempVector);
		
		for( i = 0; i < numEigenVectors + 1; i++)
		{
			for(j = 0; j < featureSize; j++)
				newEigenVectors[i][j] = eigenVectors[j][i];
		}
		
		doubleVector newClusterMean;
		
		for( i = 0; i < featureSize; i++)
		{
			double tempValue = ((numClusterSamples * oldClusterMean[i]) + doubleFeatureVector[i])/(numClusterSamples + 1);
			newClusterMean.push_back(tempValue);
		}
		
		numEigenVectors = 0;
		double eigenEnergy = 0;
		for( i = 0; i < eigenValues.size(); i++)
		{
			eigenEnergy += eigenValues[i];
		}
		
		double tempEigenEnergy = 0;
		
		while(tempEigenEnergy <= ((m_activedtwShapeRecognizer->m_percentEigenEnergy * eigenEnergy)/100))
			tempEigenEnergy += eigenValues[numEigenVectors++];
		
		
		doubleVector selectedEigenValues;
		double2DVector selectedEigenVectors;
		for( i = 0; i < numEigenVectors; i++)
		{
			selectedEigenValues.push_back(eigenValues[i]);
			selectedEigenVectors.push_back(newEigenVectors[i]);
		}
		
		//updating the cluster model
		
		errorCode = clusterToAdapt.setNumSamples(numClusterSamples + 1);
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
				" LTKAdapt::adaptCluster()" << endl;
			
			LTKReturnError(errorCode);
		}
		
		clusterToAdapt.setEigenValues(selectedEigenValues);
		selectedEigenValues.clear();
		clusterToAdapt.setEigenVectors(selectedEigenVectors);
		selectedEigenVectors.clear();
		
		clusterToAdapt.setClusterMean(newClusterMean);
		
		currentClusterModelVector[clusterId] = clusterToAdapt;
		
		shapeModelToAdapt.setClusterModelVector(currentClusterModelVector);
		
		m_activedtwShapeRecognizer->m_prototypeShapes[index] = shapeModelToAdapt;
		
		//clearing vectors
		tempVector.clear();
		coeff1.clear();
		coeff2.clear();
		gammaProjTestSample.clear();
		eigenValues.clear();
		newEigenVectors.clear();
		intermediateEigenVectors.clear();
		newClusterMean.clear();
		eigenVectors.clear();
		unitResidueVector.clear();
	}
	
	//clearing vectors
	oldEigenValues.clear();
	oldEigenVectors.clear();
	oldClusterMean.clear();
	doubleFeatureVector.clear();
	projectedTestSample.clear();
	residueVector.clear();
	reverseProjection.clear();
	diffVector.clear();
	currentClusterModelVector.clear();
	
	errorCode = m_activedtwShapeRecognizer->writePrototypeShapesToMDTFile();
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
			" LTKAdapt::adaptCluster()" << endl;
		LTKReturnError(errorCode);
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "LTKAdapt::adaptCluster()" << endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: adaptSingleton
* DESCRIPTION	        : This method adapts the singleton sey with the 
new featureVector and trains the singleton set if the 
number of singletons exceeds a certain number
* ARGUMENTS		: INPUT
featureVecToAdapt shapeFeature 
shapeId int 
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int LTKAdapt::adaptSingleton(shapeFeature& featureVecToAdapt,int shapeId)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "LTKAdapt::adaptSingleton()" << endl;
	
	//validating input parameters
	if(m_activedtwShapeRecognizer->m_shapeIDNumPrototypesMap.find(shapeId) == m_activedtwShapeRecognizer->m_shapeIDNumPrototypesMap.end())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_SHAPEID << " " <<
			" LTKAdapt::adaptCluster()" << endl;
		LTKReturnError(EINVALID_SHAPEID);
	}
	
	int errorCode;
	
	int index = 0;
	
	//iterating prototypeShapes to find the shapeModel To Adapt
	while(m_activedtwShapeRecognizer->m_prototypeShapes[index].getShapeId() != shapeId)
		index++;
	
	shapeMatrix currentSingletonVectors = m_activedtwShapeRecognizer->m_prototypeShapes[index].getSingletonVector();
	
	currentSingletonVectors.push_back(featureVecToAdapt);
	
	m_activedtwShapeRecognizer->m_prototypeShapes[index].setSingletonVector(currentSingletonVectors);
	
	int singletonSize = currentSingletonVectors.size();
	
	//train the singletons only if their number is above some threshold
	if(singletonSize >  (TRAINSINGLETONFACTOR * m_activedtwShapeRecognizer->m_minClusterSize) )
	{
		errorCode = trainSingletons(currentSingletonVectors,shapeId,index);
		
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
				" LTKAdapt::adaptSingleton()" << endl;
			LTKReturnError(errorCode);	
		}
	}
	
	currentSingletonVectors.clear();
	
	//updating the mdt file
	errorCode = m_activedtwShapeRecognizer->writePrototypeShapesToMDTFile();
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
			" LTKAdapt::adaptCluster()" << endl;
		LTKReturnError(errorCode);
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "LTKAdapt::adaptSingleton()" << endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: trainSingletons
* DESCRIPTION	        : This method trains the featureVectors in the singleton set 
* ARGUMENTS		: INPUT
singletons shapeMatrix 
shapeId int 
index int  
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int LTKAdapt::trainSingletons(const shapeMatrix &singletons,int shapeId,int index)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "LTKAdapt::trainSingletons()" << endl;	
	
	//validating input arguments
	if(m_activedtwShapeRecognizer->m_shapeIDNumPrototypesMap.find(shapeId) == m_activedtwShapeRecognizer->m_shapeIDNumPrototypesMap.end())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_SHAPEID << " " <<
			" LTKAdapt::adaptCluster()" << endl;
		LTKReturnError(EINVALID_SHAPEID);
	}
	
	if(index < 0 || index >= m_activedtwShapeRecognizer->m_prototypeShapes.size())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROTOYPESHAPE_INDEX_OUT_OF_BOUND  << " " <<
			" LTKAdapt::adaptSingleton()" << endl;
		
		LTKReturnError(EPROTOYPESHAPE_INDEX_OUT_OF_BOUND);
	}	
	
	int errorCode;
	
	LTKShapeSample tempShape;
	
	vector<LTKShapeSample> shapesToTrain;
	
	int singletonSize = singletons.size();
	
	int2DVector clusterOutput;
	
	shapeMatrix newSingletons;
	
	vector<ActiveDTWClusterModel> currentClusterModelVector = m_activedtwShapeRecognizer->m_prototypeShapes[index].getClusterModelVector();
	
	int i = 0;
	
	for(i = 0; i < singletonSize; i++)
	{
		tempShape.setFeatureVector(singletons[i]);
		shapesToTrain.push_back(tempShape);
	}
	
	//perform clustering
	errorCode = m_activedtwShapeRecognizer->performClustering(shapesToTrain,clusterOutput);
	
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
			" ActiveDTWShapeRecognizer::trainSingletons()" << endl;
		LTKReturnError(errorCode);
	}
	
	int2DVector::iterator iter = clusterOutput.begin();
	int2DVector::iterator iEnd = clusterOutput.end();
	intVector cluster;
	
	/**ITERATING THROUGH THE VARIOUS CLUSTERS **/
	for(;iter != iEnd; ++iter)
	{
		cluster = (*iter);
		
		/** SINGLETON VECTORS **/
		if(cluster.size() < m_activedtwShapeRecognizer->m_minClusterSize)
		{
			for(i = 0; i < cluster.size(); i++)
				newSingletons.push_back(shapesToTrain[cluster[i]].getFeatureVector());
		}
		
		/** CLUSTER PROCESSING **/
		else
		{
			//creating new clusters
			doubleVector tempFeature;
			
			double2DVector featureMatrix;
			
			double2DVector covarianceMatrix;
			
			doubleVector clusterMean;
			
			double2DVector intermediateEigenVectors;
			
			double2DVector eigenVectors;
			
			doubleVector eigenValues;
			
			ActiveDTWClusterModel clusterModel;
			
			//gather all the shape samples pertaining to a particular cluster
			int clusterSize = cluster.size();
			for(i = 0; i < clusterSize; i++)
			{
				floatVector floatFeatureVector;
				
				errorCode = m_activedtwShapeRecognizer->m_shapeRecUtil.shapeFeatureVectorToFloatVector(shapesToTrain[cluster[i]].getFeatureVector(),
					floatFeatureVector);
				
				if (errorCode != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
						" LTKAdapt::trainSingletons()" << endl;
					
					LTKReturnError(errorCode);
				}
				
				int floatFeatureVectorSize = floatFeatureVector.size();
				for(int i = 0; i < floatFeatureVectorSize; i++)
					tempFeature.push_back(floatFeatureVector[i]);
				
				featureMatrix.push_back(tempFeature);
				tempFeature.clear();
				floatFeatureVector.clear();
			}
			
			/** COMPUTING COVARIANCE MATRIX **/
			errorCode = m_activedtwShapeRecognizer->computeCovarianceMatrix(featureMatrix,covarianceMatrix,clusterMean);
			
			if(errorCode !=  SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
					" LTKAdapt::trainSingletons()" << endl;
				
				LTKReturnError(errorCode);
			}
			
			clusterModel.setClusterMean(clusterMean);
			
			errorCode = m_activedtwShapeRecognizer->computeEigenVectorsForLargeDimension(featureMatrix,covarianceMatrix,intermediateEigenVectors,eigenValues);
			
			if(errorCode !=  SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
					" LTKAdapt::trainSingletons()" << endl;
				
				LTKReturnError(errorCode);
			}
			
			doubleVector tempEigenVector;
			int eigenVectorDimension = intermediateEigenVectors.size();
			if(eigenVectorDimension <= 0)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EEMPTY_EIGENVECTORS << " " <<
					" LTKAdapt::trainSingletons()" << endl;
				
				LTKReturnError(EEMPTY_EIGENVECTORS);
			}
			
			int numEigenVectors = intermediateEigenVectors[0].size();
			
			if(numEigenVectors <= 0)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_NUM_OF_EIGENVECTORS << " " <<
					" LTKAdapt::trainSingletons()" << endl;
				
				LTKReturnError(EINVALID_NUM_OF_EIGENVECTORS);
			}
			
			for(i = 0; i < numEigenVectors; i++)
			{
				for(int j = 0; j < eigenVectorDimension; j++)
					tempEigenVector.push_back(intermediateEigenVectors[j][i]);
				
				eigenVectors.push_back(tempEigenVector);
				tempEigenVector.clear();
			}
			
			/**CONSTRUCTING CLUSTER MODEL **/
			
			errorCode =  clusterModel.setNumSamples(cluster.size());
			if(errorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
					" LTKAdapt::trainSingletons()" << endl;
				
				LTKReturnError(errorCode);
			}
			
			clusterModel.setEigenValues(eigenValues);
			
			clusterModel.setEigenVectors(eigenVectors);
			
			currentClusterModelVector.push_back(clusterModel);
			
			//clearing vectors
			featureMatrix.clear();
			covarianceMatrix.clear();
			clusterMean.clear();
			intermediateEigenVectors.clear();
			eigenVectors.clear();
			eigenValues.clear();
			
		}
	}
	
	//updating the shape model
	(m_activedtwShapeRecognizer->m_prototypeShapes[index]).setClusterModelVector(currentClusterModelVector);
	m_activedtwShapeRecognizer->m_prototypeShapes[index].setSingletonVector(newSingletons);
	
	//clearing vectors
	currentClusterModelVector.clear();
	clusterOutput.clear();
	shapesToTrain.clear();
	newSingletons.clear();
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "LTKAdapt::trainSingletons()" << endl;
	
	return SUCCESS;
}
