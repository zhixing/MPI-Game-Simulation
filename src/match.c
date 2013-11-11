/**
* 	Name: Yang Zhixing
*	Matric Number: A0091726B
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FIELD_LENGTH 128
#define FIELD_HEIGHT 64
#define FIELD_LEFT_PROCESS 10
#define FIELD_RIGHT_PROCESS 11
#define INVALID_PROCESS -1
#define INVALID_CHALLENGE -1
#define NUM_OF_ROUNDS 2700
#define NUM_OF_PLAYERS 10
#define NUM_OF_SKILLS 3
#define TEAM_SIZE 5
#define INITIAL_BALL_POSITION_X 64
#define INITIAL_BALL_POSITION_Y 32
#define MPI_BUFFER_SIZE 150
#define MAX_DISTANCE_PER_ROUND 10
#define MIN_DISTANCE_PER_ROUND 1
#define RAMDOM_FACTOR 117
#define MIN_SHOOTING_DISTANCE 30
#define MIN_3_POINT_DISTANCE 25
#define BALL_MISS_ROLL_RANGE 8
#define BALL_DRIBBLING_ROLL_DISTANCE 4
#define PLAYER_SUMMARY_SIZE 10

#define GOAL_LEFT_X 0
#define GOAL_LEFT_Y 32
#define GOAL_RIGHT_X 128
#define GOAL_RIGHT_Y 32

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
#define TAG_TEAMMATES_LOCATIONS 8
#define TAG_PASS_BALL 9
#define TAG_BALL_PASSED 10
#define TAG_REPORT_SUMMARY 11
#define TAG_EXCHANGE_SUMMARY 12
#define TAG_IS_SCORED 13

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
* Return the probablity of hit. Based on the distance and shooting skill.
*/
double computeHitProbablity(int d, int s){
	double v = ( 10 + 90 * s ) / ( 0.5 * d * sqrt(d) - 0.5 );
	if (v > 100) v = 100.0;
	return v;
}

void computeTeammates(int teammates[], int player){
	
	int offSet = 0, m, j = 0;

	if (player >= TEAM_SIZE){
		offSet = TEAM_SIZE;
	}

	for (m = 0; m < TEAM_SIZE; m++){
		if (m + offSet != player){
			teammates[j++] = m;
		}
	}
}

/*
* Shoot the ball to new position. The ball may not end up there. And this is determined by the play's skills
*/
Position passBallToNewPosition(Position destination, Position source, int shooting){
	Position result;
	// Probablity is from 0 to 100
	double probablity = computeHitProbablity(getDistance(destination, source), shooting);

	// If the probablity is 96, then 96% of the chances will be: the rand is less than 96.
	int random = rand() % 100;
	int isHit = random < probablity ? 1 : 0;

	if (!isHit){

		int rollDistance = getRandomNumberWithinRange(0, BALL_MISS_ROLL_RANGE);

		int rollX = getRandomNumberWithinRange(0, rollDistance);
		int rollY = rollDistance - rollX;

		int directionX = (rand() % 2) ? -1 : 1;
		int directionY = (rand() % 2) ? -1 : 1;

		//printf("Missed: %d %d\n", rollX, rollY);

		result.x = destination.x + directionX * rollX;
		result.y = destination.y + directionY * rollY;
	} else{
		result = destination;
	}
	
	//printf("Passting the ball to (%d, %d), probablity: %f, the ball ended up: (%d, %d)\n", destination.x, destination.y, probablity, result.x, result.y);

	if (getFieldBelongsTo(result) == INVALID_PROCESS){
		result.x = INITIAL_BALL_POSITION_X;
		result.y = INITIAL_BALL_POSITION_Y;
	}

	return result;
}

/*
* Get the position of the goal based on the currentRound and the rank of the player
*/
Position getGoal(int currentRound, int player){

	Position goalAtLeft;
	Position goalAtRight;

	goalAtLeft.x = GOAL_LEFT_X;
	goalAtLeft.y = GOAL_LEFT_Y;
	goalAtRight.x = GOAL_RIGHT_X;
	goalAtRight.y = GOAL_RIGHT_Y;

	if (currentRound < NUM_OF_ROUNDS){
		return (player < TEAM_SIZE) ? goalAtRight : goalAtLeft;
	} else{
		return (player < TEAM_SIZE) ? goalAtLeft : goalAtRight;
	}
}

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
	
	// //printf("In runTowardsBall function. Ball: (%d, %d). Step: %d. Original: (%d, %d). New: (%d, %d)\n", ball.currentPosition.x, ball.currentPosition.y, distanceInCurrentRound, (*self).lastPosition.x, (*self).lastPosition.y, (*self).currentPosition.x, (*self).currentPosition.y);
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

int isGoal(Position pos){
	return isLeftGoal(pos) || isRightGoal(pos);
}

int isLeftGoal(Position pos){
	return (pos.x == GOAL_LEFT_X && pos.y == GOAL_LEFT_Y);
}

int isRightGoal(Position pos){
	return (pos.x == GOAL_RIGHT_X && pos.y == GOAL_RIGHT_Y);
}

/*
* Return FIELD_LEFT_PROCESS or FIELD_RIGHT_PROCESS
* Return INVALID_PROCESS if out-of-range
*/
int getFieldBelongsTo(Position p){
	if (p.x < 0 || p.x > FIELD_LENGTH || p.y < 0 || p.y > FIELD_HEIGHT){
		return INVALID_PROCESS;
	}
	if (p.x < FIELD_LENGTH / 2){
		return FIELD_LEFT_PROCESS;
	} else{
		return FIELD_RIGHT_PROCESS;
	}
}

/*
* Return the index of the max in array.
* If multiple index contains the max value, a ramdon index of them will be returned.
* the arr[] size must be smaller than NUM_OF_PLAYERS
*/
int getMaxInArray(int arr[], int size){
	int i;
	int max = arr[0];
	int maxIndex = 0;

	int duplicateCount = 0;
	int duplicateIndex[NUM_OF_PLAYERS];
	if (size <= 1){
		return 0;
	}

	for (i = 1; i < size; i++){
		if (arr[i] > max){
			max = arr[i];
			maxIndex = i;
		}
	}

	for (i = 1; i < size; i++){
		if (arr[i] == max){
			duplicateIndex[duplicateCount++] = i;
		}
	}

	if (duplicateCount > 0){
		int choice = getRandomNumberWithinRange(0, duplicateCount - 1);
		maxIndex = duplicateIndex[choice];
	}

	return maxIndex;
}

/*	
* Main Function:
*/

int main(int argc, char *argv[]){
	int rank, numOfTasks;
	
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numOfTasks);

	if (numOfTasks != NUM_OF_PLAYERS + 2){
		//printf("Please play with exactly %d process\n", NUM_OF_PLAYERS + 2);
		MPI_Finalize();
		return 0;
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int skillSet[NUM_OF_PLAYERS][NUM_OF_SKILLS] = {
		{5, 5, 5}, {5, 4, 6}, {5, 5, 5}, {5, 2, 8}, {5, 5, 5},
		{5, 5, 5}, {5, 6, 4}, {5, 5, 5}, {5, 8, 2}, {5, 5, 5}
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

	srand(time(NULL) + rank * RAMDOM_FACTOR);

	int currentRound;
	int teamAScore = 0, teamBScore = 0;;
	int isScored = 0;
	Ball ball;
	Player self;

	// The field process has this to keep track of who is in them:
	int playersInThisField[NUM_OF_PLAYERS] = {-1}; // e.g. [2, 4, 5, ...]
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

		MPI_Barrier(MPI_COMM_WORLD);

		if (currentRound == 0 || isScored == 1 || currentRound == NUM_OF_ROUNDS){
			isScored = 0;
			
			// Field process:
			if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){
				// Reset the ball position
				ball.currentPosition.x = 64;
				ball.currentPosition.y = 32;

				ball.lastPosition = ball.currentPosition;

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
					locationRecords[i] = pos;
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
					// [0, 1, 2, 3, 4]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = j;
					}
				} else if (rank == FIELD_RIGHT_PROCESS){
					// [5, 6, 7, 8, 9]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = j + TEAM_SIZE;
					}
				}

			} else{
				if (rank == FIELD_LEFT_PROCESS){
					// [5, 6, 7, 8, 9]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = j + TEAM_SIZE;
					}
				}else if (rank == FIELD_RIGHT_PROCESS){
					// [0, 1, 2, 3, 4]
					int j;
					for(j = 0; j < TEAM_SIZE; j++){
						playersInThisField[j] = j;
					}
				}
			}

			numPlayersInThisField = 5;
		
		} // End of: if (currentRound == 0 || isScored == 1)


		else{

			// At the beginning of a round, each field has a clear picture of 10 player's location.
			// Field process: update its players and the count
			if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){
				int i;
				numPlayersInThisField = 0;

				// Update the playersInThisField[] and the count
				for(i = 0; i < NUM_OF_PLAYERS; i++){
					Position pos = locationRecords[i];
					if (getFieldBelongsTo(pos) == rank){
						playersInThisField[numPlayersInThisField++] = i;
						//printf("Rank %d belongs to %d. Based on my location record: (%d, %d)\n", i, rank, pos.x, pos.y );
					}
				}
			}
		} // End of "else" at the beginning of round


		// Normal procedures.

		// Field Process
		if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){

			// int p;
			// for (p = 0; p < numPlayersInThisField; p++){
			// 	printf("Beginning of round %d: At rank %d. The numPlayersInThisField: %d\n", currentRound, rank, playersInThisField[p]);
			// }

			// Broadcast ball location:
			int location[2] ={ball.currentPosition.x, ball.currentPosition.y};
			int m;
			for(m = 0; m < numPlayersInThisField; m++){
				//printf("----------------Ball info is broadcasted to: %d\n", playersInThisField[m]);
				MPI_Isend(&location, 2, MPI_INT, playersInThisField[m], TAG_BALL_LOCATION, MPI_COMM_WORLD, &sendRequest);
			}

			// Receive the challenge from playersInThisField[m]
			int challenges[NUM_OF_PLAYERS] = {INVALID_CHALLENGE};
			for(m = 0; m < numPlayersInThisField; m++){
				MPI_Recv(&receiveBuffer, 1, MPI_INT, playersInThisField[m], TAG_SEND_CHALLENGE, MPI_COMM_WORLD, &status);
				challenges[playersInThisField[m]] = receiveBuffer[0];
			}


			int winner;
			if (rank == FIELD_RIGHT_PROCESS){
				// 	FIELD_RIGHT_PROCESS sends all its challenges to FIELD_LEFT_PROCESS.
				//printf("right field sends the challenges\n");
				MPI_Isend(&challenges, NUM_OF_PLAYERS, MPI_INT, FIELD_LEFT_PROCESS, TAG_EXCHANGE_CHALLENGE, MPI_COMM_WORLD, &sendRequest);

				// Get the winner from FIELD_LEFT_PROCESS(because it has the max challenge):
				//printf("right field wait to receive winner\n");
				MPI_Recv(&winner, 1, MPI_INT, FIELD_LEFT_PROCESS, TAG_EXCHANGE_WINNER, MPI_COMM_WORLD, &status);
				//printf("TAG_EXCHANGE_WINNER right field received winner\n");
			} 

			// FIELD_LEFT_PROCESS
			else{
				//printf("left field wait to received all challenges from right process\n");
				MPI_Recv(&receiveBuffer, NUM_OF_PLAYERS, MPI_INT, FIELD_RIGHT_PROCESS, TAG_EXCHANGE_CHALLENGE, MPI_COMM_WORLD, &status);
				//printf("TAG_EXCHANGE_CHALLENGE left field received all challenges\n");

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
				////printf("Complete record of Challenges:\n");
				for(k = 0; k < NUM_OF_PLAYERS; k++){
					if (challenges[k] != INVALID_CHALLENGE){
						isWinnerFound = 1;
					}
					////printf("%d \n", challenges[k]);
				}

				winner = isWinnerFound ? winner : INVALID_PROCESS;

				//printf("left field sends the winner: %d\n", winner);
				MPI_Isend(&winner, 1, MPI_INT, FIELD_RIGHT_PROCESS, TAG_EXCHANGE_WINNER, MPI_COMM_WORLD, &sendRequest);
			}

			// Two field processes broadcast the winner info to all process:
			for (m = 0; m < numPlayersInThisField; m++){
				MPI_Isend(&winner, 1, MPI_INT, playersInThisField[m], TAG_BROADCAST_WINNER, MPI_COMM_WORLD, &sendRequest);
			}

			// Receive the updated location from my players.
			for (m = 0; m < numPlayersInThisField; m++){
				MPI_Recv(&receiveBuffer, 2, MPI_INT, playersInThisField[m], TAG_UPDATE_LOCATION, MPI_COMM_WORLD, &status);
				//printf("TAG_UPDATE_LOCATION\n");
				Position pos;
				pos.x = receiveBuffer[0];
				pos.y = receiveBuffer[1];
				locationRecords[playersInThisField[m]] = pos;
			}

			// Two fields exchange the location with each other:
			MPI_Isend(&locationRecords, 2 * NUM_OF_PLAYERS, MPI_INT, FIELD_THE_OTHER, TAG_EXCHANGE_ALL_LOCATION, MPI_COMM_WORLD, &sendRequest);
			MPI_Recv(&receiveBuffer, 2 * NUM_OF_PLAYERS, MPI_INT, FIELD_THE_OTHER, TAG_EXCHANGE_ALL_LOCATION, MPI_COMM_WORLD, &status);
			//printf("TAG_EXCHANGE_ALL_LOCATION\n");
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

			// Assign value back:
			for (m = 0; m < NUM_OF_PLAYERS; m++){
				locationRecords[m] = otherPositions[m];
			}

			for (m = 0; m < NUM_OF_PLAYERS; m++){
				Position pos = locationRecords[m];
				//printf("In rank %d, the merged new location is: (%d, %d)\n", FIELD_ME, pos.x, pos.y);
			}


			if (winner != INVALID_PROCESS){

				// See if I'm in charge of the winner:
				int isWinnerInMyField = 0;
				for (m = 0; m < numPlayersInThisField; m++){
					if (playersInThisField[m] == winner){
						isWinnerInMyField = 1;
					}
				}

				// If the winner is in my field, sends the location of the winner's teammates:
				if (isWinnerInMyField){

					// Compute the teammates' location of the winner
					int teammates[TEAM_SIZE - 1];
					computeTeammates(teammates, winner);
					
					Position teammatesLocation[NUM_OF_PLAYERS];
					for (m = 0; m < TEAM_SIZE - 1; m++){
						int index = teammates[m];
						teammatesLocation[index] = locationRecords[index];
					}


					// Sends the teammatesLocation to the winner:
					MPI_Isend(&teammatesLocation, 2 * NUM_OF_PLAYERS, MPI_INT, winner, TAG_TEAMMATES_LOCATIONS, MPI_COMM_WORLD, &sendRequest);
					
					// Receive where the winner wants to pass the ball:
					MPI_Recv(&receiveBuffer, 5, MPI_INT, winner, TAG_PASS_BALL, MPI_COMM_WORLD, &status);
					//printf("TAG_PASS_BALL\n");

					Position desiredDestination, srcPosition;
					desiredDestination.x = receiveBuffer[0];
					desiredDestination.y = receiveBuffer[1];
					srcPosition.x = receiveBuffer[2];
					srcPosition.y = receiveBuffer[3];
					int shooting = receiveBuffer[4];

					Position finalDestination = passBallToNewPosition(desiredDestination, srcPosition, shooting);
					ball.lastPosition = ball.currentPosition;
					ball.currentPosition = finalDestination;
					int ballFlyDistance = getDistance(ball.currentPosition, ball.lastPosition);

					if (isLeftGoal(ball.currentPosition)){
						isScored = 1;
						if (currentRound < NUM_OF_ROUNDS){
							teamBScore += ballFlyDistance > MIN_3_POINT_DISTANCE ? 3 : 2;
						} else{
							teamAScore += ballFlyDistance > MIN_3_POINT_DISTANCE ? 3 : 2;
						}
					} else if (isRightGoal(ball.currentPosition)){
						isScored = 1;
						if (currentRound < NUM_OF_ROUNDS){
							teamAScore += ballFlyDistance > MIN_3_POINT_DISTANCE ? 3 : 2;
						} else{
							teamBScore += ballFlyDistance > MIN_3_POINT_DISTANCE ? 3 : 2;
						}
					} else{
						isScored = 0;
					}

					// Sends information to the other field:
					// new ball location, isScored, teamA's score, teamB's score
					sendBuffer[0] = finalDestination.x;
					sendBuffer[1] = finalDestination.y;
					sendBuffer[2] = isScored;
					sendBuffer[3] = teamAScore;
					sendBuffer[4] = teamBScore;

					MPI_Isend(&sendBuffer, 5, MPI_INT, FIELD_THE_OTHER, TAG_BALL_PASSED, MPI_COMM_WORLD, &sendRequest);

				} // End of winner is in my fild

				else{
					// Receives the field who is in charge of the winner:
					MPI_Recv(&receiveBuffer, 5, MPI_INT, FIELD_THE_OTHER, TAG_BALL_PASSED, MPI_COMM_WORLD, &status);
					//printf("TAG_BALL_PASSED\n");
					Position newPosition;
					
					newPosition.x = receiveBuffer[0];
					newPosition.y = receiveBuffer[1];
					isScored = receiveBuffer[2];
					teamAScore = receiveBuffer[3];
					teamBScore = receiveBuffer[4];

					ball.lastPosition = ball.currentPosition;
					ball.currentPosition = newPosition;
				}

				// Broadcast isScored information to the players:
				for(m = 0; m < numPlayersInThisField; m++){
					MPI_Isend(&isScored, 1, MPI_INT, playersInThisField[m], TAG_IS_SCORED, MPI_COMM_WORLD, &sendRequest);
				}

			} // End of "if there's a winner"

			// Gather's summary from players:
			int playerSummary[PLAYER_SUMMARY_SIZE * NUM_OF_PLAYERS]; // 10 * 10
			for (m = 0; m < numPlayersInThisField; m++){
				int index = playersInThisField[m];
				MPI_Recv(&receiveBuffer, PLAYER_SUMMARY_SIZE, MPI_INT, index, TAG_REPORT_SUMMARY, MPI_COMM_WORLD, &status);
				//printf("TAG_REPORT_SUMMARY\n");
				int p;
				for (p = 0; p < PLAYER_SUMMARY_SIZE; p++){
					playerSummary[index * PLAYER_SUMMARY_SIZE + p] = receiveBuffer[p];
				}
			}

			// Field 1 sends all the info to Field 0:
			if (rank == FIELD_RIGHT_PROCESS){
				MPI_Isend(&playerSummary, PLAYER_SUMMARY_SIZE * NUM_OF_PLAYERS, MPI_INT, FIELD_LEFT_PROCESS, TAG_EXCHANGE_SUMMARY, MPI_COMM_WORLD, &sendRequest);
			}

			// Field 0 receives all the info from Field 1:
			else if (rank == FIELD_LEFT_PROCESS){
				MPI_Recv(&receiveBuffer, PLAYER_SUMMARY_SIZE * NUM_OF_PLAYERS, MPI_INT, FIELD_RIGHT_PROCESS, TAG_EXCHANGE_SUMMARY, MPI_COMM_WORLD, &status);
				//printf("TAG_EXCHANGE_SUMMARY\n");
				// Merge with self:
				for (m = 0; m < numPlayersInThisField; m++){
					int index = playersInThisField[m];
					int p;
					for (p = 0; p < PLAYER_SUMMARY_SIZE; p++){
						receiveBuffer[index * PLAYER_SUMMARY_SIZE + p] = playerSummary[index * PLAYER_SUMMARY_SIZE + p];
					}
				}

				// Printout:
				if (SHOULD_OUTPUT_DETAILS){
					printf("\n\n\nRound: %d\n", currentRound);
					printf("Scores: A: %d, B: %d\n", teamAScore, teamBScore);
					printf("Ball: (%d, %d) ==> (%d, %d)\n", ball.lastPosition.x, ball.lastPosition.y, ball.currentPosition.x, ball.currentPosition.y);

					// rank, initial location, final location, reached?, kicked?, ball challenge, shoot target location. (-1, -1) if didn't reach/win
					for (m = 0; m < NUM_OF_PLAYERS; m++){
						int index = m * PLAYER_SUMMARY_SIZE;
						printf("\nPlayer: %d\n", receiveBuffer[index]);
						printf("Location: (%d, %d) ==> (%d, %d)\n", receiveBuffer[index + 1], receiveBuffer[index + 2], receiveBuffer[index + 3], receiveBuffer[index + 4]);
						printf("Reached? %d\n", receiveBuffer[index + 5]);
						printf("Kicked? %d\n", receiveBuffer[index + 6]);
						printf("Ball Challenge: %d\n", receiveBuffer[index + 7]);
						printf("Shoot target: (%d, %d)\n", receiveBuffer[index + 8], receiveBuffer[index + 9]);
					}
				} else{
					printf("\n%d\n", currentRound);
					printf("Scores: A: %d, B: %d\n", teamAScore, teamBScore);
					printf("Ball: (%d, %d) ==> (%d, %d)\n", ball.lastPosition.x, ball.lastPosition.y, ball.currentPosition.x, ball.currentPosition.y);

					// rank, initial location, final location, reached?, kicked?, ball challenge, shoot target location. (-1, -1) if didn't reach/win
					for (m = 0; m < NUM_OF_PLAYERS; m++){
						int index = m * PLAYER_SUMMARY_SIZE;
						printf("\t%d \t(%d, %d) \t(%d, %d) \t%d \t%d \t%d \t(%d, %d)\n", receiveBuffer[index], receiveBuffer[index + 1], receiveBuffer[index + 2], receiveBuffer[index + 3], receiveBuffer[index + 4], receiveBuffer[index + 5], receiveBuffer[index + 6], receiveBuffer[index + 7], receiveBuffer[index + 8], receiveBuffer[index + 9]);
					}
				}
			}
		} // End of field process

		// Player process:
		else{
			int m;

			////printf("I am still alive ========================================================== %d\n", rank);

			self.lastPosition = self.currentPosition;
			self.isBallReached = 0;
			self.isBallWinned = 0;

			fieldInCharge = getFieldBelongsTo(self.currentPosition);
			//printf("-------------------> Player %d belongs to %d. Based on my location: (%d, %d)\n", rank, fieldInCharge, self.currentPosition.x, self.currentPosition.y);
			
			// Get the ball location.
			MPI_Recv(&receiveBuffer, 2, MPI_INT, fieldInCharge, TAG_BALL_LOCATION, MPI_COMM_WORLD, &status);
			////printf("I have recieved XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX %d\n", rank);


			ball.currentPosition.x = receiveBuffer[0];
			ball.currentPosition.y = receiveBuffer[1];

			// Run towards the ball:
			int distanceInCurrentRound = getRandomNumberWithinRange(0, self.speed);
			if (distanceInCurrentRound != 0){
				runTowardsBall(&self, ball, distanceInCurrentRound);
			}

			int challenge = getRandomNumberWithinRange(1, 10) * self.dribbling;
			challenge = self.isBallReached ? challenge : INVALID_CHALLENGE;

			// Send the challenge to field-in-charge:
			//printf("Fild in charge of rank %d is %d\n", rank, fieldInCharge);
			MPI_Isend(&challenge, 1, MPI_INT, fieldInCharge, TAG_SEND_CHALLENGE, MPI_COMM_WORLD, &sendRequest);

			// Receive the winner info:
			int winner;

			//printf("player waits for winner info\n");
			MPI_Recv(&winner, 1, MPI_INT, fieldInCharge, TAG_BROADCAST_WINNER, MPI_COMM_WORLD, &status);
			//printf("player receives the winner info: %d\n", winner);

			// Update new location to the field-in-charge
			zopeBuffer[0] = self.currentPosition.x;
			zopeBuffer[1] = self.currentPosition.y;
			MPI_Isend(&zopeBuffer, 2, MPI_INT, fieldInCharge, TAG_UPDATE_LOCATION, MPI_COMM_WORLD, &sendRequest);

			// If there's a winner, AND the winner is me. YAY!
			Position desiredDestination;
			desiredDestination.x = -1;
			desiredDestination.y = -1;
			if (winner != INVALID_PROCESS && winner == rank){

				self.isBallWinned = 1;

				// Receives the teammates' locations from the field in charge:
				//printf("winner waits for teammate info\n");
				MPI_Recv(&receiveBuffer, 2 * NUM_OF_PLAYERS, MPI_INT, fieldInCharge, TAG_TEAMMATES_LOCATIONS, MPI_COMM_WORLD, &status);
				//printf("TAG_TEAMMATES_LOCATIONS\n");

				Position teammatesLocation[NUM_OF_PLAYERS];
				for (m = 0; m < NUM_OF_PLAYERS; m++){
					Position pos;
					pos.x = receiveBuffer[m * 2];
					pos.y = receiveBuffer[m * 2 + 1];
					teammatesLocation[m] = pos;
				}

				int teammates[TEAM_SIZE - 1];
				computeTeammates(teammates, winner);

				desiredDestination = getGoal(currentRound, rank);
				int goalDistance = getDistance(desiredDestination, self.currentPosition);

				// Shoot the ball if I'm close enough.
				if(goalDistance <= MIN_SHOOTING_DISTANCE){
					sendBuffer[0] = desiredDestination.x;
					sendBuffer[1] = desiredDestination.y;
					sendBuffer[2] = self.currentPosition.x;
					sendBuffer[3] = self.currentPosition.y;
					sendBuffer[4] = self.shooting;
					MPI_Isend(&sendBuffer, 5, MPI_INT, fieldInCharge, TAG_PASS_BALL, MPI_COMM_WORLD, &sendRequest);
				} 

				// Otherwise, pass the ball
				else{
					// If there're teammates who are closer to the basket than me 
					// Pass the ball to that person
					int teammateCloserToBasket = -1;
					int minDistance = goalDistance;
					for (m = 0; m < TEAM_SIZE - 1; m++){

						int index = teammates[m];
						Position newPos = teammatesLocation[index];

						int newDistance = getDistance(desiredDestination, newPos);

						if (newDistance < minDistance){
							minDistance = newDistance;
							teammateCloserToBasket = index;
						}
					}

					if (teammateCloserToBasket != -1){
						desiredDestination = teammatesLocation[teammateCloserToBasket];
						sendBuffer[0] = desiredDestination.x;
						sendBuffer[1] = desiredDestination.y;
						sendBuffer[2] = self.currentPosition.x;
						sendBuffer[3] = self.currentPosition.y;
						sendBuffer[4] = self.shooting;
						MPI_Isend(&sendBuffer, 5, MPI_INT, fieldInCharge, TAG_PASS_BALL, MPI_COMM_WORLD, &sendRequest);
					}

					// If not, kick the ball to the front for BALL_DRIBBLING_ROLL_DISTANCE steps.
					else{
						int directionX = (desiredDestination.x - self.currentPosition.x) > 0 ? 1 : -1;
						int directionY = (desiredDestination.y - self.currentPosition.y) > 0 ? 1 : -1;
						
						int distanceX = abs(desiredDestination.x - self.currentPosition.x);
						int distanceY = abs(desiredDestination.y - self.currentPosition.y);
						
						if (distanceX > BALL_DRIBBLING_ROLL_DISTANCE){
							desiredDestination.x = self.currentPosition.x + directionX * BALL_DRIBBLING_ROLL_DISTANCE;
							desiredDestination.y = self.currentPosition.y; 
						} else if (distanceY > BALL_DRIBBLING_ROLL_DISTANCE){
							desiredDestination.x = self.currentPosition.x;
							desiredDestination.y = self.currentPosition.y + directionY * BALL_DRIBBLING_ROLL_DISTANCE;
						} else{
							desiredDestination.x = self.currentPosition.x;
							desiredDestination.y = self.currentPosition.y;
						}
						sendBuffer[0] = desiredDestination.x;
						sendBuffer[1] = desiredDestination.y;
						sendBuffer[2] = self.currentPosition.x;
						sendBuffer[3] = self.currentPosition.y;
						sendBuffer[4] = self.shooting;
						MPI_Isend(&sendBuffer, 5, MPI_INT, fieldInCharge, TAG_PASS_BALL, MPI_COMM_WORLD, &sendRequest);	
					}
				}

			} // End of "if there's a winner AND I'm winner"

			if (winner != INVALID_PROCESS){
				MPI_Recv(&isScored, 1, MPI_INT, fieldInCharge, TAG_IS_SCORED, MPI_COMM_WORLD, &status);
			}

			// Send the summary of personal info to field-in-charge:
			// initial location, final location, reached?, kicked?, ball challenge, shoot target location. (-1, -1) if didn't reach/win
			zopeBuffer[0] = rank;

			zopeBuffer[1] = self.lastPosition.x;
			zopeBuffer[2] = self.lastPosition.y;

			zopeBuffer[3] = self.currentPosition.x;
			zopeBuffer[4] = self.currentPosition.y;

			zopeBuffer[5] = self.isBallReached;
			zopeBuffer[6] = self.isBallWinned;

			zopeBuffer[7] = challenge;

			zopeBuffer[8] = desiredDestination.x;
			zopeBuffer[9] = desiredDestination.y;
 
			MPI_Isend(&zopeBuffer, PLAYER_SUMMARY_SIZE, MPI_INT, fieldInCharge, TAG_REPORT_SUMMARY, MPI_COMM_WORLD, &sendRequest);	

		} // End of player process

		MPI_Barrier(MPI_COMM_WORLD);
		//printf("Ended %d\n", rank);

	} // End of for loop. Everything ends

	MPI_Finalize();

	return 0;
}