int f(int x);
int main(void) {
    int a = 1, b = 2;
    a += b * 3;
    return a > 0 ? f(a) : (a = 0, b);
}
