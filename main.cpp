#include <iostream>
#include <fstream>
#include <error.h>
#include "mpi.h"
#include <vector>
#include <cmath>

using namespace std;

int main(int argc, char** argv) {
    long long first, last;
    first = strtoull(argv[1], 0, 0);
    last = strtoull(argv[2], 0, 0);
    if ( MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        perror("MPI not initialized");
    }
    MPI_File fout;
    MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fout);
    int size;
    MPI_Comm_size( MPI_COMM_WORLD ,  &size);
    vector<bool> prime((unsigned long long)sqrt(last), true);
    double time, all_time, max_time;

    time = MPI_Wtime();
    prime[0] = prime[1] = false;
    for (long long i = 2; i * i <= last; ++i) {
        if (prime[i]) {
            for (long long j = i * i; j * j <= last; j += i)
                prime[j] = false;
                break;
        }
    }
    int rank;
    unsigned count = 0;
    unsigned all_count;
    MPI_Comm_rank( MPI_COMM_WORLD ,  &rank);

    if (rank != 0) {
        long long beg = (last - (long long) sqrt(last)) / (size-1) * (rank-1) + (long long) sqrt(last);
        long long end;
        if (rank == size - 1 && (last - (long long) sqrt(last)) % (size-1) != 0) {
            end = last;
        } else {
            end = beg + (last - (long long) sqrt(last)) / (size-1);
        }
        for (long long i = beg; i < end; i++) {
            bool flag = true;
            for (long long j = 2; j*j < last; j++) {
                if (prime[j] && i % j == 0) {
                    flag = false;
                }
            }
            if (flag) {
                if (i > first) {
                    count++;
                    MPI_File_write(fout, &i, sizeof(i), MPI_CHAR, MPI_STATUS_IGNORE);
                }
            }
        }
    }
    time = MPI_Wtime() - time;

    if (rank == 0) {
        for (long long i = first; i*i < last; i++) {
            if (prime[i]) {
                MPI_File_write(fout,&i, sizeof(i), MPI_LONG_LONG, MPI_STATUS_IGNORE);
                count++;
            }
        }
    }

    MPI_Reduce(&count, &all_count, 1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time, &all_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        cout << "Primes: " << all_count << endl;
        cout << "Time: " << all_time << endl;
        cout << "Max Time per core: " << max_time << endl;
    }
    MPI_File_close(&fout);
    MPI_Finalize();
    return 0;
}
