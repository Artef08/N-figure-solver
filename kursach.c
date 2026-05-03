#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "algoritms.h"

#define MAX_FIGURES 15

#define DEFAULT_SIZE 8
#define MAX_SIZE_ALL 16     //max size for all solves
#define MAX_SIZE_ONE 100    //max size for one solve prob change later

#define VERT_MOVEMENT 0
#define HORZ_MOVEMENT 1
#define MAIN_DIAG 2
#define OTHER_DIAG 3

#define SP_MOVEMENT_SZ 9
#define CENTER_MOVMENT 5

#define INF 10000000        //max iterations for local conflicts

#define MAIN_WIND_VERT 800
#define MAIN_WIND_HORZ 1200
#define ADDING_WIND_VERT 500
#define ADDING_WIND_HORZ 100

figure test;
static GMutex file_list_mutex;
static GList *output_files = NULL;
gint solve_cancel = 0;

typedef struct {
    GtkWidget** btns;   //buttons 
    figure* figures;    //corresponding to those figures
    int flagAll;        //type of solution
    int size;           //number of figures
} dataSolve;

typedef struct {        
    figure fig;
    int size;
    int flag;
    GtkWidget* button; 
    const char *fullName; 
} SolveThreadData;      //struct for threads

typedef struct {
    GtkWidget** btns;
    GtkWidget* box;
    figure* figures;
    figure newFig;
    int* ind;
    char* name;
    int flagButtonSignal;
} dataAdd;

typedef struct {
    GtkWidget** btns;
    GtkWidget* box;
    figure* figures;
    int* ind;
} dataRem;

//stop solving if we ran out of time
static gboolean cancelSolveTimeout(gpointer user_data) {
    GtkWidget *button = GTK_WIDGET(user_data);
    g_atomic_int_set(&solve_cancel, 1);
    gtk_widget_set_sensitive(button, TRUE);
    return G_SOURCE_REMOVE;
}

//adding files that we added
static void addOutputFile(const char *filename) {
    g_mutex_lock(&file_list_mutex);
    output_files = g_list_append(output_files, g_strdup(filename));
    g_mutex_unlock(&file_list_mutex);
}

//so we can delete them after closing
static void deleteAllOutputFiles(void) {
    g_mutex_lock(&file_list_mutex);
    for (GList *l = output_files; l != NULL; l = l->next) {
        const char *fname = (const char *)l->data;
        remove(fname);
        g_free(l->data);
    }
    g_list_free(output_files);
    output_files = NULL;
    g_mutex_unlock(&file_list_mutex);
}

static void SizeChange(GtkRange* scaleN, gpointer userData){
    dataSolve* data=(dataSolve*)userData;
    data->size=(int)gtk_range_get_value(scaleN);
}

static void toggleSolves(GtkToggleButton* checkAll,gpointer userData){
    dataSolve* data=(dataSolve*)userData;
    GtkWidget* scale=g_object_get_data(G_OBJECT(checkAll),"scale");
    //here we adjusting max size so if we choose 80 and switch to all solves it will change to max
    if(scale){
        double new_max=gtk_check_button_get_active(GTK_CHECK_BUTTON(checkAll))? (double)MAX_SIZE_ALL : (double)MAX_SIZE_ONE;
        GtkAdjustment* adj=gtk_range_get_adjustment(GTK_RANGE(scale));
        double old_value = gtk_adjustment_get_value(adj);
        gtk_adjustment_set_upper(adj,new_max);
        if(old_value>new_max){
            gtk_adjustment_set_value(adj, new_max);
        }
        data->flagAll=gtk_check_button_get_active(GTK_CHECK_BUTTON(checkAll))? TRUE:FALSE;
        data->size = (int)gtk_adjustment_get_value(adj);
    }
}

static gboolean reenableButton(gpointer userData) {
    gtk_widget_set_sensitive(GTK_WIDGET(userData), TRUE);
    return G_SOURCE_REMOVE;
}

//threading
gpointer SolveThread(gpointer userData) {
    SolveThreadData* data = (SolveThreadData*)userData;

    if (g_atomic_int_get(&solve_cancel)) {
        g_idle_add(reenableButton, data->button);
        free(data);
        return NULL;
    }
    //and we don't start if we got a solution
    char filename[128];
    snprintf(filename, sizeof(filename), "%s_%d_%d.slv", data->fullName, data->size,data->flag);

    FILE* test = fopen(filename, "rb");
    if (test != NULL) {
        fclose(test);
        g_idle_add(reenableButton, data->button);
        free(data);
        return NULL;
    }
    //crating a new file
    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        g_idle_add(reenableButton, data->button);
        free(data);
        return NULL;
    }
    //printing size of a board
    fprintf(f, "%d\n\n", data->size);
    addOutputFile(filename);

    //flag
    int wasCancelled = 0; 

    if (data->flag == 1) {
        figure** board = InitBoard(data->size);
        if (data->fig.inf_movement[HORZ_MOVEMENT]) {
            solveRowByRow(f, board, data->fig, data->size, 0);
        } else {
            solveAll(f, board, data->fig, data->size, 0, 0);
        }
        FreeBoard(board, data->size);
        wasCancelled = g_atomic_int_get(&solve_cancel);
        if (wasCancelled)
            fprintf(f, "Search cancelled\n");
    } else {
        solveLocalConflicts(f, data->fig, data->size, INF);
        wasCancelled = g_atomic_int_get(&solve_cancel);
        if (wasCancelled)
            fprintf(f, "Search cancelled.\n");
    }

    fclose(f);
    g_idle_add(reenableButton, data->button);
    free(data);
    return NULL;
}

void showErrorNameExists(GtkButton* button){
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));

    GtkAlertDialog* dialog=gtk_alert_dialog_new("This name already exists");
    gtk_alert_dialog_set_detail(dialog,"Another figure has this name, call this name another way.");
    gtk_alert_dialog_set_buttons(dialog,(const char*[]){"OK",NULL});

    gtk_alert_dialog_show(dialog,parent);
}

static gboolean closeWindowIdle(gpointer user_data) {
    GtkWindow *win = GTK_WINDOW(user_data);
    gtk_window_destroy(win);
    return G_SOURCE_REMOVE;
}

static void checkInput(GtkButton* button,dataAdd* data){
    GtkWidget** btns=data->btns;
    const char* name=data->name;
    if (name == NULL) name = "";
    int unique=TRUE;
    int curFigs=*(data->ind);
    for(int i=0;i<curFigs;i++){
       const char* label = g_object_get_data(G_OBJECT(btns[i]), "fig-name");
        if(label && strcmp(label,name)==0){
            showErrorNameExists(button);
            unique=FALSE;
            break;
        }
    }
    if(unique==TRUE){
        GtkWidget* box=data->box;
        figure* figures=data->figures;
        figure fig=data->newFig;
        int* ind=data->ind;
        fig.name=name[0];

        figures[*ind] = fig;
        btns[*ind] = gtk_check_button_new_with_label(name);
        g_object_set_data_full(G_OBJECT(btns[*ind]), "fig-name",g_strdup(name), g_free);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(btns[*ind]), GTK_CHECK_BUTTON(btns[0]));
        gtk_box_append(GTK_BOX(box), btns[*ind]);
        (*ind)++;
        g_idle_add(closeWindowIdle, gtk_widget_get_root(GTK_WIDGET(button)));
    }
}

static void onEntryChanged(GtkEntry* entry,gpointer userData){
    dataAdd* data=(dataAdd*)userData;
    g_free(data->name);
    const char* text=gtk_editable_get_text(GTK_EDITABLE(entry));
    data->name = g_strdup(text ? text : "");
}

static void toggleInfHorz(GtkCheckButton* button,gpointer userData){
    figure* fig=(figure*)userData;
    fig->inf_movement[HORZ_MOVEMENT]=gtk_check_button_get_active(GTK_CHECK_BUTTON(button))? TRUE:FALSE;
}

static void toggleInfVert(GtkCheckButton* button,gpointer userData){
    figure* fig=(figure*)userData;
    fig->inf_movement[VERT_MOVEMENT]=gtk_check_button_get_active(GTK_CHECK_BUTTON(button))? TRUE:FALSE; 
} 

static void toggleInfDiagMain(GtkCheckButton* button,gpointer userData){
    figure* fig=(figure*)userData;
    fig->inf_movement[MAIN_DIAG]=gtk_check_button_get_active(GTK_CHECK_BUTTON(button))? TRUE:FALSE;
}

static void toggleInfDiagOther(GtkCheckButton* button,gpointer userData){
    figure* fig=(figure*)userData;
    fig->inf_movement[OTHER_DIAG]=gtk_check_button_get_active(GTK_CHECK_BUTTON(button))? TRUE:FALSE;
}

static void gridToggled(GtkToggleButton *btn, gpointer user_data) {
    dataAdd *data = (dataAdd*) user_data;
    int r = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "grid-row"));
    int c = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "grid-col"));
    gboolean active = gtk_toggle_button_get_active(btn);
    data->newFig.special_movement[r][c] = active ? TRUE : FALSE;
}

static void GetNewFig(GtkWidget* button,dataAdd* data){
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));

    GtkWidget *newWind = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(newWind),"Adding a figure");
    gtk_window_set_default_size(GTK_WINDOW(newWind),ADDING_WIND_HORZ,ADDING_WIND_VERT);
    gtk_window_set_resizable(GTK_WINDOW(newWind), FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(newWind),parent);
    gtk_window_set_modal(GTK_WINDOW(newWind),TRUE);

    data->newFig=InitEmpty();
    GtkWidget* vbox=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);

    g_free(data->name);
    data->name=g_strdup("");

    GtkWidget* entry=gtk_entry_new();
    gtk_widget_set_hexpand(entry,TRUE);
    g_signal_connect(entry,"changed", G_CALLBACK(onEntryChanged),data);

    GtkWidget* boxCheckbuttons=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);

    GtkWidget* buttonInfHorz=gtk_check_button_new_with_label("Infinite horizontal movement");
    GtkWidget* buttonInfVert=gtk_check_button_new_with_label("Infinite vertical movement");
    GtkWidget* buttonInfDiagMain=gtk_check_button_new_with_label("Infinite movement on main diagonal");
    GtkWidget* buttonInfDiagOther=gtk_check_button_new_with_label("Infinite movement on other diagonal");

    gtk_check_button_set_active(GTK_CHECK_BUTTON(buttonInfHorz),FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(buttonInfVert),FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(buttonInfDiagMain),FALSE);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(buttonInfDiagOther),FALSE);

    g_signal_connect(GTK_CHECK_BUTTON(buttonInfHorz),"toggled",G_CALLBACK(toggleInfHorz),&(data->newFig));
    g_signal_connect(GTK_CHECK_BUTTON(buttonInfVert),"toggled",G_CALLBACK(toggleInfVert),&(data->newFig));
    g_signal_connect(GTK_CHECK_BUTTON(buttonInfDiagMain),"toggled",G_CALLBACK(toggleInfDiagMain),&(data->newFig));
    g_signal_connect(GTK_CHECK_BUTTON(buttonInfDiagOther),"toggled",G_CALLBACK(toggleInfDiagOther),&(data->newFig));

    gtk_box_append(GTK_BOX(boxCheckbuttons),buttonInfHorz);
    gtk_box_append(GTK_BOX(boxCheckbuttons),buttonInfVert);
    gtk_box_append(GTK_BOX(boxCheckbuttons),buttonInfDiagMain);
    gtk_box_append(GTK_BOX(boxCheckbuttons),buttonInfDiagOther);

    GtkWidget* grid=gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid),2);
    gtk_grid_set_column_spacing(GTK_GRID(grid),2);
    gtk_widget_set_margin_top(grid,6);
    gtk_widget_set_margin_bottom(grid,6);

    for(int i=0;i<SP_MOVEMENT_SZ;i++){
        for(int j=0;j<SP_MOVEMENT_SZ;j++){
            GtkWidget* cell=gtk_toggle_button_new();

            gtk_button_set_child(GTK_BUTTON(cell),NULL);
            gtk_widget_set_size_request(cell,32,32);

            g_object_set_data(G_OBJECT(cell),"grid-row",GINT_TO_POINTER(i));
            g_object_set_data(G_OBJECT(cell),"grid-col",GINT_TO_POINTER(j));

            if(i==CENTER_MOVMENT-1 && j==CENTER_MOVMENT-1){
                gtk_widget_set_sensitive(cell,FALSE);
                GtkWidget* img=gtk_image_new_from_icon_name("image-missing");
                gtk_button_set_child(GTK_BUTTON(cell),img);
            }else{
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cell),FALSE);
                g_signal_connect(cell,"toggled",G_CALLBACK(gridToggled),data);
            }
            gtk_grid_attach(GTK_GRID(grid),cell,j,i,1,1);
        }
    }
    gtk_widget_set_halign(grid,GTK_ALIGN_CENTER);

    GtkWidget* buttonReady = gtk_button_new_with_label("Add figure");
    g_signal_connect(buttonReady, "clicked", G_CALLBACK(checkInput),data);
    gtk_box_append(GTK_BOX(vbox),entry);
    gtk_box_append(GTK_BOX(vbox),boxCheckbuttons);
    gtk_box_append(GTK_BOX(vbox),grid);
    gtk_box_append(GTK_BOX(vbox),buttonReady);
    gtk_window_set_child(GTK_WINDOW(newWind),vbox);
    gtk_window_present(GTK_WINDOW(newWind));
}

static void showErrorMax(GtkButton* button){
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));

    GtkAlertDialog* dialog=gtk_alert_dialog_new("Too many figures");
    gtk_alert_dialog_set_detail(dialog,"Cap of figures is reached. Delete old figures to add new.");
    gtk_alert_dialog_set_buttons(dialog,(const char*[]){"OK",NULL});

    gtk_alert_dialog_show(dialog,parent); 
}

static void AppendToBox(GtkButton* button,gpointer userData) {
    dataAdd* data = (dataAdd*) userData;
    if (*(data->ind) == MAX_FIGURES) {
        showErrorMax(button);
        return;
    }

    if (data->flagButtonSignal == 1) {
        GetNewFig(GTK_WIDGET(button), data);
    } else {
        GtkWidget** btns = data->btns;
        GtkWidget* box = data->box;
        figure* figures = data->figures;
        int* ind = data->ind;
        const char* name = data->name;

        figures[*ind] = data->newFig;
        btns[*ind] = gtk_check_button_new_with_label(name);
        g_object_set_data_full(G_OBJECT(btns[*ind]), "fig-name",g_strdup(name), g_free);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(btns[*ind]),GTK_CHECK_BUTTON(btns[0]));
        gtk_box_append(GTK_BOX(box), btns[*ind]);
        (*ind)++;
    }
}

void showErrorOneFig(GtkButton* button){
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));

    GtkAlertDialog* dialog=gtk_alert_dialog_new("Last figure left");
    gtk_alert_dialog_set_detail(dialog,"Last figure left, add new figure to delete this figure");
    gtk_alert_dialog_set_buttons(dialog,(const char*[]){"OK",NULL});

    gtk_alert_dialog_show(dialog,parent); 
}

void RemoveCurFig(GtkButton* button, gpointer userData){
    dataRem* data=(dataRem*)userData;
    GtkWidget* box=data->box;
    GtkWidget** btns=data->btns;
    figure* figures=data->figures;
    int* curSize = data->ind;

    if(*curSize==1){
        showErrorOneFig(button);
    }else{                
        int indAct=-1;
        for(int i=0;i<*curSize;i++){
            if(btns[i]&&gtk_check_button_get_active(GTK_CHECK_BUTTON(btns[i])))
                indAct=i;
        }

        gtk_box_remove(GTK_BOX(box),btns[indAct]);

        for(int i=indAct;i<*curSize-1;i++){
            btns[i]=btns[i+1];
            figures[i]=figures[i+1];
        }

        btns[*curSize - 1] = NULL;
        figures[*curSize - 1] = InitEmpty();

        if(indAct==*curSize-1) indAct--;

        (*curSize)--;

        gtk_check_button_set_active(GTK_CHECK_BUTTON(btns[indAct]),TRUE);
    }
}

//callback for solve button
void StartSolve(GtkButton* button, gpointer userData) {
    dataSolve* data = (dataSolve*)userData;
    int ind = -1;
    //finding what figure we chose
    for (int i = 0; i < MAX_FIGURES; i++) {
        if (data->btns[i] && gtk_check_button_get_active(GTK_CHECK_BUTTON(data->btns[i]))) {
            ind = i;
            break;
        }
    }
    if (ind != -1) {
        gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
        g_atomic_int_set(&solve_cancel, 0);
        g_timeout_add_seconds(60, cancelSolveTimeout, button);

        SolveThreadData* tdata = malloc(sizeof(SolveThreadData));
        tdata->fig = data->figures[ind];
        tdata->size = data->size;
        tdata->flag = data->flagAll;
        tdata->button = GTK_WIDGET(button);

        const char *fullName = g_object_get_data(G_OBJECT(data->btns[ind]), "fig-name");
        if (fullName == NULL || strlen(fullName) == 0)
            fullName = "unnamed";
        tdata->fullName = fullName;

        GThread* thread = g_thread_new("solver", SolveThread, tdata);
        g_thread_unref(thread);
    }
}

static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget* window;
    GtkWidget* box;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "N-Piece Solver");
    gtk_window_set_default_size(GTK_WINDOW(window),MAIN_WIND_HORZ,MAIN_WIND_VERT);

    //creating radio button box
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(box), TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_START);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget** btns = g_new(GtkWidget*, MAX_FIGURES);
    for (int i = 0; i < MAX_FIGURES; i++) btns[i] = NULL;

    figure* figures = g_new(figure,MAX_FIGURES);
    for (int i = 0; i < MAX_FIGURES; i++) figures[i] = InitEmpty();

    btns[0] = gtk_check_button_new_with_label("Queen");
    g_object_set_data_full(G_OBJECT(btns[0]), "fig-name",g_strdup("Queen"), g_free);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(btns[0]), TRUE);
    figures[0] = InitQueen();
    gtk_box_append(GTK_BOX(box), btns[0]);
    //queen adding

    int* lastInd=g_new(int,1);
    *lastInd=1;

    dataAdd* dataAddfig=g_new(dataAdd,1);
    dataAddfig->box=box;
    dataAddfig->btns=btns;
    dataAddfig->figures=figures;
    dataAddfig->ind=lastInd;
    dataAddfig->flagButtonSignal=0;
    
    GtkWidget* boxAddRem = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);

    GtkWidget* buttonAdd = gtk_button_new();
    GtkWidget* imgPlus = gtk_image_new_from_icon_name("list-add-symbolic");

    gtk_button_set_child(GTK_BUTTON(buttonAdd),imgPlus);
    gtk_widget_set_tooltip_text(buttonAdd,"Add");
    gtk_box_append(GTK_BOX(boxAddRem),buttonAdd);


    GtkWidget* buttonRem = gtk_button_new();
    GtkWidget* imgRem = gtk_image_new_from_icon_name("edit-delete");

    gtk_button_set_child(GTK_BUTTON(buttonRem),imgRem);
    gtk_widget_set_tooltip_text(buttonRem,"Delete");
    gtk_box_append(GTK_BOX(boxAddRem),buttonRem);

    g_object_set_data(G_OBJECT(buttonRem),"buttons",btns);
    g_object_set_data(G_OBJECT(buttonRem),"figures",figures);

    dataRem* dataRemCurFig=g_new(dataRem,1);
    dataRemCurFig->box=box;
    dataRemCurFig->btns=btns;
    dataRemCurFig->figures=figures;
    dataRemCurFig->ind=lastInd;

    dataAddfig->name=g_strdup("magaraga");
    dataAddfig->newFig=InitMagarg();

    AppendToBox(GTK_BUTTON(buttonAdd),dataAddfig);                                    //magaraga adding
    
    g_free(dataAddfig->name);
    dataAddfig->name=g_strdup("rooknight");
    dataAddfig->newFig=test;

    AppendToBox(GTK_BUTTON(buttonAdd),dataAddfig);          //testing of files adding

    g_free(dataAddfig->name);
    dataAddfig->name=g_strdup("");
    dataAddfig->flagButtonSignal=1;

    GtkWidget* scaleN = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,1.0,(double)MAX_SIZE_ALL,1.0);
    gtk_range_set_value(GTK_RANGE(scaleN),(double)DEFAULT_SIZE);
    gtk_widget_set_tooltip_text(scaleN,"Amount of figures");
    gtk_scale_set_draw_value(GTK_SCALE(scaleN), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(scaleN), GTK_POS_BOTTOM);
    gtk_widget_set_hexpand(scaleN, TRUE);

    GtkWidget* checkAllSolves = gtk_check_button_new_with_label("All solutions");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(checkAllSolves), TRUE);
    g_object_set_data(G_OBJECT(checkAllSolves), "scale", scaleN);
    gtk_widget_set_hexpand(checkAllSolves, TRUE);

    dataSolve* data = g_new(dataSolve, TRUE);
    data->btns = btns;
    data->figures = figures;
    data->flagAll = TRUE;
    data->size = DEFAULT_SIZE;

    GtkWidget* buttonSolve = gtk_button_new_with_label("Solve");

    g_signal_connect(checkAllSolves, "toggled", G_CALLBACK(toggleSolves), data);
    g_signal_connect(scaleN, "value-changed", G_CALLBACK(SizeChange), data);
    g_signal_connect(buttonSolve, "clicked", G_CALLBACK(StartSolve), data);
    g_signal_connect(buttonAdd,"clicked",G_CALLBACK(AppendToBox),dataAddfig);
    g_signal_connect(buttonRem,"clicked",G_CALLBACK(RemoveCurFig),dataRemCurFig);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(vbox), scaleN);
    gtk_box_append(GTK_BOX(vbox), box);
    gtk_box_append(GTK_BOX(vbox),boxAddRem);
    gtk_box_append(GTK_BOX(vbox), checkAllSolves);
    gtk_box_append(GTK_BOX(vbox), buttonSolve);
    gtk_window_set_child(GTK_WINDOW(window), vbox);
    gtk_window_present(GTK_WINDOW(window));
    g_signal_connect(window, "destroy", G_CALLBACK(deleteAllOutputFiles), NULL);
}

int main(int argc, char** argv) {
    test = AddFigure("rooknight.fig");
    GtkApplication* app = gtk_application_new("com.N-piece-Solver", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
//this thing makes vscode use compiler that knows i have gtk
//D:/C_complier/msys2_shell.cmd -ucrt64 -defterm -here -no-start