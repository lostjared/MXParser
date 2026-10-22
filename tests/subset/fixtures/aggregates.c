struct Node;
struct Node {
    int value;
    struct Node *next;
};
enum Kind {
    FIRST = 1,
    SECOND = FIRST + 1,
};
struct Node nodes[SECOND];
int main(void) {
    struct Node *p = &nodes[0];
    p->value = SECOND;
    return nodes[0].value;
}
