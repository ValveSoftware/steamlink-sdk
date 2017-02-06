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
#ifndef __POINTFLOATSHAPEFEATUREEXTRACTOR_H
#define __POINTFLOATSHAPEFEATUREEXTRACTOR_H

#define SUPPORTED_MIN_VERSION "4.0.0"
#define FEATEXTR_POINTFLOAT_DEF_SIZE 10

#include "LTKShapeFeatureExtractor.h"


class PointFloatShapeFeatureExtractor : public LTKShapeFeatureExtractor
{
public:
     /** @brief Constructor for the PointFloata feature extractor
      * Gets the cfg file path from the contorInfo
      * Reads the cfg variables and poputlates the member variables
      * @param controlInfo: LTKControlInfo : The control information
      * @return no return value as it is a constructor
      */

     PointFloatShapeFeatureExtractor(const LTKControlInfo& controlInfo);

    int extractFeatures(const LTKTraceGroup& inTraceGroup,
                          vector<LTKShapeFeaturePtr>& outFeatureVec);

    LTKShapeFeaturePtr getShapeFeatureInstance();


    int convertFeatVecToTraceGroup(const vector<LTKShapeFeaturePtr>& shapeFeature,
                                          LTKTraceGroup& outTraceGroup);

private:
     /** @brief reads the cfg file and sets the member variables
     * @param cfgFilePath : const string&: The path of the cfg file to be opened
     * @return int : The sucess or failure of the function
     */
     //int readConfigFile(const string& cfgFilePath);
     int readConfig(const string& cfgFilePath);

};


#endif
//#ifndef __POINTFLOATSHAPEFEATUREEXTRACTOR_H
