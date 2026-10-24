int classify(int n) {
    int total = 0;
    do {
        --n;
        switch (n) {
        case 0:
            break;
        case 1:
        case 2:
            total += 2;
            continue;
        case 3:
            total += 3;
        default:
            total += 1;
        }
    } while (n > 0);
    return total;
}

int main(void) { return classify(5); }
