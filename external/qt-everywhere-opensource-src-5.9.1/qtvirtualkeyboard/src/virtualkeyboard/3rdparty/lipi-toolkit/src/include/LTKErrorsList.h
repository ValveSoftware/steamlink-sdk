/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate: 2009-11-24 17:23:35 +0530 (Tue, 24 Nov 2009) $
 * $Revision: 792 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR:
 *
 * CONTENTS:
 *
 * AUTHOR:  Vijayakumara M.
 *
 * DATE:    01-Sept-2005
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/
#ifndef _LTK_ERRORS_LIST_H__
#define _LTK_ERRORS_LIST_H__

/**
* @ingroup util
*/

/** @file LTKErrorsList.h
* @brief Contains the error macors used in Lipitk
*/


// File related errors.
#define EINK_FILE_OPEN                            100  /**< @brief Ink File Open Error. */
#define ECONFIG_FILE_OPEN                         101  /**< @brief Unable to open the .cfg file. */
#define EHEADER_INFO_FILE_OPEN                    102  /**< @brief Header information file open error */
#define EMODEL_DATA_FILE_OPEN                     103  /**< @brief Model Data file open error. */
#define ETRAINLIST_FILE_OPEN                      104  /**< @brief Training List file open error. */
#define EMODEL_DATA_FILE_FORMAT                   105 /**< @brief Model Data file is not in the format. */
#define EINVALID_INPUT_FORMAT                     106 /**< @brief Model Data file has been corrupted. */

//dll related errors
#define ELOAD_SHAPEREC_DLL                        107  /**< @brief Error while Loading the shaperecognizer dll/so */
#define ELOAD_WORDREC_DLL                         108  /**< @brief Error while Loading the wordrecognizer dll/so */
#define ELOAD_PREPROC_DLL                         109  /**< @brief Error while Loading the preprocessing dll/so */
#define EDLL_FUNC_ADDRESS                         110  /**< @brief Unable to Get the function address in dll. */
#define ECREATE_SHAPEREC                          111 /**< @brief Unable to create the shaperecognizer instance. */
#define ECREATE_WORDREC                           112  /**< @brief Unable to create the wordrecognizer instance. */
#define ECREATE_PREPROC                           113  /**< @brief Unable to crete the preprocessing instance. */

// Path set related errors.
#define ELIPI_ROOT_PATH_NOT_SET                   114  /**< @brief Lipi root path is not set  */
#define EINVALID_PROJECT_NAME                     115  /**< @brief Invalid Project name given in command prompt. */
#define EINVALID_CONFIG_ENTRY                     116  /**< @brief Invalid configuration entry in cfg file */
#define ENO_SHAPE_RECOGNIZER                      117  /**< @brief Shape Reconginer is not entered in the cfg file. */
#define ENO_WORD_RECOGNIZER                       118  /**< @brief word Reconginer is not entered in the cfg file. */

//Invalid Values
#define EINVALID_NUM_OF_TRACES                    119  /**< @brief Invalide number of traces processed. */
#define EINVALID_NUM_OF_SHAPES                    120  /**< @brief Invalid value for number of shapes. */
#define EINVALID_TRACE_DIMENTION                  121  /**< @brief Invalid value for Trace Dimension. */
#define EINVALID_NUMEIGENVECTOR                   122  /**< @brief Invalid value for EigenVectors. */
#define EINVALID_FLOAT_SIZE                       123  /**< @brief Invalid size of flaot in Model data header in model data file. */
#define EINCOMPATIBLE_VERSION                     124  /**< @brief Incompatible algorithm version number. */
#define EINVALID_PREPROC_SEQUENCE                 125  /**< @brief Preproccessing sequence error. */

//General errors
#define ENO_TOKEN_FOUND                           126  /**< @brief No token found, invalid entry for project name */
#define EINVALID_LOGICAL_NAME                     127  /**< @brief Invalide Logical Name entered in project.cfg file. */

#define EINVALID_SEGMENT                          128  /**< @brief Only Boxed Recognition is supported */
#define EINVALID_REC_MODE                         129  /**< @brief Unsupported reccognizer mode */

#define EUNSUPPORTED_STATISTICS                   130  /**< @brief Bad name for the statistics to be computed */

#define EMAP_NOT_FOUND                            131  /**< @brief The function is not implemented for shape recognizer project */
#define EINVALID_SHAPEID                          132  /**< @brief Invalid value for shapID. */

#define ENOMAPFOUND_LIPIENGINECFG                 133 /**< @brief Cannot map the logical name, no entries in lipiengine.cfg */
#define EINVALID_NUM_OF_POINTS                    134  /**< @brief Number of points in the tracegroup is not normalized */
#define EEMPTY_TRACE                              135 /**< @brief Number of points in the trace is zero */
#define EEMPTY_TRACE_GROUP                        136 /**< @brief Number of traces in the trace group is zero */
#define ECONFIG_FILE_RANGE                        137  /**< @brief The config file variable is not within the correct range  */

#define EINITSHAPE_NONZERO                        138 /**< @brief Intial shape id is not zero. */
#define EINVALID_LINE_LISTFILE                    139  /**< @brief Invalid line in the listfile (train or test ) */
#define EINVALID_ORDER_LISTFILE                   140  /**< @brief Invalid order of shape-ids in the list file ( train ). */
#define ENUM_NNS                                  141 /**< @brief Invalid number of nearest neighbours specified */


#define EINKFILE_EMPTY                            142  /**< @brief Ink file name is empty */
#define EINKFILE_CORRUPTED                        143 /**< @brief ERROR: Incorrect or corrupted unipen ink file. */
#define EDLL_FUNC_ADDRESS_CREATE                  144  /**< @brief Unable to Get the create function address in dll. */
#define EDLL_FUNC_ADDRESS_DELETE                  145  /**< @brief Unable to Get the delete function address in dll. */
#define ENO_RESAMPLETRACEGROUP                    146  /**< @brief No ResampleTraceGroup in preProcSequence of cfg file */

#define EINVALID_SAMPLING_RATE                    147 /**< @brief Sampling rate cannot be negative */
#define EINVALID_X_RESOLUTION                     148  /**< @brief m_xDpi values cannot be negative */
#define EINVALID_Y_RESOLUTION                     149  /**< @brief m_yDpi values cannot be negative */
#define EINVALID_LATENCY                          150  /**< @brief m_latency cannot be negative */
#define EPOINT_INDEX_OUT_OF_BOUND                 151  /**< @brief Point index greater than number of points available */
#define ECHANNEL_INDEX_OUT_OF_BOUND               152  /**< @brief Invalid Channel */
#define ECHANNEL_SIZE_MISMATCH                    153  /**< @brief New channel data not as long as the old one */
#define ENUM_CHANNELS_MISMATCH                    154  /**< @brief Point to be added does not have the same number of channels as the trace */
#define EDUPLICATE_CHANNEL                        155  /**< @brief Channel with the new channel name already present */
#define ECHANNEL_NOT_FOUND                        156  /**< @brief Channel not found */
#define EZERO_CHANNELS                            157  /**< @brief Number of channels cannot be zero */
#define EINVALID_INPUT_STREAM                     158  /**< @brief Input stream does not match with number of channels in the trace */
#define ECOMPUTE_DISTANCE_ERROR                   159  /**< @brief Error: Cannot find distance for test sample with more than 1 stroke */
#define ECOMPARISON_ERROR                         160  /**< @brief Error: Cannot compare with train sample having more than 1 stroke */
#define ETRAIN_TEST_VECTOR_SIZE_MISMATCH          161  /**< @brief Incompatible: train vector and test vector sizes do not match */
#define EGRAMMER_FILE_NOT_EXIST                   162  /**< @brief Grammar file does not exists */
#define EVALUES_NOT_PROVIDED                      163  /**< @brief Values for the terminal is not Provided */
#define ECONFIG_FILE_FORMAT                       164  /**< @brief No productions or terminals identified in the CFG. Please check the CFG format */
#define ECYCLIC_DEPENDENCY                        165  /**< @brief Cyclic dependency exists! Unable to find paths */
#define EFILE_OPEN_ERROR                          166 /**< @brief Failed to open file */

//Feature extractor errors
#define ELOAD_FEATEXT_DLL                         167 /**< @brief Error while Loading the Feature Extractor dll/so */
#define EDLL_FUNC_ADDRESS_CREATE_FEATEXT          168 /**< @brief Unable to Get the create function address in Featuer extractor dll */
#define EDLL_FUNC_ADDRESS_DELETE_FEATEXT          169 /**< @brief Unable to Get the delete function address in Featuer extractor dll */
#define EFTR_EXTR_NOT_EXIST                       170  /**< @brief Unable to find the feature extractor code */
#define ENO_FTR_EXTR_IN_CFG                       171 /**< @brief No Feature Extractor in Config file */
#define   EFTR_RPRCLASS_NOIMPLEMENTATION          172 /**< @brief No implementation provided */
#define EINVALID_ORDER_FEATUREFILE                173  /**< @brief Invalid order of shape-ids in the Feature file. */

#define ENUMSHAPES_NOT_SET                        174 /**< @brief Error code when the NumShapes config value is not set */
#define EUNEQUAL_LENGTH_VECTORS                   175
#define EINVALID_LOG_LEVEL                        176
#define EPROJ_NOT_DYNAMIC                         177 /**< @brief Not allowed to ADD/Delete shape to project with Fixed number of Shapes */
#define EMORPH_FVEC_SIZE_MISMATCH                 178 /*Error: Cannot perform MORPH on features vectors of different sizes*/
#define ESHAPE_RECOCLASS_NOIMPLEMENTATION         179 /*No implementation provided*/

#define ENULL_POINTER                             180 /*Null Pointer*/

#define EINVALID_X_SCALE_FACTOR                   181 /**< @brief Invalid X scale factor. Scale factor must be greater than zero */
#define EINVALID_Y_SCALE_FACTOR                   182 /**< @brief Invalid Y scale factor. Scale factor must be greater than zero */
#define ECONFIG_MDT_MISMATCH                      183 /**< @brief Parameter values in config file and MDT file do not match */
#define ENEIGHBOR_INFO_VECTOR_EMPTY               184    /* "Distance Index Pair is empty"*/
#define ERECO_RESULT_EMPTY                        185 /*"Recognize result is empty"*/
#define ESHAPE_SAMPLE_FEATURES_EMPTY              186/* "Features of input TraceGroup is empty"*/
#define ENO_TOOLKIT_VERSION                       187 /* Tookit version not specified */
#define ETRACE_INDEX_OUT_OF_BOUND                 188
#define EINVALID_CFG_FILE_ENTRY                   189
#define EKEY_NOT_FOUND                            190
#define EFEATURE_INDEX_OUT_OF_BOUND               191


#define EINVALID_FILE_HANDLE                      192
#define EFEATURE_FILE_OPEN                        193 /**< @brief Feature file open error. */
#define EFTR_DISTANCE_NOT_DEFINED                 194 /**< @brief Distance between the features not defined */
#define EINVALID_CLUSTER_ID                       195 /**< @brief Distance between the features not defined */
#define EPROTOTYPE_SET_EMPTY                      196 /**< @brief Distance between the features not defined */
#define ELOG_FILE_NOT_EXIST                       197

//LTKHierarchicalClustering Errors
#define EDATA_HYPERLINK_VEC_SIZE_MISMATCH         198 //**< @brief Size of the data objects vector and their corresponding hyperlinks vector do not match */
#define EFILE_CREATION_FAILED                     199 //**< @brief File creation failed. Invalid path or no permission. */
#define EINVALID_NUM_CLUSTERS                     200 //**< @brief Invalid number of clusters specified. The number must be greater than or equal to 1 and less than number of data objects. */
#define ENO_DATA_TO_CLUSTER                       201 //**< @brief No elements in the input data vector for clustering. */
#define EINSUFFICIENT_DATA_FOR_LMETHOD            202 //**< @brief Minimum 6 data objects are required to employ LMethod. */


#define EMODULE_NOT_IN_MEMORY                     203 /**< @brief Module index not found in module vector*/
#define EINVALID_LOG_FILENAME                     204 /*Specified Log filename is empty*/
#define ECREATE_LOGGER                                 205 /*Error creating logger*/
#define EINVALID_PROJECT_TYPE                     206 /**< @brief Project type in CFG is missing or an invalid value */

#define EEMPTY_STRING                             207  /**< @brief Empty string */
#define EEMPTY_VECTOR                             208  /**< @brief Empty vector */

#define ENON_POSITIVE_NUM                         209  /**< @brief Negative or zero value */
#define EEMPTY_WORDREC_RESULTS                    210  /**< @brief The word recogniton result vector is empty */
#define ENEGATIVE_NUM                             211  /**< @brief Negative value */


#define EINVALID_CLASS_ID                         212 //**< @brief Invalid Class ID . */

#define EINVALID_CONFIDENCE_VALUE                 213 //**< @brief Invalid Confidence Value. */

#define ENO_SHAPE_RECO_PROJECT                    214 //**< @brief Shape Recognizer Project name missing in the word recognizer config file.*/

#define EINVALID_RECOGNITION_MODE                 215 //**< @brief Unsupported recognition mode */

#define ELOGGER_LIBRARY_NOT_LOADED                216

#define ESINGLE_POINT_TRACE                       217 //**< @brief Single point trace */

//ActiveDTW Errors
#define EEMPTY_FEATUREMATRIX                      218  /**Feature Matrix is empty **/

#define EEMPTY_COVARIANCEMATRIX                   219  /**Covariance Matrix is empty **/

#define EEMPTY_CLUSTERMEAN                        220  /** Empty Cluster Mean **/

#define EEMPTY_MEANCORRECTEDDATA                  221  /**Empty Mean Corrected Data **/

#define EINVALID_RANK                             222  /** The rank value to compute eigen vectors is Invalid **/

#define EINVALID_NUM_OF_EIGENVECTORS              223  /** The number of eigen vectors is Invalid **/

#define EEMPTY_EIGENVALUES                        224  /** Eigen values are empty **/

#define EEMPTY_EIGENVECTORS                       225  /** Eigen Vectors are empty **/

#define ENUM_EIGVALUES_NOTEQUALTO_NUM_EIGVECTORS  226  /** Number of eigen values is not equal to number of eigen vectors **/

#define EPROTOYPESHAPE_INDEX_OUT_OF_BOUND         227  /** Invalid Index of Prototype Shape Accessed **/

#define EINVALID_NUM_SAMPLES                      228 /** Invalid Number of Samples in Cluster **/

#define EADAPTSCHEME_NOT_SUPPORTED                229 /** Adapt scheme not supported **/

//sub stroke
#define EINVALID_SLOPE_VECTOR_DIMENSION           230 //**< @brief Slope vector has a different dimension */
#define EINVALID_SLOPE                            231 //**< @brief Nagative Slope is not allowed */
#define ENO_SUBSTROKE                             232 //**< @brief Sample has no valied substroke */
#define EINVALID_DIRECTION_CODE                   233 //**< @brief Direction code can't be nagative*/
#define EEMPTY_SLOPE_VECTOR                       234 //**< @brif Empty slope vector*/
#define ESINGLE_SUBSTROKES                        235 //**< @brief Single substroke found from the ink file*/
#define EINVALID_FEATURE_VECTOR_DIMENSION         236 //**< @brif Invalide Feature vector dimention*/
#define ENUMBER_OF_SUBSTROKES                     237 //**< @brif Number of sub-stroke is either zero or less*/

#define EINVALID_NUM_OF_INPUT_NODE                238  /**< @brief Invalid value for number of input node. */
#define EINVALID_NUM_OF_OUTPUT_NODE               239  /**< @brief Invalid value for number of output node. */
#define EINVALID_NETWORK_LAYER                    240 /**< @brief Network layer does not specified correctly. */

#endif    //#ifndef _LTK_ERRORS_LIST_H__


