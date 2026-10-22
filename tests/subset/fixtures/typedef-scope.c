typedef int Count;
int main(void) {
    Count n = 2;
    {
        int Count = 3;
        n += Count;
    }
    return (Count)n;
}
