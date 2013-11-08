/**
* 	Name: Yang Zhixing
*	Matric Number: A0091726B
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define FIELD_LENGTH 128
#define FIELD_WIDTH 64

#define	FIELD_PROCESS 5
#define	NUM_OF_ROUNDS 900
#define NUM_OF_PLAYERS 5


int main(int argc, char *argv[]){
	int rank, numOfTasks;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numOfTasks);

	if (numOfTasks != NUM_OF_PLAYERS + 1){
		printf("Please play with exactly %d process\n", NUM_OF_PLAYERS + 1);
		MPI_Finalize();
		return 0;
	}

	// Set a random seed:
	srand(time(NULL) + rand());

	
}
