int main(void) {
    const char *name = "MXParser";
    int count = 3;
    double average = 2.5;
    int written = lsprintf("Hello from %s! count=%d, average=%.2f\n", name, count, average);
    lsprintf("The previous call wrote %d characters.\n", written);
    return 0;
}
