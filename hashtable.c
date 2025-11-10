// hashtable.c
#include "hashtable.h"
#include <stdlib.h>
#include <string.h>

// Simple hash function (djb2)
static unsigned int hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash % TABLE_SIZE;
}

HashTable *hashtable_create() {
    HashTable *ht = malloc(sizeof(HashTable));
    for (int i = 0; i < TABLE_SIZE; i++) ht->buckets[i] = NULL;
    return ht;
}

void hashtable_insert(HashTable *ht, const char *key, Track *track) {
    if (!ht || !key || !track) return;
    unsigned int idx = hash(key);
    HashNode *node = malloc(sizeof(HashNode));
    node->key = strdup(key);
    node->track = track;
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
}

Track *hashtable_search(HashTable *ht, const char *key) {
    if (!ht || !key) return NULL;
    unsigned int idx = hash(key);
    HashNode *cur = ht->buckets[idx];
    while (cur) {
        if (strcmp(cur->key, key) == 0) return cur->track;
        cur = cur->next;
    }
    return NULL;
}

void hashtable_destroy(HashTable *ht) {
    if (!ht) return;
    for (int i = 0; i < TABLE_SIZE; i++) {
        HashNode *cur = ht->buckets[i];
        while (cur) {
            HashNode *tmp = cur;
            cur = cur->next;
            free(tmp->key);
            free(tmp);
        }
    }
    free(ht);
}
