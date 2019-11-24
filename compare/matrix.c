/*----------------------------------------------------------------------------------------------------
  matrix_parallel.c - using mpi to run matrix multiplex parallel.
 *----------------------------------------------------------------------------------------------------
 */
 
#include<stdio.h>  
#include<time.h>  
#include<stdlib.h>  
#include "mpi.h"  

#define TAG_MATRIX_PARTITION 0x4560
typedef unsigned long long uint64_t;
typedef struct
{ 
	unsigned int m, n; // Rows, cols
	uint64_t *data; // Data, ordered by row, then by col
	uint64_t **rows; // Pointers to rows in data
} TMatrix;

TMatrix createMatrix (const unsigned int rows, const unsigned int cols);
void freeMatrix (TMatrix *matrix);
int validMatrix (TMatrix matrix);
TMatrix initMatrix (void);
TMatrix matrixMultiply (TMatrix A, TMatrix B);
void doMatrixMultiply (TMatrix A, TMatrix B, TMatrix C);
void printMatrix (char name[128],TMatrix A);

TMatrix createMatrix(const unsigned int rows, const unsigned int cols)
{ 
	TMatrix matrix;
	unsigned long int m, n;
	unsigned int i,j;
	m = rows; n = cols;
	matrix.m = rows;
	matrix.n = cols;
	matrix.data = (uint64_t *) malloc(sizeof(uint64_t) * m * n);
	matrix.rows = (uint64_t **) malloc(sizeof(uint64_t *) * m);
	if (validMatrix(matrix))
	{
		matrix.m = rows;
		matrix.n = cols;
		for (i = 0; i < rows; i++)
		{
			matrix.rows[i] = matrix.data + (i * cols);
		}
	}
	else
	{
		freeMatrix(&matrix);
	}
	return matrix;
}

void freeMatrix (TMatrix *matrix)
{
	if (matrix == NULL) return;
	if (matrix -> data) { free(matrix -> data); matrix -> data = NULL; }
	if (matrix -> rows) { free(matrix -> rows); matrix -> rows = NULL; }
	matrix -> m = 0;
	matrix -> n = 0;
}

int validMatrix (TMatrix matrix)
{
	if ((matrix.data == NULL) || (matrix.rows == NULL) ||
			(matrix.m == 0) || (matrix.n == 0))
		return 0;
	else return 1;
}

TMatrix initMatrix()
{
	TMatrix matrix;
	matrix.m = 0;
	matrix.n = 0;
	matrix.data = NULL;
	matrix.rows = NULL;
	return matrix;
}

TMatrix matrixMultiply(TMatrix A, TMatrix B)
{
	TMatrix C;
	C = initMatrix();
	if (validMatrix(A) && validMatrix(B) && (A.n == B.m))
	{
		C = createMatrix(A.m, B.n);
		if (validMatrix(C))
		{
			doMatrixMultiply(A, B, C);
		}
	}
	return C;
}
#if 1
void doMatrixMultiply(TMatrix A, TMatrix B, TMatrix C)
{
	unsigned int i, j, k;
	uint64_t sum;
       // printf("doMatrixMultiply 1");
	for (i = 0; i < A.m; i++) // Rows
		for (k = 0; k < A.n; k++)
			for (j = 0; j < B.n; j++) // Cols
				C.rows[i][j] += A.rows[i][k] * B.rows[k][j];
}
#else
void doMatrixMultiply(TMatrix A, TMatrix B, TMatrix C)
{
	unsigned int i, j, k;
	uint64_t sum;
     //  printf("doMatrixMultiply 2");
	for (i = 0; i < A.m; i++) // Rows
	{
		for (k = 0; k < B.n; k++) // Cols
		{
			sum = 0;
			for (j = 0; j < A.n; j++)
				sum += A.rows[i][k] * B.rows[k][j];
			C.rows[i][j] = sum;
		}
	}
}
#endif

void printMatrix(char name[128], TMatrix A)
{
	unsigned int i, j;
	printf("%s:\n", name);
	if (validMatrix(A))
	{
		for (i = 0; i < A.m; i++)
		{
			for (j = 0; j < A.n; j++)
				printf ("%7.3f ", A.rows[i][j]);
			printf ("\n");
		}
	}
}

void randSquareMatrix(int order, TMatrix *A)
{ 
	unsigned int i, j;
	*A = createMatrix(order, order);
	for (i = 0; i < order; i ++)
	{
		for (j = 0; j < order; j ++)
		{ 
			A -> rows[i][j] = rand();
		}
	}
}

int main (int argc, char *argv[])
{ 
	int processor_rank = 0;
	int processor_count = 1;
	MPI_Status status;
	TMatrix A,B,C,D;
	unsigned int m, n= 4, i, j, offset;
	double timek, time0, time1;
	uint64_t ts, te;
	A = initMatrix(); B = initMatrix();
	C = initMatrix(); D = initMatrix();
	MPI_Init(&argc, &argv);
	MPI_Comm_size (MPI_COMM_WORLD, &processor_count);
	MPI_Comm_rank (MPI_COMM_WORLD, &processor_rank );
	int quota, order;
	if (processor_rank == 0)
	{ 
		order = SZ;
	}
	//MPI_Bcast is both the sender and the receiver call
	// Broadcast (send) size of matrix
	MPI_Bcast((void *)&order, 1, MPI_INT, 0, MPI_COMM_WORLD);
	quota = order / processor_count;
	if (processor_rank == 0)
	{ 
		srand((int)time(0));  
		time0 = MPI_Wtime();
		randSquareMatrix(order, &A);
		//randSquareMatrix(order, &B);
		C = createMatrix(order, order);

		// Broadcast (send) B matrix
		//MPI_Bcast((void *)B.data, order*order, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
		// Send each process it's own part of A
		for (i = 1; i < processor_count; i++)
		{
                  // if(i == processor_count - 1)
		//	MPI_Send((void *)A.rows[quota*i], (order-(quota*(processor_count-1)))*order, 
		//		MPI_UNSIGNED_LONG_LONG, i, TAG_MATRIX_PARTITION, MPI_COMM_WORLD);
		 //  else
		       MPI_Send((void *)A.rows[quota*i], quota*order, MPI_UNSIGNED_LONG_LONG,
                                        i, TAG_MATRIX_PARTITION, MPI_COMM_WORLD);
		}
		timek = MPI_Wtime();
		printf ("Total time to send data : [%lf] seconds.\n", timek-time0);
               // printf("rank[]: quota is %d\n", (order-(quota*(processor_count-1))));
		// Multiply own part of matrix A with B into already existing matrix C
		A.m = quota;
		printf("rank[%d]: quota is %d\n", processor_rank, quota);
		doMatrixMultiply(A,B,C);
		A.m = order;
		// Receive part of C matrix from each process
		for (i = 1; i < processor_count; i++)
			MPI_Recv((void *)C.rows[quota*i], quota*order, MPI_UNSIGNED_LONG_LONG,
					i, TAG_MATRIX_PARTITION, MPI_COMM_WORLD, &status);
		// Record finish time
		time1 = MPI_Wtime();
#ifdef DUMP
		printMatrix("A",A);
		printMatrix("B",B);
		printMatrix("C",C);
#endif
		// Print time statistics
		int N=order;
		printf ("Total time using [%2d] processors : [%lf] seconds. W is %f MFLOPS.\n",
				processor_count, time1-time0, (float) (N*N/(1000000*(time1-time0)))*N);
	}
	else
	{
		printf("rank[%d]: quota is %d\n", processor_rank, quota);
		// Allocate memory for matrices
		A = createMatrix(quota, order);
		B = createMatrix(order, order);
		// Broadcast (receive) B matrix
		//MPI_Bcast((void *)B.data, order*order, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
		randSquareMatrix(order, &B);
		MPI_Recv((void *)A.data, quota*order, MPI_UNSIGNED_LONG_LONG, 0, TAG_MATRIX_PARTITION,
				MPI_COMM_WORLD, &status);
		// Multiply local matrices
		C = matrixMultiply(A,B);
		// Send back result
		MPI_Send((void *)C.data, quota*order, MPI_UNSIGNED_LONG_LONG, 0, TAG_MATRIX_PARTITION,
				MPI_COMM_WORLD);
	}
	// Free matrix data
	freeMatrix(&A); freeMatrix(&B); freeMatrix(&C);
	// Wait for everyone to stop
	MPI_Barrier(MPI_COMM_WORLD);
	// Always use MPI_Finalize as the last instruction of the program
	MPI_Finalize();
	return 0;
}

