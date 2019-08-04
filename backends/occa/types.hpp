// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

#ifndef CEED_OCCA_TYPES_HEADER
#define CEED_OCCA_TYPES_HEADER

#include <ceed-backend.h>
#include <occa.hpp>

#define CeedOccaFromChk(ierr) \
  do { if (ierr) return NULL; } while (0)

namespace ceed {
  namespace occa {
    typedef int (*ceedFunction)();

    class Context {
     public:
      ::occa::device device;

      static Context& from(Ceed ceed);
    };

    typedef struct {
      const CeedScalar *inputs[16];
      CeedScalar *outputs[16];
    } QFunctionFields;

    typedef enum {
      HOST_SYNC,
      DEVICE_SYNC,
      BOTH_SYNC,
      NONE_SYNC
    } VectorSyncState;
  }
}

#endif