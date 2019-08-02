
#include "fields.hxx"

#include <math.h>

#include "common_moments.cxx"

// ======================================================================
// n

template<typename MP, typename MF>
struct Moment_n_2nd_nc
{
  using Mparticles = MP;
  using Mfields = MF;
  using real_t = typename Mparticles::real_t;
  using particles_t = typename Mparticles::Patch;
  
  constexpr static char const* name = "n_2nd_nc_double";
  constexpr static int n_comps = 1;
  static std::vector<std::string> fld_names() { return { "n" }; }
  constexpr static int flags = POFI_BY_KIND;
  
  static void run(Mfields& mflds, Mparticles& mprts)
  {
    const auto& grid = mprts.grid();
    real_t fnqs = grid.norm.fnqs;
    real_t dxi = 1.f / grid.domain.dx[0], dyi = 1.f / grid.domain.dx[1], dzi = 1.f / grid.domain.dx[2];

    auto accessor = mprts.accessor();
    for (int p = 0; p < mprts.n_patches(); p++) {
      auto flds = mflds[p];
      for (auto prt: accessor[p]) {
	int m = prt.kind();
	DEPOSIT_TO_GRID_2ND_NC(prt, flds, m, 1.f);
      }
    }
  }
};

// ======================================================================
// rho

template<typename MP, typename MF>
struct Moment_rho_2nd_nc
{
  using Mparticles = MP;
  using Mfields = MF;
  using real_t = typename Mparticles::real_t;
  using particles_t = typename Mparticles::Patch;
  
  constexpr static char const* name = "rho_2nd_nc";
  constexpr static int n_comps = 1;
  static std::vector<std::string> fld_names() { return { "rho" }; }
  constexpr static int flags = 0;
  
  static void run(Mfields& mflds, Mparticles& mprts)
  {
    const auto& grid = mprts.grid();
    real_t fnqs = grid.norm.fnqs;
    real_t dxi = 1.f / grid.domain.dx[0], dyi = 1.f / grid.domain.dx[1], dzi = 1.f / grid.domain.dx[2];

    auto accessor = mprts.accessor();
    for (int p = 0; p < mprts.n_patches(); p++) {
      auto flds = mflds[p];
      for (auto prt: accessor[p]) {
	DEPOSIT_TO_GRID_2ND_NC(prt, flds, 0, prt.q());
      }
    }
  }
};

