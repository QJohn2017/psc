
#pragma once

#include "../libpsc/psc_output_fields/fields_item_jeh.hxx"
#include "../libpsc/psc_output_fields/fields_item_moments_1st.hxx"
#include "fields_item.hxx"

#include <mrc_io.hxx>

#include <memory>

// ======================================================================

using FieldsItem_jeh = FieldsItemFields<ItemLoopPatches<Item_jeh>>;
using FieldsItem_E_cc = FieldsItemFields<ItemLoopPatches<Item_e_cc>>;
using FieldsItem_H_cc = FieldsItemFields<ItemLoopPatches<Item_h_cc>>;
using FieldsItem_J_cc = FieldsItemFields<ItemLoopPatches<Item_j_cc>>;

template <typename Mparticles>
using FieldsItem_n_1st_cc =
  FieldsItemMoment<ItemMomentAddBnd<Moment_n_1st<MfieldsC>>, Mparticles>;
template <typename Mparticles>
using FieldsItem_v_1st_cc =
  FieldsItemMoment<ItemMomentAddBnd<Moment_v_1st<MfieldsC>>, Mparticles>;
template <typename Mparticles>
using FieldsItem_p_1st_cc =
  FieldsItemMoment<ItemMomentAddBnd<Moment_p_1st<MfieldsC>>, Mparticles>;
template <typename Mparticles>
using FieldsItem_T_1st_cc =
  FieldsItemMoment<ItemMomentAddBnd<Moment_T_1st<MfieldsC>>, Mparticles>;
template <typename Mparticles>
using FieldsItem_Moments_1st_cc =
  FieldsItemMoment<ItemMomentAddBnd<Moments_1st<MfieldsC>>, Mparticles>;

// ======================================================================
// OutputFieldsCParams

struct OutputFieldsCParams
{
  const char* data_dir = {"."};

  int pfield_step = 0;
  int pfield_first = 0;

  int tfield_step = 0;
  int tfield_first = 0;
  int tfield_length = 1000000;
  int tfield_every = 1;

  Int3 rn = {};
  Int3 rx = {1000000, 1000000, 100000};
};

// ======================================================================
// OutputFieldsC

struct OutputFieldsC : public OutputFieldsCParams
{
  // ----------------------------------------------------------------------
  // ctor

  OutputFieldsC(const Grid_t& grid, const OutputFieldsCParams& prm,
                std::vector<std::unique_ptr<FieldsItemBase>>&& items = {})
    : OutputFieldsCParams{prm}, items_{std::move(items)}
  {
    pfield_next = pfield_first;
    tfield_next = tfield_first;

    for (auto& item : items_) {
      tfds_.emplace_back(grid, item->n_comps(grid), grid.ibn);
    }

    naccum = 0;

    if (pfield_step > 0) {
      io_pfd_.reset(new MrcIo{"pfd", data_dir});
    }
    if (tfield_step) {
      io_tfd_.reset(new MrcIo{"tfd", data_dir});
    }
  }

  // ----------------------------------------------------------------------
  // operator()

  void operator()(MfieldsStateBase& mflds, MparticlesBase& mprts)
  {
    const auto& grid = mflds._grid();

    static int pr;
    if (!pr) {
      pr = prof_register("output_c_field", 1., 0, 0);
    }
    prof_start(pr);

    auto timestep = grid.timestep();
    bool doaccum_tfield =
      tfield_step > 0 && (((timestep >= (tfield_next - tfield_length + 1)) &&
                           timestep % tfield_every == 0) ||
                          timestep == 0);

    if ((pfield_step > 0 && timestep >= pfield_next) ||
        (tfield_step > 0 && doaccum_tfield)) {
      for (auto& item : items_) {
        item->run(grid, mflds, mprts);
#if 0
	auto& mres = dynamic_cast<MfieldsC&>(item->mres());
	double min = 1e10, max = -1e10;
	for (int m = 0; m < mres.n_comps(); m++) {
	  for (int p = 0; p < mres.n_patches(); p++) {
	    mres.grid().Foreach_3d(0, 0, [&](int i, int j, int k) {
		min = fminf(min, mres[p](m, i,j,k));
		max = fmaxf(max, mres[p](m, i,j,k));
	      });
	  }
	  mprintf("name %s %d min %g max %g\n", item->name(), m, min, max);
	}
#endif
      }
    }

    if (pfield_step > 0 && timestep >= pfield_next) {
      mpi_printf(MPI_COMM_WORLD, "***** Writing PFD output\n"); // FIXME
      pfield_next += pfield_step;

      io_pfd_->open(grid, rn, rx);
      for (auto& item : items_) {
        item->mres().write_as_mrc_fld(io_pfd_->io_, item->name(),
                                      item->comp_names());
      }
      io_pfd_->close();
    }

    if (tfield_step > 0) {
      if (doaccum_tfield) {
        // tfd += pfd
        for (int i = 0; i < items_.size(); i++) {
          tfds_[i].axpy(1., items_[i]->mres());
        }
        naccum++;
      }
      if (timestep >= tfield_next) {
        mpi_printf(MPI_COMM_WORLD, "***** Writing TFD output\n"); // FIXME
        tfield_next += tfield_step;

        io_tfd_->open(grid, rn, rx);

        // convert accumulated values to correct temporal mean
        for (int i = 0; i < items_.size(); i++) {
          tfds_[i].scale(1. / naccum);
          tfds_[i].write_as_mrc_fld(io_tfd_->io_, items_[i]->name(),
                                    items_[i]->comp_names());
          tfds_[i].zero();
        }
        naccum = 0;
        io_tfd_->close();
      }
    }

    prof_stop(pr);
  };

public:
  int pfield_next, tfield_next;
  // storage for output
  unsigned int naccum;

private:
  std::vector<std::unique_ptr<FieldsItemBase>> items_;
  // tfd -- FIXME?! always MfieldsC
  std::vector<MfieldsC> tfds_;
  std::unique_ptr<MrcIo> io_pfd_;
  std::unique_ptr<MrcIo> io_tfd_;
};

template <typename Mparticles>
OutputFieldsC defaultOutputFieldsC(const Grid_t& grid,
                                   const OutputFieldsCParams& params)
{
  std::vector<std::unique_ptr<FieldsItemBase>> outf_items;
  outf_items.emplace_back(new FieldsItem_jeh(grid));
  outf_items.emplace_back(new FieldsItem_Moments_1st_cc<Mparticles>(grid));
  return OutputFieldsC{grid, params, std::move(outf_items)};
}
