#ifndef ALGORITHMS_H
#define ALGORITHMS_H
#include <stdint.h>
#include <stdio.h>

#define CENTER_MOVMENT 4    //constant for center of special movement(knight-like for example) 
#define INF_MOVEMENT_SZ 4   //size of array with flags of infinite movements
#define SP_MOVEMENT_SZ 9    //and matrix of special movements
#define VERT_MOVEMENT 0     //names for flags
#define HORZ_MOVEMENT 1
#define MAIN_DIAG 2
#define OTHER_DIAG 3

typedef struct {
    char name;                  //mostly to differ from other figures
    int inf_movement[4];        //each flag corresponds to its movement, prob could do with a bit mask but this is easier to understand
    int special_movement[9][9]; //we stand in the center of matrix and if an element of matrix is true we can jump to this square
} figure;

figure InitQueen();
figure InitMagarg();
figure InitEmpty();
void errormsg();
figure** InitBoard(int n);
int Safe(figure** board, figure fig, int row, int col, int n);
figure AddFigure(char* filename);
void AddFigToFile(figure fig,const char* name);
uint64_t solveAll(FILE* f, figure** board, figure fig, int n, int placed, int startCell);
void FreeBoard(figure** board, int n);
int isAttacking(figure fig, int row1, int col1, int row2, int col2);
void solveLocalConflicts(FILE* f, figure fig, int n, int maxSteps);
uint64_t solveRowByRow(FILE* f, figure** board, figure fig, int n, int row);

#endif