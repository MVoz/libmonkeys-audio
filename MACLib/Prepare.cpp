#include "All.h"
#include "Prepare.h"
#include "CRC.h"

namespace APE
{

int CPrepare::Prepare(const unsigned char * pRawData, int nBytes, const WAVEFORMATEX * pWaveFormatEx, int * pOutputX, int *pOutputY, unsigned int *pCRC, int *pSpecialCodes, intn *pPeakLevel)
{
    // error check the parameters
    if (pRawData == NULL || pWaveFormatEx == NULL)
        return ERROR_BAD_PARAMETER;

    // initialize the pointers that got passed in
    *pCRC = 0xFFFFFFFF;
    *pSpecialCodes = 0;

    // variables
    uint32 CRC = 0xFFFFFFFF;
    const int nTotalBlocks = nBytes / pWaveFormatEx->nBlockAlign;
    int R,L;

    // calculate CRC
    CRC = CRC_update(CRC, pRawData, nTotalBlocks * pWaveFormatEx->nChannels * (pWaveFormatEx->wBitsPerSample / 8));

    // the prepare code

    if (pWaveFormatEx->wBitsPerSample == 8) 
    {
        if (pWaveFormatEx->nChannels == 2) 
        {
            for (int nBlockIndex = 0; nBlockIndex < nTotalBlocks; nBlockIndex++) 
            {
                R = (int) (*((unsigned char *) pRawData++) - 128);
                L = (int) (*((unsigned char *) pRawData++) - 128);
                
                // check the peak
                if (labs(L) > *pPeakLevel)
                    *pPeakLevel = labs(L);
                if (labs(R) > *pPeakLevel)
                    *pPeakLevel = labs(R);

                // convert to x,y
                pOutputY[nBlockIndex] = L - R;
                pOutputX[nBlockIndex] = R + (pOutputY[nBlockIndex] / 2);
            }
        }
        else if (pWaveFormatEx->nChannels == 1) 
        {
            for (int nBlockIndex = 0; nBlockIndex < nTotalBlocks; nBlockIndex++) 
            {
                R = (int) (*((unsigned char *) pRawData++) - 128);
                
                // check the peak
                if (labs(R) > *pPeakLevel)
                    *pPeakLevel = labs(R);

                // convert to x,y
                pOutputX[nBlockIndex] = R;
            }
        }
    }
    else if (pWaveFormatEx->wBitsPerSample == 24) 
    {
        if (pWaveFormatEx->nChannels == 2) 
        {
            for (int nBlockIndex = 0; nBlockIndex < nTotalBlocks; nBlockIndex++) 
            {
                uint32 nTemp = 0;
                
                nTemp |= (*pRawData++ << 0);
                nTemp |= (*pRawData++ << 8);
                nTemp |= (*pRawData++ << 16);

                if (nTemp & 0x800000)
                    R = (int) (nTemp & 0x7FFFFF) - 0x800000;
                else
                    R = (int) (nTemp & 0x7FFFFF);

                nTemp = 0;

                nTemp |= (*pRawData++ << 0);                
                nTemp |= (*pRawData++ << 8);                
                nTemp |= (*pRawData++ << 16);
                                
                if (nTemp & 0x800000)
                    L = (int) (nTemp & 0x7FFFFF) - 0x800000;
                else
                    L = (int) (nTemp & 0x7FFFFF);

                // check the peak
                if (labs(L) > *pPeakLevel)
                    *pPeakLevel = labs(L);
                if (labs(R) > *pPeakLevel)
                    *pPeakLevel = labs(R);

                // convert to x,y
                pOutputY[nBlockIndex] = L - R;
                pOutputX[nBlockIndex] = R + (pOutputY[nBlockIndex] / 2);

            }
        }
        else if (pWaveFormatEx->nChannels == 1) 
        {
            for (int nBlockIndex = 0; nBlockIndex < nTotalBlocks; nBlockIndex++) 
            {
                uint32 nTemp = 0;
                
                nTemp |= (*pRawData++ << 0);                
                nTemp |= (*pRawData++ << 8);                
                nTemp |= (*pRawData++ << 16);
                
                if (nTemp & 0x800000)
                    R = (int) (nTemp & 0x7FFFFF) - 0x800000;
                else
                    R = (int) (nTemp & 0x7FFFFF);
    
                // check the peak
                if (labs(R) > *pPeakLevel)
                    *pPeakLevel = labs(R);

                // convert to x,y
                pOutputX[nBlockIndex] = R;
            }
        }
    }
    else 
    {
        if (pWaveFormatEx->nChannels == 2) 
        {
            int LPeak = 0;
            int RPeak = 0;
            int nBlockIndex = 0;
            for (nBlockIndex = 0; nBlockIndex < nTotalBlocks; nBlockIndex++) 
            {
                R = (int) *((int16 *) pRawData); pRawData += 2;
                L = (int) *((int16 *) pRawData); pRawData += 2;

                // check the peak
                if (labs(L) > LPeak)
                    LPeak = labs(L);
                if (labs(R) > RPeak)
                    RPeak = labs(R);

                // convert to x,y
                pOutputY[nBlockIndex] = L - R;
                pOutputX[nBlockIndex] = R + (pOutputY[nBlockIndex] / 2);
            }

            if (LPeak == 0) { *pSpecialCodes |= SPECIAL_FRAME_LEFT_SILENCE; }
            if (RPeak == 0) { *pSpecialCodes |= SPECIAL_FRAME_RIGHT_SILENCE; }
            if (ape_max(LPeak, RPeak) > *pPeakLevel) 
            {
                *pPeakLevel = ape_max(LPeak, RPeak);
            }

            // check for pseudo-stereo files
            nBlockIndex = 0;
            while (pOutputY[nBlockIndex++] == 0) 
            {
                if (nBlockIndex == (nBytes / 4)) 
                {
                    *pSpecialCodes |= SPECIAL_FRAME_PSEUDO_STEREO;
                    break;
                }
            }
        }
        else if (pWaveFormatEx->nChannels == 1) 
        {
            int nPeak = 0;
            for (int nBlockIndex = 0; nBlockIndex < nTotalBlocks; nBlockIndex++) 
            {
                R = (int) *((int16 *) pRawData); pRawData += 2;
                
                // check the peak
                if (labs(R) > nPeak)
                    nPeak = labs(R);

                //convert to x,y
                pOutputX[nBlockIndex] = R;
            }

            if (nPeak > *pPeakLevel)
                *pPeakLevel = nPeak;
            if (nPeak == 0) { *pSpecialCodes |= SPECIAL_FRAME_MONO_SILENCE; }
        }
    }

    CRC = CRC ^ 0xFFFFFFFF;

    // add the special code
    CRC >>= 1;

    if (*pSpecialCodes != 0) 
    {
        CRC |= ((unsigned int) (1 << 31));
    }

    *pCRC = CRC;

    return ERROR_SUCCESS;
}

void CPrepare::Unprepare(int X, int Y, const WAVEFORMATEX * pWaveFormatEx, unsigned char * pOutput)
{
    // decompress and convert from (x,y) -> (l,r)
    if (pWaveFormatEx->nChannels == 2) 
    {
        if (pWaveFormatEx->wBitsPerSample == 16) 
        {
            // get the right and left values
            int nR = X - (Y / 2);
            int nL = nR + Y;

            // error check (for overflows)
            if ((nR < -32768) || (nR > 32767) || (nL < -32768) || (nL > 32767))
            {
                throw(-1);
            }

            *(int16 *) pOutput = (int16) nR; pOutput += 2;
            *(int16 *) pOutput = (int16) nL; pOutput += 2;
        }
        else if (pWaveFormatEx->wBitsPerSample == 8) 
        {
            unsigned char R = (unsigned char) ((X - (Y / 2) + 128));
            *pOutput++ = R;
            *pOutput++ = (unsigned char) (R + Y);
        }
        else if (pWaveFormatEx->wBitsPerSample == 24) 
        {
            int32 RV, LV;

            RV = X - (Y / 2);
            LV = RV + Y;
            
            uint32 nTemp = 0;
            if (RV < 0)
                nTemp = ((uint32) (RV + 0x800000)) | 0x800000;
            else
                nTemp = (uint32) RV;    
            
            *pOutput++ = (unsigned char) ((nTemp >> 0) & 0xFF);
            *pOutput++ = (unsigned char) ((nTemp >> 8) & 0xFF);
            *pOutput++ = (unsigned char) ((nTemp >> 16) & 0xFF);

            nTemp = 0;
            if (LV < 0)
                nTemp = ((uint32) (LV + 0x800000)) | 0x800000;
            else
                nTemp = (uint32) LV;    
            
            *pOutput++ = (unsigned char) ((nTemp >> 0) & 0xFF);
            *pOutput++ = (unsigned char) ((nTemp >> 8) & 0xFF);
            *pOutput++ = (unsigned char) ((nTemp >> 16) & 0xFF);
        }
    }
    else if (pWaveFormatEx->nChannels == 1) 
    {
        if (pWaveFormatEx->wBitsPerSample == 16) 
        {
            int16 R = int16(X);
                
            *(int16 *) pOutput = (int16) R; pOutput += 2;
        }
        else if (pWaveFormatEx->wBitsPerSample == 8) 
        {
            unsigned char R = (unsigned char) (X + 128);
            *pOutput++ = R;
        }
        else if (pWaveFormatEx->wBitsPerSample == 24) 
        {
            int32 RV = X;
            
            uint32 nTemp = 0;
            if (RV < 0)
                nTemp = ((uint32) (RV + 0x800000)) | 0x800000;
            else
                nTemp = (uint32) RV;    
            
            *pOutput++ = (unsigned char) ((nTemp >> 0) & 0xFF);
            *pOutput++ = (unsigned char) ((nTemp >> 8) & 0xFF);
            *pOutput++ = (unsigned char) ((nTemp >> 16) & 0xFF);
        }
    }
}

#ifdef APE_BACKWARDS_COMPATIBILITY

int CPrepare::UnprepareOld(int *pInputX, int *pInputY, intn nBlocks, const WAVEFORMATEX *pWaveFormatEx, unsigned char *pRawData, unsigned int *pCRC, int *pSpecialCodes, intn nFileVersion)
{
	// decompress and convert from (x,y) -> (l,r)
	if (pWaveFormatEx->nChannels == 2) 
	{
		// convert the x,y data to raw data
		if (pWaveFormatEx->wBitsPerSample == 16) 
		{
			int16 R;
			unsigned char *Buffer = &pRawData[0];
			int *pX = pInputX;
			int *pY = pInputY;

			for (; pX < &pInputX[nBlocks]; pX++, pY++) 
			{
				R = int16(*pX - (*pY / 2));

				*(int16 *) Buffer = (int16) R; Buffer += 2;
				*(int16 *) Buffer = (int16) (R + *pY); Buffer += 2;
			}
		}
		else if (pWaveFormatEx->wBitsPerSample == 8) 
		{
			unsigned char *R = (unsigned char *) &pRawData[0];
			unsigned char *L = (unsigned char *) &pRawData[1];

			if (nFileVersion > 3830) 
			{
				for (int SampleIndex = 0; SampleIndex < nBlocks; SampleIndex++, L+=2, R+=2) 
				{
					*R = (unsigned char) (pInputX[SampleIndex] - (pInputY[SampleIndex] / 2) + 128);
					*L = (unsigned char) (*R + pInputY[SampleIndex]);
				}
			}
			else 
			{
				for (int SampleIndex = 0; SampleIndex < nBlocks; SampleIndex++, L+=2, R+=2)
				{
					*R = (unsigned char) (pInputX[SampleIndex] - (pInputY[SampleIndex] / 2));
					*L = (unsigned char) (*R + pInputY[SampleIndex]);
				}
			}
		}
		else if (pWaveFormatEx->wBitsPerSample == 24) 
		{
			unsigned char *Buffer = (unsigned char *) &pRawData[0];
			int32 RV, LV;

			for (int SampleIndex = 0; SampleIndex < nBlocks; SampleIndex++)
			{
				RV = pInputX[SampleIndex] - (pInputY[SampleIndex] / 2);
				LV = RV + pInputY[SampleIndex];

				uint32 nTemp = 0;
				if (RV < 0)
					nTemp = ((uint32) (RV + 0x800000)) | 0x800000;
				else
					nTemp = (uint32) RV;    

				*Buffer++ = (unsigned char) ((nTemp >> 0) & 0xFF);
				*Buffer++ = (unsigned char) ((nTemp >> 8) & 0xFF);
				*Buffer++ = (unsigned char) ((nTemp >> 16) & 0xFF);

				nTemp = 0;
				if (LV < 0)
					nTemp = ((uint32) (LV + 0x800000)) | 0x800000;
				else
					nTemp = (uint32) LV;    

				*Buffer++ = (unsigned char) ((nTemp >> 0) & 0xFF);
				*Buffer++ = (unsigned char) ((nTemp >> 8) & 0xFF);
				*Buffer++ = (unsigned char) ((nTemp >> 16) & 0xFF);
			}
		}
	}
	else if (pWaveFormatEx->nChannels == 1) 
	{
		// convert to raw data
		if (pWaveFormatEx->wBitsPerSample == 8) 
		{
			unsigned char *R = (unsigned char *) &pRawData[0];

			if (nFileVersion > 3830) 
			{
				for (int SampleIndex = 0; SampleIndex < nBlocks; SampleIndex++, R++)
					*R = (unsigned char) (pInputX[SampleIndex] + 128);
			}
			else 
			{
				for (int SampleIndex = 0; SampleIndex < nBlocks; SampleIndex++, R++)
					*R = (unsigned char) (pInputX[SampleIndex]);
			}
		}
		else if (pWaveFormatEx->wBitsPerSample == 24) 
		{
			unsigned char *Buffer = (unsigned char *) &pRawData[0];
			int32 RV;
			for (int SampleIndex = 0; SampleIndex<nBlocks; SampleIndex++) 
			{
				RV = pInputX[SampleIndex];

				uint32 nTemp = 0;
				if (RV < 0)
					nTemp = ((uint32) (RV + 0x800000)) | 0x800000;
				else
					nTemp = (uint32) RV;    

				*Buffer++ = (unsigned char) ((nTemp >> 0) & 0xFF);
				*Buffer++ = (unsigned char) ((nTemp >> 8) & 0xFF);
				*Buffer++ = (unsigned char) ((nTemp >> 16) & 0xFF);
			}
		}
		else 
		{
			unsigned char *Buffer = &pRawData[0];

			for (int SampleIndex = 0; SampleIndex < nBlocks; SampleIndex++) 
			{
				*(int16 *) Buffer = (int16) (pInputX[SampleIndex]); Buffer += 2;
			}
		}
	}

	// calculate CRC
	uint32 CRC = 0xFFFFFFFF;

	CRC = CRC_update(CRC, pRawData, int(nBlocks * pWaveFormatEx->nChannels * (pWaveFormatEx->wBitsPerSample / 8)));
	CRC = CRC ^ 0xFFFFFFFF;

	*pCRC = CRC;

	return 0;
}

#endif // #ifdef APE_BACKWARDS_COMPATIBILITY

}