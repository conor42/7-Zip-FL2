// Lzma2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../../C/fl2_errors.h"

#include "../Common/CWrappers.h"
#include "../Common/StreamUtils.h"

#include "Lzma2Encoder.h"

#ifdef MY_CPU_32BIT
static const UInt32 kMaxDictx86 = 1U << 26;
#endif

namespace NCompress {

namespace NLzma {

HRESULT SetLzmaProp(PROPID propID, const PROPVARIANT &prop, CLzmaEncProps &ep);

}

namespace NLzma2 {

CEncoder::CEncoder()
{
  _encoder = NULL;
  _fl2encoder = NULL;
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

#define CHECK_F(f) if (FL2_isError(f)) return E_INVALIDARG;  /* check and convert error code */

HRESULT CEncoder::SetFL2Properties(CLzma2EncProps& lzma2Props)
{
  if (_fl2encoder == NULL) {
    _fl2encoder = FL2_createCCtxMt(lzma2Props.numTotalThreads);
    if (_fl2encoder == NULL)
      return E_OUTOFMEMORY;
  }
  if (lzma2Props.lzmaProps.algo > 2) {
    if (lzma2Props.lzmaProps.algo > 3)
      return E_INVALIDARG;
    lzma2Props.lzmaProps.algo = 2;
    FL2_CCtx_setParameter(_fl2encoder, FL2_p_highCompression, 1);
    FL2_CCtx_setParameter(_fl2encoder, FL2_p_compressionLevel, lzma2Props.lzmaProps.level);
  }
  else {
    FL2_CCtx_setParameter(_fl2encoder, FL2_p_7zLevel, lzma2Props.lzmaProps.level);
  }
  dictSize = lzma2Props.lzmaProps.dictSize;
  if (!dictSize) {
    dictSize = (UInt32)1 << FL2_CCtx_setParameter(_fl2encoder, FL2_p_dictionaryLog, 0);
#ifdef MY_CPU_32BIT
	dictSize = min(dictSize, kMaxDictx86);
#endif
  }
  reduceSize = lzma2Props.lzmaProps.reduceSize;
  reduceSize += (reduceSize < (UInt64)-1); /* prevent extra buffer shift after read */
  dictSize = (UInt32)min(dictSize, reduceSize);
  unsigned dictLog = FL2_DICTLOG_MIN;
  while (((UInt32)1 << dictLog) < dictSize)
    ++dictLog;
  CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_dictionaryLog, dictLog));
  if (lzma2Props.lzmaProps.algo >= 0) {
    CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_strategy, (unsigned)lzma2Props.lzmaProps.algo));
  }
  if (lzma2Props.lzmaProps.fb > 0)
    CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_fastLength, lzma2Props.lzmaProps.fb));
  if (lzma2Props.lzmaProps.mc) {
    unsigned ml = 0;
    while (((UInt32)1 << ml) < lzma2Props.lzmaProps.mc)
      ++ml;
    CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_searchLog, ml));
  }
  if (lzma2Props.lzmaProps.lc >= 0)
    CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_literalCtxBits, lzma2Props.lzmaProps.lc));
  if (lzma2Props.lzmaProps.lp >= 0)
    CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_literalPosBits, lzma2Props.lzmaProps.lp));
  if (lzma2Props.lzmaProps.pb >= 0)
    CHECK_F(FL2_CCtx_setParameter(_fl2encoder, FL2_p_posBits, lzma2Props.lzmaProps.pb));
  FL2_CCtx_setParameter(_fl2encoder, FL2_p_omitProperties, 1);
  FL2_CCtx_setParameter(_fl2encoder, FL2_p_doXXHash, 0);
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
    return SetFL2Properties(lzma2Props);
  }
  else {
    _encoder = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
    if (!_encoder)
      return E_OUTOFMEMORY;
    return SResToHRESULT(Lzma2Enc_SetProps(_encoder, &lzma2Props));
  }
}


STDMETHODIMP CEncoder::SetCoderPropertiesOpt(const PROPID *propIDs,
  const PROPVARIANT *coderProps, UInt32 numProps)
{
  if (_encoder) for (UInt32 i = 0; i < numProps; i++)
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
  inBuffer.AllocAtLeast(dictSize);
  EncodingObjects objs = { outStream, progress, 0, 0, S_OK };
  FL2_blockBuffer block = { inBuffer, 0, 0, dictSize };
  do
  {
    FL2_shiftBlock(_fl2encoder, &block);
    size_t inSize = dictSize - block.start;
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
      if (block.end < dictSize)
        break;
    }
    else break;

  } while (err == S_OK);

  if (err == S_OK) {
    size_t cSize = FL2_endFrame_toFn(_fl2encoder, Write, &objs);
    if (FL2_isError(cSize))
      return S_FALSE;
    objs.out_processed += cSize;
    err = objs.res;
  }
  return err;
}

}
}
