double half(double);
double half(double x) { return x / 2.0; }
int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}
int main(void) {
    int sum = 0;
    for (int i = 0; i < 4; ++i) {
        if (i == 1)
            continue;
        sum += i;
    }
    while (sum > 3) {
        --sum;
        if (sum == 4)
            break;
    }
    {
        int sum = 2;
        sum++;
    };
    return factorial(3) + (int)half(sum);
}
