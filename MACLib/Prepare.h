#pragma once

namespace APE
{

#define SPECIAL_FRAME_MONO_SILENCE              1
#define SPECIAL_FRAME_LEFT_SILENCE              1
#define SPECIAL_FRAME_RIGHT_SILENCE             2
#define SPECIAL_FRAME_PSEUDO_STEREO             4

/*****************************************************************************
Manage the preparation stage of compression and decompression

Tasks:

1) convert data to 32-bit
2) convert L,R to X,Y
3) calculate the CRC
4) do simple analysis
5) check for the peak value
*****************************************************************************/

class IPredictorDecompress;

class CPrepare
{
public:
    int Prepare(const unsigned char * pRawData, int nBytes, const WAVEFORMATEX * pWaveFormatEx, int * pOutputX, int * pOutputY, unsigned int * pCRC, int * pSpecialCodes, intn * pPeakLevel);
    void Unprepare(int X, int Y, const WAVEFORMATEX * pWaveFormatEx, unsigned char * pOutput);
#ifdef APE_BACKWARDS_COMPATIBILITY
	int UnprepareOld(int * pInputX, int *pInputY, intn nBlocks, const WAVEFORMATEX * pWaveFormatEx, unsigned char * pRawData, unsigned int * pCRC, int * pSpecialCodes, intn nFileVersion);
#endif
};

}
