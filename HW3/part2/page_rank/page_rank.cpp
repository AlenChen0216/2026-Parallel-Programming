#include "page_rank.h"

#include <cmath>
#include <cstdlib>
#include <omp.h>
#include <stdio.h>

#include "../common/graph.h"

// page_rank --
//
// g:           graph to process (see common/graph.h)
// solution:    array of per-vertex vertex scores (length of array is num_nodes(g))
// damping:     page-rank algorithm's damping parameter
// convergence: page-rank algorithm's convergence threshold
//
void page_rank(Graph g, double *solution, double damping, double convergence)
{

    // initialize vertex weights to uniform probability. Double
    // precision scores are used to avoid underflow for large graphs

    int nnodes = num_nodes(g);
    double equal_prob = 1.0 / nnodes;
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nnodes; ++i)
    {
        solution[i] = equal_prob;
    }
    /*
       For PP students: Implement the page rank algorithm here.  You
       are expected to parallelize the algorithm using openMP.  Your
       solution may need to allocate (and free) temporary arrays.

       Basic page rank pseudocode is provided below to get you started:

       // initialization: see example code above
       score_old[vi] = 1/nnodes;

       while (!converged) {

         // compute score_new[vi] for all nodes vi:
         score_new[vi] = sum over all nodes vj reachable from incoming edges
                            { score_old[vj] / number of edges leaving vj  }
         score_new[vi] = (damping * score_new[vi]) + (1.0-damping) / nnodes;

         score_new[vi] += sum over all nodes v in graph with no outgoing edges
                            { damping * score_old[v] / nnodes }

         // compute how much per-node scores have changed
         // quit once algorithm has converged

         global_diff = sum over all nodes vi { abs(score_new[vi] - score_old[vi]) };
         converged = (global_diff < convergence)
       }

     */
    double *score_old = new double[nnodes];
    double *score_new = new double[nnodes];
    bool converged = false;
    double global_diff = 0.0;
    double all = 0.0;

    #pragma omp parallel for schedule(static)
    for(int i = 0 ; i < nnodes; i++){
      score_old[i] = solution[i];
    }
    
    while(!converged){
      global_diff = 0.0;
      all = 0.0;
      #pragma omp parallel for schedule(static) reduction(+:all)
      for(int v = 0; v < nnodes; v++){
        if(outgoing_size(g, v) == 0){
          all += score_old[v];
        }
      }
      all *= damping / nnodes;

      #pragma omp parallel for reduction(+:global_diff) schedule(static)
      for(int vi = 0 ; vi< nnodes; vi++){
        //  sum over all nodes vj reachable from incoming edges
        double sum = 0.0;
        for(const Vertex* vj = incoming_begin(g, vi); vj < incoming_end(g, vi); vj++){
          sum += score_old[*vj] / outgoing_size(g, *vj);
        }


        score_new[vi] = (damping * sum) + (1.0 - damping) / nnodes;

        score_new[vi] +=  all;
        global_diff += std::abs(score_new[vi] - score_old[vi]);
      }

      std::swap(score_old, score_new);
      converged = (global_diff < convergence);
    }
    #pragma omp parallel for schedule(static)
    for(int i = 0 ; i < nnodes; i++){
      solution[i] = score_old[i];
    }
    delete[] score_old;
    delete[] score_new;
}
