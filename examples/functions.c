double average(double a, double b);

double average(double a, double b) { return (a + b) / 2.0; }

int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

int main(void) {
    double mean = average(2.5, 5.0);
    return (int)(mean * 4.0) + factorial(3);
}
