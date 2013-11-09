/**
* 	Name: Yang Zhixing
*	Matric Number: A0091726B
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define FIELD_LENGTH 128
#define FIELD_WIDTH 64
#define FIELD_PROCESS 5
#define INVALID_PROCESS -1
#define NUM_OF_ROUNDS 900
#define NUM_OF_PLAYERS 5
#define INITIAL_BALL_POSITION_X 64
#define INITIAL_BALL_POSITION_Y 32
#define MPI_BUFFER_SIZE 128
#define MAX_DISTANCE_PER_ROUND 10
#define MIN_DISTANCE_PER_ROUND 1
#define RAMDOM_FACTOR 100
// SHOULD_OUTPUT_DETAILS: 1: print detailed info. 0: print out brief info.
#define SHOULD_OUTPUT_DETAILS 0

typedef struct{
	int x;
	int y;
} Position;

typedef struct{
	Position lastPosition;
	Position currentPosition;
	int isBallReached;
	int isBallWinned;
	int runnedDistance;
	int countReachBall;
	int countWinBall;
} Player;

typedef struct{
	Position currentPosition;
	Position lastPosition;
} Ball;

/*
* Run towards the ball.
* If able to reach, reach. If not, go all the way horizontally,
* and then vertically.
*/
void runTowardsBall(Player *self, Ball ball, int distanceInCurrentRound){

	int distance = getDistance(ball.currentPosition, (*self).currentPosition);
	
	if (distance <= distanceInCurrentRound){
		(*self).isBallReached = 1;
		(*self).runnedDistance += distance;
		(*self).countReachBall++;
		(*self).currentPosition.x = ball.currentPosition.x;
		(*self).currentPosition.y = ball.currentPosition.y;
	}

	else{
		(*self).isBallReached = 0;
		(*self).runnedDistance += distanceInCurrentRound;

		int distanceX = ball.currentPosition.x - (*self).currentPosition.x;
		int distanceY = ball.currentPosition.y - (*self).currentPosition.y;
		int directionX = distanceX >= 0 ? 1 : -1;
		int directionY = distanceY >= 0 ? 1 : -1;
		
		if (distanceInCurrentRound >= abs(distanceX)){
			(*self).currentPosition.x = ball.currentPosition.x;
			(*self).currentPosition.y = (*self).currentPosition.y + directionY * (distanceInCurrentRound - abs(distanceX));
		} else if (distanceInCurrentRound >= abs(distanceY)){
			(*self).currentPosition.x = (*self).currentPosition.x + directionX * (distanceInCurrentRound - abs(distanceY));
			(*self).currentPosition.y = ball.currentPosition.y;
		} else{
			int stepHorizontal = getRandomNumberWithinRange(0, abs(distanceInCurrentRound));
			(*self).currentPosition.x = (*self).currentPosition.x + directionX * stepHorizontal;
			(*self).currentPosition.y = (*self).currentPosition.y + directionY * (distanceInCurrentRound - stepHorizontal);			
		}
	}

	//printf("In runTowardsBall function. Ball: (%d, %d). Step: %d. Original: (%d, %d). New: (%d, %d)\n", ball.currentPosition.x, ball.currentPosition.y, distanceInCurrentRound, (*self).lastPosition.x, (*self).lastPosition.y, (*self).currentPosition.x, (*self).currentPosition.y);
}


/*
* Return a random integer in the range [start, end], inclusively.
*/
int getRandomNumberWithinRange(int start, int end){
	return rand() % (end - start + 1) + start;
}

int getDistance(Position p, Position q){
	return abs(p.x - q.x) + abs(p.y - q.y);
}

/*	
* Main Function:
*/

int main(int argc, char *argv[]){
	int rank, numOfTasks;
	int currentRound;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numOfTasks);

	if (numOfTasks != NUM_OF_PLAYERS + 1){
		printf("Please play with exactly %d process\n", NUM_OF_PLAYERS + 1);
		MPI_Finalize();
		return 0;
	}

	// Set a random seed:
	srand(time(NULL) + rank * RAMDOM_FACTOR);

	// Information:
	Ball ball;
	Player self;

	// Auxiliary Variables:
	int receiveBuffer[MPI_BUFFER_SIZE];
	int sendBuffer[MPI_BUFFER_SIZE];

	for (currentRound = 0; currentRound < NUM_OF_ROUNDS; currentRound++){
		// Field process:
		if(rank == FIELD_PROCESS){
			if(currentRound == 0){
				// Initialize and broadcast the location of ball:
				ball.currentPosition.x = INITIAL_BALL_POSITION_X;
				ball.currentPosition.y = INITIAL_BALL_POSITION_Y;
			}

			ball.lastPosition = ball.currentPosition;

			// Mark A. Broadcast the ball's location.
			sendBuffer[0] = ball.currentPosition.x;
			sendBuffer[1] = ball.currentPosition.y;
			MPI_Bcast(&sendBuffer, 2, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);

			// Mark B. Gather info about whether any player has reached the ball
			MPI_Gather(&sendBuffer, 1, MPI_INT, &receiveBuffer, 1, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);

			int reachCount = 0;
			int reachedPlayers[NUM_OF_PLAYERS] = {0}; // e.g. [0, 2, 3, 0, 0] means rank 0, 2 and 3 has reached.
			int i;
			for (i = 0; i < NUM_OF_PLAYERS; i++){
				if (receiveBuffer[i] == 1){
					reachedPlayers[reachCount++] = i;
				}
			}
			
			int winnerRank;
			if (reachCount == 0){
				winnerRank = INVALID_PROCESS;	
			} else if (reachCount == 1){
				winnerRank = reachedPlayers[0];
			} else{
				// Multiple person has reached the ball. Select a random.
				int winnerIndex = getRandomNumberWithinRange(0, reachCount - 1);
				winnerRank = reachedPlayers[winnerIndex];
				sendBuffer[0] = winnerRank;
			}

			// Mark C. Tell the players who win.
			sendBuffer[0] = winnerRank;
			MPI_Bcast(&sendBuffer, 1, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);

			// If someone wins:	
			if (winnerRank != INVALID_PROCESS){
				// Mark D. Receive the new location of the ball.
				MPI_Scatter(&sendBuffer, 2, MPI_INT, &receiveBuffer, 2, MPI_INT, winnerRank, MPI_COMM_WORLD);
				if (reachCount != 0){
					if (SHOULD_OUTPUT_DETAILS){
						printf("In field process: detected %d people reach in the next round:", reachCount);
						int p;
						for (p = 0; p < reachCount; p++) {
							printf("%d ", reachedPlayers[p]);
						}
						printf("\nAnd rank %d wins and kicks the ball to: (%d, %d)\n", winnerRank, receiveBuffer[0], receiveBuffer[1]);
					}
					ball.currentPosition.x = receiveBuffer[0];
					ball.currentPosition.y = receiveBuffer[1];
				}
			}

			// Mark E. Gather updates from each one, after the current round.
			MPI_Gather(&sendBuffer, 9, MPI_INT, &receiveBuffer, 9, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);
			
			// Print out brief info:
			if (!SHOULD_OUTPUT_DETAILS){
				printf("%d\n", currentRound);
				printf("%d %d\n", ball.currentPosition.x, ball.currentPosition.y);
				int j;
				for (j = 0; j < NUM_OF_PLAYERS; j++){
					int k;
					printf("%d ", j);
					for (k = 0; k < 9; k++){
						printf("%d ", receiveBuffer[j * 9 + k]);
					}
					printf("\n");
				}
				printf("\n");
			} else{
				//Print-out with details:
				printf("\n\nRound %d\n", currentRound);
				printf("Ball's location: (%d, %d) ==> (%d, %d)\n", ball.lastPosition.x, ball.lastPosition.y, ball.currentPosition.x, ball.currentPosition.y);
				int j;
				for (j = 0; j < NUM_OF_PLAYERS; j++){
						printf("\nRank: %d\n", j);
						printf("Location: (%d, %d) ==> (%d, %d)\n", receiveBuffer[j * 9 + 0], receiveBuffer[j * 9 + 1], receiveBuffer[j * 9 + 2], receiveBuffer[j * 9 + 3]);
						printf("Reached? %d\n", receiveBuffer[j * 9 + 4]);
						printf("Winned? %d\n", receiveBuffer[j * 9 + 5]);
						printf("Total Running: %d\n", receiveBuffer[j * 9 + 6]);
						printf("Reach Count: %d\n", receiveBuffer[j * 9 + 7]);
						printf("Win Count: %d\n", receiveBuffer[j * 9 + 8]);
				}
				printf("\n");
			}
		} 

		// Player process:
		else{

			if(currentRound == 0){
				self.currentPosition.x = getRandomNumberWithinRange(0, FIELD_LENGTH);
				self.currentPosition.y = getRandomNumberWithinRange(0, FIELD_WIDTH);
				self.runnedDistance = 0;
				self.countReachBall = 0;
				self.countWinBall = 0;
			}

			self.lastPosition = self.currentPosition;

			// Mark A. Respond to the field's broadcast of ball location:
			MPI_Bcast(&receiveBuffer, 2, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);
			ball.currentPosition.x = receiveBuffer[0];
			ball.currentPosition.y = receiveBuffer[1];

			//printf("Received ball location: (%d, %d)\n", ball.currentPosition.x, ball.currentPosition.y);

			int distanceInCurrentRound = getRandomNumberWithinRange(MIN_DISTANCE_PER_ROUND, MAX_DISTANCE_PER_ROUND);
			runTowardsBall(&self, ball, distanceInCurrentRound);

			// Mark B. Send the info about whether I've reached the ball:
			sendBuffer[0] = self.isBallReached;
			MPI_Gather(&sendBuffer, 1, MPI_INT, &receiveBuffer, 1, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);

			// Mark C. Receive the information about who wins.
			MPI_Bcast(&receiveBuffer, 1, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);
			int winnerRank = receiveBuffer[0];
			
			// If someone wins:
			if (winnerRank != INVALID_PROCESS){
				
				// If I win:
				if (winnerRank == rank){

					self.isBallWinned = 1;
					self.countWinBall++;

					// Kick the ball to a random location:
					sendBuffer[FIELD_PROCESS * 2] = getRandomNumberWithinRange(0, FIELD_LENGTH);
					sendBuffer[FIELD_PROCESS * 2 + 1] = getRandomNumberWithinRange(0, FIELD_WIDTH);

					// Mark D. Winner sends the new location.
					MPI_Scatter(&sendBuffer, 2, MPI_INT, &receiveBuffer, 2, MPI_INT, winnerRank, MPI_COMM_WORLD);
					
				} 

				// If the winner is not me.
				else{
					self.isBallWinned = 0;
					// Mark D. Non-Winner receives the scatter and do nothing.
					MPI_Scatter(&sendBuffer, 2, MPI_INT, &receiveBuffer, 2, MPI_INT, winnerRank, MPI_COMM_WORLD);
				}
			} 
			// Nobody wins. Everyone set this field to zero:
			else{
				self.isBallWinned = 0;
			}
			
			// Mark E. Send updates of personal info.
			MPI_Gather(&self, 9, MPI_INT, &receiveBuffer, 9, MPI_INT, FIELD_PROCESS, MPI_COMM_WORLD);
		}
		
		
		MPI_Barrier(MPI_COMM_WORLD);
	}
	MPI_Finalize();

	return 0;
}


