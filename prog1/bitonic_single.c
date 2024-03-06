#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bitonic_merge(int *arr, int low_index, int count, int direction) { // NOLINT(*-no-recursion)
    if (count <= 1) return;
    int half = count / 2;
    // move the numbers to the correct half
    for (int i = low_index; i < low_index + half; i++) {
        if (direction == (arr[i] > arr[i + half])) {
            int temp = arr[i];
            arr[i] = arr[i + half];
            arr[i + half] = temp;
        }
    }
    // merge left half
    bitonic_merge(arr, low_index, half, direction);
    // merge right half
    bitonic_merge(arr, low_index + half, half, direction);
}

void bitonic_sort(int *arr, int low_index, int count, int direction) { // NOLINT(*-no-recursion)
    if (count <= 1) return;
    int half = count / 2;
    // sort left half in ascending order
    bitonic_sort(arr, low_index, half, 1);
    // sort right half in descending order
    bitonic_sort(arr, low_index + half, half, 0);
    // merge the two halves
    bitonic_merge(arr, low_index, count, direction);
}

int main(int argc, char *argv[]) {
    FILE *file;

    for (int i = 1; i < argc; i++) {
        file = fopen(argv[i], "rb");
        if (file == NULL) {
            printf("Error opening file %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }

        // read the size of the array
        int size;
        if (fread(&size, sizeof(int), 1, file) != 1) {
            printf("Error: could not read the size of the array\n");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // size must be power of 2
        if ((size & (size - 1)) != 0) {
            printf("Error: The size of the array must be a power of 2\n");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        printf("Array size: %d\n", size);

        // allocate memory for the array
        int *arr = (int *) malloc(size * sizeof(int));
        if (arr == NULL) {
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // load array into memory
        int num, j = 0;
        while (fread(&num, sizeof(int), 1, file) == 1 && j < size) {
            arr[j++] = num;
        }

        // bitonic sort on the array in descending order
        bitonic_sort(arr, 0, size, 0);

        // print results
        if (size <= 32) {
            printf("Sorted array:\n");
            for (int k = 0; k < size; k++) {
                printf("%d: %d\n", k, arr[k]);
            }
        } else {
            printf("First 16:\n");
            for (int k = 0; k < 16; k++) {
                printf("%d ", arr[k]);
            }
            printf("\n");
            printf("Last 16:\n");
            for (int k = size - 16; k < size; k++) {
                printf("%d ", arr[k]);
            }
            printf("\n");
        }
        free(arr);
        fclose(file);
    }
    return 0;
}