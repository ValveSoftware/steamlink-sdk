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
 * $LastChangedDate: 2008-01-11 14:11:25 +0530 (Fri, 11 Jan 2008) $
 * $Revision: 359 $
 * $Author: sundarn $
 *
 ************************************************************************/
#ifndef __DYNAMICTIMEWARPING_H
#define __DYNAMICTIMEWARPING_H

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#ifndef _WIN32
//#include <values.h>
#endif
#include <limits>
#include <algorithm>
#include <cmath>
#include <deque>


using namespace std;

template <typename TimeSeriesElementType, typename DistanceType>
//TimeSeriesElementType - pointer of type feature (LTKShapeFeature * etc.)
//DistanceType - type of distance (float, int etc.)



/**
 * @class DTWShapeRecognizer
 */
class DynamicTimeWarping
{
    private:

        vector<vector <DistanceType> > m_cumulativeDistance;		// Cumulative distance matrix

        vector<vector<int> > m_phi;						// phi matrix required for path computation

        //function pointer type of the function that defines local distance function
        typedef void (*FN_PTR_DISTANCE)(const TimeSeriesElementType&, const TimeSeriesElementType&, DistanceType&) ; 								


        DistanceType m_maxVal;	
        // Max val (like FLT_MAX etc),  
        // this max value that can be returned
        // local distance			


        /**
         * This function back tracks and calculates the DTW warping path from the DTW distance matrix
         * @param1 lastPointYIndex is the x index of the point from which back tracking has to start
         * @param1 lastPointXIndex is the y index of the point from which back tracking has to start
         */
        void populatePath(int lastPointYIndex, int lastPointXIndex, deque<vector<int> >& warpingPath)
        {

            int yIndex = lastPointYIndex;
            int xIndex = lastPointXIndex;
            vector<int > indices(2);
            int tb;					// req for warping path
            int k,p;				// loop variables

            //Path Computation

            indices[0] = yIndex;
            indices[1] = xIndex;
            warpingPath.push_front(indices);

            while((yIndex > 0) && (xIndex > 0))
            {
                tb = m_phi[yIndex][xIndex];
                if (tb == 2)
                {
                    yIndex--; 
                    xIndex--;
                    indices[0] = yIndex;
                    indices[1] = xIndex;
                }
                else if(tb == 0)
                {
                    yIndex--;
                    indices[0] = yIndex;
                    indices[1] = xIndex;
                }
                else if (tb == 1)
                {
                    xIndex--;
                    indices[0] = yIndex;
                    indices[1] = xIndex;
                }

                warpingPath.push_front(indices);

            }

            if((yIndex > 0) && (xIndex == 0))
            {
                --yIndex;
                for(k = yIndex; k >= 0; --k)
                { 
                    indices[0]  = (k);
                    indices[1] = (0);
                    warpingPath.push_front(indices);
                }
            }
            else if(( yIndex == 0) && (xIndex > 0))
            {
                --xIndex;
                for(p = xIndex; p >= 0; --p)
                { 
                    indices[0]  = (0);
                    indices[1] = (p);
                    warpingPath.push_front(indices);
                }
            }

        }

    public:



        /** @name Constructors and Destructor */
        //@{

        /**
         * This Constructor takes the size of the input train character, 
         * size of input test character, Max Value (ie FLT_MAX, INT_MAX etc)
         * Local distance function, reduce by half function, and banding value
         */

        DynamicTimeWarping()
        {


        }


        /**
         * Destructor
         */

        ~DynamicTimeWarping(){};

        /**
         * @name Methods
         */

        //@{


        /**
         * This function computes the DTW distance between the two vectors.
         * @param train This is an input parameter and corresponds to the first vector
         * @param test This is an input parameter and corresponds to the second vector
         * @param local This is the function pointer to the local distance function 
         * @param distanceDTW This is the output parameter which gives the DTW distance between the two vectors.
         * @param warpingPath This is the output parameter which gives DTW warping path
         * @param banding This is the banding value for the DTW Matrix
         * @param bestSoFar This is an imput parameter as stoping criteria based on Best So Far
         * @param maxVal This is used to pass the maximum possible value for the DTW distance
         */
/*
        int computeDTW(const vector <TimeSeriesElementType>& train, 
                const vector <TimeSeriesElementType>& test,
                FN_PTR_DISTANCE localDistPtr, 
                DistanceType& distanceDTW, 
                deque<vector<int> >& warpingPath, 
                int banding=0, DistanceType bestSoFar=numeric_limits<DistanceType>::infinity(),
                DistanceType maxVal=numeric_limits<DistanceType>::infinity())
                */
        int computeDTW(const vector <TimeSeriesElementType>& train, 
                const vector <TimeSeriesElementType>& test,
                FN_PTR_DISTANCE localDistPtr, 
                DistanceType& distanceDTW, 
                deque<vector<int> >& warpingPath, 
                float banding=0, DistanceType bestSoFar=numeric_limits<DistanceType>::infinity(),
                DistanceType maxVal=numeric_limits<DistanceType>::infinity())

        {
            m_maxVal = maxVal;
            if(localDistPtr == NULL)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< ENULL_POINTER <<":"<< getErrorMessage(ENULL_POINTER)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/

                LTKReturnError(ENULL_POINTER);
            }
            int trainSize = train.size(); //Temporary variables to store the size of the train and test vectors.
            if(trainSize == 0)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/
                LTKReturnError(EEMPTY_VECTOR);
            }
            int testSize = test.size();	//Temporary variables to store the size of the train and test vectors.
            if(testSize == 0)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/
                LTKReturnError(EEMPTY_VECTOR);
            }

            float testBanding = floor(testSize*(1-banding));
			float trainBanding = floor(trainSize*(1-banding));
			banding = (trainBanding < testBanding)?trainBanding:testBanding;

            int banded = 0;
            
            if(banding < 0 || banding >= trainSize || banding >= testSize)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
            else
            {
                banded = banding;	     // is the variable used for fixing the band in the computation of the distance matrix.
                // this is the number of elements from the matrix
                // boundary for which computation will be skipped
            }

            int trunkI = banded; //Temporary variables used to keep track of the band in the distance matrix.

            int	trunkJ = 0;	//Temporary variables used to keep track of the band in the distance matrix.

            int i,j,l; //index variables

            if(m_cumulativeDistance.size() == 0)
            {
                vector <DistanceType> dVec(testSize,m_maxVal);
                vector <int> phiVec(testSize,0);

                for(i = 0; i < trainSize; ++i)
                {
                    m_cumulativeDistance.push_back(dVec);
                    m_phi.push_back(phiVec);	
                }
            }
            else if(trainSize > m_cumulativeDistance.size() || testSize > m_cumulativeDistance[0].size())
            {
                m_cumulativeDistance.clear();
                m_phi.clear();
                vector <DistanceType> dVec(testSize,m_maxVal);
                vector <int> phiVec(testSize,0);

                for(i = 0; i < trainSize; ++i)
                {
                    m_cumulativeDistance.push_back(dVec);
                    m_phi.push_back(phiVec);	
                }

            }
            else
            {
                for(i=0; i<m_cumulativeDistance.size(); i++)
                {
                    for(j=0; j<m_cumulativeDistance[0].size();j++)
                    {
                        m_cumulativeDistance[i][j] = m_maxVal;
                        m_phi[i][j] = 0;
                    }
                }
            }

            DistanceType rowMin;	//Temporary variable which contains the minimum value of the distance of a row.

            DistanceType tempDist[2];	//Temporary variable to store the  distance

            DistanceType tempMinDist;	//Temporary variable to store the minimum distance

            DistanceType tempVal;

            int tempIndex;

            typename vector <vector<DistanceType> > ::iterator cumDistIter1;	//iterator for cumDist i
            typename vector<DistanceType> ::iterator cumDistIter2;	//iterator for cumDist j
            typename vector<DistanceType> ::iterator cumDistIter3;	//iterator for cumDist j 

            
            (localDistPtr)(train[0],test[0],m_cumulativeDistance[0][0]);

            m_phi[0][0]=2;	

            /**
              Computing the first row of the distance matrix.
             */

            cumDistIter2 = m_cumulativeDistance[0].begin();
            for(i = 1; i < testSize; ++i)
            {
                DistanceType tempVal;

                (localDistPtr)(train[0],test[i],tempVal);

                *(cumDistIter2 + i) = *(cumDistIter2 + i-1) + tempVal;
                m_phi[0][i]=1;
            }
            if(trunkI > 0)
            {
                --trunkI;
            }
            /**
              Computing the first column of the distance matrix.
             */

            for(j = 1; j < trainSize; ++j)
            {


                (localDistPtr)(train[j],test[0],tempVal);

                m_cumulativeDistance[j][0] = m_cumulativeDistance[j-1][0]+tempVal;
                m_phi[j][0]=0;
            }


            /**
              Computing the values of the rest of the cells in the distance matrix.
             */
            cumDistIter1 = m_cumulativeDistance.begin();
            for(i = 1; i < trainSize; ++i)
            {
                rowMin = m_maxVal;
                cumDistIter2 = (*(cumDistIter1+i-1)).begin();
                cumDistIter3 = (*(cumDistIter1+i)).begin();

                for(j = 1+trunkJ; j < testSize-trunkI; ++j)
                {
                    tempDist[0] = *(cumDistIter2+j-1);
                    tempIndex = 2;

                    tempMinDist = tempDist[0];
                    tempDist[0] = *(cumDistIter2+j);
                    tempDist[1] = *(cumDistIter3+j-1); 

                    for(l = 0; l < 2; ++l)
                    {
                        if(tempMinDist>=tempDist[l])
                        {
                            tempMinDist=tempDist[l];
                            tempIndex = l;
                        }
                    }

                    m_phi[i][j] = tempIndex;
                    localDistPtr(train[i],test[j],tempVal);
                    tempMinDist= tempMinDist+tempVal;
                    m_cumulativeDistance[i][j]=tempMinDist;

                    if(rowMin > tempMinDist)
                        rowMin = tempMinDist;

                }
                if(trunkI > 0)
                    --trunkI;
                if(i > (trainSize-banded-1))
                    ++trunkJ;

                if(rowMin > bestSoFar)
                {
                    distanceDTW = m_maxVal;
                    return SUCCESS;
                }
            }

            distanceDTW = tempMinDist;

			distanceDTW = distanceDTW/(trainSize + testSize);

            populatePath(trainSize-1,testSize-1,warpingPath);

            return SUCCESS;

        }	  


        /**
         * This function computes the DTW distance between the two vectors.
         * @param train This is an input parameter and corresponds to the first vector
         * @param test This is an input parameter and corresponds to the second vector
         * @param local This is the function pointer to the local distance function 
         * @param distanceDTW This is the output parameter which gives the DTW distance between the two vectors.
         * @param banding This is the banding value for the DTW Matrix
         * @param bestSoFar This is an imput parameter as stoping criteria based on Best So Far
         * @param maxVal This is used to pass the maximum possible value for the DTW distance
         */
/*
        int computeDTW(const vector <TimeSeriesElementType>& train, 
                const vector <TimeSeriesElementType>& test,
                FN_PTR_DISTANCE localDistPtr, DistanceType& distanceDTW,  
                int banding=0, DistanceType bestSoFar=(numeric_limits<DistanceType>::infinity()),
                DistanceType maxVal=(numeric_limits<DistanceType>::infinity()))
                */
        int computeDTW(const vector <TimeSeriesElementType>& train, 
                const vector <TimeSeriesElementType>& test,
                FN_PTR_DISTANCE localDistPtr, DistanceType& distanceDTW,  
                float banding=0, DistanceType bestSoFar=(numeric_limits<DistanceType>::infinity()),
                DistanceType maxVal=(numeric_limits<DistanceType>::infinity()))
        {

            m_maxVal = maxVal;
            if(localDistPtr == NULL)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< ENULL_POINTER <<":"<< getErrorMessage(ENULL_POINTER)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/

                LTKReturnError(ENULL_POINTER);
            }
            int trainSize = train.size(); //Temporary variables to store the size of the train and test vectors.
            if(trainSize == 0)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/
                LTKReturnError(EEMPTY_VECTOR);
            }
            int testSize = test.size();	//Temporary variables to store the size of the train and test vectors.
            if(testSize == 0)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/
                LTKReturnError(EEMPTY_VECTOR);
            }

            float testBanding = floor(testSize*(1-banding));
			float trainBanding = floor(trainSize*(1-banding));
			banding = (trainBanding < testBanding)?trainBanding:testBanding;


            int banded = 0;
            if(banding < 0 || banding >= trainSize || banding >= testSize)
            {
                /*LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
                  <<" DynamicTimeWarping::computeDTW()" <<endl;*/
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
            else
            {
                banded = banding;	     // is the variable used for fixing the band in the computation of the distance matrix.
                // this is the number of elements from the matrix
                // boundary for which computation will be skipped
            }

            int trunkI = banded; //Temporary variables used to keep track of the band in the distance matrix.

            int	trunkJ = 0;	//Temporary variables used to keep track of the band in the distance matrix.

           int i,j,k; //index variables
            vector<DistanceType> currentRow(testSize,m_maxVal);
            vector<DistanceType> previousRow(testSize,m_maxVal);

            DistanceType rowMin;		//Temporary variable which contains the minimum value of the distance of a row.

            DistanceType tempDist[3];	//Temporary variable to store the  distance

            DistanceType tempMinDist;	//Temporary variable to store the minimum distance

            DistanceType tempVal;

            
            (localDistPtr)(train[0],test[0],previousRow[0]);

            /**
              Computing the first row of the distance matrix.
             */

            for(i = 1; i < testSize; ++i)
            {

                (localDistPtr)(train[0],test[i],tempVal);
                previousRow[i] = previousRow[i-1] + tempVal;
            }
            if(trunkI > 0)
            {
                --trunkI;
            }


            for(i = 1; i < trainSize; ++i)
            {
                rowMin = m_maxVal;


                localDistPtr(train[i],test[trunkJ],tempVal);
                currentRow[trunkJ]=tempVal+previousRow[trunkJ];

                for(j = 1+trunkJ; j < testSize-trunkI; ++j)
                {

                    tempDist[0] = currentRow[j-1];
                    tempDist[1] = previousRow[j];
                    tempDist[2] = previousRow[j-1];

                    tempMinDist = tempDist[0];

                    for(k = 0; k < 3; k++)
                    {
                        if(tempMinDist>=tempDist[k])
                        {
                            tempMinDist=tempDist[k];
                        }
                    }

                    localDistPtr(train[i],test[j],tempVal);
                    tempMinDist= tempMinDist+tempVal;
                    currentRow[j]=tempMinDist;
                    if(rowMin > tempMinDist)
                        rowMin = tempMinDist;

                }
                if(rowMin > bestSoFar)
                {
                    distanceDTW = m_maxVal;
                    return SUCCESS;
                }

                if(i > (trainSize-banded-1))
                    ++trunkJ;
                if(trunkI > 0)
                    --trunkI;
                copy(currentRow.begin()+trunkJ, currentRow.end()-trunkI, previousRow.begin()+trunkJ);
            }

            distanceDTW = tempMinDist;
			distanceDTW = distanceDTW/(trainSize + testSize);

		    return SUCCESS;

        }	  

};

#endif
