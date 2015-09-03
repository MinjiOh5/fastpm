#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <mpi.h>
#include "pmpfft.h"

void pm_ghost_data_init(PMGhostData * ppd, PM * pm, void * pdata, size_t np) {
    ppd->pdata = pdata;
    ppd->np = np;
    ppd->Nsend = calloc(pm->NTask, sizeof(int));
    ppd->Osend = calloc(pm->NTask, sizeof(int));
    ppd->Nrecv = calloc(pm->NTask, sizeof(int));
    ppd->Orecv = calloc(pm->NTask, sizeof(int));
}
void pm_ghost_data_destroy(PMGhostData * ppd) {
    free(ppd->Nsend);
    free(ppd->Osend);
    free(ppd->Nrecv);
    free(ppd->Orecv);
}

static void pm_iter_ghosts(PM * pm, PMGhostData * ppd, 
    pm_iter_ghosts_func iter_func) {

    ptrdiff_t i;
    ptrdiff_t ighost = 0;
    double CellSize[3];
    int d;
    for(d = 0; d < 3; d ++) {
        CellSize[d] = pm->BoxSize[d] / pm->Nmesh[d];
    }
    for (i = 0; i < ppd->np; i ++) {
        double pos[3];
        int rank;
        pm->iface.get_position(ppd->pdata, i, pos);

        /* probe neighbours */
        double j[3];
        int ranks[1000];
        int used = 0;
        ppd->ipar = i;
        for(j[2] = pm->Below[2]; j[2] <= pm->Above[2]; j[2] ++)
        for(j[0] = pm->Below[0]; j[0] <= pm->Above[0]; j[0] ++)
        for(j[1] = pm->Below[1]; j[1] <= pm->Above[1]; j[1] ++) {
            double npos[3];
            int d;
            for(d = 0; d < 3; d ++) {
                npos[d] = pos[d] + j[d] * CellSize[d];
            }
            rank = pm_pos_to_rank(pm, npos);
            if(rank == pm->ThisTask)  continue;
            int ptr;
            for(ptr = 0; ptr < used; ptr++) {
                if(rank == ranks[ptr]) break;
            } 
            if(ptr == used) {
                ranks[used++] = rank;
                ppd->ighost = ighost;
                ppd->rank = rank;
                iter_func(pm, ppd);
                ighost ++;
            } 
        }
    }
}

static void count_ghosts(PM * pm, PMGhostData * ppd) {
    ppd->Nsend[ppd->rank] ++;
}

static void build_ghost_buffer(PM * pm, PMGhostData * ppd) {
    pm->iface.pack(ppd->pdata, ppd->ipar, 
        (char*) ppd->send_buffer + ppd->ighost * ppd->elsize, pm->init.GhostAttributes);
}

size_t pm_append_ghosts(PM * pm, size_t np_upper, PMGhostData * ppd) {
    ptrdiff_t i;
    size_t Nsend;
    size_t Nrecv;
    size_t elsize = pm->iface.pack(NULL, 0, NULL, pm->init.GhostAttributes);

    ppd->elsize = elsize;

    memset(ppd->Nsend, 0, sizeof(ppd->Nsend[0]) * pm->NTask);

    pm_iter_ghosts(pm, ppd, count_ghosts);

    Nsend = cumsum(ppd->Osend, ppd->Nsend, pm->NTask);

    ppd->send_buffer = malloc(Nsend * ppd->elsize);

    pm_iter_ghosts(pm, ppd, build_ghost_buffer);

    /* exchange */
    MPI_Alltoall(ppd->Nsend, 1, MPI_INT, ppd->Nrecv, 1, MPI_INT, pm->Comm2D);

    Nrecv = cumsum(ppd->Orecv, ppd->Nrecv, pm->NTask);
    
    ppd->recv_buffer = malloc(Nrecv * ppd->elsize);

    ppd->nghosts = Nrecv;

    if(Nrecv + ppd->np > np_upper) {
        fprintf(stderr, "Too many ghosts; asking for %td, space for %td\n", Nrecv, np_upper - ppd->np);
        MPI_Abort(pm->Comm2D, -1);
    }

    MPI_Datatype GHOST_TYPE;
    MPI_Type_contiguous(ppd->elsize, MPI_BYTE, &GHOST_TYPE);
    MPI_Type_commit(&GHOST_TYPE);
    MPI_Alltoallv(ppd->send_buffer, ppd->Nsend, ppd->Osend, GHOST_TYPE,
                  ppd->recv_buffer, ppd->Nrecv, ppd->Orecv, GHOST_TYPE,
                    pm->Comm2D);
    MPI_Type_free(&GHOST_TYPE);

    for(i = 0; i < Nrecv; i ++) {
        pm->iface.unpack(ppd->pdata, ppd->np + i, 
                (char*) ppd->recv_buffer + i * ppd->elsize, 
                        pm->init.GhostAttributes);
    }
    free(ppd->recv_buffer);
    free(ppd->send_buffer);

    /*
    free(ppd->Orecv);
    free(ppd->Osend);
    free(ppd->Nrecv);
    free(ppd->Nsend);
    */
    return Nrecv;
}

static void reduce_ghosts(PM * pm, PMGhostData * ppd) {
    pm->iface.reduce(ppd->pdata, ppd->ipar, 
        (char*) ppd->send_buffer + ppd->ighost * ppd->elsize, 
        ppd->ReductionAttributes);
}

void pm_reduce_ghosts(PM * pm, PMGhostData * ppd, int attributes) {
    size_t Nsend = cumsum(NULL, ppd->Nsend, pm->NTask);
    size_t Nrecv = cumsum(NULL, ppd->Nrecv, pm->NTask);
    ptrdiff_t i;

    ppd->elsize = pm->iface.pack(NULL, 0, NULL, attributes);
    ppd->recv_buffer = malloc(Nrecv * ppd->elsize);
    ppd->send_buffer = malloc(Nsend * ppd->elsize);
    ppd->ReductionAttributes = attributes;

    for(i = 0; i < ppd->nghosts; i ++) {
        pm->iface.pack(ppd->pdata, i + ppd->np, 
            (char*) ppd->recv_buffer + i * ppd->elsize, 
            ppd->ReductionAttributes);
    }

    MPI_Datatype GHOST_TYPE;
    MPI_Type_contiguous(ppd->elsize, MPI_BYTE, &GHOST_TYPE);
    MPI_Type_commit(&GHOST_TYPE);
    MPI_Alltoallv(ppd->recv_buffer, ppd->Nrecv, ppd->Orecv, GHOST_TYPE,
                  ppd->send_buffer, ppd->Nsend, ppd->Osend, GHOST_TYPE,
                    pm->Comm2D);
    MPI_Type_free(&GHOST_TYPE);

    /* now reduce the attributes. */
    pm_iter_ghosts(pm, ppd, reduce_ghosts);
}
