// Copyright (c) 2017-2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory. LLNL-CODE-734707.
// All Rights reserved. See files LICENSE and NOTICE for details.
//
// This file is part of CEED, a collection of benchmarks, miniapps, software
// libraries and APIs for efficient high-order finite element and spectral
// element discretizations for exascale applications. For more information and
// source code availability see http://github.com/ceed.
//
// The CEED research is supported by the Exascale Computing Project 17-SC-20-SC,
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.

#include "ceed-magma.h"

static int CeedElemRestrictionApply_Magma(CeedElemRestriction r,
    CeedTransposeMode tmode, CeedVector u, CeedVector v, CeedRequest *request) {

  int ierr;
  CeedElemRestriction_Magma *impl;
  ierr = CeedElemRestrictionGetData(r, &impl); CeedChk(ierr);

  Ceed ceed;
  ierr = CeedElemRestrictionGetCeed(r, &ceed); CeedChk(ierr);

  Ceed_Magma *data;
  ierr = CeedGetData(ceed, &data); CeedChk(ierr);

  CeedInt nelem;
  CeedElemRestrictionGetNumElements(r, &nelem);

  CeedInt esize;
  CeedElemRestrictionGetElementSize(r, &esize);

  CeedInt ncomp;
  CeedElemRestrictionGetNumComponents(r, &ncomp);

  const CeedScalar *du;
  CeedScalar *dv;

  ierr = CeedVectorGetArrayRead(u, CEED_MEM_DEVICE, &du); CeedChk(ierr);
  ierr = CeedVectorGetArray(v, CEED_MEM_DEVICE, &dv); CeedChk(ierr);

  bool isStrided;
  ierr = CeedElemRestrictionIsStrided(r, &isStrided); CeedChk(ierr);

  if (isStrided) {  // Strided Restriction

    CeedInt strides[3];
    CeedInt *dstrides;
    ierr = magma_malloc( (void **)&dstrides,
                         3 * sizeof(CeedInt)); CeedChk(ierr);

    // Check to see if we should use magma Q-/E-Vector layout
    //  (dimension = slowest index, then component, then element,
    //    then node)
    bool backendstrides;
    ierr = CeedElemRestrictionHasBackendStrides(r, &backendstrides);
    CeedChk(ierr);

    if (backendstrides) {

      strides[0] = 1;             // node stride
      strides[1] = esize * nelem; //component stride
      strides[2] = esize;         //element stride
      magma_setvector(3, sizeof(CeedInt), strides, 1, dstrides, 1, data->queue);

    } else {

      // Get the new strides
      ierr = CeedElemRestrictionGetStrides(r, &strides); CeedChk(ierr);
      magma_setvector(3, sizeof(CeedInt), strides, 1, dstrides, 1, data->queue);
    }

    // Perform strided restriction with dstrides
    if (tmode == CEED_TRANSPOSE) {
      magma_writeDofsStrided(ncomp, esize, nelem, dstrides, du, dv,
                             data->queue);
    } else {
      magma_readDofsStrided(ncomp, esize, nelem, dstrides, du, dv,
                            data->queue);
    }

    ierr = magma_free(dstrides);  CeedChk(ierr);

  } else { // Offsets array provided, standard restriction

    CeedInt compstride;
    ierr = CeedElemRestrictionGetCompStride(r, &compstride); CeedChk(ierr);

    if (tmode == CEED_TRANSPOSE) {
      magma_writeDofsOffset(ncomp, compstride, esize, nelem, impl->doffsets,
                            du, dv, data->queue);
    } else {
      magma_readDofsOffset(ncomp, compstride, esize, nelem, impl->doffsets,
                           du, dv, data->queue);
    }

  }

  ierr = CeedVectorRestoreArrayRead(u, &du); CeedChk(ierr);
  ierr = CeedVectorRestoreArray(v, &dv); CeedChk(ierr);

  return 0;
}

int CeedElemRestrictionApplyBlock_Magma(CeedElemRestriction r, CeedInt block,
                                        CeedTransposeMode tmode, CeedVector u,
                                        CeedVector v, CeedRequest *request) {
  int ierr;
  Ceed ceed;
  ierr = CeedElemRestrictionGetCeed(r, &ceed); CeedChk(ierr);
  // LCOV_EXCL_START
  return CeedError(ceed, 1, "Backend does not implement blocked restrictions");
  // LCOV_EXCL_STOP
}

static int CeedElemRestrictionGetOffsets_Magma(CeedElemRestriction rstr,
    CeedMemType mtype, const CeedInt **offsets) {
  int ierr;
  CeedElemRestriction_Magma *impl;
  ierr = CeedElemRestrictionGetData(rstr, &impl); CeedChk(ierr);

  switch (mtype) {
  case CEED_MEM_HOST:
    *offsets = impl->offsets;
    break;
  case CEED_MEM_DEVICE:
    *offsets = impl->doffsets;
    break;
  }
  return 0;
}

static int CeedElemRestrictionDestroy_Magma(CeedElemRestriction r) {
  int ierr;
  CeedElemRestriction_Magma *impl;
  ierr = CeedElemRestrictionGetData(r, &impl); CeedChk(ierr);

  // Free if we own the data
  if (impl->own_) {
    ierr = magma_free_pinned(impl->offsets); CeedChk(ierr);
    ierr = magma_free(impl->doffsets);       CeedChk(ierr);
  } else if (impl->down_) {
    ierr = magma_free(impl->doffsets);       CeedChk(ierr);
  }
  ierr = CeedFree(&impl); CeedChk(ierr);
  return 0;
}

int CeedElemRestrictionCreate_Magma(CeedMemType mtype, CeedCopyMode cmode,
                                    const CeedInt *offsets, CeedElemRestriction r) {
  int ierr;
  Ceed ceed;
  ierr = CeedElemRestrictionGetCeed(r, &ceed); CeedChk(ierr);

  Ceed_Magma *data;
  ierr = CeedGetData(ceed, &data); CeedChk(ierr);

  CeedInt elemsize, nelem;
  ierr = CeedElemRestrictionGetNumElements(r, &nelem); CeedChk(ierr);
  ierr = CeedElemRestrictionGetElementSize(r, &elemsize); CeedChk(ierr);
  CeedInt size = elemsize * nelem;

  CeedElemRestriction_Magma *impl;
  ierr = CeedCalloc(1, &impl); CeedChk(ierr);

  impl->doffsets = NULL;
  impl->offsets  = NULL;
  impl->own_ = 0;
  impl->down_= 0;

  if (mtype == CEED_MEM_HOST) {
    // memory is on the host; own_ = 0
    switch (cmode) {
    case CEED_COPY_VALUES:
      impl->own_ = 1;

      if (offsets != NULL) {

        ierr = magma_malloc( (void **)&impl->doffsets,
                             size * sizeof(CeedInt)); CeedChk(ierr);
        ierr = magma_malloc_pinned( (void **)&impl->offsets,
                                    size * sizeof(CeedInt)); CeedChk(ierr);
        memcpy(impl->offsets, offsets, size * sizeof(CeedInt));

        magma_setvector(size, sizeof(CeedInt), offsets, 1, impl->doffsets, 1,
                        data->queue);
      }
      break;
    case CEED_OWN_POINTER:
      impl->own_ = 1;

      if (offsets != NULL) {
        ierr = magma_malloc( (void **)&impl->doffsets,
                             size * sizeof(CeedInt)); CeedChk(ierr);
        // TODO: possible problem here is if we are passed non-pinned memory;
        //       (as we own it, lter in destroy, we use free for pinned memory).
        impl->offsets = (CeedInt *)offsets;

        magma_setvector(size, sizeof(CeedInt), offsets, 1, impl->doffsets, 1,
                        data->queue);
      }
      break;
    case CEED_USE_POINTER:
      if (offsets != NULL) {
        ierr = magma_malloc( (void **)&impl->doffsets,
                             size * sizeof(CeedInt)); CeedChk(ierr);
        magma_setvector(size, sizeof(CeedInt), offsets, 1, impl->doffsets, 1,
                        data->queue);
      }
      impl->down_ = 1;
      impl->offsets  = (CeedInt *)offsets;
    }
  } else if (mtype == CEED_MEM_DEVICE) {
    // memory is on the device; own = 0
    switch (cmode) {
    case CEED_COPY_VALUES:
      ierr = magma_malloc( (void **)&impl->doffsets,
                           size * sizeof(CeedInt)); CeedChk(ierr);
      ierr = magma_malloc_pinned( (void **)&impl->offsets,
                                  size * sizeof(CeedInt)); CeedChk(ierr);
      impl->own_ = 1;

      if (offsets)
        magma_getvector(size, sizeof(CeedInt), impl->doffsets, 1, (void *)offsets, 1,
                        data->queue);
      break;
    case CEED_OWN_POINTER:
      impl->doffsets = (CeedInt *)offsets;
      ierr = magma_malloc_pinned( (void **)&impl->offsets,
                                  size * sizeof(CeedInt)); CeedChk(ierr);
      impl->own_ = 1;

      break;
    case CEED_USE_POINTER:
      impl->doffsets = (CeedInt *)offsets;
      impl->offsets  = NULL;
    }

  } else
    return CeedError(ceed, 1, "Only MemType = HOST or DEVICE supported");

  ierr = CeedElemRestrictionSetData(r, impl); CeedChk(ierr);
  CeedInt layout[3] = {1, elemsize*nelem, elemsize};
  ierr = CeedElemRestrictionSetELayout(r, layout); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "ElemRestriction", r, "Apply",
                                CeedElemRestrictionApply_Magma); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "ElemRestriction", r, "ApplyBlock",
                                CeedElemRestrictionApplyBlock_Magma);
  CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "ElemRestriction", r, "GetOffsets",
                                CeedElemRestrictionGetOffsets_Magma);
  CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "ElemRestriction", r, "Destroy",
                                CeedElemRestrictionDestroy_Magma); CeedChk(ierr);
  return 0;
}

int CeedElemRestrictionCreateBlocked_Magma(const CeedMemType mtype,
    const CeedCopyMode cmode, const CeedInt *offsets,
    const CeedElemRestriction r) {
  int ierr;
  Ceed ceed;
  ierr = CeedElemRestrictionGetCeed(r, &ceed); CeedChk(ierr);
  // LCOV_EXCL_START
  return CeedError(ceed, 1, "Backend does not implement blocked restrictions");
  // LCOV_EXCL_STOP

  return 0;
}
