#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "player.h"
#include "playlist.h"
#include "hashtable.h"
#include "queue.h"
#include "stack.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *treeview;
    GtkListStore *liststore;
    GtkWidget *play_button;
    GtkWidget *pause_button;
    GtkWidget *next_button;
    GtkWidget *prev_button;
    GtkWidget *repeat_button;
    GtkWidget *shuffle_button;
    GtkWidget *volume_button;
    GtkWidget *volume_scale;
    GtkWidget *search_entry;
    GtkWidget *search_button;
    GtkWidget *progress_scale;
    GtkWidget *time_label;
    GtkWidget *video_label;
    GtkWidget *video_display;
    GtkWidget *current_track_label;
    Player *player;
    Playlist *playlist;
    HashTable *hashtable;
    Queue *play_queue;
    Stack *history_stack;
    Track *current_track;
    guint progress_timer_id;
} UIContext;

UIContext* ui_create(Player *player, Playlist *playlist, HashTable *hashtable);
void ui_destroy(UIContext *ui);

#endif