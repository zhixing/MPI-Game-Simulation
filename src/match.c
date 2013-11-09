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

	if (numOfTasks != NUM_OF_PLAYERS + 2){
		printf("Please play with exactly %d process\n", NUM_OF_PLAYERS + 2);
		MPI_Finalize();
		return 0;
	}

	MPI_Group originalGroup, newGroup;
	MPI_Comm MPI_COMM_MY;

	int skillSet[NUM_OF_PLAYERS][NUM_OF_SKILLS] = {
		{5, 5, 5}, {5, 6, 4}, {5, 7, 3}, {5, 8, 2}, {5, 9, 1}, // Team A good at dribbling 
		{5, 5, 5}, {5, 4, 6}, {5, 3, 7}, {5, 2, 8}, {5, 1, 9}  // Team B good at shooting
	};
	
	/* The location is arranged as follows. x is the player and o is the ball.

		x 	x | x 	x
			x o x 	
		x 	x | x 	x

	*/
	int locationSet[NUM_OF_PLAYERS][NUM_OF_SKILLS] = {
		{48, 22}, {58, 22}, {58, 32}, {58, 42}, {48, 42}, // Left Half of the field
		{82, 42}, {72, 42}, {72, 32}, {72, 22}, {82, 22}  // Right Half of the field
	}

	int currentRound;
	int isScored = 0;
	Ball ball;
	Player self;
	for (currentRound = 0; currentRound < NUM_OF_ROUNDS; currentRound++){

		if (currentRound == 0 || isScored == 1){

			isScored = 0;

			// Extract the original group handle:
			MPI_Comm_group(MPI_COMM_WORLD, &originalGroup);

			if (currentRound < NUM_OF_ROUNDS){
				int fieldLeft[TEAM_SIZE] = {0, 1, 2, 3, 4, FIELD_LEFT_PROCESS};
				int fieldRight[TEAM_SIZE] = {5, 6, 7, 8, 9, FIELD_RIGHT_PROCESS};
				
				if (rank < TEAM_SIZE || rank == FIELD_LEFT_PROCESS){
					MPI_Group_incl(originalGroup, TEAM_SIZE + 1, fieldLeft, &newGroup);
				} else{
					MPI_Group_incl(originalGroup, TEAM_SIZE + 1, fieldRight, &newGroup);
				}

			} else{
				int fieldLeft[TEAM_SIZE] = {5, 6, 7, 8, 9, FIELD_LEFT_PROCESS};
				int fieldRight[TEAM_SIZE] = {0, 1, 2, 3, 4, FIELD_RIGHT_PROCESS};

				if (rank < TEAM_SIZE || rank == FIELD_LEFT_PROCESS){
					MPI_Group_incl(originalGroup, TEAM_SIZE + 1, fieldRight, &newGroup);
				} else{
					MPI_Group_incl(originalGroup, TEAM_SIZE + 1, fieldLeft, &newGroup);
				}
			}

			MPI_Comm_create(MPI_COMM_WORLD, newGroup, &MPI_COMM_MY);


			if (rank == FIELD_LEFT_PROCESS || rank == FIELD_RIGHT_PROCESS){
				ball.currentPosition.x = 64;
				ball.currentPosition.y = 32;
			} else{
				// rank, new position. first round, new skill.
			}


		} else{



		} // End of else loop. One round ends
	} // End of for loop. All round ends

	return 0;
}
