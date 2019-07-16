
#include <psc.hxx>
#include <setup_particles.hxx>
#include <setup_fields.hxx>
#include "../src/libpsc/psc_output_fields/fields_item_moments_1st.hxx"

#include "psc_config.hxx"

// EDIT to change particle shape order / floating point type / 2d/3d / ...
using dim_t = dim_yz;
using PscConfig = PscConfig1vbecSingle<dim_t>;

// ======================================================================
// PscWhistler

struct PscWhistler : Psc<PscConfig>
{
  using Dim = PscConfig::dim_t;

  enum {
    ELECTRON,
    ION,
  };
  
  // ----------------------------------------------------------------------
  // ctor
  
  PscWhistler()
  {
    auto comm = grid().comm();

    mpi_printf(comm, "*** Setting up...\n");

    // Parameters
    mi_over_me_ = 10.;
    vA_over_c_ = .1;
    beta_e_par_ = .1;
    beta_i_par_ = .1;
    Ti_perp_over_Ti_par_ = 1.;
    Te_perp_over_Te_par_ = 1.;
    
    double B0 = vA_over_c_;
    double Te_par = beta_e_par_ * sqr(B0) / 2.;
    double Te_perp = Te_perp_over_Te_par_ * Te_par;
    double Ti_par = beta_i_par_ * sqr(B0) / 2.;
    double Ti_perp = Ti_perp_over_Ti_par_ * Ti_par;
    double mi = 1.;
    double me = 1. / mi_over_me_;

    mpi_printf(comm, "d_i = 1., d_e = %g\n", sqrt(me));
    mpi_printf(comm, "om_ci = %g, om_ce = %g\n", B0, B0 / me);
    mpi_printf(comm, "\n");
    mpi_printf(comm, "v_i,perp = %g [c] T_i,perp = %g\n", sqrt(2.*Ti_perp), Ti_perp);
    mpi_printf(comm, "v_i,par  = %g [c] T_i,par = %g\n", sqrt(2.*Ti_par), Ti_par);
    mpi_printf(comm, "v_e,perp = %g [c] T_e,perp = %g\n", sqrt(2*Te_perp / me), Te_perp);
    mpi_printf(comm, "v_e,par  = %g [c] T_e,par = %g\n", sqrt(2.*Te_par / me), Te_par);
    mpi_printf(comm, "\n");
  
    p_.nmax = 16001;
    p_.cfl = 0.98;

    // -- setup particle kinds
    Grid_t::Kinds kinds = {{ -1., me, "e"}, {1., mi, "i"}, };

    // --- setup domain
    Grid_t::Real3 LL = {1., 32., 256.}; // domain size (normalized units, ie, in d_i)
    Int3 gdims = {1, 32, 256}; // global number of grid points
    Int3 np = {1, 1, 4}; // division into patches

    auto grid_domain = Grid_t::Domain{gdims, LL, {}, np};
    
    auto grid_bc = GridBc{{ BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			  { BND_FLD_PERIODIC, BND_FLD_PERIODIC, BND_FLD_PERIODIC },
			  { BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC },
			  { BND_PRT_PERIODIC, BND_PRT_PERIODIC, BND_PRT_PERIODIC }};

    // --- generic setup
    auto norm_params = Grid_t::NormalizationParams::dimensionless();
    norm_params.nicell = 100;

    double dt = p_.cfl * courant_length(grid_domain);
    define_grid(grid_domain, grid_bc, kinds, dt, norm_params);

    define_field_array();

    mprts_.reset(new Mparticles{grid()});

    // -- Balance
    balance_interval = -1;
    balance_.reset(new Balance_t{balance_interval, .1, false});

    // -- Sort
    // FIXME, needs a way to make sure it gets set?
    sort_interval = 100;

    // -- Collision
    int collision_interval = -1;
    double collision_nu = .1;
    collision_.reset(new Collision_t{grid(), collision_interval, collision_nu});

    // -- Checks
    ChecksParams checks_params{};
    checks_params.continuity_every_step = 50;
    checks_params.continuity_threshold = 1e-5;
    checks_params.continuity_verbose = false;
    checks_.reset(new Checks_t{grid(), comm, checks_params});

    // -- Marder correction
    double marder_diffusion = 0.9;
    int marder_loop = 3;
    bool marder_dump = false;
    marder_interval = -1;
    marder_.reset(new Marder_t(grid(), marder_diffusion, marder_loop, marder_dump));

    // -- output fields
    OutputFieldsCParams outf_params;
    outf_params.pfield_step = 10;
    std::vector<std::unique_ptr<FieldsItemBase>> outf_items;
    outf_items.emplace_back(new FieldsItemFields<ItemLoopPatches<Item_e_cc>>(grid()));
    outf_items.emplace_back(new FieldsItemFields<ItemLoopPatches<Item_h_cc>>(grid()));
    outf_items.emplace_back(new FieldsItemFields<ItemLoopPatches<Item_j_cc>>(grid()));
    outf_items.emplace_back(new FieldsItemMoment<ItemMomentAddBnd<Moment_n_1st<Mparticles, MfieldsC>>>(grid()));
    outf_items.emplace_back(new FieldsItemMoment<ItemMomentAddBnd<Moment_v_1st<Mparticles, MfieldsC>>>(grid()));
    outf_items.emplace_back(new FieldsItemMoment<ItemMomentAddBnd<Moment_T_1st<Mparticles, MfieldsC>>>(grid()));
    outf_.reset(new OutputFieldsC{grid(), outf_params, std::move(outf_items)});

    // --- partition particles and initial balancing
    mpi_printf(comm, "**** Partitioning...\n");
    auto n_prts_by_patch = setup_initial_partition();
    balance_->initial(grid_, n_prts_by_patch);
    // balance::initial does not rebalance particles, because the old way of doing this
    // does't even have the particle data structure created yet -- FIXME?
    mprts_->reset(grid());
    
    mpi_printf(comm, "**** Setting up particles...\n");
    setup_initial_particles(*mprts_, n_prts_by_patch);
    
    mpi_printf(comm, "**** Setting up fields...\n");
    setup_initial_fields(*mflds_);

    // do remainder of generic initialization
    init();
  }

private:
  void init_npt(int kind, double crd[3], psc_particle_npt& npt)
  {
    double B0 = vA_over_c_;
    double Te_par = beta_e_par_ * sqr(B0) / 2.;
    double Te_perp = Te_perp_over_Te_par_ * Te_par;
    double Ti_par = beta_i_par_ * sqr(B0) / 2.;
    double Ti_perp = Ti_perp_over_Ti_par_ * Ti_par;

    switch (kind) {
    case ELECTRON:
      npt.n = 1.;
      
      npt.p[0] = 0.;
      npt.p[1] = 0.;
      npt.p[2] = .01;

      npt.T[0] = Te_perp;
      npt.T[1] = Te_perp;
      npt.T[2] = Te_par;
      break;
    case ION:
      npt.n = 1.;
      
      npt.p[0] = 0.;
      npt.p[1] = 0.;
      npt.p[2] = 0.;

      npt.T[0] = Ti_perp;
      npt.T[1] = Ti_perp;
      npt.T[2] = Ti_par;
      break;
    default:
      assert(0);
    }
  }
  
  // ----------------------------------------------------------------------
  // setup_initial_partition
  
  std::vector<uint> setup_initial_partition()
  {
    SetupParticles<Mparticles> setup_particles;
    return setup_particles.setup_partition(grid(), [&](int kind, double crd[3], psc_particle_npt& npt) {
	this->init_npt(kind, crd, npt);
      });
  }
  
  // ----------------------------------------------------------------------
  // setup_initial_particles
  
  void setup_initial_particles(Mparticles& mprts, std::vector<uint>& n_prts_by_patch)
  {
    SetupParticles<Mparticles> setup_particles;
    setup_particles.setup_particles(mprts, n_prts_by_patch, [&](int kind, double crd[3], psc_particle_npt& npt) {
	this->init_npt(kind, crd, npt);
      });
  }

  // ----------------------------------------------------------------------
  // setup_initial_fields
  
  void setup_initial_fields(MfieldsState& mflds)
  {
    double B0 = vA_over_c_;

    SetupFields<MfieldsState>::set(mflds, [&](int m, double crd[3]) {
	switch (m) {
	  //case HZ: return B0;
	  
	default: return 0.;
	}
      });
  }

private:
  double mi_over_me_;
  double vA_over_c_;
  double beta_e_par_;
  double beta_i_par_;
  double Ti_perp_over_Ti_par_;
  double Te_perp_over_Te_par_;
};

// ======================================================================
// main

int
main(int argc, char **argv)
{
  psc_init(argc, argv);

  auto psc = new PscWhistler;

  psc->initialize();
  psc->integrate();

  delete psc;
  
  libmrc_params_finalize();
  MPI_Finalize();

  return 0;
}

