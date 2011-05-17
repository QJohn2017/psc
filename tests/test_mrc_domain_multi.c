
#include <mrc_params.h>
#include <mrc_domain.h>
#include <mrc_fld.h>
#include <mrc_io.h>
#include <mrctest.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

static void
test_read_write(struct mrc_domain *domain)
{
  struct mrc_io *io = mrc_io_create(mrc_domain_comm(domain));

  mrc_io_set_from_options(io);
  mrc_io_setup(io);
  mrc_io_open(io, "w", 0, 0.);
  mrc_domain_write(domain, io);
  mrc_io_close(io);
  mrc_io_destroy(io);

  io = mrc_io_create(mrc_domain_comm(domain));
  mrc_io_set_from_options(io);
  mrc_io_setup(io);
  mrc_io_open(io, "r", 0, 0.);
  struct mrc_domain *domain2 = mrc_domain_read(io, mrc_domain_name(domain));
  mrc_io_close(io);
  mrc_io_destroy(io);

  mrctest_crds_compare(mrc_domain_get_crds(domain),
		       mrc_domain_get_crds(domain2));

  mrc_domain_destroy(domain2);
}

int
main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);
  libmrc_params_init(argc, argv);

  struct mrc_domain *domain = mrc_domain_create(MPI_COMM_WORLD);
  mrc_domain_set_type(domain, "multi");
  struct mrc_crds *crds = mrc_domain_get_crds(domain);

  int testcase = 1;
  mrc_params_get_option_int("case", &testcase);

  switch (testcase) {
  case 1:
    mrc_crds_set_type(crds, "multi_uniform");
    mrc_domain_set_from_options(domain);
    mrc_domain_setup(domain);
    test_read_write(domain);
    break;
  case 2: ;
    mrc_crds_set_type(crds, "multi_rectilinear");
    mrc_crds_set_param_int(crds, "sw", 2);
    mrc_domain_set_from_options(domain);
    mrc_domain_setup(domain);
    int sw;
    mrc_crds_get_param_int(crds, "sw", &sw);
    struct mrc_patch *patches = mrc_domain_get_patches(domain, NULL);
    for (int d = 0; d < 3; d++) {
      mrc_m1_foreach_patch(crds->mcrd[d], p) {
	struct mrc_m1_patch *m1p = mrc_m1_patch_get(crds->mcrd[d], p);
	mrc_m1_foreach(m1p, ix, sw, sw) {
	  int jx = ix + patches[p].off[d];
	  MRC_M1(m1p, 0, ix) = jx*jx;
	} mrc_m1_foreach_end;
	mrc_m1_patch_put(crds->mcrd[d]);
      }
    }
    test_read_write(domain);
    break;
  }
  mrc_domain_destroy(domain);

  MPI_Finalize();
}
