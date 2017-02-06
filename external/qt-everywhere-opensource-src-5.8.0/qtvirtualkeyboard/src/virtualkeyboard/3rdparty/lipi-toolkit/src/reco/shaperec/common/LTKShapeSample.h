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
 * $LastChangedDate: 2009-07-01 16:41:37 +0530 (Wed, 01 Jul 2009) $
 * $Revision: 783 $
 * $Author: mnab $
 *
 ************************************************************************/
#ifndef __LTKSHAPESAMPLE_H
#define __LTKSHAPESAMPLE_H

#include "LTKInc.h"

#include "LTKShapeFeatureMacros.h"

class LTKShapeFeature;


class LTKShapeSample
{
    private:
        vector<LTKShapeFeaturePtr> m_featureVector;
        int m_classId;

    public:
        /** @name Constructors and Destructor */
        //@{

        /**
         * Default Constructor.
         */

        LTKShapeSample();

        /** 
         * Copy Constructor
         */
        LTKShapeSample(const LTKShapeSample& shapeFeatures);

        /**
         * Destructor
         */
        ~LTKShapeSample();
        //@}

        /**
         * @name Assignment operator 
         */
        //@{

        /**
         * Assignment operator
         * @param shapeFeatures The object to be copied by assignment
         *
         * @return LTKShapeSample object
         */

        LTKShapeSample& operator=(const LTKShapeSample& shapeFeatures);
        //@}

        /**
         * @name Getter Functions
         */
        //@{ 


        /**
         * @brief getter method for feature vector
         * @param none
         * @return featureVec	Type : LTKShapeFeaturePtrtor
         */
        const vector<LTKShapeFeaturePtr>& getFeatureVector() const;


        /**
         * @brief getter method for feature vector
         * @param none
         * @return featureVec	Type : LTKShapeFeaturePtrtor
         */
        vector<LTKShapeFeaturePtr>& getFeatureVectorRef();


        /**
         * @brief setter method for class ID
         * @param none
         * @return classID Type : int
         */  
        int getClassID() const;
        //@}


        /**
         * @name Setter Functions
         */
        //@{

        /**
         * @brief setter method for feature vector
         * @param featureVec	Type : LTKShapeFeaturePtrtor
         */
        void setFeatureVector(const vector<LTKShapeFeaturePtr>& inFeatureVec);


        /**
         * @brief setter method for class ID
         * @param classID Type : int
         */  
        void setClassID(int inClassId);
        //@}

        /**
         * @brief setter method for strokes count
         * @param none
		 * @return strokes count Type : int
		 */  
		int getCountStrokes() const; 

        void clearShapeSampleFeatures();

        //Since pointer is a member variable we should implement constructor, copy-constructor, assignment operator, virtual destructor
};

#endif

