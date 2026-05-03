#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "algoritms.h"

#define CENTER_MOVMENT 5    //constant for center of special movement(knight-like for example) 
#define INF_MOVEMENT_SZ 4   //size of array with flags of infinite movements
#define SP_MOVEMENT_SZ 9    //and matrix of special movements
#define VERT_MOVEMENT 0     //names for flags
#define HORZ_MOVEMENT 1
#define MAIN_DIAG 2
#define OTHER_DIAG 3

extern gint solve_cancel;

figure InitQueen(){
    figure queen;
    queen.name='Q';
    for(int i=0;i<INF_MOVEMENT_SZ;i++){
        queen.inf_movement[i]=TRUE;                 //queen moves in all infinite movements
    }
    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            queen.special_movement[i][j]=FALSE;     //no jumping over pieces(
        }
    }
    return queen;  
}

                                                    //magaraga(maybe i type wrong rly sry for ones who care)
figure InitMagarg(){
    figure magr;
    magr.name='M';
    for(int i=0;i<INF_MOVEMENT_SZ;i++){
        magr.inf_movement[i]=TRUE;                  //same as queen
    }
    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            magr.special_movement[i][j]=FALSE;
        }
    }
                                                    //but knight moves
    magr.special_movement[CENTER_MOVMENT-2][CENTER_MOVMENT-1]=TRUE;
    magr.special_movement[CENTER_MOVMENT-2][CENTER_MOVMENT+1]=TRUE;
    magr.special_movement[CENTER_MOVMENT-1][CENTER_MOVMENT-2]=TRUE;
    magr.special_movement[CENTER_MOVMENT-1][CENTER_MOVMENT+2]=TRUE;
    magr.special_movement[CENTER_MOVMENT+1][CENTER_MOVMENT-2]=TRUE;
    magr.special_movement[CENTER_MOVMENT+1][CENTER_MOVMENT+2]=TRUE;
    magr.special_movement[CENTER_MOVMENT+2][CENTER_MOVMENT-1]=TRUE;
    magr.special_movement[CENTER_MOVMENT+2][CENTER_MOVMENT+1]=TRUE;
    return magr;
}

                                                    //we need a figure that means there isn't a figure here
figure InitEmpty(){
    figure empty;
    empty.name=' ';
    for(int i=0;i<INF_MOVEMENT_SZ;i++){
        empty.inf_movement[i]=FALSE;
    }
    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            empty.special_movement[i][j]=FALSE;
        }
    }
    return empty;
}

void errormsg(){
    g_print("Error with malloc");
    exit(-1);
}

                                                    //figure** is a great representation of board in my opinion(prob false) 
figure** InitBoard(int n){
    figure** board;
    figure air=InitEmpty();
    board=(figure**)malloc(n*sizeof(figure*));
    if(board){
        for(int i=0;i<n;i++){
            board[i]=(figure*)malloc(n*sizeof(figure));
            if(board[i]==NULL){
                errormsg();
            }
        }
    }else{
        errormsg();
    }
                                                    //filling board with air
    for(int i=0;i<n;i++){
        for(int j=0;j<n;j++){
            board[i][j]=air;
        }
    }
    return board;
}

                                                    //function to see if we can insert a figure at row,col for any figure 
int Safe(figure** board, figure fig, int row, int col, int n){
    if(fig.inf_movement[VERT_MOVEMENT]==TRUE){
        for(int i=0;i<n;i++){
            if(i==row) continue;
            if(board[i][col].name==fig.name) return FALSE;
        }
    }
    if(fig.inf_movement[HORZ_MOVEMENT]==TRUE){
        for(int j=0;j<col;j++){
            if(board[row][j].name==fig.name) return FALSE;
        }
    }
    if(fig.inf_movement[MAIN_DIAG]==TRUE){
        int i=row-1, j=col-1;
        while(i>=0 && j>=0){
            if(board[i][j].name==fig.name) return FALSE;
            i--;j--;
        }
        i=row+1; j=col+1;
        while(i<n && j<n){
            if(board[i][j].name==fig.name) return FALSE;
            i++;j++;
        }
    }
    if(fig.inf_movement[OTHER_DIAG]==TRUE){
        int i=row-1, j=col+1;
        while(i>=0 && j<n){
            if(board[i][j].name==fig.name) return FALSE;
            i--;j++;
        }
        i=row+1; j=col-1;
        while(i<n && j>=0){
            if(board[i][j].name==fig.name) return FALSE;
            i++;j--;
        }
    }
    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            if(fig.special_movement[i][j]!=FALSE){
                int u=row+i-CENTER_MOVMENT;
                int v=col+j-CENTER_MOVMENT;
                if((u>=0)&&(u<n)&&(v>=0)&&(v<n))
                    if(board[u][v].name==fig.name) return FALSE;
            }
        }
    }
    return TRUE;
}

                                                    //generating all solves and appending to file 
long long solveAll(FILE* f, figure** board, figure fig, int n, int placed, int startCell){
    static _Thread_local int call_count = 0;
                                                    //if we can't find a solution in time
    if (++call_count >= 1024) {
        call_count = 0;
        if (g_atomic_int_get(&solve_cancel))
            return 0;
    }
                                                    //we found a solve
    if(placed == n){
        for(int i=0;i<n;i++){
            for(int j=0;j<n;j++){
                if(board[i][j].name==fig.name){
                    fprintf(f,"%d %d\n",i,j);
                }
                                                    //we print a pairs of coords where a figure is
            }
        }
        fputc('\n',f);
        fflush(f);
        return 1;
    }
    figure air=InitEmpty();
    long long ans=0;
                                                    //iterating over all cells bcs we can put for example pawns in one row
    for(int cell=startCell; cell<n*n; cell++){
        int row=cell/n;
        int col=cell%n;
        if(Safe(board,fig,row,col,n)==TRUE){
            board[row][col]=fig;
            ans+=solveAll(f,board,fig,n,placed+1,cell+1);
            board[row][col]=air;
        }
    }
    return ans;
}

                                                    //if we have inf in rows we do this faster for big n(althoug size matters 16 here is very big)
long long solveRowByRow(FILE* f, figure** board, figure fig, int n, int row) {
    static _Thread_local int call_count = 0;
    if (++call_count >= 1024) {
        call_count = 0;
        if (g_atomic_int_get(&solve_cancel))
            return 0;
    }
    if (row == n) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (board[i][j].name == fig.name) {
                    fprintf(f, "%d %d\n", i, j);
                    break;
                }
            }
        }
        fputc('\n', f);
        fflush(f);
        return 1;
    }
                                                //we do same thing but we iterate over cols
    figure air = InitEmpty();
    long long ans = 0;
    for (int col = 0; col < n; col++) {
        if (Safe(board, fig, row, col, n)) {
            board[row][col] = fig;
            ans += solveRowByRow(f, board, fig, n, row + 1);
            board[row][col] = air;
        }
    }
    return ans;
}

void FreeBoard(figure** board,int n){
    for(int i=0;i<n;i++){
        free(board[i]);
    }
    free(board);
}

                                                //checknig for 2 coords
int isAttacking(figure fig, int row1, int col1, int row2, int col2) {
    if (row1 == row2 && col1 == col2) return 0;
    int deltaRow = row2 - row1;
    int deltaCol = col2 - col1;
    if (deltaRow == 0 && fig.inf_movement[HORZ_MOVEMENT]) return 1;
    if (deltaCol == 0 && fig.inf_movement[VERT_MOVEMENT]) return 1;
    if (abs(deltaRow) == abs(deltaCol)) {
        if (deltaRow == deltaCol && fig.inf_movement[MAIN_DIAG]) return 1;
        if (deltaRow == -deltaCol && fig.inf_movement[OTHER_DIAG]) return 1;
    }
    if (abs(deltaRow) <= CENTER_MOVMENT && abs(deltaCol) <= CENTER_MOVMENT) {
        int idxRow = deltaRow + CENTER_MOVMENT;
        int idxCol = deltaCol + CENTER_MOVMENT;
        if (idxRow >= 0 && idxRow < SP_MOVEMENT_SZ && idxCol >= 0 && idxCol < SP_MOVEMENT_SZ) {
            if (fig.special_movement[idxRow][idxCol]) return 1;
        }
    }
    return 0;
}

                                                //super fast but not always right(can solve in our lifetime for a very large n)
void solveLocalConflicts(FILE* f, figure fig, int n, int maxSteps) {
    srand((unsigned)time(NULL));
    int total = n * n;
    int *prows = (int*)malloc(n * sizeof(int));
    int *pcols = (int*)malloc(n * sizeof(int));
    if (!prows || !pcols) errormsg();
                                                //we set initial config differently for each type of figure to speed up the process
    if (fig.inf_movement[HORZ_MOVEMENT]) {
        for (int r = 0; r < n; r++) {
            prows[r] = r;
            pcols[r] = rand() % n;
        }
    } else if (fig.inf_movement[VERT_MOVEMENT]) {
        for (int c = 0; c < n; c++) {
            prows[c] = rand() % n;
            pcols[c] = c;
        }
    } else {
        int *used = (int*)calloc(total, sizeof(int));
        if (!used) errormsg();
        for (int i = 0; i < n; i++) {
            int cell;
            do { cell = rand() % total; } while (used[cell]);
            used[cell] = 1;
            prows[i] = cell / n;
            pcols[i] = cell % n;
        }
        free(used);
    }
    
    int stepsWithoutProgress = 0;
    int prevConflicts = n * n + 1;

    for (int step = 0; step < maxSteps; step++) {
        static _Thread_local int check_count = 0;
        if (++check_count >= 64) {
            check_count = 0;
            if (g_atomic_int_get(&solve_cancel)) {
                fprintf(f, "Search cancelled after %d steps.\n", step);
                fflush(f);
                free(prows);
                free(pcols);
                return;
            }
        }

        int *conflicted = (int*)malloc(n * sizeof(int));
        int conflictCnt = 0;
        int totalConflicts = 0;
        for (int p = 0; p < n; p++) {
            int cnt = 0;
                                                //counting conflicts
            for (int q = 0; q < n; q++){
                if (q != p && isAttacking(fig, prows[p], pcols[p], prows[q], pcols[q]))
                    cnt++;
            }
            if (cnt > 0)
                conflicted[conflictCnt++] = p;
            totalConflicts += cnt;
        }
                                                //hooray we solve all conflicts we are happy
        if (conflictCnt == 0) {
            free(conflicted);
            for (int i = 0; i < n; i++)
                fprintf(f, "%d %d\n", prows[i], pcols[i]);
            fprintf(f, "\n");
            fflush(f);
            free(prows);
            free(pcols);
            return;
        }

                                                //we do this to find a local min
        if (totalConflicts >= prevConflicts) {
            stepsWithoutProgress++;
        } else {
            stepsWithoutProgress = 0;
        }
        prevConflicts = totalConflicts;
                                                //to start over if we found it
        if (stepsWithoutProgress > n * 2) {
            if (fig.inf_movement[HORZ_MOVEMENT]) {
                for (int r = 0; r < n; r++) {
                    prows[r] = r;
                    pcols[r] = rand() % n;
                }
            } else if (fig.inf_movement[VERT_MOVEMENT]) {
                for (int c = 0; c < n; c++) {
                    prows[c] = rand() % n;
                    pcols[c] = c;
                }
            } else {
                int *used2 = (int*)calloc(total, sizeof(int));
                if (!used2) errormsg();
                for (int i = 0; i < n; i++) {
                    int cell;
                    do { cell = rand() % total; } while (used2[cell]);
                    used2[cell] = 1;
                    prows[i] = cell / n;
                    pcols[i] = cell % n;
                }
                free(used2);
            }
            stepsWithoutProgress = 0;
            prevConflicts = n * n + 1;
            free(conflicted);
            continue;
        }

                                                //finding best conflicts
        int p = conflicted[rand() % conflictCnt];
        free(conflicted);

        int *bestRows = (int*)malloc(total * sizeof(int));
        int *bestCols2 = (int*)malloc(total * sizeof(int));
        if (!bestRows || !bestCols2) errormsg();

        int bestCount = 0;
        int minConflicts = n + 1;
                                                //trying to move a piece to other place and count conflicts
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                int occupied = 0;
                for (int q = 0; q < n; q++){
                    if (q != p && prows[q] == r && pcols[q] == c){
                        occupied = 1;
                        break;
                    }
                }
                if (occupied) continue;

                int conflicts = 0;
                for (int q = 0; q < n; q++){
                    if (q != p && isAttacking(fig, r, c, prows[q], pcols[q]))
                        conflicts++;
                }                               //and there we minimize it
                if (conflicts < minConflicts) {
                    minConflicts = conflicts;
                    bestCount = 0;
                    bestRows[bestCount] = r;
                    bestCols2[bestCount] = c;
                    bestCount++;
                } else if (conflicts == minConflicts) {
                    bestRows[bestCount] = r;
                    bestCols2[bestCount] = c;
                    bestCount++;
                }
            }
        }
                                             //picking any good solution to one conflict
        int pick = rand() % bestCount;
        prows[p] = bestRows[pick];
        pcols[p] = bestCols2[pick];
        free(bestRows);
        free(bestCols2);
    }
    free(prows);
    free(pcols);
}

                                            //adding a figure from a file
figure AddFigure(char* filename){
    FILE* fig=fopen(filename,"rb");
    if(fig==NULL){
        perror("fopen");
        errormsg();
    }
    figure newfig=InitEmpty();
                                            //first is name of a figure to differ from air
    fscanf(fig,"%c",&(newfig.name));
                                            //flags for inf movement
    for(int i=0;i<INF_MOVEMENT_SZ;i++){
        fscanf(fig,"%d",&(newfig.inf_movement[i]));
    }
                                            //matrix of specials
    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            fscanf(fig,"%d",&(newfig.special_movement[i][j]));
        }
    }
    fclose(fig);
    return newfig;
}
