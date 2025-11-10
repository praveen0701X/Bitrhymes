#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "playlist.h"
#include "player.h"
#include "ui.h"

#define AUDIO_DIR "assets/songs"
#define VIDEO_DIR "assets/videos"

void scan_media_files(Playlist *pl, HashTable *ht) {
    GDir *audio_dir, *video_dir;
    GError *error = NULL;
    const gchar *filename;

    // Scan audio files
    audio_dir = g_dir_open(AUDIO_DIR, 0, &error);
    if (!audio_dir) {
        printf("No media files found in '%s'\n", AUDIO_DIR);
        return;
    }

    while ((filename = g_dir_read_name(audio_dir)) != NULL) {
        if (g_str_has_suffix(filename, ".mp3")) {
            char audio_path[1024];
            snprintf(audio_path, sizeof(audio_path), "%s/%s", AUDIO_DIR, filename);

            // Check for matching video
            char video_path[1024];
            char base_name[256];
            strcpy(base_name, filename);
            char *dot = strrchr(base_name, '.');
            if (dot) *dot = '\0';
            
            snprintf(video_path, sizeof(video_path), "%s/%s.mp4", VIDEO_DIR, base_name);
            
            // Check if video file exists
            if (!g_file_test(video_path, G_FILE_TEST_EXISTS)) {
                video_path[0] = '\0';
            }

            Track *track = playlist_add(pl, audio_path, video_path[0] ? video_path : NULL);
            
            // Add to hash table for searching
            hashtable_insert(ht, base_name, track);
        }
    }

    g_dir_close(audio_dir);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    Playlist *pl = playlist_create();
    HashTable *ht = hashtable_create();
    scan_media_files(pl, ht);

    if (pl->size == 0) {
        printf("No media files found. Exiting.\n");
        return 1;
    }

    Player *player = player_create();
    UIContext *ui = ui_create(player, pl, ht);

    gtk_main();

    player_destroy(player);
    playlist_destroy(pl);
    hashtable_destroy(ht);
    ui_destroy(ui);

    return 0;
}