#include "bfs.h"

#include <cstdlib>
#include <omp.h>

#include "../common/graph.h"
// #define VERBOSE true
#ifdef VERBOSE
#include "../common/cycle_timer.h"
#include <stdio.h>
#endif // VERBOSE

constexpr int ROOT_NODE_ID = 0;
constexpr int NOT_VISITED_MARKER = -1;

void vertex_set_clear(VertexSet *list)
{
    list->count = 0;
}

void vertex_set_init(VertexSet *list, int count)
{
    list->max_vertices = count;
    list->vertices = new int[list->max_vertices];
    vertex_set_clear(list);
}

void vertex_set_destroy(VertexSet *list)
{
    delete[] list->vertices;
}

// Take one step of "top-down" BFS.  For each vertex on the frontier,
// follow all outgoing edges, and add all neighboring vertices to the
// new_frontier.
int top_down_step(Graph g, int *distances, int depth)
{   

    int count = 0;

    #pragma omp parallel for reduction(+:count) schedule(dynamic, 1024)
    for (int i = 0; i < g->num_nodes; i++)
    {
        if(distances[i] != depth){
            continue;
        }
        int start_edge = g->outgoing_starts[i];
        int end_edge = (i == g->num_nodes - 1) ? g->num_edges : g->outgoing_starts[i + 1];
        // attempt to add all neighbors to the new frontier
        for (int neighbor = start_edge; neighbor < end_edge; neighbor++)
        {
            int outgoing = g->outgoing_edges[neighbor];
            if(distances[outgoing] == NOT_VISITED_MARKER)
            {
                distances[outgoing] = depth + 1;
                count+=1;
            }
        }
    }
    return count;
}

// Implements top-down BFS.
//
// Result of execution is that, for each node in the graph, the
// distance to the root is stored in sol.distances.
void bfs_top_down(Graph graph, solution *sol)
{


    // initialize all nodes to NOT_VISITED
    #pragma omp parallel for
    for (int i = 0; i < graph->num_nodes; i++){
        sol->distances[i] = NOT_VISITED_MARKER;
    }

    // setup frontier with the root node
    sol->distances[ROOT_NODE_ID] = 0;
    int depth = 0;
    int current_count = 1;
    while (current_count != 0)
    {

#ifdef VERBOSE
        double start_time = CycleTimer::current_seconds();
#endif

        current_count = top_down_step(graph, sol->distances, depth);

#ifdef VERBOSE
        double end_time = CycleTimer::current_seconds();
        printf("frontier=%-10d %.4f sec\n", frontier->count, end_time - start_time);
#endif

        depth ++;
    }

}

int bottom_up_step(Graph g, int *distances, int depth)
{   
    int count = 0;
    #pragma omp parallel for reduction(+:count) schedule(dynamic, 1024)
    for(int i = 0 ; i < g->num_nodes; i++){
        if(distances[i] != NOT_VISITED_MARKER){
            continue;
        }
        int start_edge = g->incoming_starts[i];
        int end_edge = (i == g->num_nodes - 1) ? g->num_edges : g->incoming_starts[i + 1];
        for(int neighbor = start_edge; neighbor < end_edge; neighbor++){
            int incoming = g->incoming_edges[neighbor];
            if(distances[incoming] == depth){
                distances[i] = depth + 1;
                count++;
                break;
            }
        }
    }
    return count;
}

void bfs_bottom_up(Graph graph, solution *sol)
{
    // For PP students:
    //
    // You will need to implement the "bottom up" BFS here as
    // described in the handout.
    //
    // As a result of your code's execution, sol.distances should be
    // correctly populated for all nodes in the graph.
    //
    // As was done in the top-down case, you may wish to organize your
    // code by creating subroutine bottom_up_step() that is called in
    // each step of the BFS process.

    #pragma omp parallel for
    for (int i = 0; i < graph->num_nodes; i++){
        sol->distances[i] = NOT_VISITED_MARKER;
    }

    sol->distances[ROOT_NODE_ID] = 0;
    int depth = 0;
    int current_count = 1;
    while(current_count != 0){
        current_count = bottom_up_step(graph, sol->distances, depth);
        depth++;
    }
    
}

#define PAR 0.3

void bfs_hybrid(Graph graph, solution *sol)
{
    // For PP students:
    //
    // You will need to implement the "hybrid" BFS here as
    // described in the handout.
    #pragma omp parallel for
    for (int i = 0; i < graph->num_nodes; i++){
        sol->distances[i] = NOT_VISITED_MARKER;
    }
    sol->distances[ROOT_NODE_ID] = 0;
    int depth = 0;
    int current_count = 1;
    int threshold = graph->num_nodes * PAR;
    while(current_count != 0){
        if(current_count > threshold){
            current_count = bottom_up_step(graph, sol->distances, depth);
        }
        else{
            current_count = top_down_step(graph, sol->distances, depth);
        }
        depth++;
    }
}
