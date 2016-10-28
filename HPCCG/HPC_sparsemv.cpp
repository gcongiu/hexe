
//@HEADER
// ************************************************************************
// 
//               HPCCG: Simple Conjugate Gradient Benchmark Code
//                 Copyright (2006) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ************************************************************************
//@HEADER

/////////////////////////////////////////////////////////////////////////

// Routine to compute matrix vector product y = Ax where:
// First call exchange_externals to get off-processor values of x

// A - known matrix 
// x - known vector
// y - On exit contains Ax.

/////////////////////////////////////////////////////////////////////////

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <string>
#include <cmath>
#include "HPC_sparsemv.hpp"
#ifdef PREFETCH
#include "prefetch_config.h" 
#include <omp.h>

int HPC_sparsemv( HPC_Sparse_Matrix *A, 
		 const double * const x, double * const y, Prefetch *prefetcher)
{

  const int nrow = (const int) A->local_nrow;
//printf("here \n");

  double *cache;
  int q = 0;
 // int prefetch_size = (size_t) A->ptr_to_vals_in_row[PREFETCH_CHUNK-1]+sizeof(double)*sizeof(double)*A->nnz_in_row[PREFETCH_CHUNK-1]-(size_t)A->ptr_to_vals_in_row[0];
 // prefetcher->start_fetch_continous(A->ptr_to_vals_in_row[0],prefetch_size, q);
 size_t  prefetch_size = (size_t) A->ptr_to_inds_in_row[PREFETCH_CHUNK-1]+sizeof(int)*27-(size_t)A->ptr_to_inds_in_row[0];
  prefetcher->start_fetch_continous(A->ptr_to_inds_in_row[0],prefetch_size, q);
  //printf("here 2 \n");
  for (int k = 0; k< nrow; k+=PREFETCH_CHUNK)
  {
	  q= 1-q;  
	  if(k+PREFETCH_CHUNK<nrow){
//		  prefetch_size = (size_t) A->ptr_to_vals_in_row[k+2*PREFETCH_CHUNK-1]+
//			  sizeof(double)*A->nnz_in_row[k+2*PREFETCH_CHUNK-1] - (size_t)A->ptr_to_vals_in_row[k+PREFETCH_CHUNK];
//		  prefetcher->start_fetch_continous(A->ptr_to_vals_in_row[k+PREFETCH_CHUNK], prefetch_size, q);
	          prefetch_size = (size_t) A->ptr_to_inds_in_row[k+2*PREFETCH_CHUNK-1]+
			  sizeof(double)*A->nnz_in_row[k+2*PREFETCH_CHUNK-1] - (size_t)A->ptr_to_inds_in_row[k+PREFETCH_CHUNK];
  		prefetcher->start_fetch_continous(A->ptr_to_inds_in_row[k+PREFETCH_CHUNK],prefetch_size, q);
	  }



	  int nz_sum = 0;

   //   cache = (double*) prefetcher->sync_fetch(1-q);
      int *cache2 = (int*) prefetcher->sync_fetch(1-q);
#ifdef USING_OMP
#pragma omp parallel for schedule(static)
#endif
  for (int i=k; i< k+PREFETCH_CHUNK; i++)
    {
//	printf("nnz is %d\n", nz_sum);
      //size_t offset = (uintptr_t) A->ptr_to_vals_in_row[i] - (uintptr_t) A->ptr_to_vals_in_row[k];

      size_t offset2 = (uintptr_t) A->ptr_to_inds_in_row[i] - (uintptr_t) A->ptr_to_inds_in_row[k];

      double sum = 0.0;
      const double * const cur_vals =// (const double* const) ((uintptr_t)cache+offset);
				      (const double* const)  A->ptr_to_vals_in_row[i]; 

      const int    * const cur_inds =  (const int* const) ((uintptr_t)cache2+offset2);

  //  (const int    * const) A->ptr_to_inds_in_row[i];

      const int cur_nnz = (const int) A->nnz_in_row[i];

      for (int j=0; j< cur_nnz; j++){
          sum += cur_vals[j]*x[cur_inds[j]];
	}
      y[i] = sum;
    }
}


  return(0);
}
#else
int HPC_sparsemv( HPC_Sparse_Matrix *A, 
		 const double * const x, double * const y)
{

  const int nrow = (const int) A->local_nrow;


#ifdef USING_OMP
#pragma omp parallel for
#endif
  for (int i=0; i< nrow; i++)
    {
      double sum = 0.0;
      const double * const cur_vals = 
     (const double * const) A->ptr_to_vals_in_row[i];

      const int    * const cur_inds = 
     (const int    * const) A->ptr_to_inds_in_row[i];

      const int cur_nnz = (const int) A->nnz_in_row[i];

      for (int j=0; j< cur_nnz; j++)
          sum += cur_vals[j]*x[cur_inds[j]];
      y[i] = sum;
    }
  return(0);
}
#endif
