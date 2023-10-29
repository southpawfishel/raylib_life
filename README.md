# Raylib Game of Life

Simple implementation of Conway's Game of Life built as a quick way for me to get re-acclimated to C++ for a new job.

Performance is pretty bad for a big board (the size of a Steam Deck screen) but it is nice and simple to understand.

See the `threaded` branch for a version that splits the work across threads and has some drawing optimizations that can reach 120fps on a Steam Deck sized game world.
