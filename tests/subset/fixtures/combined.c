typedef struct Item {
    int value;
} Item;
enum { COUNT = 3 };
static Item items[COUNT] = {{1}, {2}, {3}};
int total(Item *p, int n);
int total(Item *p, int n) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        switch (p[i].value) {
        case 2:
            continue;
        default:
            sum += p[i].value;
        }
    }
    do {
        --n;
    } while (n > 0);
    return sum;
}
int main(void) { return total(items, COUNT); }
