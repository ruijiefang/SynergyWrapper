/* 88888888888888888888888888888888888888888888888888
 Created by Ruijie Fang on 3/23/18.
   88888888888888888888888888888888888888888888888888 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include "../ts/ts.h"

static unsigned SIZE = 2000;
static unsigned PART = 250;
char buf[30];
double *A, *B, *C, *P;

/* Synergy4-Wrapper Matrix Multiplication Master */

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


const static double eps = 0.000000001;
int verify()
{
    double ij;
    unsigned i, j, k;
    for (i = 0; i < SIZE; ++i)
        for (j = 0; j < SIZE; ++j) {
            ij = 0;
            for (k = 0; k < SIZE; ++k)
                ij += A[i * SIZE + k] * B[k * SIZE + j];
            if (!(ij <= C[i * SIZE + j] + eps && ij >= C[i * SIZE + j] - eps)) {
                printf("C[%u][%u]:=%f, != %f\n", i, j, C[i * SIZE + j], ij);
                return 0;
            }
        }
    return 1;
}

#if 0
void print()
{
    unsigned i,j;
    for(i=0;i<SIZE;++i) {
        for (j = 0; j < SIZE; ++j)
            printf("%.3f ",C[i*SIZE+j]);
        printf("\n");
    }
}
#endif

int main(int argc, char **argv)
{
    ts_t matrix_space;
    sng_tuple_t tB = {
            .anon_id = 0,
            .name = "B_",
            .len = SIZE * SIZE * sizeof(double),
            .data=B};
    double t_start, t_end, t_diff;
    int pc, rank, r;
    unsigned i, j, p, pp, s1;

    /* init MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &pc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    assert(argv[1]);

    if (argc >= 3)
         SIZE = (unsigned)strtoul(argv[2],NULL,10);
    if (argc == 4)
         PART = (unsigned)strtoul(argv[3],NULL,10);

    //puts("Opening matrix space...");

    if (rank == 0) {
        /* init ts */
        r = ts_open(&matrix_space, "/matrix", argv[1], 0);
        assert(r == 0);
        t_start = MPI_Wtime();
    }

    //puts("Precomputation phase start");

    /* common stuff */
    pp = PART / pc;
    s1 = pp;
    B = malloc(SIZE * SIZE * sizeof(double));
    assert(B);
    /* distribute */
    if (rank == 0) {
        //puts("master: delivering tuples");
        C = malloc(SIZE * SIZE * sizeof(double));
        A = malloc(SIZE * SIZE * sizeof(double)),
                assert(C), assert(A);

        for (i = 0; i < SIZE; ++i)
            for (j = 0; j < SIZE; ++j)
                A[i * SIZE + j] = (double) i + j, B[i * SIZE + j] = i + j;
        tB.data = B;
        tB.len = sizeof(double) * SIZE * SIZE;
        //puts("master: putting tB tuple");
        r = ts_in(&matrix_space, &tB, TsPut);
        assert(r == 0);

        //puts("master: sending A_Parts...");
        for (i = PART, p = 0; i < SIZE; i += PART, ++p) {
            sng_tuple_t tA = {
                    .anon_id = 0,
                    .name = _get_name(buf, p),
                    .len = PART * SIZE * sizeof(double),
                    .data = &A[i * SIZE]
            };
            printf("Sending part %d[%d]\n", i, p);
            r = ts_in(&matrix_space, &tA, TsPut);
            assert(r == 0);
        }
        puts("master: synchronizing");
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(B, SIZE * SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        //puts("master: sending to local processes");
        for (i = 1; i < pc - 1; ++i)
            MPI_Send(A + i * pp * SIZE, pp * SIZE, MPI_DOUBLE, i, 2, MPI_COMM_WORLD);
        MPI_Send(A + i * pp * SIZE, (PART - (i * pp)) * SIZE, MPI_DOUBLE, i, 2, MPI_COMM_WORLD);
        //puts("master: sending complete");
        P = A;
        s1 = pp;
    } else {
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Bcast(B, SIZE * SIZE, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (rank != pc - 1) {
            //puts("Client: receiving parts");
            s1 = pp;
            P = malloc(sizeof(double) * pp * SIZE);
            C = malloc(sizeof(double) * pp * SIZE);
            MPI_Recv(P, pp * SIZE, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            //puts("client: receiving parts");
            s1 = (PART - pp * (pc - 1));
            P = malloc(sizeof(double) * SIZE * s1);
            C = malloc(sizeof(double) * SIZE * s1);
            MPI_Recv(P, SIZE * s1, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    //puts("Do work...");
    unsigned _i, _j, _k;
    for (_i = 0; _i < s1; ++_i)
        for (_k = 0; _k < SIZE; ++_k)
            for (_j = 0; _j < SIZE; ++_j)
                C[_i * SIZE + _j] += P[_i * SIZE + _k] * B[_k * SIZE + _j];
    //puts("Work done");
    MPI_Barrier(MPI_COMM_WORLD);
    /* collect */
    if (rank == 0) {
        //puts("master: collecting local results");
        /* local recv */
        for (i = 1; i < pc - 1; ++i)
            MPI_Recv(C + i * pp * SIZE, pp * SIZE, MPI_DOUBLE, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(C + i * pp * SIZE, (PART - i * pp) * SIZE, MPI_DOUBLE, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        //puts("master: collecting remote results");
        memset(buf, '\0', 30);
        for (i = PART, p = 0; i < SIZE; i += PART, ++p) {
            sng_tuple_t tC =
                    {
                            .anon_id = 0,
                            .name = _get_name1(buf, p),
                            .len = 0,
                            .data = NULL
                    };
            printf("master: collecting remote %d[%d]\n", i, p);
            r = ts_out(&matrix_space, &tC, TsGet_Blocking);
            assert(r == 0);
          //  memcpy(&C[i * SIZE], tC.data, tC.len);
            free(tC.data);
        }
        //puts("Master: verifying computation...");
        //  verify();
    } else {
        printf("Client[%d]: sending back to master\n", rank);
        MPI_Send(C, s1 * SIZE, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD);
    }
    /* Finished :-) */
    if (rank == 0) {
        t_end = MPI_Wtime();
        printf(" === Time spent: %f seconds\n", t_end - t_start);
        free(A);
        free(B);
        free(C);
        MPI_Finalize();
        ts_close(&matrix_space);
    } else {
        MPI_Finalize();
        free(C);
        free(P);
    }
    puts(" ====================== Congradulations! Matrix Multiplication successfully performed =================== ");
    return 0;
}
