// Copyright (c) 2014, Intel Corporation
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Intel Corporation nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <Windows.h>
#include <string>

using namespace std;

typedef bool (*IPGInitialize) ();
typedef bool (*IPGGetNumNodes) (int *nNodes);
typedef bool (*IPGGetNumMsrs) (int *nMsr);
typedef bool (*IPGGetMsrName) (int iMsr, wchar_t *szName);
typedef bool (*IPGGetMsrFunc) (int iMsr, int *funcID);
typedef bool (*IPGGetIAFrequency) (int iNode, int *freqInMHz);
typedef bool (*IPGGetTDP) (int iNode, double *TDP);
typedef bool (*IPGGetMaxTemperature) (int iNode, int *degreeC);
typedef bool (*IPGGetTemperature) (int iNode, int *degreeC);
typedef bool (*IPGReadSample) ();
typedef bool (*IPGGetSysTime) (SYSTEMTIME *pSysTime);
typedef bool (*IPGGetRDTSC) (UINT64 *TSC);
typedef bool (*IPGGetTimeInterval) (double *offset);
typedef bool (*IPGGetBaseFrequency) (int iNode, double *baseFrequency);
typedef bool (*IPGGetPowerData) (int iNode, int iMSR, double *result, int *nResult);
typedef bool (*IPGStartLog) (wchar_t *szFileName);
typedef bool (*IPGStopLog) ();

class CIntelPowerGadgetLib
{
public:
	CIntelPowerGadgetLib(void);
	~CIntelPowerGadgetLib(void);
	
	bool IntelEnergyLibInitialize(void);
	bool GetNumNodes(int * nNodes);
	bool GetNumMsrs(int *nMsrs);
	bool GetMsrName(int iMsr, wchar_t *szName);
	bool GetMsrFunc(int iMsr, int *funcID);
	bool GetIAFrequency(int iNode, int *freqInMHz);
	bool GetTDP(int iNode, double *TDP);
	bool GetMaxTemperature(int iNode, int *degreeC);
	bool GetTemperature(int iNode, int *degreeC);
	bool ReadSample();
	bool GetSysTime(SYSTEMTIME *sysTime);
	bool GetRDTSC(UINT64 *TSC);
	bool GetTimeInterval(double *offset);
	bool GetBaseFrequency(int iNode, double *baseFrequency);
	bool GetPowerData(int iNode, int iMSR, double *results, int *nResult);
	bool StartLog(wchar_t *szFilename);
	bool StopLog();
	string GetLastError();

private:
	IPGInitialize pInitialize;
	IPGGetNumNodes pGetNumNodes;
	IPGGetNumMsrs pGetNumMsrs;
	IPGGetMsrName pGetMsrName;
	IPGGetMsrFunc pGetMsrFunc;
	IPGGetIAFrequency pGetIAFrequency;
	IPGGetTDP pGetTDP;
	IPGGetMaxTemperature pGetMaxTemperature;
	IPGGetTemperature pGetTemperature;
	IPGReadSample pReadSample; 
	IPGGetSysTime pGetSysTime;
	IPGGetRDTSC pGetRDTSC;
	IPGGetTimeInterval pGetTimeInterval;
	IPGGetBaseFrequency pGetBaseFrequency;
	IPGGetPowerData pGetPowerData;
	IPGStartLog pStartLog;
	IPGStopLog pStopLog;
};

