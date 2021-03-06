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

#include <string.h>
#include "ceed-opt.h"

//------------------------------------------------------------------------------
// Backend Destroy
//------------------------------------------------------------------------------
static int CeedDestroy_Opt(Ceed ceed) {
  int ierr;
  Ceed_Opt *data;
  ierr = CeedGetData(ceed, &data); CeedChk(ierr);
  ierr = CeedFree(&data); CeedChk(ierr);

  return 0;
}

//------------------------------------------------------------------------------
// Backend Init
//------------------------------------------------------------------------------
static int CeedInit_Opt_Serial(const char *resource, Ceed ceed) {
  int ierr;
  if (strcmp(resource, "/cpu/self")
      && strcmp(resource, "/cpu/self/opt/serial"))
    // LCOV_EXCL_START
    return CeedError(ceed, 1, "Opt backend cannot use resource: %s", resource);
  // LCOV_EXCL_STOP
  ierr = CeedSetDeterministic(ceed, true); CeedChk(ierr);

  // Create reference CEED that implementation will be dispatched
  //   through unless overridden
  Ceed ceedref;
  CeedInit("/cpu/self/ref/serial", &ceedref);
  ierr = CeedSetDelegate(ceed, ceedref); CeedChk(ierr);

  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "Destroy",
                                CeedDestroy_Opt); CeedChk(ierr);
  ierr = CeedSetBackendFunction(ceed, "Ceed", ceed, "OperatorCreate",
                                CeedOperatorCreate_Opt); CeedChk(ierr);

  // Set blocksize
  Ceed_Opt *data;
  ierr = CeedCalloc(1, &data); CeedChk(ierr);
  data->blksize = 1;
  ierr = CeedSetData(ceed, data); CeedChk(ierr);

  return 0;
}

//------------------------------------------------------------------------------
// Backend Register
//------------------------------------------------------------------------------
CEED_INTERN int CeedRegister_Opt_Serial(void) {
  return CeedRegister("/cpu/self/opt/serial", CeedInit_Opt_Serial, 45);
}

//------------------------------------------------------------------------------
