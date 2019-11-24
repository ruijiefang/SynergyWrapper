/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 3/23/18.
   88888888888888888888888888888888888888888888888888 */

/* Synergy4-Wrapper Matrix Multiplication Worker Program */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include "../ts/ts.h"

static unsigned SIZE = 2000;
static unsigned PART = 250;
char buf[30];
double *P, *A, *B, *C;

inline static char *_get_name(char buf[], unsigned pt)
{
    sprintf(buf, "A_%u", pt);
    return buf;
}

inline static char *_get_name1(char buf[], unsigned pt)
{
    sprintf(buf, "C_%u", pt);
    return buf;
}

#if 0
void print(double *mat, size_t dim1, size_t dim2)
{
    //puts("=====================================");
    for (size_t i = 0; i < dim1; ++i) {
        for (size_t j = 0; j < dim2; ++j)
            //printf("%.2f ", mat[i * SIZE + j]);
        //puts("");
    }
    //puts("=====================================");
}
#endif

int main(int argc, char **argv)
{

    ts_t matrix_space;
    sng_tuple_t tB = {
            .anon_id = 0,
            .name = "B_",
            .len = 0,
            .data = NULL
    };
    sng_tuple_t tP = {
            .anon_id = 0,
            .name = ":A_*",
            .len = 0,
            .data = NULL
    };
    char *tplname = NULL;
    int pc, rank, r;
    unsigned i, pp, s1, _i, _j, _k;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &pc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    assert(argv[1]);

    if (argc >= 3) SIZE = (unsigned)strtoul(argv[2],NULL,10);
    if (argc == 4) PART = (unsigned)strtoul(argv[3],NULL,10);

    pp = PART / pc;

    if (rank == 0) {
        puts("Proxy: Opening tuple space");
        r = ts_open(&matrix_space, "/matrix", argv[1], 0);
        puts("Proxy: tuple space opened");
        assert(r == 0);
        puts("Proxy: taking tB from space");
        r = ts_out(&matrix_space, &tB, TsRead_Blocking);
        assert(r == 0);
	B = (double*) tB.data;
    } else {
	
        B = malloc(sizeof(double) * SIZE * SIZE);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(B, SIZE * SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    do {
        if (rank == 0) {
            puts("Proxy: Waiting on tP data");
            tP.name = ":A_*"; /*reset after each round */
            r = ts_out(&matrix_space, &tP, TsGet_Blocking);
            if (r != 0) {
                //puts("Proxy: exit()");
                MPI_Abort(MPI_COMM_WORLD, 0); /* Not the most elegant solution, but works */
                /* For more on termination, see
                 * https://stackoverflow.com/questions/5433697/terminating-all-processes-with-mpi
                 */
                ts_close(&matrix_space);
                return 0;
            } /*exit here */
            A = (double *) tP.data;
            tplname = tP.name;
            printf("Proxy: Got tP data [tP=%s,len=%lu]\n", tP.name, tP.len);
            puts("Proxy: Broadcasting B_Data");
            
	    /* XXX: Uncomment this if you want more fair load balancing */
	    //B = (double *) tB.data;
            //MPI_Barrier(MPI_COMM_WORLD);
            //MPI_Bcast(B, SIZE * SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            
	    /* before start */
            //puts("Proxy: synchronizing communicators");
            MPI_Barrier(MPI_COMM_WORLD);
            //puts("Proxy: sending tP to each node");
            P = A;
            for (i = 1; i < pc - 1; ++i)
                MPI_Send(A + i * pp * SIZE, pp * SIZE, MPI_DOUBLE, i, 2, MPI_COMM_WORLD);
            MPI_Send(A + i * pp * SIZE, (PART - i * pp) * SIZE, MPI_DOUBLE, i, 2, MPI_COMM_WORLD);
            //puts("Proxy: receiving our own tP");
            C = malloc(sizeof(double) * PART * SIZE);
            memset(C, 0, PART * SIZE * sizeof(double));
            s1 = pp;
        } else {
            //printf("Worker[%d]: Waiting on B\n", rank);
            
	    /* XXX */
	    //MPI_Barrier(MPI_COMM_WORLD);
            //MPI_Bcast(B, SIZE * SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            
	    //printf("Worker[%d]: Got B data\n", rank);

            //printf("Client[%d]: Allocating P,C and waiting on tP\n", rank);
//!!            memset(C, 0, pp * SIZE * sizeof(double));
            /*before start */
            MPI_Barrier(MPI_COMM_WORLD);
            //printf("Client %d: Received tP\n", rank);
            if (rank != pc - 1) {
                s1 = pp;
                P = malloc(sizeof(double) * s1 * SIZE);
                C = malloc(sizeof(double) * s1 * SIZE);
                MPI_Recv(P, pp * SIZE, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                s1 = PART - pp * (pc - 1);
                P = malloc(sizeof(double) * s1 * SIZE);
                C = malloc(sizeof(double) * s1 * SIZE);
                MPI_Recv(P, s1 * SIZE, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            // print(P, s1, SIZE);
        }
        //printf("[%d]: doing computation...\n", rank);
        for (_i = 0; _i < s1; ++_i)
            for (_k = 0; _k < SIZE; ++_k)
                for (_j = 0; _j < SIZE; ++_j)
                    C[_i * SIZE + _j] += P[_i * SIZE + _k] * B[_k * SIZE + _j];
        if (rank == 0) {
            //puts("Proxy: Collecting local results...");
            for (i = 1; i < pc - 1; ++i)
                MPI_Recv(C + i * pp * SIZE, pp * SIZE, MPI_DOUBLE, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(C + i * pp * SIZE, (PART - i * pp) * SIZE, MPI_DOUBLE, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	   
            tplname[0] = 'C';
            //printf("Proxy: putting result tuple back [tpname=%s,len=%lu]\n", tplname, PART * SIZE * sizeof(double));
            //print(C, PART, SIZE);
            sng_tuple_t Ctuple = {
                    .len = PART * SIZE * sizeof(double),
                    .data = C,
                    .anon_id = 0,
                    .name = tplname
            };
            r = ts_in(&matrix_space, &Ctuple, TsPut);
            free(tplname);
            puts("Proxy: Finished 1 round of computation.");
        } else {
            MPI_Send(C, s1 * SIZE, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);
            //printf("Client [%d]: Finished 1 round of computation.\n", rank);
        }
        /* finalize for this round */
        free(P); /* !! for proxy, A==P <- result ensured by valgrind :-) */
        free(C);
        P = A = C = NULL;
    } while (1);
    return 0;
}
