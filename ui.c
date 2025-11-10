#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static UIContext *global_ui = NULL;

// Add forward declarations for all signal handlers at the top
static void on_volume_changed(GtkRange *range, gpointer user_data);
static void on_progress_changed(GtkRange *range, gpointer user_data);
static void on_search_clicked(GtkButton *button, gpointer user_data);

static char* format_time(gint64 nanoseconds) {
    gint64 seconds = nanoseconds / GST_SECOND;
    int minutes = seconds / 60;
    int secs = seconds % 60;
    char *buffer = malloc(10);
    snprintf(buffer, 10, "%02d:%02d", minutes, secs);
    return buffer;
}

static void update_progress_display(UIContext *ui) {
    if (!ui || !ui->player) return;
    
    player_update_position(ui->player);
    gint64 position = player_get_position(ui->player);
    gint64 duration = player_get_duration(ui->player);
    
    if (duration > 0) {
        // Update progress scale
        g_signal_handlers_block_by_func(ui->progress_scale, G_CALLBACK(on_progress_changed), ui);
        gtk_range_set_value(GTK_RANGE(ui->progress_scale), (gdouble)position / duration * 100.0);
        g_signal_handlers_unblock_by_func(ui->progress_scale, G_CALLBACK(on_progress_changed), ui);
        
        // Update time label
        char *current_time = format_time(position);
        char *total_time = format_time(duration);
        char time_text[50];
        snprintf(time_text, sizeof(time_text), "%s / %s", current_time, total_time);
        gtk_label_set_text(GTK_LABEL(ui->time_label), time_text);
        
        free(current_time);
        free(total_time);
    }
}

static gboolean progress_timer_callback(gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (ui && ui->player && player_is_playing(ui->player)) {
        update_progress_display(ui);
        return G_SOURCE_CONTINUE;
    }
    return G_SOURCE_CONTINUE;
}

static void update_button_states(UIContext *ui) {
    if (!ui || !ui->player) return;
    
    int is_playing = player_is_playing(ui->player);
    int repeat_mode = player_get_repeat_mode(ui->player);
    int has_video = player_has_video(ui->player);
    int shuffle_mode = playlist_get_shuffle_mode(ui->playlist);
    
    // Enable/disable buttons based on state
    gtk_widget_set_sensitive(ui->play_button, !is_playing);
    gtk_widget_set_sensitive(ui->pause_button, is_playing);
    
    // Update repeat button
    if (repeat_mode) {
        gtk_button_set_label(GTK_BUTTON(ui->repeat_button), "🔂 Repeat");
    } else {
        gtk_button_set_label(GTK_BUTTON(ui->repeat_button), "🔁 Repeat");
    }
    
    // Update shuffle button
    if (shuffle_mode) {
        gtk_button_set_label(GTK_BUTTON(ui->shuffle_button), "🔀 Shuffle");
    } else {
        gtk_button_set_label(GTK_BUTTON(ui->shuffle_button), "➡️ Shuffle");
    }
    
    // Update volume button
    double volume = player_get_volume(ui->player);
    char volume_text[20];
    snprintf(volume_text, sizeof(volume_text), "🔊 %.0f%%", volume * 100);
    gtk_button_set_label(GTK_BUTTON(ui->volume_button), volume_text);
    
    // Show/hide video display
    if (has_video && ui->video_display) {
        gtk_widget_show(ui->video_display);
        gtk_window_set_title(GTK_WINDOW(ui->window), "Media Player - Video Mode");
        if (ui->video_label) {
            gtk_label_set_text(GTK_LABEL(ui->video_label), "Video: Playing in embedded display");
        }
    } else {
        if (ui->video_display) {
            gtk_widget_hide(ui->video_display);
        }
        gtk_window_set_title(GTK_WINDOW(ui->window), "Media Player - Audio Mode");
        if (ui->video_label) {
            gtk_label_set_text(GTK_LABEL(ui->video_label), "Video: Not Available");
        }
    }
    
    // Update current track label
    if (ui->current_track && ui->current_track_label) {
        char track_info[200];
        snprintf(track_info, sizeof(track_info), "Now Playing: %s", ui->current_track->name);
        gtk_label_set_text(GTK_LABEL(ui->current_track_label), track_info);
    }
}

static void clear_video_display(UIContext *ui) {
    if (!ui || !ui->video_display) return;
    
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(ui->video_display));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
}

static const char* get_filename_from_path(const char* path) {
    if (!path) return "Unknown";
    const char* filename = strrchr(path, '\\');
    if (filename) return filename + 1;
    filename = strrchr(path, '/');
    if (filename) return filename + 1;
    return path;
}

static void select_track_in_list(UIContext *ui, Track *track) {
    if (!ui || !track) return;
    
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ui->liststore), &iter);
    
    while (valid) {
        gchar *audio_path;
        gtk_tree_model_get(GTK_TREE_MODEL(ui->liststore), &iter, 1, &audio_path, -1); // Changed to column 1 for path
        
        if (audio_path && strcmp(audio_path, track->audio_path) == 0) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->treeview));
            gtk_tree_selection_select_iter(selection, &iter);
            g_free(audio_path);
            break;
        }
        
        if (audio_path) g_free(audio_path);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(ui->liststore), &iter);
    }
}

static void play_selected_track(UIContext *ui) {
    if (!ui || !ui->current_track || !ui->player) return;
    
    printf("UI: Playing selected track - %s\n", ui->current_track->name);
    
    // Stop any current playback first
    if (ui->player->pipeline) {
        player_stop(ui->player);
    }
    
    // Add to history
    if (ui->history_stack) {
        stack_push(ui->history_stack, ui->current_track);
    }
    
    // Update player's current track
    player_set_current_track(ui->player, ui->current_track->audio_path, ui->current_track->video_path);
    
    // Clear previous video display
    clear_video_display(ui);
    
    // Create new video widget if needed
    GtkWidget *video_widget = NULL;
    if (ui->current_track->video_path && ui->video_display) {
        video_widget = gtk_drawing_area_new();
        gtk_widget_set_size_request(video_widget, 640, 360);
        gtk_widget_set_hexpand(video_widget, TRUE);
        gtk_widget_set_vexpand(video_widget, TRUE);
        gtk_container_add(GTK_CONTAINER(ui->video_display), video_widget);
        gtk_widget_show_all(ui->video_display);
        printf("UI: Created video widget for embedded display\n");
    }
    
    // Play the track
    player_play_track(ui->player, ui->current_track->audio_path, ui->current_track->video_path, video_widget);
    
    // Start progress timer
    if (ui->progress_timer_id == 0) {
        ui->progress_timer_id = g_timeout_add(100, progress_timer_callback, ui);
    }
    
    update_button_states(ui);
}

// Signal handlers
static void on_play_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player) return;

    if (!ui->current_track && ui->playlist && ui->playlist->head) {
        ui->current_track = ui->playlist->head;
        ui->playlist->current = ui->current_track;
        select_track_in_list(ui, ui->current_track);
    }

    if (!ui->current_track) {
        printf("UI: No track selected to play\n");
        return;
    }

    printf("UI: Play button clicked - Track: %s\n", ui->current_track->name);
    
    if (player_is_playing(ui->player)) {
        player_stop(ui->player);
    }
    
    play_selected_track(ui);
}

static void on_pause_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player) return;

    printf("UI: Pause button clicked\n");
    player_pause(ui->player);
    update_button_states(ui);
}

static void on_repeat_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player) return;

    printf("UI: Repeat button clicked\n");
    player_toggle_repeat(ui->player);
    update_button_states(ui);
}

static void on_shuffle_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->playlist) return;

    printf("UI: Shuffle button clicked\n");
    playlist_toggle_shuffle(ui->playlist);
    update_button_states(ui);
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player) return;

    double volume = gtk_range_get_value(range) / 100.0;
    player_set_volume(ui->player, volume);
    update_button_states(ui);
}

static void on_progress_changed(GtkRange *range, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player) return;

    double percent = gtk_range_get_value(range);
    gint64 duration = player_get_duration(ui->player);
    if (duration > 0) {
        gint64 position = (gint64)(duration * percent / 100.0);
        player_seek(ui->player, position);
        update_progress_display(ui);
    }
}

static void on_search_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->hashtable) return;

    const char *search_text = gtk_entry_get_text(GTK_ENTRY(ui->search_entry));
    if (!search_text || strlen(search_text) == 0) return;

    printf("UI: Searching for: %s\n", search_text);
    
    // First try exact match in hashtable
    Track *found_track = hashtable_search(ui->hashtable, search_text);
    
    // If not found, search in playlist
    if (!found_track) {
        found_track = playlist_find_by_name(ui->playlist, search_text);
    }
    
    if (found_track) {
        ui->current_track = found_track;
        ui->playlist->current = found_track;
        select_track_in_list(ui, found_track);
        printf("UI: Found track: %s\n", found_track->name);
        
        // Auto-play the found track
        play_selected_track(ui);
    } else {
        printf("UI: No track found matching: %s\n", search_text);
        // You could show a dialog here
    }
}

static void on_next_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player || !ui->playlist) return;

    if (!ui->current_track && ui->playlist->head) {
        ui->current_track = ui->playlist->head;
        ui->playlist->current = ui->current_track;
    } else if (!ui->current_track) {
        printf("UI: No tracks in playlist\n");
        return;
    }

    if (player_get_repeat_mode(ui->player)) {
        // Restart current song in repeat mode
        GtkWidget *video_widget = ui->video_display ? gtk_bin_get_child(GTK_BIN(ui->video_display)) : NULL;
        player_play_current(ui->player, video_widget);
    } else {
        // Move to next track
        ui->current_track = playlist_next(ui->current_track);
        ui->playlist->current = ui->current_track;
        select_track_in_list(ui, ui->current_track);
        play_selected_track(ui);
    }
    
    update_button_states(ui);
}

static void on_prev_clicked(GtkButton *button, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->player || !ui->playlist) return;

    if (!ui->current_track && ui->playlist->head) {
        ui->current_track = ui->playlist->head;
        ui->playlist->current = ui->current_track;
    } else if (!ui->current_track) {
        printf("UI: No tracks in playlist\n");
        return;
    }

    if (player_get_repeat_mode(ui->player)) {
        // Restart current song in repeat mode
        GtkWidget *video_widget = ui->video_display ? gtk_bin_get_child(GTK_BIN(ui->video_display)) : NULL;
        player_play_current(ui->player, video_widget);
    } else {
        // Move to previous track
        ui->current_track = playlist_prev(ui->current_track);
        ui->playlist->current = ui->current_track;
        select_track_in_list(ui, ui->current_track);
        play_selected_track(ui);
    }
    
    update_button_states(ui);
}

static void on_treeview_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (!ui || !ui->playlist) return;

    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *audio_path;
        gtk_tree_model_get(model, &iter, 1, &audio_path, -1); // Get from column 1 (path)
        
        if (audio_path) {
            // Find and set the selected track
            Track *node = ui->playlist->head;
            if (node) {
                do {
                    if (strcmp(node->audio_path, audio_path) == 0) {
                        ui->playlist->current = node;
                        ui->current_track = node;
                        player_set_current_track(ui->player, node->audio_path, node->video_path);
                        
                        // Stop any current playback when user selects a new track
                        if (ui->player->pipeline) {
                            player_stop(ui->player);
                        }
                        break;
                    }
                    node = node->next;
                } while (node != ui->playlist->head);
            }
            g_free(audio_path);
        }
    }
}

static gboolean periodic_update(gpointer user_data) {
    UIContext *ui = (UIContext*)user_data;
    if (ui && ui->player) {
        update_button_states(ui);
    }
    return G_SOURCE_CONTINUE;
}

UIContext* ui_create(Player *player, Playlist *playlist, HashTable *hashtable) {
    UIContext *ui = malloc(sizeof(UIContext));
    ui->player = player;
    ui->playlist = playlist;
    ui->hashtable = hashtable;
    ui->play_queue = queue_create();
    ui->history_stack = stack_create();
    ui->current_track = playlist->current;
    ui->video_label = NULL;
    ui->video_display = NULL;
    ui->progress_timer_id = 0;
    global_ui = ui;

    gtk_init(0, NULL);

    // Main window
    ui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Advanced Media Player");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 900, 800);
    g_signal_connect(ui->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(ui->window), vbox);

    // Search box
    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), search_box, FALSE, FALSE, 0);
    
    ui->search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->search_entry), "Search for a song...");
    gtk_box_pack_start(GTK_BOX(search_box), ui->search_entry, TRUE, TRUE, 0);
    
    ui->search_button = gtk_button_new_with_label("🔍 Search");
    g_signal_connect(ui->search_button, "clicked", G_CALLBACK(on_search_clicked), ui);
    gtk_box_pack_start(GTK_BOX(search_box), ui->search_button, FALSE, FALSE, 0);

    // Current track label
    ui->current_track_label = gtk_label_new("Select a track to play");
    gtk_box_pack_start(GTK_BOX(vbox), ui->current_track_label, FALSE, FALSE, 0);

    // Video status label
    ui->video_label = gtk_label_new("Video: Not Available");
    gtk_box_pack_start(GTK_BOX(vbox), ui->video_label, FALSE, FALSE, 0);

    // Video display area
    ui->video_display = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(ui->video_display), GTK_SHADOW_IN);
    gtk_widget_set_size_request(ui->video_display, 640, 360);
    gtk_widget_set_vexpand(ui->video_display, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), ui->video_display, FALSE, FALSE, 0);
    gtk_widget_hide(ui->video_display);

    // Progress bar
    GtkWidget *progress_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), progress_box, FALSE, FALSE, 0);
    
    ui->progress_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(ui->progress_scale), 0);
    g_signal_connect(ui->progress_scale, "value-changed", G_CALLBACK(on_progress_changed), ui);
    gtk_box_pack_start(GTK_BOX(progress_box), ui->progress_scale, TRUE, TRUE, 0);
    
    ui->time_label = gtk_label_new("00:00 / 00:00");
    gtk_box_pack_start(GTK_BOX(progress_box), ui->time_label, FALSE, FALSE, 0);

    // TreeView for playlist
    ui->liststore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING); // Name and path
    ui->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ui->liststore));
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_treeview_selection_changed), ui);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col1 = gtk_tree_view_column_new_with_attributes("Track", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ui->treeview), col1);
    gtk_box_pack_start(GTK_BOX(vbox), ui->treeview, TRUE, TRUE, 0);

    // Fill playlist
    Track *node = playlist->head;
    if (node) {
        do {
            GtkTreeIter iter;
            gtk_list_store_append(ui->liststore, &iter);
            gtk_list_store_set(ui->liststore, &iter, 0, node->name, 1, node->audio_path, -1);
            node = node->next;
        } while (node != playlist->head);
    }

    // Select the current track
    if (playlist->current) {
        select_track_in_list(ui, playlist->current);
    }

    // Control buttons - First row
    GtkWidget *hbox_controls1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_controls1, FALSE, FALSE, 0);

    ui->prev_button = gtk_button_new_with_label("⏮ Prev");
    g_signal_connect(ui->prev_button, "clicked", G_CALLBACK(on_prev_clicked), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls1), ui->prev_button, TRUE, TRUE, 0);

    ui->play_button = gtk_button_new_with_label("▶ Play");
    g_signal_connect(ui->play_button, "clicked", G_CALLBACK(on_play_clicked), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls1), ui->play_button, TRUE, TRUE, 0);

    ui->pause_button = gtk_button_new_with_label("⏸ Pause");
    g_signal_connect(ui->pause_button, "clicked", G_CALLBACK(on_pause_clicked), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls1), ui->pause_button, TRUE, TRUE, 0);

    ui->next_button = gtk_button_new_with_label("⏭ Next");
    g_signal_connect(ui->next_button, "clicked", G_CALLBACK(on_next_clicked), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls1), ui->next_button, TRUE, TRUE, 0);

    // Control buttons - Second row
    GtkWidget *hbox_controls2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_controls2, FALSE, FALSE, 0);

    ui->repeat_button = gtk_button_new_with_label("🔁 Repeat");
    g_signal_connect(ui->repeat_button, "clicked", G_CALLBACK(on_repeat_clicked), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls2), ui->repeat_button, TRUE, TRUE, 0);

    ui->shuffle_button = gtk_button_new_with_label("➡️ Shuffle");
    g_signal_connect(ui->shuffle_button, "clicked", G_CALLBACK(on_shuffle_clicked), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls2), ui->shuffle_button, TRUE, TRUE, 0);

    ui->volume_button = gtk_button_new_with_label("🔊 100%");
    gtk_box_pack_start(GTK_BOX(hbox_controls2), ui->volume_button, FALSE, FALSE, 0);

    ui->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
    gtk_range_set_value(GTK_RANGE(ui->volume_scale), 100);
    g_signal_connect(ui->volume_scale, "value-changed", G_CALLBACK(on_volume_changed), ui);
    gtk_box_pack_start(GTK_BOX(hbox_controls2), ui->volume_scale, TRUE, TRUE, 0);

    // Info label
    GtkWidget *info_label = gtk_label_new("Click any track to select it, then click Play. Use search to find songs quickly.");
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    // Initial button states
    update_button_states(ui);

    // Set up periodic update
    g_timeout_add(500, periodic_update, ui);

    gtk_widget_show_all(ui->window);

    return ui;
}

void ui_destroy(UIContext *ui) {
    if (!ui) return;
    
    // Stop progress timer
    if (ui->progress_timer_id > 0) {
        g_source_remove(ui->progress_timer_id);
    }
    
    gtk_widget_destroy(ui->window);
    
    if (ui->play_queue) queue_destroy(ui->play_queue);
    if (ui->history_stack) stack_destroy(ui->history_stack);
    
    free(ui);
    global_ui = NULL;
}