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

#include <ceed-impl.h>
#include <string.h>
#include "ceed-ref.h"

static int CeedOperatorDestroy_Ref(CeedOperator op) {
  CeedOperator_Ref *impl = op->data;
  int ierr;

  ierr = CeedVectorDestroy(&impl->etmp); CeedChk(ierr);
  ierr = CeedVectorDestroy(&impl->qdata); CeedChk(ierr);
  ierr = CeedFree(&op->data); CeedChk(ierr);
  return 0;
}

static int CeedOperatorApply_Ref(CeedOperator op, CeedVector qdata,
                                 CeedVector ustate,
                                 CeedVector residual, CeedRequest *request) {
  CeedOperator_Ref *impl = op->data;
  CeedVector etmp;
  CeedInt Q;
  const CeedInt nc = op->basis->ndof, dim = op->basis->dim;
  CeedScalar *Eu;
  char *qd;
  int ierr;
  CeedTransposeMode lmode = CEED_NOTRANSPOSE;

  if (!impl->etmp) {
    ierr = CeedVectorCreate(op->ceed,
                            nc * op->Erestrict->nelem * op->Erestrict->elemsize,
                            &impl->etmp); CeedChk(ierr);
    // etmp is allocated when CeedVectorGetArray is called below
  }
  etmp = impl->etmp;
  if (op->qf->inmode & ~CEED_EVAL_WEIGHT) {
    ierr = CeedElemRestrictionApply(op->Erestrict, CEED_NOTRANSPOSE,
                                    nc, lmode, ustate, etmp,
                                    CEED_REQUEST_IMMEDIATE); CeedChk(ierr);
  }
  ierr = CeedBasisGetNumQuadraturePoints(op->basis, &Q); CeedChk(ierr);
  ierr = CeedVectorGetArray(etmp, CEED_MEM_HOST, &Eu); CeedChk(ierr);
  ierr = CeedVectorGetArray(qdata, CEED_MEM_HOST, (CeedScalar**)&qd);
  CeedChk(ierr);
  for (CeedInt e=0; e<op->Erestrict->nelem; e++) {
    CeedScalar BEu[Q*nc*(dim+2)], BEv[Q*nc*(dim+2)], *out[5] = {0,0,0,0,0};
    const CeedScalar *in[5] = {0,0,0,0,0};
    // TODO: quadrature weights can be computed just once
    ierr = CeedBasisApply(op->basis, CEED_NOTRANSPOSE, op->qf->inmode,
                          &Eu[e*op->Erestrict->elemsize*nc], BEu);
    CeedChk(ierr);
    CeedScalar *u_ptr = BEu, *v_ptr = BEv;
    if (op->qf->inmode & CEED_EVAL_INTERP) { in[0] = u_ptr; u_ptr += Q*nc; }
    if (op->qf->inmode & CEED_EVAL_GRAD) { in[1] = u_ptr; u_ptr += Q*nc*dim; }
    if (op->qf->inmode & CEED_EVAL_WEIGHT) { in[4] = u_ptr; u_ptr += Q; }
    if (op->qf->outmode & CEED_EVAL_INTERP) { out[0] = v_ptr; v_ptr += Q*nc; }
    if (op->qf->outmode & CEED_EVAL_GRAD) { out[1] = v_ptr; v_ptr += Q*nc*dim; }
    ierr = CeedQFunctionApply(op->qf, &qd[e*Q*op->qf->qdatasize], Q, in, out);
    CeedChk(ierr);
    ierr = CeedBasisApply(op->basis, CEED_TRANSPOSE, op->qf->outmode, BEv,
                          &Eu[e*op->Erestrict->elemsize*nc]);
    CeedChk(ierr);
  }
  ierr = CeedVectorRestoreArray(etmp, &Eu); CeedChk(ierr);
  ierr = CeedVectorRestoreArray(qdata, (CeedScalar**)&qd); CeedChk(ierr);
  if (residual) {
    CeedScalar *res;
    ierr = CeedVectorGetArray(residual, CEED_MEM_HOST, &res); CeedChk(ierr);
    for (int i = 0; i < residual->length; i++)
      res[i] = (CeedScalar)0;
    ierr = CeedElemRestrictionApply(op->Erestrict, CEED_TRANSPOSE,
                                    nc, lmode, etmp, residual,
                                    CEED_REQUEST_IMMEDIATE); CeedChk(ierr);
    ierr = CeedVectorRestoreArray(residual, &res); CeedChk(ierr);
  }
  if (request != CEED_REQUEST_IMMEDIATE && request != CEED_REQUEST_ORDERED)
    *request = NULL;
  return 0;
}

static int CeedOperatorGetQData_Ref(CeedOperator op, CeedVector *qdata) {
  CeedOperator_Ref *impl = op->data;
  int ierr;

  if (!impl->qdata) {
    CeedInt Q;
    ierr = CeedBasisGetNumQuadraturePoints(op->basis, &Q); CeedChk(ierr);
    ierr = CeedVectorCreate(op->ceed,
                            op->Erestrict->nelem * Q
                            * op->qf->qdatasize / sizeof(CeedScalar),
                            &impl->qdata); CeedChk(ierr);
  }
  *qdata = impl->qdata;
  return 0;
}

int CeedOperatorCreate_Ref(CeedOperator op) {
  CeedOperator_Ref *impl;
  int ierr;

  ierr = CeedCalloc(1, &impl); CeedChk(ierr);
  op->data = impl;
  op->Destroy = CeedOperatorDestroy_Ref;
  op->Apply = CeedOperatorApply_Ref;
  op->GetQData = CeedOperatorGetQData_Ref;
  return 0;
}