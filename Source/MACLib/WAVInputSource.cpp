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

unsigned long UCHAR_TO_ULONG_LE(unsigned char* buf)
    /* converts 4 bytes stored in little-endian format to an unsigned long */
{
    return (unsigned long)((buf[3] << 24) + (buf[2] << 16) + (buf[1] << 8) + buf[0]);
}

unsigned short UCHAR_TO_USHORT_LE(unsigned char* buf)
    /* converts 2 bytes stored in little-endian format to an unsigned short */
{
    return (unsigned short)((buf[1] << 8) + buf[0]);
}

CInputSource * CreateInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode)
{ 
    // error check the parameters
    if ((pSourceName == NULL) || (wcslen(pSourceName) == 0))
    {
        if (pErrorCode) *pErrorCode = ERROR_BAD_PARAMETER;
        return NULL;
    }

    return new CWAVInputSource(pSourceName, pwfeSource, pTotalBlocks, pHeaderBytes, pTerminatingBytes, pErrorCode);
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
    unsigned char* p = m_sFullHeader;

    // seek to the beginning (just in case)
    m_spIO->SetSeekMethod(APE_FILE_BEGIN);
    m_spIO->SetSeekPosition(0);
    m_spIO->PerformSeek();

    // get the file size
    int64 nRealFileBytes = m_spIO->GetSize();

    // get the RIFF header
    RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_HEADER)))

    // make sure the RIFF header is valid
    if (!(p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F'))
        return ERROR_INVALID_INPUT_FILE;

    // get the file size from RIFF header
    int64 nFileBytes = UCHAR_TO_ULONG_LE(p + 4) + sizeof(RIFF_HEADER);
    p += sizeof(RIFF_HEADER);

    m_nFileBytes = ape_max(nFileBytes, nRealFileBytes);

    // read the data type header
    RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(DATA_TYPE_ID_HEADER)))

    // make sure it's the right data type
    if (!(p[0] == 'W' && p[1] == 'A' && p[2] == 'V' && p[3] == 'E'))
        return ERROR_INVALID_INPUT_FILE;
    p += sizeof(DATA_TYPE_ID_HEADER);

    // find the 'fmt ' chunk
    RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))

    while (!(p[0] == 'f' && p[1] == 'm' && p[2] == 't' && p[3] == ' '))
    {
        // move the file pointer to the end of this chunk
        uint32 nChunkBytes = UCHAR_TO_ULONG_LE(p + 4);
        p += sizeof(RIFF_CHUNK_HEADER);
        RETURN_ON_ERROR(ReadSafe(m_spIO, p, nChunkBytes))
        p += nChunkBytes;

        // check again for the data chunk
        RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))
    }

    uint32 nFmtChunkBytes = UCHAR_TO_ULONG_LE(p + 4);
    p += sizeof(RIFF_CHUNK_HEADER);

    // read the format info
    RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(WAV_FORMAT_HEADER)))

    // error check the header to see if we support it
    uint16 nFormatTag = UCHAR_TO_USHORT_LE(p);
    if (nFormatTag != 1 && nFormatTag != 65534)
        return ERROR_INVALID_INPUT_FILE;

    // copy the format information to the WAVEFORMATEX passed in
    FillWaveFormatEx(&m_wfeSource, UCHAR_TO_ULONG_LE(p + 4), UCHAR_TO_USHORT_LE(p + 14), UCHAR_TO_USHORT_LE(p + 2));

    p += sizeof(WAV_FORMAT_HEADER);

    // skip over any extra data in the header
    int nWAVFormatHeaderExtra = nFmtChunkBytes - sizeof(WAV_FORMAT_HEADER);
    if (nWAVFormatHeaderExtra < 0)
        return ERROR_INVALID_INPUT_FILE;
    else {
        RETURN_ON_ERROR(ReadSafe(m_spIO, p, nWAVFormatHeaderExtra))
        p += nWAVFormatHeaderExtra;
    }
    
    // find the data chunk
    RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))

    while (!(p[0] == 'd' && p[1] == 'a' && p[2] == 't' && p[3] == 'a'))
    {
        // move the file pointer to the end of this chunk
        uint32 nChunkBytes = UCHAR_TO_ULONG_LE(p + 4);
        p += sizeof(RIFF_CHUNK_HEADER);
        RETURN_ON_ERROR(ReadSafe(m_spIO, p, nChunkBytes))
        p += nChunkBytes;

        // check again for the data chunk
        RETURN_ON_ERROR(ReadSafe(m_spIO, p, sizeof(RIFF_CHUNK_HEADER)))
    }

    // we're at the data block
    m_nDataBytes = UCHAR_TO_ULONG_LE(p + 4);
    p += sizeof(RIFF_CHUNK_HEADER);
    m_nHeaderBytes = p - m_sFullHeader;
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

    memcpy(pBuffer, m_sFullHeader, m_nHeaderBytes);

    return ERROR_SUCCESS;
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