#include "algorithms.h"
#include "math.h"

#define CONST0 1
#define CONST1 6
#define CONST2 14
#define CONST3 16
#define MAX_SIZE 30
#define MAX_LOWER_CONSTR 1
#define MAX_HIGHER_CONSTR 8

int countSpMoves(figure fig){
    int count=0;
    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            if(i==CENTER_MOVMENT&&j==CENTER_MOVMENT)continue;
            if(fig.special_movement[i][j])count++;
        }
    }    
    return count;
}

int computeMaxSize(figure fig){
    const double w=0.18;
    int spec=countSpMoves(fig);
    if(fig.inf_movement[HORZ_MOVEMENT]){
        int SInf=fig.inf_movement[VERT_MOVEMENT]+fig.inf_movement[MAIN_DIAG]+fig.inf_movement[OTHER_DIAG];
        double c=SInf+w*spec;
        int nMax;
        if(c<=1.0)
            nMax=(int)(CONST0+(CONST1-CONST0)*c);
        else if(c<=3.0)
            nMax=(int)(CONST1+(CONST2-CONST1)*(c-1)/2.0);
        else
            nMax=(int)(CONST2+(CONST3-CONST2)*(c-3)/5.0);
        if(nMax>MAX_SIZE)nMax=MAX_SIZE;    
        return nMax;
    }else{
        int SAll=fig.inf_movement[HORZ_MOVEMENT]+fig.inf_movement[VERT_MOVEMENT]+fig.inf_movement[MAIN_DIAG]+fig.inf_movement[OTHER_DIAG];
        double c=SAll+w*spec;
        int nMax=(int)(MAX_LOWER_CONSTR+(MAX_HIGHER_CONSTR-MAX_LOWER_CONSTR)*(c/4.0));
        if(nMax>9)nMax=9;
        if(nMax<6)nMax=6;
        return nMax;
    }
}