
#pragma once

#include "../libpsc/psc_output_fields/fields_item_fields.hxx"
#include "../libpsc/psc_output_fields/fields_item_moments_1st.hxx"
#include "fields_item.hxx"
#include "psc_particles_double.h"

#include <mrc_io.hxx>

#include <memory>

class WriterMRC
{
public:
  ~WriterMRC()
  {
    if (io_) {
      close();
    }
  }
  
  void open(const std::string& pfx, const std::string& dir = ".")
  {
    assert(!io_);
    io_.reset(new MrcIo(pfx.c_str(), dir.c_str()));
  }

  void close()
  {
    assert(io_);
    mrc_io_destroy(io_->io_);
    io_.reset();
  }

  void begin_step(const Grid_t& grid, Int3 rn = {},
                  Int3 rx = {1000000, 1000000, 1000000})
  {
    mrc_io_open(io_->io_, "w", grid.timestep(), grid.timestep() * grid.dt);

    // save some basic info about the run in the output file
    struct mrc_obj* obj = mrc_obj_create(mrc_io_comm(io_->io_));
    mrc_obj_set_name(obj, "psc");
    mrc_obj_dict_add_int(obj, "timestep", grid.timestep());
    mrc_obj_dict_add_float(obj, "time", grid.timestep() * grid.dt);
    mrc_obj_dict_add_float(obj, "cc", grid.norm.cc);
    mrc_obj_dict_add_float(obj, "dt", grid.dt);
    mrc_obj_write(obj, io_->io_);
    mrc_obj_destroy(obj);

    if (strcmp(mrc_io_type(io_->io_), "xdmf_collective") == 0) {
      auto gdims = grid.domain.gdims;
      int slab_off[3], slab_dims[3];
      for (int d = 0; d < 3; d++) {
        if (rx[d] > gdims[d])
          rx[d] = gdims[d];

        slab_off[d] = rn[d];
        slab_dims[d] = rx[d] - rn[d];
      }

      mrc_io_set_param_int3(io_->io_, "slab_off", slab_off);
      mrc_io_set_param_int3(io_->io_, "slab_dims", slab_dims);
    }
  }

  void end_step() { mrc_io_close(io_->io_); }

  template <typename Mfields>
  void write(const Mfields& mflds, const Grid_t& grid, const std::string& name,
             const std::vector<std::string>& comp_names)
  {
    MrcIo::write_mflds(io_->io_, mflds, grid, name, comp_names);
  }

private:
  std::unique_ptr<MrcIo> io_;
};

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
      tfd_jeh_{grid, Item_jeh<MfieldsFake>::n_comps(), {}},
      tfd_moments_{grid,
                   FieldsItem_Moments_1st_cc<MparticlesFake>::n_comps(grid),
                   grid.ibn},
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
      io_pfd_.open("pfd", data_dir);
    }
    if (pfield_moments_interval > 0) {
      io_pfd_moments_.open("pfd_moments", data_dir);
    }
    if (tfield_interval > 0) {
      io_tfd_.open("tfd", data_dir);
    }
    if (tfield_moments_interval > 0) {
      io_tfd_moments_.open("tfd_moments", data_dir);
    }
  }

  // ----------------------------------------------------------------------
  // operator()

  template <typename MfieldsState, typename Mparticles>
  void operator()(MfieldsState& mflds, Mparticles& mprts)
  {
    const auto& grid = mflds._grid();

    static int pr;
    if (!pr) {
      pr = prof_register("outf", 1., 0, 0);
    }
#if 0
    static int pr_field, pr_moment, pr_field_calc, pr_moment_calc, pr_field_write, pr_moment_write, pr_field_acc, pr_moment_acc;
    if (!pr_field) {
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
#endif
    prof_start(pr);

    auto timestep = grid.timestep();
    bool do_pfield = pfield_interval > 0 && timestep >= pfield_next_;
    bool do_tfield = tfield_interval > 0 && timestep >= tfield_next_;
    bool doaccum_tfield =
      tfield_interval > 0 &&
      (((timestep >= (tfield_next_ - tfield_average_length + 1)) &&
        timestep % tfield_average_every == 0) ||
       timestep == 0);

    if (do_pfield || doaccum_tfield) {
      /* prof_start(pr_field); */
      /* prof_start(pr_field_calc); */
      Item_jeh<MfieldsState> pfd_jeh{mflds};
      /* prof_stop(pr_field_calc); */

      if (do_pfield) {
        mpi_printf(grid.comm(), "***** Writing PFD output\n");
        pfield_next_ += pfield_interval;

        /* prof_start(pr_field_write); */
        io_pfd_.begin_step(grid, rn, rx);
        _write_pfd(io_pfd_, pfd_jeh);
        io_pfd_.end_step();
        /* prof_stop(pr_field_write); */
      }

      if (doaccum_tfield) {
        // tfd += pfd
        /* prof_start(pr_field_acc); */
        tfd_jeh_ += pfd_jeh;
        /* prof_stop(pr_field_acc); */
        naccum_++;
      }
      if (do_tfield) {
        mpi_printf(grid.comm(), "***** Writing TFD output\n");
        tfield_next_ += tfield_interval;

        /* prof_start(pr_field_write); */
        io_tfd_.begin_step(grid, rn, rx);
        _write_tfd(io_tfd_, tfd_jeh_, pfd_jeh, naccum_);
        io_tfd_.end_step();
        naccum_ = 0;
        /* prof_stop(pr_field_write); */
      }
      /* prof_stop(pr_field); */
    }

    bool do_pfield_moments =
      pfield_moments_interval > 0 && timestep >= pfield_moments_next_;
    bool do_tfield_moments =
      tfield_moments_interval > 0 && timestep >= tfield_moments_next_;
    bool doaccum_tfield_moments =
      tfield_moments_interval > 0 &&
      (((timestep >=
         (tfield_moments_next_ - tfield_moments_average_length + 1)) &&
        timestep % tfield_moments_average_every == 0) ||
       timestep == 0);

    if (do_pfield_moments || doaccum_tfield_moments) {
      /* prof_start(pr_moment); */
      /* prof_start(pr_moment_calc); */
      FieldsItem_Moments_1st_cc<Mparticles> pfd_moments{mprts};
      /* prof_stop(pr_moment_calc); */

      if (do_pfield_moments) {
        mpi_printf(grid.comm(), "***** Writing PFD moment output\n");
        pfield_moments_next_ += pfield_moments_interval;

        /* prof_start(pr_moment_write); */
        io_pfd_moments_.begin_step(grid, rn, rx);
        _write_pfd(io_pfd_moments_, pfd_moments);
        io_pfd_moments_.end_step();
        /* prof_stop(pr_moment_write); */
      }

      if (doaccum_tfield_moments) {
        // tfd += pfd
        /* prof_start(pr_moment_acc); */
        tfd_moments_ += pfd_moments;
        /* prof_stop(pr_moment_acc); */
        naccum_moments_++;
      }
      if (do_tfield_moments) {
        mpi_printf(grid.comm(), "***** Writing TFD moment output\n");
        tfield_moments_next_ += tfield_moments_interval;

        /* prof_start(pr_moment_write); */
        io_tfd_moments_.begin_step(grid, rn, rx);
        _write_tfd(io_tfd_moments_, tfd_moments_, pfd_moments, naccum_moments_);
        io_tfd_moments_.end_step();
        /* prof_stop(pr_moment_write); */
        naccum_moments_ = 0;
      }
      /* prof_stop(pr_moment); */
    }

    prof_stop(pr);
  };

private:
  template <typename EXP>
  static void _write_pfd(WriterMRC& io, EXP& pfd)
  {
    io.write(adaptMfields(pfd), pfd.grid(), pfd.name(), pfd.comp_names());
  }

  template <typename EXP>
  static void _write_tfd(WriterMRC& io, MfieldsC& tfd, EXP& pfd, int naccum)
  {
    // convert accumulated values to correct temporal mean
    tfd.scale(1. / naccum);
    io.write(tfd, tfd.grid(), pfd.name(), pfd.comp_names());
    tfd.zero();
  }

private:
  // tfd -- FIXME?! always MfieldsC
  MfieldsC tfd_jeh_;
  MfieldsC tfd_moments_;
  WriterMRC io_pfd_;
  WriterMRC io_pfd_moments_;
  WriterMRC io_tfd_;
  WriterMRC io_tfd_moments_;
  int pfield_next_;
  int pfield_moments_next_;
  int tfield_next_;
  int tfield_moments_next_;
  int naccum_ = 0;
  int naccum_moments_ = 0;
};
