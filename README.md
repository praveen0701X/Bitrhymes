# Bitrhymes

A terminal-based media player written in C, built around core data
structures: doubly linked lists, queues, stacks, and hash tables.
Created as a DSA project to apply data structures to a real system.

## Features
- Play, pause, and skip tracks
- Playlist navigation (forward/backward with linked list)
- Queue-based playback order
- Undo last action (stack)
- Fast song lookup by title (hash table, O(1) average)

## Data structures used

| Structure | Used for |
|-----------|----------|
| Doubly linked list | Playlist — navigate prev/next tracks |
| Queue | Playback order management |
| Stack | Undo history |
| Hash table | O(1) song search by name |

## Build and run

**Requirements:** GCC (any version)

**Linux / macOS**
```bash
gcc -o media_player main.c player.c playlist.c queue.c \
    stack.c hashtable.c ui.c
./media_player
```

**Windows**
```cmd
gcc -o media_player.exe main.c player.c playlist.c queue.c ^
    stack.c hashtable.c ui.c
media_player.exe
```

## Project structure
```
Bitrhymes/
├── main.c          — Entry point
├── player.c/h      — Playback logic
├── playlist.c/h    — Doubly linked list playlist
├── queue.c/h       — Queue for playback order
├── stack.c/h       — Stack for undo
├── hashtable.c/h   — Hash table for song search
└── ui.c/h          — Terminal UI
```

## License
MIT © praveen0701X
