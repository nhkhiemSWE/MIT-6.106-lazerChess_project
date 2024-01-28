// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff
#ifndef LOOKUP_H
#define LOOKUP_H

#define OPEN_BOOK_DEPTH 2

int lookup_sizes[OPEN_BOOK_DEPTH] = {1, 1, };

const char* lookup_table_depth_2[] = { "e1d2", "h7h6",
};
const char** lookup_tables[OPEN_BOOK_DEPTH] = {
lookup_table_depth_2,
};
#endif  // LOOKUP_H
