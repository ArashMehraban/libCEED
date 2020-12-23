#include "navierstokes.h"

PetscErrorCode NS_ADVECTION2D(problemData *problem) {

  PetscFunctionBeginUser;
  problem->dim                       = 2;
  problem->qdatasizeVol              = 5;
  problem->qdatasizeSur              = 3;
  problem->setupVol                  = Setup2d;
  problem->setupVol_loc              = Setup2d_loc;
  problem->setupSur                  = SetupBoundary2d;
  problem->setupSur_loc              = SetupBoundary2d_loc;
  problem->ics                       = ICsAdvection2d;
  problem->ics_loc                   = ICsAdvection2d_loc;
  problem->applyVol_rhs              = Advection2d;
  problem->applyVol_rhs_loc          = Advection2d_loc;
  problem->applyVol_ifunction        = IFunction_Advection2d;
  problem->applyVol_ifunction_loc    = IFunction_Advection2d_loc;
  problem->applySur                  = Advection2d_Sur;
  problem->applySur_loc              = Advection2d_Sur_loc;
  problem->bc                        = BC_ADVECTION2D;
  problem->non_zero_time             = PETSC_TRUE;
  PetscFunctionReturn(0);
}

PetscErrorCode BC_ADVECTION2D(DM dm, SimpleBC bc, void *ctxSetupData) {

  PetscErrorCode ierr;
  PetscInt len;
  PetscBool flg;
  MPI_Comm comm = PETSC_COMM_WORLD;

  // Default boundary conditions
  //   slip bc on all faces and no wall bc
  bc->nslip[0] = bc->nslip[1] = bc->nslip[2] = 2;
  bc->slips[0][0] = 5;
  bc->slips[0][1] = 6;
  bc->slips[1][0] = 3;
  bc->slips[1][1] = 4;
  bc->slips[2][0] = 1;
  bc->slips[2][1] = 2;

  PetscFunctionBeginUser;
  // Parse command line options
  ierr = PetscOptionsBegin(comm, NULL, "Options for advection2d",
                           NULL); CHKERRQ(ierr);
  ierr = PetscOptionsIntArray("-bc_wall",
                              "Use wall boundary conditions on this list of faces",
                              NULL, bc->walls,
                              (len = sizeof(bc->walls) / sizeof(bc->walls[0]),
                               &len), &flg); CHKERRQ(ierr);
  if (flg) {
    bc->nwall = len;
    // Using a no-slip wall disables automatic slip walls (they must be set explicitly)
    bc->nslip[0] = bc->nslip[1] = bc->nslip[2] = 0;
  }
  for (PetscInt j=0; j<3; j++) {
    const char *flags[3] = {"-bc_slip_x", "-bc_slip_y", "-bc_slip_z"};
    ierr = PetscOptionsIntArray(flags[j],
                                "Use slip boundary conditions on this list of faces",
                                NULL, bc->slips[j],
                                (len = sizeof(bc->slips[j]) / sizeof(bc->slips[j][0]),
                                 &len), &flg); CHKERRQ(ierr);
    if (flg) {
      bc->nslip[j] = len;
      bc->userbc = PETSC_TRUE;
    }
  }
  ierr = PetscOptionsEnd(); CHKERRQ(ierr);

  {
    // Slip boundary conditions
    PetscInt comps[1] = {1};
    ierr = DMAddBoundary(dm, DM_BC_ESSENTIAL, "slipx", "Face Sets", 0,
                         1, comps, (void(*)(void))NULL, NULL, bc->nslip[0],
                         bc->slips[0], ctxSetupData); CHKERRQ(ierr);
    comps[0] = 2;
    ierr = DMAddBoundary(dm, DM_BC_ESSENTIAL, "slipy", "Face Sets", 0,
                         1, comps, (void(*)(void))NULL, NULL, bc->nslip[1],
                         bc->slips[1], ctxSetupData); CHKERRQ(ierr);
    comps[0] = 3;
    ierr = DMAddBoundary(dm, DM_BC_ESSENTIAL, "slipz", "Face Sets", 0,
                         1, comps, (void(*)(void))NULL, NULL, bc->nslip[2],
                         bc->slips[2], ctxSetupData); CHKERRQ(ierr);
  }
  if (bc->userbc == PETSC_TRUE) {
    for (PetscInt c = 0; c < 3; c++) {
      for (PetscInt s = 0; s < bc->nslip[c]; s++) {
        for (PetscInt w = 0; w < bc->nwall; w++) {
          if (bc->slips[c][s] == bc->walls[w])
            SETERRQ1(PETSC_COMM_SELF, PETSC_ERR_ARG_WRONG,
                     "Boundary condition already set on face %D!\n",
                     bc->walls[w]);
        }
      }
    }
  }
  {
    // Wall boundary conditions
    //   zero energy density and zero flux
    PetscInt comps[1] = {4};
    ierr = DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall", "Face Sets", 0,
                         1, comps, (void(*)(void))Exact_Advection2d, NULL,
                         bc->nwall, bc->walls, ctxSetupData); CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}