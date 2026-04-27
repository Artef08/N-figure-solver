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
#define INF 10000000        //max iterations for local conflicts

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
} SolveThreadData;      //struct for threads

//stop solving if we ran out of time
static gboolean cancel_solve_timeout(gpointer user_data) {
    GtkWidget *button = GTK_WIDGET(user_data);
    g_atomic_int_set(&solve_cancel, 1);
    gtk_widget_set_sensitive(button, TRUE);
    return G_SOURCE_REMOVE;
}

//adding files that we added
static void add_output_file(const char *filename) {
    g_mutex_lock(&file_list_mutex);
    output_files = g_list_append(output_files, g_strdup(filename));
    g_mutex_unlock(&file_list_mutex);
}

//so we can delete them after closing
static void delete_all_output_files(void) {
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

static void toggle_solves(GtkToggleButton* checkAll,gpointer userData){
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

static gboolean reenable_button(gpointer userData) {
    gtk_widget_set_sensitive(GTK_WIDGET(userData), TRUE);
    return G_SOURCE_REMOVE;
}

//threading
gpointer SolveThread(gpointer userData) {
    SolveThreadData* data = (SolveThreadData*)userData;

    if (g_atomic_int_get(&solve_cancel)) {
        g_idle_add(reenable_button, data->button);
        free(data);
        return NULL;
    }
    //and we don't start if we got a solution
    char filename[64];
    snprintf(filename, sizeof(filename), "%c_%d_%d.txt", data->fig.name, data->size,data->flag);

    FILE* test = fopen(filename, "r");
    if (test != NULL) {
        fclose(test);
        g_idle_add(reenable_button, data->button);
        free(data);
        return NULL;
    }
    //crating a new file
    FILE* f = fopen(filename, "w");
    if (f == NULL) {
        g_idle_add(reenable_button, data->button);
        free(data);
        return NULL;
    }
    //printing size of a board
    fprintf(f, "%d\n\n", data->size);
    add_output_file(filename);

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
    g_idle_add(reenable_button, data->button);
    free(data);
    return NULL;
}

//TODO add button to use this function in gui
static void AppendToBox(GtkWidget* box, GtkWidget** btns, figure* figures, figure fig, int ind, char* name) {
    figures[ind] = fig;
    btns[ind] = gtk_check_button_new_with_label(name);
    gtk_check_button_set_group(GTK_CHECK_BUTTON(btns[ind]), GTK_CHECK_BUTTON(btns[0]));
    gtk_box_append(GTK_BOX(box), btns[ind]);
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
        g_timeout_add_seconds(60, cancel_solve_timeout, button);

        SolveThreadData* tdata = malloc(sizeof(SolveThreadData));
        tdata->fig = data->figures[ind];
        tdata->size = data->size;
        tdata->flag = data->flagAll;
        tdata->button = GTK_WIDGET(button);
        GThread* thread = g_thread_new("solver", SolveThread, tdata);
        g_thread_unref(thread);
    }
}

static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget* window;
    GtkWidget* box;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "N-Piece Solver");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    //creating radio button box
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(box), TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_START);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget** btns = (GtkWidget**)malloc(MAX_FIGURES * sizeof(GtkWidget*));
    if (btns == NULL) { g_object_unref(app); return; }
    for (int i = 0; i < MAX_FIGURES; i++) btns[i] = NULL;

    figure* figures = (figure*)malloc(MAX_FIGURES * sizeof(figure));
    if (figures == NULL) { g_object_unref(app); return; }
    for (int i = 0; i < MAX_FIGURES; i++) figures[i] = InitEmpty();

    btns[0] = gtk_check_button_new_with_label("Queen");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(btns[0]), TRUE);
    figures[0] = InitQueen();
    gtk_box_append(GTK_BOX(box), btns[0]);
    //queen adding

    AppendToBox(box, btns, figures, InitMagarg(), 1, "Mahoraga");   //magaraga adding, ill change it later(maybe)
    AppendToBox(box, btns, figures, test, 2, "rooknight");          //testing of files adding

    GtkWidget* scaleN = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,1.0,(double)MAX_SIZE_ALL,1.0);
    gtk_range_set_value(GTK_RANGE(scaleN),(double)DEFAULT_SIZE);
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

    g_signal_connect(checkAllSolves, "toggled", G_CALLBACK(toggle_solves), data);
    g_signal_connect(scaleN, "value-changed", G_CALLBACK(SizeChange), data);

    GtkWidget* buttonSolve = gtk_button_new_with_label("Solve");
    g_signal_connect(buttonSolve, "clicked", G_CALLBACK(StartSolve), data);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(vbox), scaleN);
    gtk_box_append(GTK_BOX(vbox), box);
    gtk_box_append(GTK_BOX(vbox), checkAllSolves);
    gtk_box_append(GTK_BOX(vbox), buttonSolve);
    gtk_window_set_child(GTK_WINDOW(window), vbox);
    gtk_window_present(GTK_WINDOW(window));
    g_signal_connect(window, "destroy", G_CALLBACK(delete_all_output_files), NULL);
}

int main(int argc, char** argv) {
    test = AddFigure("rooknight.txt");
    GtkApplication* app = gtk_application_new("com.N-piece-Solver", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
//this thing makes vscode use compiler that knows i have gtk
//D:/C_complier/msys2_shell.cmd -ucrt64 -defterm -here -no-start