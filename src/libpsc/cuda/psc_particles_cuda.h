
#ifndef PSC_PARTICLES_CUDA_H
#define PSC_PARTICLES_CUDA_H

#include "particles.hxx"
#include "particles_traits.hxx"
#include "psc_bits.h"

#include <vector>

#include "cuda_iface.h"

template<>
struct Mparticles_traits<MparticlesCuda>
{
  static constexpr const char* name = "cuda";
  static MPI_Datatype mpi_dtype() { return MPI_FLOAT; }
};


#endif
