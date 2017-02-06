/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE
********************************************************************************/

/************************************************************************
 * FILE DESCR: Definitions for Version Info module.
 *
 * CONTENTS:
 *
 * AUTHOR:     VijayaKumara M.
 *
 * DATE:       Aug 02, 2005
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/

#ifndef _VERSION_H_
#define _VERSION_H_

#define MAJOR_VERSION    4
#define MINOR_VERSION    0
#define BUGFIX_VERSION   0


/**
 * This method get the version numbers for the variable which are passed to the
 * function as its arguments.
 *
 * @param iMajor - An integer variable for Major number.
 * @param iMinor - An integer variable for Minor number.
 * @param iBugFix - An integer variable for BugFix number.
 *
 */

void getVersion(int& iMajor, int& iMinor, int& iBugFix)
{
     iMajor=MAJOR_VERSION;
     iMinor=MINOR_VERSION;
     iBugFix=BUGFIX_VERSION;
}

#endif //#ifndef _VERSION_H_

