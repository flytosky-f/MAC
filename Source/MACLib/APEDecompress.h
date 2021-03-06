#pragma once

#include "UnBitArrayBase.h"
#include "MACLib.h"
#include "Prepare.h"
#include "CircleBuffer.h"

namespace APE
{

class CUnBitArray;
class CPrepare;
class CAPEInfo;
class IPredictorDecompress;

class CAPEDecompress : public IAPEDecompress
{
public:
    CAPEDecompress(int * pErrorCode, CAPEInfo * pAPEInfo, int64 nStartBlock = -1, int64 nFinishBlock = -1);
    ~CAPEDecompress();

    int GetData(char * pBuffer, int64 nBlocks, int64 * pBlocksRetrieved);
    int Seek(int64 nBlockOffset);

    int64 GetInfo(APE_DECOMPRESS_FIELDS Field, int64 nParam1 = 0, int64 nParam2 = 0);

protected:
    // file info
    int64 m_nBlockAlign;
    int64 m_nCurrentFrame;
    
    // start / finish information
    int64 m_nStartBlock;
    int64 m_nFinishBlock;
    int64 m_nCurrentBlock;
    bool m_bIsRanged;
    bool m_bDecompressorInitialized;

    // decoding tools    
    CPrepare m_Prepare;
    WAVEFORMATEX m_wfeInput;
    unsigned int m_nCRC;
    unsigned int m_nStoredCRC;
    int m_nSpecialCodes;
    int64 * m_paryChannelData;
    
    int SeekToFrame(int64 nFrameIndex);
    void DecodeBlocksToFrameBuffer(int64 nBlocks);
    int FillFrameBuffer();
    void StartFrame();
    void EndFrame();
    int InitializeDecompressor();

    // more decoding components
    CSmartPtr<CAPEInfo> m_spAPEInfo;
    CSmartPtr<CUnBitArrayBase> m_spUnBitArray;
    UNBIT_ARRAY_STATE m_aryBitArrayStates[32];
    IPredictorDecompress * m_aryPredictor[32];
    int64 m_nLastX;
    
    // decoding buffer
    bool m_bErrorDecodingCurrentFrame;
    bool m_bLegacyMode;
    int64 m_nErrorDecodingCurrentFrameOutputSilenceBlocks;
    int64 m_nCurrentFrameBufferBlock;
    int64 m_nFrameBufferFinishedBlocks;
    CCircleBuffer m_cbFrameBuffer;
};

}
