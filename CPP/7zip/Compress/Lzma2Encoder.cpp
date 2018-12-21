// Lzma2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../../../C/fast-lzma2/fl2_errors.h"

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
  _encoder = Lzma2Enc_Create(&g_AlignedAlloc, &g_BigAlloc);
  if (!_encoder)
    throw 1;
}

CEncoder::~CEncoder()
{
  if (_encoder)
    Lzma2Enc_Destroy(_encoder);
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
  return SResToHRESULT(Lzma2Enc_SetProps(_encoder, &lzma2Props));
}


STDMETHODIMP CEncoder::SetCoderPropertiesOpt(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps)
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    PROPID propID = propIDs[i];
    if (propID == NCoderPropID::kExpectedDataSize)
      if (prop.vt == VT_UI8)
        Lzma2Enc_SetDataSize(_encoder, prop.uhVal.QuadPart);
  }
  return S_OK;
}


STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  Byte prop = Lzma2Enc_WriteProperties(_encoder);
  return WriteStream(outStream, &prop, 1);
}


#define RET_IF_WRAP_ERROR(wrapRes, sRes, sResErrorCode) \
  if (wrapRes != S_OK /* && (sRes == SZ_OK || sRes == sResErrorCode) */) return wrapRes;

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
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
  
CFastEncoder::CFastEncoder()
{
  _encoder = NULL;
  reduceSize = 0;
}

CFastEncoder::~CFastEncoder()
{
  if (_encoder)
    FL2_freeCStream(_encoder);
}


#define CHECK_F(f) if (FL2_isError(f)) return E_INVALIDARG;  /* check and convert error code */

STDMETHODIMP CFastEncoder::SetCoderProperties(const PROPID *propIDs,
  const PROPVARIANT *coderProps, UInt32 numProps)
{
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props));
  }
  if (_encoder == NULL) {
    _encoder = FL2_createCStreamMt(lzma2Props.numTotalThreads, 1);
    if (_encoder == NULL)
      return E_OUTOFMEMORY;
  }
  if (lzma2Props.lzmaProps.algo > 2) {
    if (lzma2Props.lzmaProps.algo > 3)
      return E_INVALIDARG;
    lzma2Props.lzmaProps.algo = 2;
    FL2_CCtx_setParameter(_encoder, FL2_p_highCompression, 1);
    FL2_CCtx_setParameter(_encoder, FL2_p_compressionLevel, lzma2Props.lzmaProps.level);
  }
  else {
    FL2_CCtx_setParameter(_encoder, FL2_p_compressionLevel, lzma2Props.lzmaProps.level);
  }
  dictSize = lzma2Props.lzmaProps.dictSize;
  if (!dictSize) {
    dictSize = (UInt32)FL2_CCtx_getParameter(_encoder, FL2_p_dictionarySize);
  }
  reduceSize = lzma2Props.lzmaProps.reduceSize;
  reduceSize += (reduceSize < (UInt64)-1); /* prevent extra buffer shift after read */
  dictSize = (UInt32)min(dictSize, reduceSize);
  dictSize = max(dictSize, FL2_DICTSIZE_MIN);
  CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_dictionarySize, dictSize));
  if (lzma2Props.lzmaProps.algo >= 0) {
    CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_strategy, (unsigned)lzma2Props.lzmaProps.algo));
  }
  if (lzma2Props.lzmaProps.fb > 0)
    CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_fastLength, lzma2Props.lzmaProps.fb));
  if (lzma2Props.lzmaProps.mc) {
    unsigned ml = 0;
    while (((UInt32)1 << ml) < lzma2Props.lzmaProps.mc)
      ++ml;
    CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_searchLog, ml));
  }
  if (lzma2Props.lzmaProps.lc >= 0)
    CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_literalCtxBits, lzma2Props.lzmaProps.lc));
  if (lzma2Props.lzmaProps.lp >= 0)
    CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_literalPosBits, lzma2Props.lzmaProps.lp));
  if (lzma2Props.lzmaProps.pb >= 0)
    CHECK_F(FL2_CCtx_setParameter(_encoder, FL2_p_posBits, lzma2Props.lzmaProps.pb));
  FL2_CCtx_setParameter(_encoder, FL2_p_omitProperties, 1);
  size_t res = FL2_initCStream(_encoder, 0);
  if (FL2_isError(res)) {
    if (FL2_getErrorCode(res) == FL2_error_memory_allocation)
      return E_OUTOFMEMORY;
    return S_FALSE;
  }
  return S_OK;
}


#define LZMA2_DIC_SIZE_FROM_PROP(p) (((UInt32)2 | ((p) & 1)) << ((p) / 2 + 11))

STDMETHODIMP CFastEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  Byte prop;
  unsigned i;
  for (i = 0; i < 40; i++)
    if (dictSize <= LZMA2_DIC_SIZE_FROM_PROP(i))
      break;
  prop = (Byte)i;
  return WriteStream(outStream, &prop, 1);
}


STDMETHODIMP CFastEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
  const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
  HRESULT err = S_OK;
  UInt64 out_processed = 0;
  do
  {
    FL2_outBuffer dict;
    FL2_getDictionaryBuffer(_encoder, &dict);
    size_t inSize = dict.size - dict.pos;
    err = ReadStream(inStream, (BYTE*)dict.dst + dict.pos, &inSize);
    if (err != S_OK)
      break;

    size_t res = FL2_updateDictionary(_encoder, inSize);
    if (FL2_isError(res))
      return S_FALSE;

    if (!res)
      out_processed += FL2_remainingOutputSize(_encoder);

    if (progress && (inSize || !res)) {
      UInt64 in_processed = FL2_getCStreamProgress(_encoder);
      err = progress->SetRatioInfo(&in_processed, &out_processed);
      if (err != S_OK) {
        FL2_cancelOperation(_encoder);
        break;
      }
    }

    if (!res) do {
      FL2_inBuffer cbuf;
      if (!FL2_getNextCStreamBuffer(_encoder, &cbuf))
        break;
      err = WriteStream(outStream, (BYTE*)cbuf.src + cbuf.pos, cbuf.size - cbuf.pos);

    } while (err == S_OK);

    if (inSize < dict.size - dict.pos)
      break;

  } while (err == S_OK);

  if (err == S_OK) {
    size_t res;
    do {
      res = FL2_endStream(_encoder, NULL);
      if (FL2_isError(res))
        return S_FALSE;

      if(res) do {
        FL2_inBuffer cbuf;
        if (!FL2_getNextCStreamBuffer(_encoder, &cbuf))
          break;
        err = WriteStream(outStream, (BYTE*)cbuf.src + cbuf.pos, cbuf.size - cbuf.pos);
        if (err != S_OK) {
          FL2_cancelOperation(_encoder);
          break;
        }

      } while (err == S_OK);

    } while (res);
  }
  return err;
}

}}
