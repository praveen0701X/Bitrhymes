#ifndef HASHTABLE_H
#define HASHTABLE_H

// Forward declaration instead of including playlist.h
typedef struct Track Track;

#define TABLE_SIZE 101

typedef struct HashNode {
    char *key;
    Track *track;
    struct HashNode *next;
} HashNode;

typedef struct HashTable {
    HashNode *buckets[TABLE_SIZE];
} HashTable;

HashTable *hashtable_create();
void hashtable_insert(HashTable *ht, const char *key, Track *track);
Track *hashtable_search(HashTable *ht, const char *key);
void hashtable_destroy(HashTable *ht);

#endif