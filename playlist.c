#include "playlist.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>  // For g_ascii_strcasecmp

// Case-insensitive string search function
static char* my_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char *p = haystack; *p; p++) {
        if (g_ascii_strncasecmp(p, needle, needle_len) == 0) {
            return (char*)p;
        }
    }
    return NULL;
}

Playlist* playlist_create() {
    Playlist *pl = malloc(sizeof(Playlist));
    pl->head = NULL;
    pl->current = NULL;
    pl->size = 0;
    pl->shuffle_mode = 0;
    return pl;
}

Track* playlist_add(Playlist *pl, const char *audio, const char *video) {
    Track *t = malloc(sizeof(Track));
    t->audio_path = strdup(audio);
    t->video_path = video ? strdup(video) : NULL;
    
    // Extract track name from audio path
    const char *filename = strrchr(audio, '/');
    if (!filename) filename = strrchr(audio, '\\');
    if (filename) filename++;
    else filename = audio;
    
    t->name = strdup(filename);
    // Remove extension
    char *dot = strrchr(t->name, '.');
    if (dot) *dot = '\0';
    
    t->next = t->prev = NULL;

    if (!pl->head) {
        pl->head = t;
        t->next = t->prev = t;
        pl->current = t;
    } else {
        Track *tail = pl->head->prev;
        tail->next = t;
        t->prev = tail;
        t->next = pl->head;
        pl->head->prev = t;
    }
    pl->size++;
    return t;
}

Track* playlist_next(Track *current) {
    if (!current) return NULL;
    return current->next;
}

Track* playlist_prev(Track *current) {
    if (!current) return NULL;
    return current->prev;
}

void playlist_destroy(Playlist *pl) {
    if (!pl) return;
    if (!pl->head) { free(pl); return; }

    Track *node = pl->head;
    do {
        Track *next = node->next;
        free(node->audio_path);
        if (node->video_path) free(node->video_path);
        free(node->name);
        free(node);
        node = next;
    } while (node != pl->head);
    free(pl);
}

// Get track by index
Track* playlist_get_track(Playlist *pl, int index) {
    if (!pl || !pl->head || index < 0 || index >= pl->size) return NULL;
    
    Track *node = pl->head;
    for (int i = 0; i < index; i++) {
        node = node->next;
    }
    return node;
}

// Shuffle the playlist
void playlist_shuffle(Playlist *pl) {
    if (!pl || pl->size < 2) return;
    
    // Create an array of tracks
    Track **tracks = malloc(pl->size * sizeof(Track*));
    Track *node = pl->head;
    for (int i = 0; i < pl->size; i++) {
        tracks[i] = node;
        node = node->next;
    }
    
    // Fisher-Yates shuffle
    srand(time(NULL));
    for (int i = pl->size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Track *temp = tracks[i];
        tracks[i] = tracks[j];
        tracks[j] = temp;
    }
    
    // Rebuild circular links
    for (int i = 0; i < pl->size; i++) {
        tracks[i]->next = tracks[(i + 1) % pl->size];
        tracks[i]->prev = tracks[(i + pl->size - 1) % pl->size];
    }
    
    pl->head = tracks[0];
    if (pl->current) {
        // Keep current track reference but update its position
        pl->current = tracks[0]; // Simplified - could be more sophisticated
    }
    
    free(tracks);
    pl->shuffle_mode = 1;
}

void playlist_unshuffle(Playlist *pl) {
    if (!pl || !pl->shuffle_mode) return;
    
    // This is simplified - in a real implementation you'd want to store original order
    // For now, we'll just set shuffle mode off
    pl->shuffle_mode = 0;
}

int playlist_get_shuffle_mode(Playlist *pl) {
    return pl ? pl->shuffle_mode : 0;
}

void playlist_toggle_shuffle(Playlist *pl) {
    if (!pl) return;
    
    if (pl->shuffle_mode) {
        playlist_unshuffle(pl);
    } else {
        playlist_shuffle(pl);
    }
}

// Find track by name (case-insensitive partial match)
Track* playlist_find_by_name(Playlist *pl, const char *name) {
    if (!pl || !pl->head || !name) return NULL;
    
    Track *node = pl->head;
    do {
        if (my_strcasestr(node->name, name) != NULL) {
            return node;
        }
        node = node->next;
    } while (node != pl->head);
    
    return NULL;
}

void playlist_clear(Playlist *pl) {
    if (!pl) return;
    playlist_destroy(pl);
    // Note: This function needs to be rethought as it destroys the object
    // For now, it's better to create a new playlist
}