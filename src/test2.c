
#include <stdio.h>

#include "psc.h"
#include "util/profile.h"

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);

  psc_create_test_1("fortran");
  //  psc_dump_particles("part-0.asc");
  psc_push_part_yz_a();
  //  psc_dump_particles("part-1.asc");
  psc_save_particles_ref();
  psc_destroy();

  psc_create_test_1("generic_c");
  psc_push_part_yz_a();
  //  psc_dump_particles("part-2.asc");
  psc_check_particles_ref();
  psc_destroy();
  
  psc_create_test_1("sse2");
  psc_push_part_yz_a();
  //  psc_dump_particles("part-3.asc");
  psc_check_particles_ref();
  psc_destroy();

  psc_create_test_1("fortran");
  //  psc_dump_particles("part-0.asc");
  psc_push_part_yz_b();
  //  psc_dump_particles("part-1.asc");
  psc_save_particles_ref();
  psc_destroy();

  psc_create_test_1("generic_c");
  psc_push_part_yz_b();
  //  psc_dump_particles("part-2.asc");
  psc_check_particles_ref();
  psc_destroy();

  psc_create_test_1("cuda");
  psc_push_part_yz_b();
  //  psc_dump_particles("part-3.asc");
  psc_check_particles_ref();
  psc_destroy();

  prof_print();

  MPI_Finalize();
}
