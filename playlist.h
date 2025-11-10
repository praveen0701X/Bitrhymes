#ifndef PLAYLIST_H
#define PLAYLIST_H

// Remove hashtable.h include to avoid circular dependency

typedef struct Track {
    char *audio_path;
    char *video_path;
    char *name;  // Track name for display and searching
    struct Track *next;
    struct Track *prev;
} Track;

typedef struct Playlist {
    Track *head;
    Track *current;
    int size;
    int shuffle_mode;
} Playlist;

Playlist* playlist_create();
Track* playlist_add(Playlist *pl, const char *audio, const char *video);
Track* playlist_next(Track *current);
Track* playlist_prev(Track *current);
void playlist_destroy(Playlist *pl);

// New functions for enhanced functionality
Track* playlist_get_track(Playlist *pl, int index);
void playlist_shuffle(Playlist *pl);
void playlist_unshuffle(Playlist *pl);
int playlist_get_shuffle_mode(Playlist *pl);
void playlist_toggle_shuffle(Playlist *pl);
Track* playlist_find_by_name(Playlist *pl, const char *name);
void playlist_clear(Playlist *pl);

#endif