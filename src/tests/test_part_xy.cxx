
#include "psc_testing.h"
#include "psc_push_particles.h"
#include <mrc_profile.h>
#include <mrc_params.h>

#include <stdio.h>
#include <mpi.h>

int main(int argc, char** argv)
{
#if 0
  psc_testing_init(&argc, &argv);

  struct psc_case *_case = psc_create_test_xy();
  psc_push_particles_set_type(ppsc->push_particles, "fortran");
  psc_case_setup(_case);
  struct psc_mparticles *particles = ppsc->particles;
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  psc_save_particles_ref(ppsc, particles);
  psc_save_fields_ref(ppsc, ppsc->flds);
  psc_case_destroy(_case);

  _case = psc_create_test_xy();
  psc_push_particles_set_type(ppsc->push_particles, "generic_c");
  psc_case_setup(_case);
  particles = ppsc->particles;
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-7, "push_part_xy -- generic_c");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-7, 3);
  psc_case_destroy(_case);

#ifdef USE_SSE2
  _case = psc_create_test_xy();
  psc_push_particles_set_type(ppsc->push_particles, "sse2");
  psc_case_setup(_case);
  particles = ppsc->particles;
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-7, "push_part_xy -- sse2");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-6, 3);
  psc_case_destroy(_case);
#endif

#ifdef USE_CBE
  _case = psc_create_test_xy();
  psc_push_particles_set_type(ppsc->push_particles, "cbe");
  psc_case_setup(_case);
  particles = &ppsc->particles;
  psc_push_particles_run(ppsc->push_particles, particles, ppsc->flds);
  psc_check_particles_ref(ppsc, particles, 1e-7, "push_part_xy -- cbe");
  psc_check_currents_ref(ppsc, ppsc->flds, 1e-6, 3);
  psc_case_destroy(_case);
#endif

  psc_testing_finalize();
#endif
}
