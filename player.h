#ifndef PLAYER_H
#define PLAYER_H

#include <gst/gst.h>
#include <gtk/gtk.h>

typedef struct {
    GstElement *pipeline;
    GstElement *video_sink;
    GstElement *audio_sink;
    int is_playing;
    int repeat_mode;
    int has_video;
    char *current_audio_path;
    char *current_video_path;
    double volume;
    gint64 duration;
    gint64 position;
} Player;

Player* player_create();
void player_destroy(Player *player);
void player_play_track(Player *player, const char *audio_path, const char *video_path, GtkWidget *video_widget);
void player_play_current(Player *player, GtkWidget *video_widget);
void player_stop(Player *player);
void player_pause(Player *player);
void player_resume(Player *player);
int player_is_playing(Player *player);
void player_toggle_repeat(Player *player);
int player_get_repeat_mode(Player *player);
void player_set_current_track(Player *player, const char *audio_path, const char *video_path);
const char* player_get_current_audio_path(Player *player);
int player_has_video(Player *player);
GstElement* player_get_video_sink(Player *player);

// New functions for volume and seeking
void player_set_volume(Player *player, double volume);
double player_get_volume(Player *player);
void player_seek(Player *player, gint64 position);
gint64 player_get_duration(Player *player);
gint64 player_get_position(Player *player);
gboolean player_update_position(Player *player);

#endif