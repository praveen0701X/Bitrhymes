#include "player.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void bus_message_handler(GstBus *bus, GstMessage *msg, gpointer user_data) {
    Player *player = (Player*)user_data;
    
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        printf("PLAYER: End of stream reached\n");
        if (player && player->repeat_mode) {
            printf("PLAYER: Repeat mode - restarting track\n");
            gst_element_seek_simple(player->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
            gst_element_set_state(player->pipeline, GST_STATE_PLAYING);
        } else {
            player->is_playing = 0;
            printf("PLAYER: Playback finished\n");
        }
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err;
        gchar *dbg;
        gst_message_parse_error(msg, &err, &dbg);
        g_printerr("PLAYER: GStreamer Error: %s\n", err->message);
        if (dbg) g_printerr("PLAYER: Debug info: %s\n", dbg);
        g_error_free(err);
        g_free(dbg);
        player->is_playing = 0;
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_STATE_CHANGED) {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(player->pipeline)) {
            player->is_playing = (new_state == GST_STATE_PLAYING);
            
            // Update duration when pipeline goes to PAUSED/PLAYING
            if (new_state == GST_STATE_PAUSED || new_state == GST_STATE_PLAYING) {
                if (gst_element_query_duration(player->pipeline, GST_FORMAT_TIME, &player->duration)) {
                    printf("PLAYER: Duration updated: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(player->duration));
                }
            }
        }
    } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_DURATION_CHANGED) {
        // Duration might have changed
        if (gst_element_query_duration(player->pipeline, GST_FORMAT_TIME, &player->duration)) {
            printf("PLAYER: Duration changed: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(player->duration));
        }
    }
}

static char* encode_uri(const char *filepath) {
    GFile *file = g_file_new_for_path(filepath);
    char *uri = g_file_get_uri(file);
    g_object_unref(file);
    return uri;
}

Player* player_create() {
    gst_init(NULL, NULL);
    Player *player = malloc(sizeof(Player));
    player->pipeline = NULL;
    player->video_sink = NULL;
    player->audio_sink = NULL;
    player->is_playing = 0;
    player->repeat_mode = 0;
    player->current_audio_path = NULL;
    player->current_video_path = NULL;
    player->has_video = 0;
    player->volume = 1.0; // 100% volume
    player->duration = 0;
    player->position = 0;
    printf("PLAYER: Player created successfully\n");
    return player;
}

void player_destroy(Player *player) {
    if (!player) return;
    
    printf("PLAYER: Destroying player\n");
    
    if (player->pipeline) {
        printf("PLAYER: Stopping and destroying pipeline\n");
        gst_element_set_state(player->pipeline, GST_STATE_NULL);
        gst_object_unref(player->pipeline);
        player->pipeline = NULL;
    }
    
    if (player->current_audio_path) {
        free(player->current_audio_path);
        player->current_audio_path = NULL;
    }
    
    if (player->current_video_path) {
        free(player->current_video_path);
        player->current_video_path = NULL;
    }
    
    free(player);
    printf("PLAYER: Player destroyed\n");
}

void player_stop(Player *player) {
    if (!player) return;
    
    printf("PLAYER: Stopping playback\n");
    
    if (player->pipeline) {
        gst_element_set_state(player->pipeline, GST_STATE_NULL);
        player->is_playing = 0;
        player->position = 0;
    }
}

void player_pause(Player *player) {
    if (!player || !player->pipeline) return;
    
    printf("PLAYER: Pausing playback\n");
    
    GstState state;
    gst_element_get_state(player->pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    
    if (state == GST_STATE_PLAYING) {
        gst_element_set_state(player->pipeline, GST_STATE_PAUSED);
        player->is_playing = 0;
    }
}

void player_resume(Player *player) {
    if (!player || !player->pipeline) return;
    
    printf("PLAYER: Resuming playback\n");
    
    GstState state;
    gst_element_get_state(player->pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    
    if (state == GST_STATE_PAUSED) {
        gst_element_set_state(player->pipeline, GST_STATE_PLAYING);
        player->is_playing = 1;
    }
}

void player_play_track(Player *player, const char *audio_path, const char *video_path, GtkWidget *video_widget) {
    if (!player || !audio_path) {
        printf("PLAYER: Cannot play - invalid player or audio path\n");
        return;
    }

    printf("PLAYER: Starting playback of track\n");
    printf("PLAYER:   Audio: %s\n", audio_path);
    printf("PLAYER:   Video: %s\n", video_path ? video_path : "None");

    // Store current track
    if (player->current_audio_path) free(player->current_audio_path);
    if (player->current_video_path) free(player->current_video_path);
    
    player->current_audio_path = strdup(audio_path);
    player->current_video_path = video_path ? strdup(video_path) : NULL;
    player->has_video = (video_path != NULL && strlen(video_path) > 0);

    // Stop and destroy current pipeline
    if (player->pipeline) {
        gst_element_set_state(player->pipeline, GST_STATE_NULL);
        gst_object_unref(player->pipeline);
        player->pipeline = NULL;
        player->video_sink = NULL;
        player->audio_sink = NULL;
    }

    // Choose which file to play (prefer video if available)
    const char *file_to_play = player->has_video ? video_path : audio_path;
    
    if (!file_to_play) {
        fprintf(stderr, "PLAYER: No valid file to play\n");
        return;
    }

    // Create URI
    char *uri = encode_uri(file_to_play);
    printf("PLAYER: Loading URI: %s\n", uri);

    // Create playbin pipeline
    player->pipeline = gst_element_factory_make("playbin", "player");
    if (!player->pipeline) {
        fprintf(stderr, "PLAYER: Failed to create playbin pipeline\n");
        g_free(uri);
        return;
    }

    g_object_set(G_OBJECT(player->pipeline), "uri", uri, NULL);
    g_free(uri);

    // Set up video sink if video widget is provided
    if (video_widget && player->has_video) {
        player->video_sink = gst_element_factory_make("gtksink", "video_sink");
        if (player->video_sink) {
            g_object_set(player->video_sink, "widget", video_widget, NULL);
            g_object_set(G_OBJECT(player->pipeline), "video-sink", player->video_sink, NULL);
            printf("PLAYER: Video sink connected to GTK widget\n");
        }
    }

    // Set up audio sink and volume
    player->audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");
    if (player->audio_sink) {
        g_object_set(G_OBJECT(player->pipeline), "audio-sink", player->audio_sink, NULL);
    }

    // Set initial volume
    player_set_volume(player, player->volume);

    // Set up bus monitoring
    GstBus *bus = gst_element_get_bus(player->pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK(bus_message_handler), player);
    gst_object_unref(bus);

    // Start playback
    GstStateChangeReturn ret = gst_element_set_state(player->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        fprintf(stderr, "PLAYER: Failed to start playback\n");
        gst_element_set_state(player->pipeline, GST_STATE_NULL);
        gst_object_unref(player->pipeline);
        player->pipeline = NULL;
        return;
    }
    
    player->is_playing = 1;
    printf("PLAYER: Playback started successfully - %s\n", 
           player->has_video ? "Video mode" : "Audio mode");
}

void player_play_current(Player *player, GtkWidget *video_widget) {
    if (!player || !player->current_audio_path) {
        printf("PLAYER: Cannot play current - no track loaded\n");
        return;
    }
    
    // Stop current playback
    if (player->pipeline) {
        gst_element_set_state(player->pipeline, GST_STATE_NULL);
        gst_object_unref(player->pipeline);
        player->pipeline = NULL;
        player->video_sink = NULL;
        player->audio_sink = NULL;
    }

    // Play the current track again
    player_play_track(player, player->current_audio_path, player->current_video_path, video_widget);
}

int player_is_playing(Player *player) {
    if (!player || !player->pipeline) return 0;
    
    GstState state;
    gst_element_get_state(player->pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    int playing = (state == GST_STATE_PLAYING);
    
    player->is_playing = playing;
    return playing;
}

void player_toggle_repeat(Player *player) {
    if (!player) return;
    
    player->repeat_mode = !player->repeat_mode;
    printf("PLAYER: Repeat mode %s\n", player->repeat_mode ? "ON" : "OFF");
}

int player_get_repeat_mode(Player *player) {
    return player ? player->repeat_mode : 0;
}

void player_set_current_track(Player *player, const char *audio_path, const char *video_path) {
    if (!player) return;
    
    // Free existing paths
    if (player->current_audio_path) free(player->current_audio_path);
    if (player->current_video_path) free(player->current_video_path);
    
    // Set new paths
    if (audio_path) player->current_audio_path = strdup(audio_path);
    if (video_path) player->current_video_path = strdup(video_path);
    
    player->has_video = (video_path != NULL && strlen(video_path) > 0);
}

const char* player_get_current_audio_path(Player *player) {
    return player ? player->current_audio_path : NULL;
}

int player_has_video(Player *player) {
    return player ? player->has_video : 0;
}

GstElement* player_get_video_sink(Player *player) {
    return player ? player->video_sink : NULL;
}

// Volume control functions
void player_set_volume(Player *player, double volume) {
    if (!player) return;
    
    // Clamp volume between 0.0 and 1.0
    player->volume = fmax(0.0, fmin(1.0, volume));
    
    if (player->pipeline) {
        g_object_set(G_OBJECT(player->pipeline), "volume", player->volume, NULL);
    }
    
    printf("PLAYER: Volume set to %.0f%%\n", player->volume * 100);
}

double player_get_volume(Player *player) {
    return player ? player->volume : 0.0;
}

// Seek functions
void player_seek(Player *player, gint64 position) {
    if (!player || !player->pipeline) return;
    
    // Clamp position between 0 and duration
    if (player->duration > 0) {
        position = MAX(0, MIN(position, player->duration));
    }
    
    if (gst_element_seek_simple(player->pipeline, GST_FORMAT_TIME, 
                               GST_SEEK_FLAG_FLUSH, position)) {
        player->position = position;
        printf("PLAYER: Seeked to %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(position));
    }
}

gint64 player_get_duration(Player *player) {
    if (player && player->pipeline) {
        if (!gst_element_query_duration(player->pipeline, GST_FORMAT_TIME, &player->duration)) {
            player->duration = 0;
        }
    }
    return player ? player->duration : 0;
}

gint64 player_get_position(Player *player) {
    if (player && player->pipeline) {
        if (!gst_element_query_position(player->pipeline, GST_FORMAT_TIME, &player->position)) {
            player->position = 0;
        }
    }
    return player ? player->position : 0;
}

gboolean player_update_position(Player *player) {
    if (!player || !player->pipeline) return FALSE;
    return gst_element_query_position(player->pipeline, GST_FORMAT_TIME, &player->position);
}