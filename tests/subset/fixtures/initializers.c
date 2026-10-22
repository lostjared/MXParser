struct Pair {
    int x;
    int y;
};
struct Pair points[] = {{1, 2},
                        {
                            3,
                        }};
char label[] = "ok";
int main(void) {
    int values[3] = {
        1,
        2,
    };
    return points[1].x + values[2] + label[0];
}
