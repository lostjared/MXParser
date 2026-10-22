int main(void) {
    int n = 0;
    do {
        ++n;
        switch (n) {
        case 1:
            continue;
        case 2:
            n += 1;
        default:
            break;
        }
    } while (n < 4);
    return n;
}
