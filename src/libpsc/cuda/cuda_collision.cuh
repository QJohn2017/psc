
#pragma once

#include "cuda_base.cuh"
#include "cuda_mparticles.cuh"
#include "cuda_mparticles_sort.cuh"
#include "rng_state.cuh"
// FIXME, horrible hack...
#define DEVICE __device__
#include "binary_collision.hxx"
#include <curand_kernel.h>

#define THREADS_PER_BLOCK 128

template <typename cuda_mparticles, typename RngState>
struct CudaCollision;

template <typename cuda_mparticles, typename RngState>
__global__ static void k_collide(
  DMparticlesCuda<typename cuda_mparticles::BS> dmprts, uint* d_off, uint* d_id,
  float nudt0, typename RngState::Device rng_state, uint n_cells)
{
  CudaCollision<cuda_mparticles, RngState>::d_collide(
    dmprts, d_off, d_id, nudt0, rng_state, n_cells);
}

// ======================================================================
// cuda_collision

template <typename cuda_mparticles, typename RngState>
struct CudaCollision
{
  using real_t = typename cuda_mparticles::real_t;
  using DMparticles = DMparticlesCuda<typename cuda_mparticles::BS>;
  using DParticle = DParticleProxy<DMparticles>;

  CudaCollision(int interval, double nu, int nicell, double dt)
    : interval_{interval}, nu_{nu}, nicell_(nicell), dt_(dt)
  {}

  int interval() const { return interval_; }

  void operator()(cuda_mparticles& cmprts)
  {
    if (cmprts.n_prts == 0) {
      return;
    }
    sort_.find_indices_ids(cmprts);
    sort_.sort();
    sort_.find_offsets();
    // for (int c = 0; c <= cmprts.n_cells(); c++) {
    //   printf("off[%d] = %d\n", c, int(sort_by_cell.d_off[c]));
    // }

    int blocks = cmprts.n_cells();
    if (blocks > 32768)
      blocks = 32768;
    dim3 dimGrid(blocks);

    if (blocks * THREADS_PER_BLOCK > rng_state_.size()) {
      rng_state_.resize(blocks * THREADS_PER_BLOCK);
    }

    // all particles need to have same weight!
    real_t wni = 1.; // FIXME, there should at least be some assert to enforce
                     // this //prts[n_start].w());
    real_t nudt0 = wni / nicell_ * interval_ * dt_ * nu_;
    
    //JOHNS DEBUG
    if(!cmprts.check_ordered()) printf("ORDER FAIL! JOHNS DEBUG\n");
    
    k_collide<cuda_mparticles, RngState><<<dimGrid, THREADS_PER_BLOCK>>>(
      cmprts, sort_.d_off.data().get(), sort_.d_id.data().get(), nudt0,
      rng_state_, cmprts.n_cells());
    cuda_sync_if_enabled();
    
    //JOHNS DEBUG
    if(!cmprts.check_ordered()) printf("ORDER FAIL! JOHNS DEBUG\n");
  }

  __device__ static void d_collide(DMparticles dmprts, uint* d_off, uint* d_id,
                                   float nudt0,
                                   typename RngState::Device rng_state,
                                   uint n_cells)
  {

    int id = threadIdx.x + blockIdx.x * THREADS_PER_BLOCK;
    /* Copy state to local memory for efficiency */
    auto rng = rng_state[id];
    BinaryCollision<DParticle> bc;

    for (uint bidx = blockIdx.x; bidx < n_cells; bidx += gridDim.x) {
      uint beg = d_off[bidx];
      uint end = d_off[bidx + 1];
      real_t nudt1 =
        nudt0 * (end - beg) * (end - beg) / ((end - beg) & ~1); // somewhat counteract that we don't collide
                                  // the last particle if odd
      for (uint n = beg + 2 * threadIdx.x; n + 1 < end;
           n += 2 * THREADS_PER_BLOCK) {
        // printf("%d/%d: n = %d off %d\n", blockIdx.x, threadIdx.x, n,
        // d_off[blockIdx.x]);
        auto prt1 = DParticle{dmprts.storage.load_proxy(dmprts, d_id[n])};
        auto prt2 = DParticle{dmprts.storage.load_proxy(dmprts, d_id[n + 1])};
        bc(prt1, prt2, nudt1, rng);
        // xi4 is not modified, don't need to store
        dmprts.storage.store_momentum(prt1, d_id[n]);
        dmprts.storage.store_momentum(prt2, d_id[n + 1]);
      }
    }

    rng_state[id] = rng;
  }

private:
  int interval_;
  double nu_;
  int nicell_;
  double dt_;
  RngState rng_state_;
  cuda_mparticles_randomize_sort sort_;
};
