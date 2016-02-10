#include <syscall.h>
#include <stdio.h>

int main (int argc, char *argv[]) {
    int file;
    int result;
    result = create("myfile", 100);
    file = open("myfile");
    if (file == -1) {
        printf("Couldn't open file :(\n");
        return 1;
    }
    char buffer[100] = {'a', 'b', 'c', '\0'};
    int written = 0;
    written = write(file, buffer, 100);
    printf("Number written: %d\n", written);
    return 0;
}
