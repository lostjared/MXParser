struct Pair {
    int x;
};
int main(void) {
    struct Pair a = {1};
    struct Pair b = a;
    return b.x;
}
