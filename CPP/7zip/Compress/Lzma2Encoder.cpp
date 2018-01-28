// Lzma2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../../../C/fl2_errors.h"

#include "../Common/CWrappers.h"
#include "../Common/StreamUtils.h"

#include "Lzma2Encoder.h"

namespace NCompress {

namespace NLzma {

HRESULT SetLzmaProp(PROPID propID, const PROPVARIANT &prop, CLzmaEncProps &ep);

}

namespace NLzma2 {

CEncoder::CEncoder()
{
  _encoder = NULL;
  _fl2encoder = NULL;
  inBuffer = NULL;
  dictAlloc = 0;
  reduceSize = 0;
}

CEncoder::~CEncoder()
{
  if (_encoder)
    Lzma2Enc_Destroy(_encoder);
  if (_fl2encoder)
      FL2_freeCCtx(_fl2encoder);
}


HRESULT SetLzma2Prop(PROPID propID, const PROPVARIANT &prop, CLzma2EncProps &lzma2Props)
{
  switch (propID)
  {
    case NCoderPropID::kBlockSize:
    {
      if (prop.vt == VT_UI4)
        lzma2Props.blockSize = prop.ulVal;
      else if (prop.vt == VT_UI8)
        lzma2Props.blockSize = prop.uhVal.QuadPart;
      else
        return E_INVALIDARG;
      break;
    }
    case NCoderPropID::kNumThreads:
      if (prop.vt != VT_UI4) return E_INVALIDARG; lzma2Props.numTotalThreads = (int)(prop.ulVal); break;
    default:
      RINOK(NLzma::SetLzmaProp(propID, prop, lzma2Props.lzmaProps));
  }
  return S_OK;
}


STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps)
{
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props));
  }
  if (lzma2Props.lzmaProps.btMode < 0 || lzma2Props.lzmaProps.btMode > 1) {
      if (_fl2encoder == NULL) {
          _fl2encoder = FL2_createCCtxMt(lzma2Props.numTotalThreads);
          if (_fl2encoder == NULL)
              return E_OUTOFMEMORY;
      }
      FL2_CCtx_setParameter(_fl2encoder, FL2_p_7zLevel, lzma2Props.lzmaProps.level);
      dictSize = lzma2Props.lzmaProps.dictSize;
      if (!dictSize) {
          dictSize = (UInt32)1 << FL2_CCtx_setParameter(_fl2encoder, FL2_p_dictionaryLog, 0);
      }
      reduceSize = lzma2Props.lzmaProps.reduceSize;
      dictSize = (UInt32)min(dictSize, reduceSize);
      unsigned dictLog = FL2_DICTLOG_MIN;
      while (((UInt32)1 << dictLog) < dictSize)
          ++dictLog;
      FL2_CCtx_setParameter(_fl2encoder, FL2_p_dictionaryLog, dictLog);
      if (lzma2Props.lzmaProps.algo >= 0) {
        FL2_CCtx_setParameter(_fl2encoder, FL2_p_strategy, (unsigned)lzma2Props.lzmaProps.algo);
      }
      if (lzma2Props.lzmaProps.fb > 0)
          FL2_CCtx_setParameter(_fl2encoder, FL2_p_fastLength, lzma2Props.lzmaProps.fb);
      if (lzma2Props.lzmaProps.mc) {
          unsigned ml = 0;
          while (((UInt32)1 << ml) < lzma2Props.lzmaProps.mc)
              ++ml;
          FL2_CCtx_setParameter(_fl2encoder, FL2_p_searchLog, ml);
      }
      if (lzma2Props.lzmaProps.lc >= 0)
          FL2_CCtx_setParameter(_fl2encoder, FL2_p_literalCtxBits, lzma2Props.lzmaProps.lc);
      if (lzma2Props.lzmaProps.lp >= 0)
          FL2_CCtx_setParameter(_fl2encoder, FL2_p_literalPosBits, lzma2Props.lzmaProps.lp);
      if (lzma2Props.lzmaProps.pb >= 0)
          FL2_CCtx_setParameter(_fl2encoder, FL2_p_posBits, lzma2Props.lzmaProps.pb);
      FL2_CCtx_setParameter(_fl2encoder, FL2_p_omitProperties, 1);
      FL2_CCtx_setParameter(_fl2encoder, FL2_p_doXXHash, 0);
  }
  else {
      _encoder = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
      if (!_encoder)
          return E_OUTOFMEMORY;
      return SResToHRESULT(Lzma2Enc_SetProps(_encoder, &lzma2Props));
  }
  return S_OK;
}


STDMETHODIMP CEncoder::SetCoderPropertiesOpt(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps)
{
  if(_encoder) for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    PROPID propID = propIDs[i];
    if (propID == NCoderPropID::kExpectedDataSize)
      if (prop.vt == VT_UI8)
        Lzma2Enc_SetDataSize(_encoder, prop.uhVal.QuadPart);
  }
  return S_OK;
}

#define LZMA2_DIC_SIZE_FROM_PROP(p) (((UInt32)2 | ((p) & 1)) << ((p) / 2 + 11))

STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
    Byte prop;
    if (_fl2encoder != NULL) {
        unsigned i;
        for (i = 0; i < 40; i++)
            if (dictSize <= LZMA2_DIC_SIZE_FROM_PROP(i))
                break;
        prop = (Byte)i;
    }
    else {
        prop = Lzma2Enc_WriteProperties(_encoder);
    }
  return WriteStream(outStream, &prop, 1);
}


#define RET_IF_WRAP_ERROR(wrapRes, sRes, sResErrorCode) \
  if (wrapRes != S_OK /* && (sRes == SZ_OK || sRes == sResErrorCode) */) return wrapRes;

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
    if (_fl2encoder != NULL)
        return FL2Code(inStream, outStream, 0, 0, progress);
  CSeqInStreamWrap inWrap;
  CSeqOutStreamWrap outWrap;
  CCompressProgressWrap progressWrap;

  inWrap.Init(inStream);
  outWrap.Init(outStream);
  progressWrap.Init(progress);

  SRes res = Lzma2Enc_Encode2(_encoder,
      &outWrap.vt, NULL, NULL,
      &inWrap.vt, NULL, 0,
      progress ? &progressWrap.vt : NULL);

  RET_IF_WRAP_ERROR(inWrap.Res, res, SZ_ERROR_READ)
  RET_IF_WRAP_ERROR(outWrap.Res, res, SZ_ERROR_WRITE)
  RET_IF_WRAP_ERROR(progressWrap.Res, res, SZ_ERROR_PROGRESS)

  return SResToHRESULT(res);
}
 
typedef struct
{
    ISequentialOutStream* outStream;
    ICompressProgressInfo* progress;
    UInt64 in_processed;
    UInt64 out_processed;
    HRESULT res;
} EncodingObjects;

static int FL2LIB_CALL Progress(size_t done, void* opaque)
{
    EncodingObjects* p = (EncodingObjects*)opaque;
    if (p && p->progress) {
        UInt64 in_processed = p->in_processed + done;
        p->res = p->progress->SetRatioInfo(&in_processed, &p->out_processed);
        return p->res != S_OK;
    }
    return 0;
}

static int FL2LIB_CALL Write(const void* src, size_t srcSize, void* opaque)
{
    EncodingObjects* p = (EncodingObjects*)opaque;
    p->res = WriteStream(p->outStream, src, srcSize);
    return p->res != S_OK;
}

HRESULT CEncoder::FL2Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
    HRESULT err = S_OK;
    EncodingObjects objs = { outStream, progress, 0, 0, S_OK };
    try {
        if (inBuffer == NULL || dictAlloc < dictSize) {
            delete[] inBuffer;
            dictAlloc = dictSize + (dictSize >= reduceSize); /* prevent overlap copy */
            inBuffer = new BYTE[dictAlloc];
        }
        FL2_blockBuffer block = { inBuffer, 0, 0, dictAlloc };
        do
        {
            size_t inSize = dictAlloc - block.start;
            err = ReadStream(inStream, inBuffer + block.start, &inSize);
            if (err != S_OK)
                break;
            block.end += inSize;
            if (inSize) {
                size_t cSize = FL2_compressCCtxBlock_toFn(_fl2encoder, Write, &objs, &block, Progress);
                if (FL2_isError(cSize)) {
                    if (FL2_getErrorCode(cSize) == FL2_error_memory_allocation)
                        return E_OUTOFMEMORY;
                    return objs.res != S_OK ? objs.res : S_FALSE;
                }
                if (objs.res != S_OK)
                    return objs.res;
                objs.out_processed += cSize;
                objs.in_processed += inSize;
                if (progress) {
                    err = progress->SetRatioInfo(&objs.in_processed, &objs.out_processed);
                    if (err != S_OK)
                        break;
                }
                if (block.end == dictAlloc) {
                    FL2_shiftBlock(_fl2encoder, &block);
                }
                else {
                    objs.out_processed += FL2_endFrame_toFn(_fl2encoder, Write, &objs);
                    break;
                }
            }
            else break;

        } while (err == S_OK);
    }
    catch (...) {
        err = E_OUTOFMEMORY;
    }
    return err;
}

}}
