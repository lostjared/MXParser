int main(void) {
    {
        typedef int Local;
        Local n = 1;
    }
    Local n = 2;
    return n;
}
