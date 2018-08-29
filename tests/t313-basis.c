/// @file
/// Test grad with a 2D Simplex non-tensor H1 basis
/// \test Test grad with a 2D Simplex non-tensor H1 basis
#include <ceed.h>
#include <math.h>
#include "t310-basis.h"

double feval(double x1, double x2) {
  return x1*x1 + x2*x2 + x1*x2 + 1;
}

double dfeval(double x1, double x2) {
  return 2*x1 + x2;
}

int main(int argc, char **argv) {
  Ceed ceed;
  const CeedInt P = 6, Q = 4, dim = 2;
  CeedBasis b;
  CeedScalar qref[dim*Q], qweight[Q];
  CeedScalar interp[P*Q], grad[dim*P*Q];
  CeedScalar xq[] = {0.2, 0.6, 1./3., 0.2, 0.2, 0.2, 1./3., 0.6};
  CeedScalar xr[] = {0., 0.5, 1., 0., 0.5, 0., 0., 0., 0., 0.5, 0.5, 1.};
  CeedScalar in[P], out[dim*Q], value;

  buildmats(qref, qweight, interp, grad);

  CeedInit(argv[1], &ceed);
  CeedBasisCreateH1(ceed, CEED_TRIANGLE, 1, P, Q, interp, grad, qref, qweight,
                    &b);

  // Interpolate function to quadrature points
  for (int i=0; i<P; i++)
    in[i] = feval(xr[0*P+i], xr[1*P+i]);
  CeedBasisApply(b, 1, CEED_NOTRANSPOSE, CEED_EVAL_GRAD, in, out);

  // Check values at quadrature points
  for (int i=0; i<Q; i++) {
    value = dfeval(xq[0*Q+i], xq[1*Q+i]);
    if (fabs(out[0*Q+i] - value) > 1e-10)
      printf("[%d] %f != %f\n", i, out[0*Q+i], value);
    value = dfeval(xq[1*Q+i], xq[0*Q+i]);
    if (fabs(out[1*Q+i] - value) > 1e-10)
      printf("[%d] %f != %f\n", i, out[1*Q+i], value);
  }

  CeedBasisDestroy(&b);
  CeedDestroy(&ceed);
  return 0;
}
