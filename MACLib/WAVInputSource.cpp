#include "All.h"
#include "WAVInputSource.h"
#include IO_HEADER_FILE
#include "MACLib.h"
#include "GlobalFunctions.h"

namespace APE
{

struct RIFF_HEADER 
{
    char cRIFF[4];          // the characters 'RIFF' indicating that it's a RIFF file
    uint32 nBytes;          // the number of bytes following this header
};

struct DATA_TYPE_ID_HEADER 
{
    char cDataTypeID[4];      // should equal 'WAVE' for a WAV file
};

struct WAV_FORMAT_HEADER
{
    uint16 nFormatTag;            // the format of the WAV...should equal 1 for a PCM file
    uint16 nChannels;             // the number of channels
    uint32 nSamplesPerSecond;     // the number of samples per second
    uint32 nBytesPerSecond;       // the bytes per second
    uint16 nBlockAlign;           // block alignment
    uint16 nBitsPerSample;        // the number of bits per sample
};

struct RIFF_CHUNK_HEADER
{
    char cChunkLabel[4];      // should equal "data" indicating the data chunk
    uint32 nChunkBytes;       // the bytes of the chunk  
};


CInputSource * CreateInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode)
{ 
    // error check the parameters
    if ((pSourceName == NULL) || (wcslen(pSourceName) == 0))
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return NULL;
    }

    // get the extension
    const wchar_t * pExtension = &pSourceName[wcslen(pSourceName)];
    while ((pExtension > pSourceName) && (*pExtension != '.'))
        pExtension--;

    // create the proper input source
    if (StringIsEqual(pExtension, L".wav", false))
    {
        if (pErrorCode) *pErrorCode = ERROR_SUCCESS;
        return new CWAVInputSource(pSourceName, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode);
    }
    else
    {
        if (pErrorCode) *pErrorCode = ERROR_INVALID_INPUT_FILE;
        return NULL;
    }
}

CWAVInputSource::CWAVInputSource(CIO * pIO, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode)
    : CInputSource(pIO, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode)
{
    m_bIsValid = false;

    if (pIO == NULL || pwfeSource == NULL)
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return;
    }
    
    m_spIO.Assign(pIO, false, false);

    int nResult = AnalyzeSource();
    if (nResult == ERROR_SUCCESS)
    {
        // fill in the parameters
        if (pwfeSource) memcpy(pwfeSource, &m_wfeSource, sizeof(WAVEFORMATEX));
        if (pTotalBlocks) *pTotalBlocks = int(m_nDataBytes / m_wfeSource.nBlockAlign);
        if (pHeaderBytes) *pHeaderBytes = m_nHeaderBytes;
        if (pTerminatingBytes) *pTerminatingBytes = m_nTerminatingBytes;

        m_bIsValid = true;
    }
    
    if (pErrorCode) *pErrorCode = nResult;
}

CWAVInputSource::CWAVInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode)
    : CInputSource(pSourceName, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode)
{
    m_bIsValid = false;

    if (pSourceName == NULL || pwfeSource == NULL)
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return;
    }
    
    m_spIO.Assign(new IO_CLASS_NAME);
    if (m_spIO->Open(pSourceName, true) != ERROR_SUCCESS)
    {
        m_spIO.Delete();
        if (pErrorCode) *pErrorCode = ERROR_INVALID_INPUT_FILE;
        return;
    }

    int nResult = AnalyzeSource();
    if (nResult == ERROR_SUCCESS)
    {
        // fill in the parameters
        if (pwfeSource) memcpy(pwfeSource, &m_wfeSource, sizeof(WAVEFORMATEX));
        if (pTotalBlocks) *pTotalBlocks = int(m_nDataBytes / m_wfeSource.nBlockAlign);
        if (pHeaderBytes) *pHeaderBytes = m_nHeaderBytes;
        if (pTerminatingBytes) *pTerminatingBytes = m_nTerminatingBytes;

        m_bIsValid = true;
    }
    
    if (pErrorCode) *pErrorCode = nResult;
}

CWAVInputSource::~CWAVInputSource()
{
}

int CWAVInputSource::AnalyzeSource()
{
    // seek to the beginning (just in case)
	m_spIO->SetSeekMethod(APE_FILE_BEGIN);
	m_spIO->SetSeekPosition(0);
	m_spIO->PerformSeek();
    
    // get the file size
    m_nFileBytes = m_spIO->GetSize();

    // get the RIFF header
    RIFF_HEADER RIFFHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFHeader, sizeof(RIFFHeader))) 

    // make sure the RIFF header is valid
    if (!(RIFFHeader.cRIFF[0] == 'R' && RIFFHeader.cRIFF[1] == 'I' && RIFFHeader.cRIFF[2] == 'F' && RIFFHeader.cRIFF[3] == 'F')) 
        return ERROR_INVALID_INPUT_FILE;

    // read the data type header
    DATA_TYPE_ID_HEADER DataTypeIDHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &DataTypeIDHeader, sizeof(DataTypeIDHeader))) 
    
    // make sure it's the right data type
    if (!(DataTypeIDHeader.cDataTypeID[0] == 'W' && DataTypeIDHeader.cDataTypeID[1] == 'A' && DataTypeIDHeader.cDataTypeID[2] == 'V' && DataTypeIDHeader.cDataTypeID[3] == 'E')) 
        return ERROR_INVALID_INPUT_FILE;

    // find the 'fmt ' chunk
    RIFF_CHUNK_HEADER RIFFChunkHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
    
    while (!(RIFFChunkHeader.cChunkLabel[0] == 'f' && RIFFChunkHeader.cChunkLabel[1] == 'm' && RIFFChunkHeader.cChunkLabel[2] == 't' && RIFFChunkHeader.cChunkLabel[3] == ' ')) 
    {
        // move the file pointer to the end of this chunk
		m_spIO->SetSeekMethod(APE_FILE_CURRENT);
		m_spIO->SetSeekPosition(RIFFChunkHeader.nChunkBytes);
		m_spIO->PerformSeek();

        // check again for the data chunk
        RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
    }
    
    // read the format info
    WAV_FORMAT_HEADER WAVFormatHeader;
    RETURN_ON_ERROR(ReadSafe(m_spIO, &WAVFormatHeader, sizeof(WAVFormatHeader))) 

    // error check the header to see if we support it
    if (WAVFormatHeader.nFormatTag != 1)
        return ERROR_INVALID_INPUT_FILE;

    // copy the format information to the WAVEFORMATEX passed in
    FillWaveFormatEx(&m_wfeSource, WAVFormatHeader.nSamplesPerSecond, WAVFormatHeader.nBitsPerSample, WAVFormatHeader.nChannels);

    // skip over any extra data in the header
    int nWAVFormatHeaderExtra = RIFFChunkHeader.nChunkBytes - sizeof(WAVFormatHeader);
    if (nWAVFormatHeaderExtra < 0)
        return ERROR_INVALID_INPUT_FILE;
	else
	{
		m_spIO->SetSeekMethod(APE_FILE_CURRENT);
		m_spIO->SetSeekPosition(nWAVFormatHeaderExtra);
		m_spIO->PerformSeek();
	}
    
    // find the data chunk
    RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 

    while (!(RIFFChunkHeader.cChunkLabel[0] == 'd' && RIFFChunkHeader.cChunkLabel[1] == 'a' && RIFFChunkHeader.cChunkLabel[2] == 't' && RIFFChunkHeader.cChunkLabel[3] == 'a')) 
    {
        // move the file pointer to the end of this chunk
		m_spIO->SetSeekMethod(APE_FILE_CURRENT);
		m_spIO->SetSeekPosition(RIFFChunkHeader.nChunkBytes);
		m_spIO->PerformSeek();

        // check again for the data chunk
        RETURN_ON_ERROR(ReadSafe(m_spIO, &RIFFChunkHeader, sizeof(RIFFChunkHeader))) 
    }

    // we're at the data block
    m_nHeaderBytes = m_spIO->GetPosition();
    m_nDataBytes = RIFFChunkHeader.nChunkBytes;
	if (m_nDataBytes > (m_nFileBytes - m_nHeaderBytes))
		m_nDataBytes = m_nFileBytes - m_nHeaderBytes;

    // make sure the data bytes is a whole number of blocks
    if ((m_nDataBytes % m_wfeSource.nBlockAlign) != 0)
        return ERROR_INVALID_INPUT_FILE;

    // calculate the terminating byts
    m_nTerminatingBytes = m_nFileBytes - m_nDataBytes - m_nHeaderBytes;
    
    // we made it this far, everything must be cool
    return ERROR_SUCCESS;
}

int CWAVInputSource::GetData(unsigned char * pBuffer, int nBlocks, int * pBlocksRetrieved)
{
    if (!m_bIsValid) return ERROR_UNDEFINED;

    int nBytes = (m_wfeSource.nBlockAlign * nBlocks);
    unsigned int nBytesRead = 0;

    if (m_spIO->Read(pBuffer, nBytes, &nBytesRead) != ERROR_SUCCESS)
        return ERROR_IO_READ;

    if (pBlocksRetrieved) *pBlocksRetrieved = (nBytesRead / m_wfeSource.nBlockAlign);

    return ERROR_SUCCESS;
}

int CWAVInputSource::GetHeaderData(unsigned char * pBuffer)
{
    if (!m_bIsValid) return ERROR_UNDEFINED;

    int nResult = ERROR_SUCCESS;

    if (m_nHeaderBytes > 0)
    {
        int64 nOriginalFileLocation = m_spIO->GetPosition();

		m_spIO->SetSeekMethod(APE_FILE_BEGIN);
		m_spIO->SetSeekPosition(0);
		m_spIO->PerformSeek();
        
        unsigned int nBytesRead = 0;
        int nReadRetVal = m_spIO->Read(pBuffer, uint32(m_nHeaderBytes), &nBytesRead);

        if ((nReadRetVal != ERROR_SUCCESS) || (m_nHeaderBytes != int(nBytesRead)))
        {
            nResult = ERROR_UNDEFINED;
        }

		m_spIO->SetSeekMethod(APE_FILE_BEGIN);
		m_spIO->SetSeekPosition(nOriginalFileLocation);
		m_spIO->PerformSeek();
    }

    return nResult;
}

int CWAVInputSource::GetTerminatingData(unsigned char * pBuffer)
{
    if (!m_bIsValid) return ERROR_UNDEFINED;

    int nResult = ERROR_SUCCESS;

    if (m_nTerminatingBytes > 0)
    {
        int64 nOriginalFileLocation = m_spIO->GetPosition();

		m_spIO->SetSeekMethod(APE_FILE_END);
		m_spIO->SetSeekPosition(-m_nTerminatingBytes);
		m_spIO->PerformSeek();
        
        unsigned int nBytesRead = 0;
        int nReadRetVal = m_spIO->Read(pBuffer, uint32(m_nTerminatingBytes), &nBytesRead);

        if ((nReadRetVal != ERROR_SUCCESS) || (m_nTerminatingBytes != int(nBytesRead)))
        {
            nResult = ERROR_UNDEFINED;
        }

		m_spIO->SetSeekMethod(APE_FILE_BEGIN);
		m_spIO->SetSeekPosition(nOriginalFileLocation);
		m_spIO->PerformSeek();
    }

    return nResult;
}

}