/**
* 	Name: Yang Zhixing
*	Matric Number: A0091726B
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define FIELD_LENGTH 128
#define FIELD_WIDTH 64
#define FIELD_LEFT_PROCESS 10
#define FIELD_RIGHT_PROCESS 11
#define INVALID_PROCESS -1
#define NUM_OF_ROUNDS 9
#define NUM_OF_PLAYERS 10
#define NUM_OF_SKILLS 3
#define TEAM_SIZE 5
#define INITIAL_BALL_POSITION_X 64
#define INITIAL_BALL_POSITION_Y 32
#define MPI_BUFFER_SIZE 128
#define MAX_DISTANCE_PER_ROUND 10
#define MIN_DISTANCE_PER_ROUND 1
#define RAMDOM_FACTOR 100
// SHOULD_OUTPUT_DETAILS: 1: print detailed info. 0: print out brief info.
#define SHOULD_OUTPUT_DETAILS 0

// Tags:
#define TAG_SEND_CHALLENGE 0
#define TAG_BALL_LOCATION 1
#define TAG_FIELD_TO_FIELD 2
#define TAG_EXCHANGE_WINNER 3
#define TAG_EXCHANGE_CHALLENGE 4
#define TAG_BROADCAST_WINNER 5
#define TAG_UPDATE_LOCATION 6
#define TAG_EXCHANGE_ALL_LOCATION 7


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
	int speed;
	int dribbling;
	int shooting;
} Player;

typedef struct{
	Position currentPosition;
	Position lastPosition;
	int fieldProcess;
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
		(*self).currentPosition.x = ball.currentPosition.x;
		(*self).currentPosition.y = ball.currentPosition.y;
	}

	else{
		(*self).isBallReached = 0;

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
* Return FIELD_LEFT_PROCESS or FIELD_RIGHT_PROCESS
* Return -1 if out-of-range
*/
int getFieldBelongsTo(Position p){
	if (p.x < 0 || p.x > FIELD_LENGTH || p.y < 0 || p.y > FIELD_WIDTH){
		return -1;
	}
	if (p.x < FIELD_WIDTH / 2){
		return FIELD_LEFT_PROCESS;
	} else{
		return FIELD_RIGHT_PROCESS;
	}
}

/*
* Return the index of the max in array.
*/
int getMaxInArray(int arr[], int size){
	int i;
	int max = arr[0];
	int maxIndex = 0;
	if (size <= 1){
		return 0;
	}
	for (i = 1; i < size; i++){
		if (arr[i] > max){
			max = arr[i];
			maxIndex = i;
		}
	}
	return maxIndex;
}

/*	
* Main Function:
*/

int main(int argc, char *argv[]){
	int rank, numOfTasks;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numOfTasks);

	if (numOfTasks != NUM_OF_PLAYERS + 2){
		printf("Please play with exactly %d process\n", NUM_OF_PLAYERS + 2);
		MPI_Finalize();
		return 0;
	}

	int skillSet[NUM_OF_PLAYERS][NUM_OF_SKILLS] = {
		{5, 5, 5}, {5, 6, 4}, {5, 7, 3}, {5, 8, 2}, {5, 9, 1}, // Team A good at dribbling 
		{5, 5, 5}, {5, 4, 6}, {5, 3, 7}, {5, 2, 8}, {5, 1, 9}  // Team B good at shooting
	};
	
	int initialLocations[NUM_OF_PLAYERS][NUM_OF_SKILLS] = {
		{48, 22}, {58, 22}, {58, 32}, {58, 42}, {48, 42}, // Left Half of the field
		{82, 42}, {72, 42}, {72, 32}, {72, 22}, {82, 22}  // Right Half of the field
	};
	/* The initial location is arranged as follows. x is the player and o is the ball.

		FIELD_LEFT_PROCESS		FIELD_RIGHT_PROCESS
					x 	x 	| 	x 	x
						x 	o 	x 	
					x 	x 	| 	x 	x

	*/

	int currentRound;
	int isScored = 0;
	Ball ball;
	Player self;

	// The field process has this to keep track of who is in them:
	int playersInThisField[NUM_OF_PLAYERS] = {-1};
	int numPlayersInThisField = 0;

	int sendBuffer[MPI_BUFFER_SIZE];
	int tempBuffer[MPI_BUFFER_SIZE];
	int zopeBuffer[MPI_BUFFER_SIZE];
	int receiveBuffer[MPI_BUFFER_SIZE];
	Position locationRecords[NUM_OF_PLAYERS];

	MPI_Status status;
	MPI_Request sendRequest, receiveRequest;
	int fieldInCharge;
	int FIELD_ME, FIELD_THE_OTHER;

	if (rank == FIELD_LEFT_PROCESS){
		FIELD_ME = FIELD_LEFT_PROCESS;
		FIELD_THE_OTHER = FIELD_RIGHT_PROCESS;
	} else if (rank == FIELD_RIGHT_PROCESS){
		FIELD_ME = FIELD_RIGHT_PROCESS;
		FIELD_THE_OTHER = FIELD_LEFT_PROCESS;
	}

	for (currentRound = 0; currentRound < NUM_OF_ROUNDS * 2; currentRound++){

		if (currentRound == 0 || isScored == 1){
			isScored = 0;
			
			// Field process:
			if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){
				// Reset the ball position
				ball.currentPosition.x = 64;
				ball.currentPosition.y = 32;

				// Reset the locationRecords[]
				int i;
				for(i = 0; i < NUM_OF_PLAYERS; i++){
					Position pos = locationRecords[i];
					if (currentRound < NUM_OF_ROUNDS){
						pos.x = initialLocations[i][0];
						pos.y = initialLocations[i][1];
					} else{
						pos.x = initialLocations[NUM_OF_PLAYERS - 1 - i][0];
						pos.y = initialLocations[NUM_OF_PLAYERS - 1 - i][1];
					}
				}
			} 

			// Player process:
			else{
				// Reset the position
				Position pos;
				if (currentRound < NUM_OF_ROUNDS){
					pos.x = initialLocations[rank][0];
					pos.y = initialLocations[rank][1];
				} else{
					pos.x = initialLocations[NUM_OF_PLAYERS - 1 - rank][0];
					pos.y = initialLocations[NUM_OF_PLAYERS - 1 - rank][1];
				}
				self.currentPosition = pos;

				if (currentRound == 0){
					self.speed = skillSet[rank][0];
					self.dribbling = skillSet[rank][1];
					self.shooting = skillSet[rank][2];
				}
			}

			// For field process: Initialize the playersInThisField and its count:
			if (currentRound < NUM_OF_ROUNDS){
				if (rank == FIELD_LEFT_PROCESS){
					// [0, 1, 2, 3, 4, FIELD_LEFT_PROCESS]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = i;
					}
					playersInThisField[j] = FIELD_LEFT_PROCESS
				} else if (rank == FIELD_RIGHT_PROCESS){
					// [5, 6, 7, 8, 9, FIELD_RIGHT_PROCESS]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = j + TEAM_SIZE;
					}
					playersInThisField[j] = FIELD_RIGHT_PROCESS;
				}

			} else{
				if (rank == FIELD_LEFT_PROCESS){
					// [5, 6, 7, 8, 9, FIELD_LEFT_PROCESS]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = j + TEAM_SIZE;
					}
					playersInThisField[j] = FIELD_LEFT_PROCESS;
				}else if (rank == FIELD_RIGHT_PROCESS){
					// [0, 1, 2, 3, 4, FIELD_RIGHT_PROCESS]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = i;
					}
					playersInThisField[j] = FIELD_RIGHT_PROCESS;
				}
			}

			numPlayersInThisField = 6;
		
		} // End of: if (currentRound == 0 || isScored == 1)

		else{

			// At the beginning of a round, each field has a clear picture of 10 player's location.
			// Field process: update its players and the count
			if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){
				int i;
				numPlayersInThisField = 0;

				// Update the playersInThisField[] and the count
				for(i = 0; i < NUM_OF_PLAYERS; i++){
					if (getFieldBelongsTo(locationRecords[i]) == rank){
						playersInThisField[numPlayersInThisField++] = i;
					}
				}
				// printf("In field %d, my incharge:", rank);
			}
		} // End of "else" at the beginning of round

		// Normal procedures.

		// Field Process
		if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){

			// Broadcast ball location:
			sendBuffer[0] = ball.currentPosition.x;
			sendBuffer[1] = ball.currentPosition.y;
			int m;
			for(m = 0; m < numPlayersInThisField; m++){
				MPI_Isend(&sendBuffer, 2, MPI_INT, playersInThisField[m], TAG_BALL_LOCATION, MPI_COMM_WORLD, &sendRequest);
			}

			// Receive the challenge from playersInThisField[m]
			int challenges[NUM_OF_PLAYERS] = {0};
			for(m = 0; m < numPlayersInThisField; m++){
				MPI_Recv(&receiveBuffer, 1, MPI_INT, playersInThisField[m], TAG_SEND_CHALLENGE, MPI_COMM_WORLD, &status);
				challenges[playersInThisField[m]] = receiveBuffer[0];
			}

			int winner;
			if (rank == FIELD_RIGHT_PROCESS){
				// 	FIELD_RIGHT_PROCESS sends all its challenges to FIELD_LEFT_PROCESS.
				MPI_Isend(&challenges, NUM_OF_PLAYERS, MPI_INT, FIELD_LEFT_PROCESS, TAG_EXCHANGE_CHALLENGE, MPI_COMM_WORLD, &sendRequest);

				// Get the winner from FIELD_LEFT_PROCESS(because it has the max challenge):
				MPI_Recv(&winner, NUM_OF_PLAYERS, MPI_INT, FIELD_LEFT_PROCESS, TAG_EXCHANGE_WINNER, MPI_COMM_WORLD, &status);
			} 

			/// FIELD_LEFT_PROCESS
			else{
				MPI_Recv(&receiveBuffer, NUM_OF_PLAYERS, MPI_INT, FIELD_RIGHT_PROCESS, TAG_EXCHANGE_CHALLENGE, MPI_COMM_WORLD, &status);
				
				int k;
				for(k = 0; k < NUM_OF_PLAYERS; k++){
					if (receiveBuffer[k] != 0){
						challenges[k] = receiveBuffer[k];
					}
				}

				winner = getMaxInArray(challenges, NUM_OF_PLAYERS);

				// Check whether there's a winner
				// If there's a winner, there must be a non-zero challenge.
				int isWinnerFound = 0;
				printf("Complete record of Challenges:");
				for(k = 0; k < NUM_OF_PLAYERS; k++){
					if (challenges[k] != 0){
						isWinnerFound = 1;
					}
					printf("%d ", challenges[k]);
				}

				winner = isWinnerFound ? winner : INVALID_PROCESS;

				MPI_Isend(&winner, 1, MPI_INT, FIELD_RIGHT_PROCESS, TAG_EXCHANGE_WINNER, MPI_COMM_WORLD, &sendRequest);
			}

			// Two field processes broadcast the winner info to all process:
			for (m = 0; m < numPlayersInThisField; m++){
				MPI_Isend(&winner, 1, MPI_INT, playersInThisField[m], TAG_BROADCAST_WINNER, MPI_COMM_WORLD, &sendRequest);
			}

			// Receive the updated location from my players.
			for (m = 0; m < numPlayersInThisField; m++){
				MPI_Recv(&receiveBuffer, 2, MPI_INT, playersInThisField[m], TAG_UPDATE_LOCATION, MPI_COMM_WORLD, &status);
				Position pos;
				pos.x = receiveBuffer[0];
				pos.y = receiveBuffer[1];
				locationRecords[playersInThisField[m]];
			}

			// Two fields exchange the location with each other:
			MPI_Isend(&locationRecords, 2 * NUM_OF_PLAYERS, MPI_INT, FIELD_THE_OTHER, TAG_EXCHANGE_ALL_LOCATION, MPI_COMM_WORLD, &sendRequest);
			MPI_Recv(&receiveBuffer, 2 * NUM_OF_PLAYERS, MPI_INT, FIELD_THE_OTHER, TAG_EXCHANGE_ALL_LOCATION, MPI_COMM_WORLD, &status);

			// Read the receiveBuffer:

			Position otherPositions[NUM_OF_PLAYERS];
			for (m = 0; m < NUM_OF_PLAYERS; m++){
				Position pos;
				pos.x = receiveBuffer[m * 2];
				pos.y = receiveBuffer[m * 2 + 1];
				otherPositions[m] = pos;
			}

			// Merge with self:

			for(m = 0; m < numPlayersInThisField; m++){
				int index = playersInThisField[m];
				otherPositions[index] = locationRecords[index];
			}

			// Assign value:
			for (m = 0; m < NUM_OF_PLAYERS; m++){
				locationRecords[m] = otherPositions[m];
			}

			printf("In rank %d, the merged new location is:", FIELD_ME);
			for (m = 0; m < NUM_OF_PLAYERS; m++){
				Position pos = locationRecords[m];
				printf("(%d, %d) ", pos.x, pos.y);
			}


			if (winner != INVALID_PROCESS){
				



			}

		} // End of field process

		// Player process:
		else{
			self.lastPosition = self.currentPosition;
			self.isBallReached = 0;
			self.isBallWinned = 0;
			fieldInCharge = getFieldBelongsTo(self.currentPosition);
			
			// Get the ball location.
			MPI_Recv(&receiveBuffer, 2, MPI_INT, fieldInCharge, TAG_BALL_LOCATION, MPI_COMM_WORLD, &status);
			ball.currentPosition.x = receiveBuffer[0];
			ball.currentPosition.y = receiveBuffer[1];

			// Run towards the ball:
			int distanceInCurrentRound = getRandomNumberWithinRange(0, self.speed);
			if (distanceInCurrentRound != 0){
				runTowardsBall(self, ball, distanceInCurrentRound);
			}

			int challenge = getRandomNumberWithinRange(1, 10) * self.dribbling;
			challenge = self.isBallReached ? challenge : 0;

			// Send the challenge to field-in-charge:
			MPI_Isend(&challenge, 1, MPI_INT, i, TAG_SEND_CHALLENGE, MPI_COMM_WORLD, &sendRequest);

			// Receive the winner info:
			int winner;
			MPI_Recv(&winner, 1, MPI_INT, fieldInCharge, TAG_BROADCAST_WINNER, MPI_COMM_WORLD, &status);

			// Update new location to the field-in-charge
			zopeBuffer[0] = self.currentPosition.x;
			zopeBuffer[1] = self.currentPosition.y;
			MPI_Isend(&zopeBuffer, 2, MPI_INT, fieldInCharge, TAG_UPDATE_LOCATION, MPI_COMM_WORLD, &sendRequest);


			// If there's a winner:
			if (winner != INVALID_PROCESS){





			}

		} // End of player process


	} // End of for loop. One round ends

	return 0;
}