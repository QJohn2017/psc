
#pragma once

#include "../libpsc/psc_output_fields/fields_item_fields.hxx"
#include "../libpsc/psc_output_fields/fields_item_moments_1st.hxx"
#include "fields_item.hxx"

#include <mrc_io.hxx>

#include <memory>

template <typename Mparticles>
using FieldsItem_Moments_1st_cc = Moments_1st<Mparticles>;

// ======================================================================
// OutputFieldsParams

struct OutputFieldsParams
{
  const char* data_dir = {"."};

  int pfield_interval = 0;
  int pfield_first = 0;

  int pfield_moments_interval = -1;
  int pfield_moments_first = -1;

  int tfield_interval = 0;
  int tfield_first = 0;
  int tfield_average_length = 1000000;
  int tfield_average_every = 1;

  int tfield_moments_interval = -1;
  int tfield_moments_first = -1;
  int tfield_moments_average_length = -1;
  int tfield_moments_average_every = -1;

  Int3 rn = {};
  Int3 rx = {1000000, 1000000, 100000};
};

// ======================================================================
// OutputFields

class OutputFields : public OutputFieldsParams
{
  using MfieldsFake = MfieldsC;
  using MparticlesFake = MparticlesDouble;

public:
  // ----------------------------------------------------------------------
  // ctor

  OutputFields(const Grid_t& grid, const OutputFieldsParams& prm)
    : OutputFieldsParams{prm},
      tfd_jeh_{grid, Item_jeh<MfieldsFake>::n_comps(), grid.ibn},
      tfd_moments_{grid, FieldsItem_Moments_1st_cc<MparticlesFake>::n_comps(grid), grid.ibn},
      pfield_next_{pfield_first},
      pfield_moments_next_{pfield_moments_first},
      tfield_next_{tfield_first},
      tfield_moments_next_{tfield_moments_first}
  {
    if (pfield_moments_interval < 0) {
      pfield_moments_interval = pfield_interval;
    }
    if (pfield_moments_first < 0) {
      pfield_moments_first = pfield_first;
    }
    
    if (tfield_moments_interval < 0) {
      tfield_moments_interval = tfield_interval;
    }
    if (tfield_moments_first < 0) {
      tfield_moments_first = tfield_first;
    }
    if (tfield_moments_average_length < 0) {
      tfield_moments_average_length = tfield_average_length;
    }
    if (tfield_moments_average_every < 0) {
      tfield_moments_average_every = tfield_average_every;
    }
    
    if (pfield_interval > 0) {
      io_pfd_.reset(new MrcIo{"pfd", data_dir});
    }
    if (pfield_moments_interval > 0) {
      io_pfd_moments_.reset(new MrcIo{"pfd_moments", data_dir});
    }
    if (tfield_interval > 0) {
      io_tfd_.reset(new MrcIo{"tfd", data_dir});
    }
    if (tfield_moments_interval > 0) {
      io_tfd_moments_.reset(new MrcIo{"tfd_moments", data_dir});
    }
  }

  // ----------------------------------------------------------------------
  // operator()

  template <typename MfieldsState, typename Mparticles>
  void operator()(MfieldsState& mflds, Mparticles& mprts)
  {
    const auto& grid = mflds._grid();

    static int pr, pr_field, pr_moment, pr_field_calc, pr_moment_calc, pr_field_write, pr_moment_write, pr_field_acc, pr_moment_acc;
    if (!pr) {
      pr = prof_register("outf", 1., 0, 0);
      pr_field = prof_register("outf_field", 1., 0, 0);
      pr_moment = prof_register("outf_moment", 1., 0, 0);
      pr_field_calc = prof_register("outf_field_calc", 1., 0, 0);
      pr_moment_calc = prof_register("outf_moment_calc", 1., 0, 0);
      pr_field_write = prof_register("outf_field_write", 1., 0, 0);
      pr_moment_write = prof_register("outf_moment_write", 1., 0, 0);
      pr_field_acc = prof_register("outf_field_acc", 1., 0, 0);
      pr_moment_acc = prof_register("outf_moment_acc", 1., 0, 0);
    }
    prof_start(pr);

    auto timestep = grid.timestep();
    bool do_pfield = pfield_interval > 0 && timestep >= pfield_next_;
    bool do_tfield = tfield_interval > 0 && timestep >= tfield_next_;
    bool doaccum_tfield =
      tfield_interval > 0 && (((timestep >= (tfield_next_ - tfield_average_length + 1)) &&
                           timestep % tfield_average_every == 0) ||
                          timestep == 0);

    mpi_printf(MPI_COMM_WORLD, "do_pfield %d do_tfield %d doaccum %d\n", do_pfield, do_tfield, doaccum_tfield);
    if (do_pfield || doaccum_tfield) {
      prof_start(pr_field);
      prof_start(pr_field_calc);
      Item_jeh<MfieldsState> pfd_jeh{mflds};
      prof_stop(pr_field_calc);
      
      if (do_pfield) {
	mpi_printf(grid.comm(), "***** Writing PFD output\n");
	pfield_next_ += pfield_interval;
	
	prof_start(pr_field_write);
	io_pfd_->open(grid, rn, rx);
	_write_pfd(*io_pfd_, pfd_jeh);
	io_pfd_->close();
	prof_stop(pr_field_write);
      }

      if (doaccum_tfield) {
	// tfd += pfd
	prof_start(pr_field_acc);
	tfd_jeh_ += pfd_jeh;
	prof_stop(pr_field_acc);
	naccum_++;
      }
      if (do_tfield) {
	mpi_printf(grid.comm(), "***** Writing TFD output\n");
	tfield_next_ += tfield_interval;

	prof_start(pr_field_write);
	io_tfd_->open(grid, rn, rx);
	_write_tfd(*io_tfd_, tfd_jeh_, pfd_jeh, naccum_);
	io_tfd_->close();
	naccum_ = 0;
	prof_stop(pr_field_write);
      }
      prof_stop(pr_field);
    }

    bool do_pfield_moments = pfield_moments_interval > 0 && timestep >= pfield_moments_next_;
    bool do_tfield_moments = tfield_moments_interval > 0 && timestep >= tfield_moments_next_;
    bool doaccum_tfield_moments =
      tfield_moments_interval > 0 && (((timestep >= (tfield_moments_next_ - tfield_moments_average_length + 1)) &&
					timestep % tfield_moments_average_every == 0) ||
				       timestep == 0);
    mpi_printf(MPI_COMM_WORLD, "mom: do_pfield %d do_tfield %d doaccum %d\n", do_pfield_moments, do_tfield_moments, doaccum_tfield_moments);
    
    if (do_pfield_moments || doaccum_tfield_moments) {
      prof_start(pr_moment);
      prof_start(pr_moment_calc);
      FieldsItem_Moments_1st_cc<Mparticles> pfd_moments{mprts};
      prof_stop(pr_moment_calc);
      
      if (do_pfield_moments) {
	mpi_printf(grid.comm(), "***** Writing PFD moment output\n");
	pfield_moments_next_ += pfield_moments_interval;
	
	prof_start(pr_moment_write);
	io_pfd_moments_->open(grid, rn, rx);
	_write_pfd(*io_pfd_moments_, pfd_moments);
	io_pfd_moments_->close();
	prof_stop(pr_moment_write);
      }

      if (doaccum_tfield_moments) {
	// tfd += pfd
	prof_start(pr_moment_acc);
	tfd_moments_ += pfd_moments;
	prof_stop(pr_moment_acc);
	naccum_moments_++;
      }
      if (do_tfield_moments) {
	mpi_printf(grid.comm(), "***** Writing TFD moment output\n");
	tfield_moments_next_ += tfield_moments_interval;

	prof_start(pr_moment_write);
	io_tfd_moments_->open(grid, rn, rx);
	_write_tfd(*io_tfd_moments_, tfd_moments_, pfd_moments, naccum_moments_);
	io_tfd_moments_->close();
	prof_stop(pr_moment_write);
	naccum_moments_ = 0;
      }
      prof_stop(pr_moment);
    }

    prof_stop(pr);
  };

private:
  template <typename EXP>
  static void _write_pfd(MrcIo& io, EXP& pfd)
  {
    MrcIo::write_mflds(io.io_, adaptMfields(pfd), pfd.grid(), pfd.name(),
                       pfd.comp_names());
  }

  template <typename EXP>
  static void _write_tfd(MrcIo& io, MfieldsC& tfd, EXP& pfd, int naccum)
  {
    // convert accumulated values to correct temporal mean
    tfd.scale(1. / naccum);
    tfd.write_as_mrc_fld(io.io_, pfd.name(), pfd.comp_names());
    tfd.zero();
  }

private:
  // tfd -- FIXME?! always MfieldsC
  MfieldsC tfd_jeh_;
  MfieldsC tfd_moments_;
  std::unique_ptr<MrcIo> io_pfd_;
  std::unique_ptr<MrcIo> io_pfd_moments_;
  std::unique_ptr<MrcIo> io_tfd_;
  std::unique_ptr<MrcIo> io_tfd_moments_;
  int pfield_next_;
  int pfield_moments_next_;
  int tfield_next_;
  int tfield_moments_next_;
  int naccum_ = 0;
  int naccum_moments_ = 0;
};
