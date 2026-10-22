typedef unsigned long Count;
typedef int Row[4];

int *items[4];
int (*row)[4];
const int *pointer, count = 3;

int sum(const int values[], Count length);
int sum(const int values[], Count length) {
    int total = 0;
    for (Count i = 0; i < length; ++i)
        total += values[i];
    return total;
}

int (*select_row(Row *rows, Count index))[4] { return &rows[index]; }
