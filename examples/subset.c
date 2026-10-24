typedef unsigned long Count;
enum Kind { SKIP, VALUE, STOP };
struct Item;
struct Item {
    enum Kind kind;
    int samples[2];
    struct Item *next;
};
static struct Item items[] = {{VALUE, {2, 3}, 0}, {SKIP, {0}, 0}, {STOP, {4, 1}, 0}};
static const char caption[] = "C\x39"
                              "9";

struct Result {
    int total;
    Count visited;
};
struct Result summarize(const struct Item *restrict data, Count count);
struct Result summarize(const struct Item *restrict data, Count count) {
    struct Result result = {0, 0};
    for (Count i = 0; i < count; ++i) {
        const struct Item *entry = &data[i];
        switch (entry->kind) {
        case SKIP:
            continue;
        case VALUE:
        case STOP:
            result.total += entry->samples[0] + entry->samples[1];
            break;
        default:
            return result;
        }
        ++result.visited;
        if (entry->kind == STOP)
            break;
    }
    return result;
}
int main(void) {
    struct Result result = {0, 0};
    result = summarize(items, sizeof items / sizeof items[0]);
    int n = 2;
    do {
        --n;
    } while (n > 0);
    while (n < 1) {
        ++n;
    }
    if (result.visited > 0)
        if (caption[0] == 'C')
            result.total += n;
        else
            result.total = 0;
    return result.total > 0 ? (int)result.visited : (n = 0, n);
}
