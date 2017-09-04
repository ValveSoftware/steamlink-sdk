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
 * FILE DESCR: Definitions of Agglomerative Hierarchical Clustering module
 *
 * CONTENTS:
 *             cluster
 *             getProximityMatrix
 *             setOutputConfig
 *             setHyperlinkMap
 *             getClusterResult
 *             computeProximityMatrix
 *             computeDistances
 *             clusterToFindNumClusters
 *             getInterObjectDistance
 *             findGroup
 *             findInterClusterDistance
 *             writeClustersAsHTML
 *             determineNumOfClusters
 *             determineKnee
 *             findRMSE
 *             computeAvgSil
 *
 *
 * AUTHOR:     Bharath A
 *
 * DATE:       February 22, 2005
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/
#ifndef __LTKHIERARCHICALCLUSTERING_H
#define __LTKHIERARCHICALCLUSTERING_H


#ifndef _WIN32
//#include <values.h>
#endif

#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKLoggerUtil.h"
#include "LTKException.h"
#include "LTKErrors.h"

/*Enumerator for stopping criterion to be used*/
enum ELTKHCStoppingCriterion
{
     LMETHOD,
     AVG_SIL
};

/*Enumerator for methods in hierarchical clustering*/
 enum ELTKHCMethod
 {
     SINGLE_LINKAGE,
     COMPLETE_LINKAGE,
     AVERAGE_LINKAGE
 };

#define OUTPUT_HTML_FILE_NAME "output.html"
#define MIN_CUTOFF 20

/**
 * @class LTKHierarchicalClustering
 * <p> This class does agglomerative hierarchical clustering. The data objects
        which could be LTKTrace or LTKTraceGroup, are supplied as a vector.
        Function that defines the distance between two data objects needs to be
       supplied as a function pointer.One of the 3 methods (Single,Average or
       Complete linkage) needs to be selected to define the way inter-cluster
       has to be determined. In case number of clusters is not supplied,
       it is determined using the L-method (default stopping criterion)<p> */

template <class ClusterObjType,class DistanceClass>
class LTKHierarchicalClustering
{

     private:

          //reference to the vector containing the data objects to be clustered
         const vector<ClusterObjType>& m_data;

          //triangular matrix containing the pairwise distances between data
          //objects
          float2DVector m_proximityMatrix;

          //data structure that stores current (intermediate) state of the
          //clusters
          int2DVector m_intermediateCG;

          //vector mapping the data object id and path to the data (unipen) file
          stringVector m_hyperlinksVec;

          //contains the number of clusters required
          int m_numOfClusters;

          //output file handle to write the cluster results as html
          //with name of the file as OUTPUT_HTML_FILE_NAME
          ofstream m_output;

          //flag to indicate whether the output is required as html
          bool m_writeHTML;

          //flag to indicate whether to show all levels of the hierarchy in the
          //html file
          bool m_showAllLevels;

          //vector to hold merging distance for each number of clusters
          floatVector m_mergingDist;

          //flag for determining number of clusters
          bool m_determineClusters;

          //output result directory
          string m_outputDir;

          //extension of the image corresponding to each data object in
          //order to write in the html
          string m_imageFileExtn;

          //Method for defining inter-cluster distance
          ELTKHCMethod m_method;

           //number of clusters determined by Average Silhouette method
          int m_numBySil;

          //cached clustering result corresponding minimum average silhouette
          int2DVector m_cachedResult;

          //stopping criterion selected - LMethod or Average Silhouette
          ELTKHCStoppingCriterion m_stoppingCriterion;

          //pointer to the class that has the definition of distance function
          DistanceClass* m_distClassPtr;

          //function pointer type of the function that defines inter-object distance
          typedef int (DistanceClass::*FN_PTR_DISTANCE)(const ClusterObjType&,
                                                      const ClusterObjType&,
                                                      float&) ;

          //distance function pointer
          FN_PTR_DISTANCE m_distancePtr;


     public:

/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : LTKHierarchicalClustering
* DESCRIPTION  : Constructor to initialise the required parameters
* ARGUMENTS         : clusterObjects - vector of data objects which could be LKTrace or
*                                            LTKTraceGroup for HWR
*                     noOfClusters - Number of clusters required
*                     clusteringMethod - One of the 3 methods:
*                                             SINGLE_LINKAGE,AVERAGE_LINKAGE
*                                             or COMPLETE_LINKAGE
*
* RETURNS      :
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/


LTKHierarchicalClustering(const vector<ClusterObjType>& clusterObjects,int noOfClusters,
                                ELTKHCMethod clusteringMethod=AVERAGE_LINKAGE) :
                                m_data(clusterObjects),m_method(clusteringMethod),
                                m_numOfClusters(noOfClusters),m_writeHTML(false),
                                m_showAllLevels(false),
                                m_determineClusters(false)
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::LTKHierarchicalClustering"
          <<"(vector<ClusterObjType>,int,ELTKHCMethod)"<<endl;

     if(m_numOfClusters < 1 || m_numOfClusters>=clusterObjects.size())
     {
           LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                <<"Number of clusters:"<<m_numOfClusters<<endl;

           LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Error : "<< EINVALID_NUM_CLUSTERS <<":"<< getErrorMessage(EINVALID_NUM_CLUSTERS)
            <<" LTKHierarchicalClustering::"
               <<"LTKHierarchicalClustering(vector<ClusterObjType>,int,ELTKHCMethod)"<<endl;

          throw LTKException(EINVALID_NUM_CLUSTERS);
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::LTKHierarchicalClustering"
          <<"(vector<ClusterObjType>,int,ELTKHCMethod)"<<endl;

}


/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : LTKHierarchicalClustering
* DESCRIPTION  : Constructor to initialise the required parameters.
*                 Number of clusters is determined by L-method
* ARGUMENTS         : clusterObjects - vector of data objects which could be LKTrace or
*                                            LTKTraceGroup for HWR
*                     clusteringMethod - One of the 3 methods:
*                                             SINGLE_LINKAGE,AVERAGE_LINKAGE
*                                             or COMPLETE_LINKAGE
*                     stoppingCriterion - stopping criterion to determine the
*                                              right set of clusters: LMETHOD or AVG_SIL
*
* RETURNS      :
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/


LTKHierarchicalClustering(const vector<ClusterObjType>& clusterObjects,
                                ELTKHCMethod clusteringMethod=AVERAGE_LINKAGE,
                                ELTKHCStoppingCriterion stoppingCriterion=LMETHOD) :
                                m_data(clusterObjects),
                                m_method(clusteringMethod),
                                m_stoppingCriterion(stoppingCriterion),
                                m_writeHTML(false),
                                m_showAllLevels(false),
                                m_determineClusters(true)
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::LTKHierarchicalClustering"
          <<"(vector<ClusterObjType>,ELTKHCMethod,ELTKHCStoppingCriterion)"<<endl;

     if(clusterObjects.size()==0)
     {
          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Number of elements in clusterObjects vector is zero"<<endl;

           LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Error : "<< ENO_DATA_TO_CLUSTER <<":"<< getErrorMessage(ENO_DATA_TO_CLUSTER)
            <<" LTKHierarchicalClustering::LTKHierarchicalClustering"
               <<"(vector<ClusterObjType>,ELTKHCMethod,ELTKHCStoppingCriterion)"<<endl;


          throw LTKException(ENO_DATA_TO_CLUSTER);
     }

     if(clusterObjects.size() < 6 && stoppingCriterion == LMETHOD)
     {
          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Number of elements in clusterObjects vector is:"
               <<clusterObjects.size()<<endl;

           LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Error : "<< EINSUFFICIENT_DATA_FOR_LMETHOD
               <<":"<< getErrorMessage(EINSUFFICIENT_DATA_FOR_LMETHOD)
            <<" LTKHierarchicalClustering::LTKHierarchicalClustering"
               <<"(vector<ClusterObjType>,ELTKHCMethod,ELTKHCStoppingCriterion)"<<endl;

          throw LTKException(EINSUFFICIENT_DATA_FOR_LMETHOD);
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::LTKHierarchicalClustering"
          <<"(vector<ClusterObjType>,ELTKHCMethod,ELTKHCStoppingCriterion)"<<endl;

}



/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : cluster
* DESCRIPTION  : Clusters the input data objects. The number of clusters is determined
*                     based on the stopping criterion or as specified by the user.
* ARGUMENTS         : distanceClassPtr - pointer to the class that contains the distance
*                                             function defintion
*                     distFuncPtr - distance function pointer
* RETURNS      : error code
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int cluster(DistanceClass* distanceClassPtr,FN_PTR_DISTANCE distFuncPtr)
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::cluster()"<<endl;


     m_distancePtr=distFuncPtr;

     m_distClassPtr=distanceClassPtr;


     //To compute inter-object distances
     int errorCode = computeDistances();

     if (errorCode != SUCCESS )
     {
          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: LTKHierarchicalClustering::cluster()"<<endl;

          LTKReturnError(errorCode)
     }

     //if the user has specified the number of clusters
     if(!m_determineClusters)
     {
           clusterToFindNumClusters();
     }
     else
     {
               m_numOfClusters=1;

               //clustering to determine the number of
               //clusters vs merging distance curve
               clusterToFindNumClusters();

               m_determineClusters=false;

               if(m_stoppingCriterion==LMETHOD)
               {
                    //Number of clusters determined by L-method
                   m_numOfClusters=determineNumOfClusters();

                    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
                         <<"Number of clusters determined using L-Method"
                         <<m_numOfClusters<<endl;

               }
               else if(m_stoppingCriterion==AVG_SIL)
               {

                    //Number of clusters determined by silhouette method
                   m_numOfClusters=m_numBySil;

                    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
                         <<"Number of clusters determined using Average Silhouette method"
                         <<m_numOfClusters<<endl;

               }

               //clearing intermediate clusters formed during evaluation
               //of the stopping criterion
               m_intermediateCG.clear();

               //clustering to the number of clusters determined
               //by the stopping criterion
               clusterToFindNumClusters();

       }


     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::cluster()"<<endl;
      return SUCCESS;

}



/**********************************************************************************
* AUTHOR       : Dinesh M
* DATE              : 23-Jan-2006
* NAME              : getProximityMatrix
* DESCRIPTION  : returns the distance matrix
* ARGUMENTS         :
* RETURNS      : proximity matrix (float2DVector)
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

const float2DVector& getProximityMatrix() const
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::getProximityMatrix()"<<endl;


     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::getProximityMatrix()"<<endl;

     return m_proximityMatrix;
}


/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : setOutputConfig
* DESCRIPTION  : This function sets the configuration for the output of clustering.
* ARGUMENTS         : outputDirectory - path to the directory where output html
*                                            is to be generated
*                     displayAllLevels - flag to indicate whether all levels in the
*                                             clustering need to written to the file
*                     imageFileExtension - extension of the image file (ex)"png".
*                                            If not specified <img> tag in the output html is not created.
* RETURNS      : error code
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
//int setOutputConfig(const string& outputDirectory,
//                              bool displayAllLevels=false,
//                              string imageFileExtension="")
int setOutputConfig(const string& outputDirectory,bool displayAllLevels=false,
                         const string& imageFileExtension="")
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::setOutputConfig()"<<endl;

     m_writeHTML=true;

     m_showAllLevels=displayAllLevels;

     string tempOutputFilePath=outputDirectory+"/"+OUTPUT_HTML_FILE_NAME;

     m_output.open(tempOutputFilePath.c_str());

     if(m_output.fail())
     {

          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Unable to create file:"
               <<tempOutputFilePath<<endl;

          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Error : "<< EFILE_CREATION_FAILED <<":"
               <<getErrorMessage(EFILE_CREATION_FAILED)
            <<" LTKHierarchicalClustering::setOutputConfig()" <<endl;

          LTKReturnError(EFILE_CREATION_FAILED)
     }

     m_outputDir=outputDirectory;

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
          <<"Clustering results output directory:"
          <<m_outputDir<<endl;

     m_output.close();

     //If it takes the default value, <img> tag in the output is not generated
     m_imageFileExtn=imageFileExtension;

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
          <<"Image file extension:"<<m_imageFileExtn<<endl;

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::setOutputConfig()"<<endl;

     return SUCCESS;
}


/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : setHyperlinkMap
* DESCRIPTION  : To set hyperlinks for each data object which refers to actual data file.
*                     Assumes one-to-one correspondence with the data vector
*                     passed in the constructor.
* ARGUMENTS         : hyperlinksVector - Vector containing paths to physical
*                                             files of each data object.
*
* RETURNS      : error code
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
//int setHyperlinkMap(const vector<string>& hyperlinksVector)
int setHyperlinkMap(const vector<string>& hyperlinksVector)
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::setHyperlinkMap()"<<endl;

     if(m_data.size()!=hyperlinksVector.size())
     {
          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"cluster objects vector size:"<<m_data.size()
               <<" and hyperlinks vector size:"<<hyperlinksVector.size()<<endl;

          LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Error : "<< EDATA_HYPERLINK_VEC_SIZE_MISMATCH
               <<":"<< getErrorMessage(EDATA_HYPERLINK_VEC_SIZE_MISMATCH)
            <<" LTKHierarchicalClustering::setHyperlinkMap()" <<endl;

          LTKReturnError(EDATA_HYPERLINK_VEC_SIZE_MISMATCH);
     }

     m_hyperlinksVec=hyperlinksVector; //Vector for hyperlinks is set


     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::setHyperlinkMap()"<<endl;

     return SUCCESS;

}


/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : getClusterResult
* DESCRIPTION  : Populates the argument (vector of vectors) with data objects indices.
*                     Each row (inner vector) corresponds to a cluster.
* ARGUMENTS         : outClusterResult - reference to result vector of vectors.
*
* RETURNS      :
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void getClusterResult(vector<vector<int> >& outClusterResult) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::getClusterResult()"<<endl;

     for(int v=0;v<m_intermediateCG.size();v++)
     {

          outClusterResult.push_back(m_intermediateCG[v]);
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::getClusterResult()"<<endl;
}

/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : computeProximityMatrix
* DESCRIPTION  : Populates the argument (vector of vectors) with data objects indices.
*                     Each inner vector corresponds to a cluster.
* ARGUMENTS         : distanceClassPtr - pointer to the class that has the distance definition
*                     distFuncPtr - function pointer to the distance function
* RETURNS      : error code
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

//void computeProximityMatrix(ComputeDistanceFunc computeDistFuncObj)
int computeProximityMatrix(DistanceClass* distanceClassPtr,
                                 FN_PTR_DISTANCE distFuncPtr)
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::computeProximityMatrix()"<<endl;

     m_distancePtr=distFuncPtr;
     m_distClassPtr=distanceClassPtr;

     int errorCode;

     if((errorCode=computeDistances())!=SUCCESS)
     {
           LOG(LTKLogger::LTK_LOGLEVEL_ERR)
               <<"Error: LTKHierarchicalClustering::computeProximityMatrix()"<<endl;

          LTKReturnError(errorCode);
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::computeProximityMatrix()"<<endl;

     return SUCCESS;
}



private:

/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : computeDistances
* DESCRIPTION  : Computes inter-object distances and puts them in the distance matrix
* ARGUMENTS         :
* RETURNS      : error code
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

int computeDistances()
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::computeDistances()"<<endl;

     for(int i=0;i<(m_data.size()-1);++i)
     {
          vector<float> eachRow((m_data.size()-i)-1);//added -1 at the end

          int c=0;

          for(int j=i+1;j<m_data.size();++j)
          {
               //external distance function called
               int errorCode = (m_distClassPtr->*m_distancePtr)(m_data[i],m_data[j], eachRow[c]);

               if (errorCode != SUCCESS )
               {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                         <<"Error while calling distance function"<<endl;

                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                         <<"Error: LTKHierarchicalClustering::computeDistances()"<<endl;

                    LTKReturnError(errorCode);
               }

               ++c;

          }

          m_proximityMatrix.push_back(eachRow);

     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::computeDistances()"<<endl;

    return SUCCESS;

}



/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : clusterToFindNumClusters
* DESCRIPTION  : Clusters the data objects hierarchically (agglomerative)
*                     till the desired number of
*                     clusters and also evaluates the stopping criterion selected.
* ARGUMENTS         :
* RETURNS      : error code
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

int clusterToFindNumClusters()
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::clusterToFindNumClusters()"<<endl;

   if(m_stoppingCriterion==LMETHOD)
   {
          //Number of clusters needs to be determined
          if(m_determineClusters)
          {
               //map for number of clusters vs. merging distance
               m_mergingDist.reserve(m_data.size());
          }
   }
   else if(m_stoppingCriterion==AVG_SIL)
   {
          if(m_writeHTML==false && m_cachedResult.size()>0)
          {
               m_intermediateCG=m_cachedResult;

               return SUCCESS;
          }
   }

     for(int i=0;i<m_data.size();i++)
     {
          vector<int> v;
          v.push_back(i);

          //To begin with, each data object is cluster by itself
          m_intermediateCG.push_back(v);
     }

     if(m_writeHTML)  //If output is needed as html
     {
          string outputFilePath=m_outputDir+"/"+OUTPUT_HTML_FILE_NAME;

          m_output.open(outputFilePath.c_str()); //Cluster output file is created

          if(m_output.fail())
          {
               LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                    <<"Unable to create file:"
                    <<outputFilePath<<endl;

               LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                    <<"Error : "<< EFILE_CREATION_FAILED
                    <<":"<< getErrorMessage(EFILE_CREATION_FAILED)
                <<" LTKHierarchicalClustering::clusterToFindNumClusters()" <<endl;

               LTKReturnError(EFILE_CREATION_FAILED);
          }

          /*Html tags are written*/
          m_output<<"<html>\n";
          m_output<<"<body>\n";
          m_output<<"<table border='1' bordercolor='black'>\n";


          m_output<<"<tr>\n";


          for(int v=0;v<m_intermediateCG.size();v++)
          {

               int clusterSize=m_intermediateCG[v].size();

               m_output<<"<td colspan=\""<<clusterSize<<"\">";

               for(int w=0;w<clusterSize;w++)
               {
                    if(m_hyperlinksVec.size()>0)
                    {
                         m_output<<"<a href='"
                                   <<m_hyperlinksVec[m_intermediateCG[v][w]]
                                   <<"'>"<<m_intermediateCG[v][w]<<"</a>&nbsp;";
                    }
                    else
                    {
                         m_output<<m_intermediateCG[v][w]<<"&nbsp;";
                    }

                    //if there is an image file corresponding to each data object
                    if(!m_imageFileExtn.empty())
                    {
                         m_output<<"<img src=\""
                                   <<m_intermediateCG[v][w]<<"."<<m_imageFileExtn
                                   <<"\" border=\"0\"/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
                    }


               }
          }

          m_output<<"<td><b>";
          m_output<<"Inter-cluster Dist";
          m_output<<"</b></td>";

          m_output<<"</tr>\n";

     }

     if(m_numOfClusters<m_data.size() || m_determineClusters==true)
     {

          int currNumOfClusters = m_data.size();

          //this local variable is used only for Average Silhouette method
          float minSil=FLT_MAX;


          for(int it=0;it<(m_data.size()-m_numOfClusters);++it)
          {
               vector<int> toCluster;

               //to find the clusters that need to be merged
               float interClusterDistance=findGroup(toCluster);

               currNumOfClusters=m_data.size()-it-1;

               if(m_stoppingCriterion==AVG_SIL)
               {
                    float silDiff=computeAvgSil(toCluster[0],toCluster[1]);

                    if(silDiff<minSil)
                    {
                         minSil=silDiff;
                         if(currNumOfClusters > 2)
                         {
                              m_numBySil=currNumOfClusters+1;
                              m_cachedResult=m_intermediateCG;
                         }

                    }
               }
               else if(m_stoppingCriterion==LMETHOD && m_determineClusters==true)
               {
                         m_mergingDist[currNumOfClusters]=interClusterDistance;
               }


               //clusters are merged
               m_intermediateCG[toCluster[0]].insert(m_intermediateCG[toCluster[0]].end(),
                                                              m_intermediateCG[toCluster[1]].begin(),
                                                              m_intermediateCG[toCluster[1]].end());

               //old cluster deleted
               m_intermediateCG.erase(m_intermediateCG.begin()+ toCluster[1]);



                    if(m_writeHTML)
                    {
                         if(!m_showAllLevels)
                         {
                              if(currNumOfClusters==m_numOfClusters)
                              {
                                   writeClustersAsHTML(interClusterDistance);
                              }
                         }
                         else
                         {
                              writeClustersAsHTML(interClusterDistance);
                         }

                    }


          }


     }


     //closing the html tags
     if(m_writeHTML)
     {
          m_output<<"</table>\n";
          m_output<<"</body>\n";
          m_output<<"</html>";

          m_output.close();
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::clusterToFindNumClusters()"<<endl;

     return SUCCESS;

}







/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : getInterObjectDistance
* DESCRIPTION  : Returns the distance between two data objects from the distance matrix
* ARGUMENTS         : firstObjIndex - index of the first data object
*                     secondObjIndex -  index of the second data object
* RETURNS      : distance (float)
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float getInterObjectDistance(int firstObjIndex,int secondObjIndex) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::getInterObjectDistance()"<<endl;

     int row = 0;
     int col = 0;

     if(firstObjIndex < secondObjIndex)
     {
          row=firstObjIndex;
          col=secondObjIndex;
     }
     else
     {
          row=secondObjIndex;
          col=firstObjIndex;
     } //lesser index is made as row and the other as column

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::getInterObjectDistance()"<<endl;

     return m_proximityMatrix[row][col-row-1];
}

/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : findGroup
* DESCRIPTION  : Finds the indices in the m_intermediateCG (clusters) that
*                     need to be merged
* ARGUMENTS         : pairToCombine - vector for storing the cluster indices
* RETURNS      : inter cluster distance (float)
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
//double findGroup(vector<int> pairToCombine)
float findGroup(vector<int>& pairToCombine) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::findGroup()"<<endl;

     float minDistance=FLT_MAX;

     pairToCombine.clear();

     pairToCombine.resize(2);

     for(int i=0;i<m_intermediateCG.size();i++)
     {
          for(int j=i+1;j<m_intermediateCG.size();j++)
          {
               float tempDist=findInterClusterDistance(m_intermediateCG[i],
                                                                  m_intermediateCG[j]);

               if(tempDist<minDistance)
               {

                    minDistance=tempDist;
                    pairToCombine[0]=i;
                    pairToCombine[1]=j;
               }
          }

     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::findGroup()"<<endl;


     return minDistance;

}


/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : findInterClusterDistance
* DESCRIPTION  : Finds the inter-cluster distance.
*                     The contents of each cluster are in the vectors.
* ARGUMENTS         : v1 cluster one
*                     v2 cluster two
* RETURNS      : inter cluster distance
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

//double findInterClusterDistance(const vector<int>& v1,const vector<int>& v2)
float findInterClusterDistance(const vector<int>& cluster1, const vector<int>& cluster2) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::findInterClusterDistance()"<<endl;

     float groupDistance=0.0;

     /*For single-linkage algorithm*/
     if(m_method==SINGLE_LINKAGE)
     {
          groupDistance= FLT_MAX;

          for(int i=0;i<cluster1.size();i++)
          {
               for(int j=0;j<cluster2.size();j++)
               {
                    float temp=getInterObjectDistance(cluster1[i],cluster2[j]);

                    if(temp < groupDistance)
                    {
                         groupDistance=temp;
                    }
               }
          }
     }


     /*For average-linkage algorithm*/
     if(m_method==AVERAGE_LINKAGE)
     {
          groupDistance=0.0;

          for(int i=0;i<cluster1.size();i++)
          {
               for(int j=0;j<cluster2.size();j++)
               {

                    groupDistance+=getInterObjectDistance(cluster1[i],cluster2[j]);
               }
          }

          groupDistance/=((float)(cluster1.size()*cluster2.size()));
     }


     /*For complete-linkage algorithm*/
     if(m_method==COMPLETE_LINKAGE)
     {
          groupDistance=0.0;

          for(int i=0;i<cluster1.size();i++)
          {
               for(int j=0;j<cluster2.size();j++)
               {
                    float temp=getInterObjectDistance(cluster1[i],cluster2[j]);

                    if(temp > groupDistance)
                    {
                         groupDistance=temp;
                    }
               }
          }
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::findInterClusterDistance()"<<endl;


     return groupDistance;


}

/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : writeClustersAsHTML
* DESCRIPTION  : Writes the cluster results as html with data objects' and clusters' ids.
*                     If hyperlinks vector is set, provides links to actual files.
* ARGUMENTS         : interClustDist - merging distance of the new cluster formed
* RETURNS      :
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

void writeClustersAsHTML(float interClustDist)
{

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::writeClustersAsHTML()"<<endl;

     m_output<<"<tr>\n";

     for(int v=0;v<m_intermediateCG.size();v++)
     {
          int clusterSize=m_intermediateCG[v].size();

          m_output<<"<td colspan=\""<<clusterSize<<"\">";

          m_output<<"("<<v<<")<br>";

          for(int w=0;w<clusterSize;w++)
          {
               if(m_hyperlinksVec.size()>0)
               {
                    m_output<<"<a href='"<<m_hyperlinksVec[m_intermediateCG[v][w]]
                            <<"'>"<<m_intermediateCG[v][w]<<"</a>&nbsp;";
               }
               else
               {
                    m_output<<m_intermediateCG[v][w]<<"&nbsp;";
               }
               if(!m_imageFileExtn.empty())
               {
                    m_output<<"<img src=\""<<m_intermediateCG[v][w]<<"."
                            <<m_imageFileExtn
                            <<"\" border=\"0\"/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
               }

          }
     }

     m_output<<"<td>";

     m_output<<"("<<m_intermediateCG.size()
               <<")&nbsp;&nbsp;&nbsp;<b>"<<interClustDist<<"</b>";

     m_output<<"</td>";

     m_output<<"</tr>\n";

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::writeClustersAsHTML()"<<endl;

}

/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : determineNumOfClusters
* DESCRIPTION  : Determines the number of clusters to be formed using
*                     iterative refinement of L-method.
*                     REFERENCE:S. Salvador and P. Chan. Determining the number of clusters/
*                     segments in hierarchical clustering/segmentation algorithms.
*                     Proceedings of 16th IEEE International Conference
*                     on Tools with Artificial Intelligence, 3:1852-1857, 2004.
* ARGUMENTS         :
* RETURNS      : number of clusters
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/

int determineNumOfClusters() const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::determineNumOfClusters()"<<endl;

     int cutOff=(int)m_mergingDist.capacity()-1;

     int lastKnee=0;
     int currentKnee=0;

     lastKnee=cutOff;

     currentKnee=lastKnee;

     bool trueCutOff=false;

     /*Iterative refinement of the L-method result*/

     while(true)
     {

          lastKnee=currentKnee;

          currentKnee=determineKnee(cutOff);

          if(trueCutOff)
          {

               if(currentKnee >= lastKnee)
               {

                    break;
               }
          }

          int temp=currentKnee*2;

          if(temp>cutOff)
          {
               --cutOff;

               trueCutOff=false;
          }
          else
          {
               cutOff=temp;

               trueCutOff=true;
          }


          if(cutOff < MIN_CUTOFF)
          {

               break;
          }


     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
          <<"Number of clusters determined by iterative refinement:"
          <<currentKnee<<endl;

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::determineNumOfClusters()"<<endl;

     return currentKnee;
}




/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : determineKnee
* DESCRIPTION  : Determines the knee of the given number of clusters vs. merging distance curve
* ARGUMENTS         : maxNumClusters - maximum number of clusters
* RETURNS      : knee point of the curve
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int determineKnee(int maxNumClusters) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::determineKnee()"<<endl;

     int result=0;

     float minRMSE = FLT_MAX;

     //atleast two points are required to fit lines, hence c ranges
     //from 3 to maxNumClusters-2
     for(int c=3;c<maxNumClusters-2;c++)
     {

          float lRMSE=0,rRMSE=0;

          findRMSE(c,maxNumClusters,lRMSE,rRMSE);

          float     cRMSE=(((float)(c-1)/(float)(maxNumClusters-1))*lRMSE)
                           +(((float)(maxNumClusters-c)/(float)(maxNumClusters-1))*rRMSE);

          if(cRMSE<minRMSE)
          {

               minRMSE=cRMSE;

               result=c;
          }
     }

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::determineKnee()"<<endl;

     return result+1;
}




/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : determineKnee
* DESCRIPTION  : Determines left and right RMSE values of the given point
*                     on the no. of clusters vs. merging distance curve.
*                     It fits two regression lines on either side of the point to find RMSEs
* ARGUMENTS         : candidateKnee - candidata knee point
*                     maxNumClusters - upper bound on number of clusters
*                     lRMSE - output RMSE on the left of c
*                     rRMSE - output RMSE on the right of c
* RETURNS      :
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void findRMSE(int candidateKnee,int maxNumClusters,float& outLRMSE,float& outRRMSE) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::findRMSE()"<<endl;

     float avgXLeft = 0;
     float avgYLeft = 0;
     float avgXRight = 0;
     float avgYRight = 0;

     /*Regression line coefficients*/
     float beta0Left = 0;
     float beta1Left = 0;
     float beta0Right = 0;
     float beta1Right = 0;

     int i = 0;
     int j = 0;

     for(i=2;i<=candidateKnee;i++)
     {
          avgYLeft+=m_mergingDist[i];

          avgXLeft+=i;
     }


     avgYLeft /= (candidateKnee-1);
     avgXLeft /= (candidateKnee-1);


     for(j=candidateKnee+1;j<=maxNumClusters;j++)
     {

          avgYRight += m_mergingDist[j];
          avgXRight += j;
     }

     avgYRight /= (maxNumClusters-candidateKnee);
     avgXRight /= (maxNumClusters-candidateKnee);

     float numer=0;
     float denom=0;

     for(i=2;i<=candidateKnee;i++)
     {

          numer += ((i-avgXLeft)*(m_mergingDist[i]-avgYLeft));
          denom += ((i-avgXLeft)*(i-avgXLeft));
     }

     beta1Left = numer/denom;
     beta0Left = avgYLeft-(beta1Left*avgXLeft);

     numer=0;denom=0;

     for(j=candidateKnee+1;j<=maxNumClusters;j++)
     {
          numer += ((j-avgXRight)*(m_mergingDist[j]-avgYRight));
          denom += ((j-avgXRight)*(j-avgXRight));
     }

    if(denom > EPS)
    {
          beta1Right = numer / denom;
     }
     else
     {
          beta1Right = 0;
     }

     beta0Right = avgYRight-(beta1Right*avgXRight);

     float errorSOS=0;

     for(i=2;i<=candidateKnee;i++)
     {

          float yCap = (beta0Left+(beta1Left*i));

          errorSOS += ((m_mergingDist[i]-yCap)*(m_mergingDist[i]-yCap));

     }

     outLRMSE=sqrt(errorSOS/(candidateKnee-2));

     errorSOS=0;

     for(j=candidateKnee+1;j<=maxNumClusters;j++)
     {

          float yCap = beta0Right + (beta1Right*j);

          errorSOS += (m_mergingDist[j]-yCap)*(m_mergingDist[j]-yCap);

     }

     outRRMSE=sqrt(errorSOS/(maxNumClusters-candidateKnee-1));

     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::findRMSE()"<<endl;

}




/**********************************************************************************
* AUTHOR       : Bharath A
* DATE              : 22-FEB-2005
* NAME              : computeAvgSil
* DESCRIPTION  : Funtion that determines the change in the ratio of
*                     intra and inter cluster similarity before and after merging
* ARGUMENTS         : clust1Index - index of cluster one
*                     clust2Index - index of cluster two
* RETURNS      : average silhouette computed
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float computeAvgSil(int clust1Index,int clust2Index) const
{
     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: "
          <<"LTKHierarchicalClustering::computeAvgSil()"<<endl;

     const vector<int>& clust1=m_intermediateCG[clust1Index];

     const vector<int>& clust2=m_intermediateCG[clust2Index];

     vector<int> combinedClust;

     combinedClust.insert(combinedClust.end(),clust1.begin(),clust1.end());

     combinedClust.insert(combinedClust.end(),clust2.begin(),clust2.end());

     float clust1TotalSils=0.0f,clust2TotalSils=0.0f,combinedClustTotalSils=0.0f;

     for(int i=0;i<clust1.size();i++)
     {
          int dataObj=clust1[i];

          float avgIntraDist=0.0;

          if(clust1.size()>1)
          {
               for(int j=0;j<clust1.size();j++)
               {
                    if(clust1[j]!=dataObj)
                    {
                         avgIntraDist+=getInterObjectDistance(dataObj,clust1[j]);

                    }
               }

               avgIntraDist/=((float)(clust1.size()-1));
          }

          float minInterDist= FLT_MAX;

          for(int r=0;r<m_intermediateCG.size();r++)
          {
               float avgInterDist=0.0;

               if(r!=clust1Index)
               {
                    for(int c=0;c<m_intermediateCG[r].size();c++)
                    {
                         avgInterDist+=getInterObjectDistance(dataObj,m_intermediateCG[r][c]);
                    }

                    avgInterDist/=((float)m_intermediateCG[r].size());

                    if(avgInterDist<minInterDist)
                    {
                         minInterDist=avgInterDist;
                    }
               }

          }

          float dataObjSil=0.0;

          if(minInterDist > avgIntraDist && minInterDist > EPS)
          {

               dataObjSil=(minInterDist-avgIntraDist)/minInterDist;

          }
          else if(avgIntraDist > EPS)
          {

               dataObjSil=(minInterDist-avgIntraDist)/avgIntraDist;

          }

          clust1TotalSils+=dataObjSil;
     }

     for(int ii=0;ii<clust2.size();ii++)
     {
          int dataObj=clust2[ii];

          float avgIntraDist=0.0;

          if(clust2.size()>1)
          {
               for(int j=0;j<clust2.size();j++)
               {
                    if(clust2[j]!=dataObj)
                    {

                         avgIntraDist+=getInterObjectDistance(dataObj,clust2[j]);

                    }
               }

               avgIntraDist/=((float)(clust2.size()-1));
          }

          float minInterDist= FLT_MAX;

          for(int r=0;r<m_intermediateCG.size();r++)
          {
               float avgInterDist=0.0;

               if(r!=clust2Index)
               {
                    for(int c=0;c<m_intermediateCG[r].size();c++)
                    {
                         avgInterDist+=getInterObjectDistance(dataObj,
                                                                       m_intermediateCG[r][c]);
                    }

                    avgInterDist/=((float)m_intermediateCG[r].size());

                    if(avgInterDist < minInterDist)
                    {
                         minInterDist=avgInterDist;
                    }
               }

          }
          float dataObjSil=0.0;

          if(minInterDist > avgIntraDist && minInterDist > EPS)
          {
               dataObjSil=(minInterDist-avgIntraDist)/minInterDist;
          }
          else if(avgIntraDist > EPS)
          {
               dataObjSil=(minInterDist-avgIntraDist)/avgIntraDist;
          }

          clust2TotalSils+=dataObjSil;
     }


     for(int iii=0;iii<combinedClust.size();iii++)
     {
          int dataObj=combinedClust[iii];

          float avgIntraDist=0.0;

          if(combinedClust.size()>1)
          {
               for(int j=0;j<combinedClust.size();j++)
               {
                    if(combinedClust[j]!=dataObj)
                    {
                         avgIntraDist+=getInterObjectDistance(dataObj,combinedClust[j]);

                    }
               }
               avgIntraDist/=((float)(combinedClust.size()-1));
          }

          float minInterDist=FLT_MAX;

          for(int r=0;r<m_intermediateCG.size();r++)
          {

               if(r!=clust1Index && r!=clust2Index)
               {
                    float avgInterDist=0.0f;

                    for(int c=0;c<m_intermediateCG[r].size();c++)
                    {
                         avgInterDist+=getInterObjectDistance(dataObj,m_intermediateCG[r][c]);
                    }

                    avgInterDist = (float)avgInterDist/ ((float)m_intermediateCG[r].size());

                    if(avgInterDist<minInterDist)
                    {
                         minInterDist=avgInterDist;

                    }
               }

          }

          float dataObjSil=0.0;

          if(minInterDist>avgIntraDist && minInterDist > EPS)
          {

               dataObjSil=(minInterDist-avgIntraDist)/minInterDist;

          }
          else if(avgIntraDist > EPS)
          {

               dataObjSil=(minInterDist-avgIntraDist)/avgIntraDist;

          }

          combinedClustTotalSils+=dataObjSil;
     }


     LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: "
          <<"LTKHierarchicalClustering::computeAvgSil()"<<endl;


     return (combinedClustTotalSils-clust1TotalSils-clust2TotalSils);

}



};
#endif

