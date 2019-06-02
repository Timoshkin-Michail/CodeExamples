#include <cstdio>
#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <complex>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <vector>
#include "QuantumGates.cpp"


typedef std::complex<double> complexd;

static int ROOT = 0;
static int THREADS;
static int PROCESSES;
static int RANK;
static unsigned PROCESS_VEC_SIZE;


complexed* read_vector(char* path) {
    MPI_File fin;
    MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDONLY, MPI_INFO_NULL, &fin);
    auto a = new complexed[ PROCESS_VEC_SIZE ];
    MPI_Offset offset = sizeof(unsigned) + sizeof(complexed) * RANK * PROCESS_VEC_SIZE;
    MPI_File_seek(fin, offset, MPI_SEEK_SET);
    MPI_File_read(fin, a, PROCESS_VEC_SIZE, MPI_C_DOUBLE_COMPLEX, MPI_STATUS_IGNORE);
    return a;
}

void print_vector(complexed* a, unsigned n, char* path) {
    MPI_File fout;
    unsigned size = 1u << n;
    MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &fout);
    MPI_Offset offset = sizeof(size) + sizeof(complexed) * RANK * PROCESS_VEC_SIZE;
    if (RANK == ROOT) {
        MPI_File_write(fout, &size, 1, MPI_UNSIGNED, MPI_STATUS_IGNORE);
    }
    MPI_File_seek(fout, offset, MPI_SEEK_SET);
    MPI_File_write(fout, a, PROCESS_VEC_SIZE, MPI_C_DOUBLE_COMPLEX, MPI_STATUS_IGNORE);
    MPI_File_close(&fout);
}

complexed* random_vector() {
    int root_seed = static_cast<int>(MPI_Wtime());
    MPI_Bcast(&root_seed, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    auto a = new complexed[PROCESS_VEC_SIZE];
    double sum = 0;
    double total_sum;
    #pragma omp parallel for reduction(+:sum)
    for (unsigned i = 0; i <  PROCESS_VEC_SIZE ; i++) {
        unsigned j = i + root_seed + THREADS * RANK;
        a[i].real(static_cast<double>(rand_r(&j)) / RAND_MAX);
        a[i].imag(static_cast<double>(rand_r(&j)) / RAND_MAX);
        sum += norm(a[i]);
    }
    MPI_Allreduce(&sum, &total_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    total_sum = sqrt(total_sum);
    for (unsigned i = 0; i < PROCESS_VEC_SIZE; i++) {
        a[i].real(a[i].real() / total_sum);
        a[i].imag(a[i].imag() / total_sum);
    }
    return a;
}

unsigned int reverseBits(unsigned int num, unsigned bits) {
    unsigned int reverse_num = 0;
    int i;
    for (i = 0; i < bits; i++) {
        if((num & (1 << i)))
            reverse_num |= 1 << ((bits - 1) - i);
    }
    return reverse_num;
}


int main(int argc, char **argv) {
    /**
     *  argv[1] - threads_num
     *  argv[2] - N
     *  argv[3] - input mode: f - from file, r - random vector
     *  argv[4] - if input from file, then file_path
     *  argv[5] - file path to write input
     *  argv[6] - output file path
     *
     */

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        perror("MPI not initialized");
        exit(1);
    }
    THREADS = atoi(argv[1]);
    omp_set_num_threads(THREADS);
    unsigned N = atoi(argv[2]);
    char mode = argv[3][0];
    MPI_Comm_size(MPI_COMM_WORLD, &PROCESSES);
    MPI_Comm_rank(MPI_COMM_WORLD, &RANK);
    PROCESS_VEC_SIZE = (1u << N) / PROCESSES;
    complexed* a;
    if (mode == 'r') {
        a = random_vector();
    } else if (mode == 'f') {
        a = read_vector(argv[4]);
    }
    print_vector(a, N, argv[argc-2]);
    auto b = new complexed[PROCESS_VEC_SIZE];
    double time = MPI_Wtime();
    for (int i = N-1; i >= 0; i--) {
        Hadamar(i, a, b, RANK, PROCESS_VEC_SIZE);
        complexd* swap = a;
        a = b;
        b = swap;
        int m = 2;
        for (int j = i - 1; j >= 0; j--) {
            double phi = 2 * M_PI / (1 << m);
            CRW(j, i, phi, a, b, RANK, PROCESS_VEC_SIZE);
            complexd* swap = a;
            a = b;
            b = swap;
            m++;
        }
    }
    unsigned transfer_size = 0;
    for (unsigned i = 0; i < PROCESS_VEC_SIZE; i++) {
        unsigned real_index = i + RANK * PROCESS_VEC_SIZE;
        unsigned reversed_index = reverseBits(real_index, N);
        unsigned dest = reversed_index / PROCESS_VEC_SIZE;
        if (dest != RANK) {
            transfer_size++;
        }
    }
    complexed* transfer_data;
    unsigned* swap_indexes;
    unsigned* destinations;
    if (transfer_size != 0) {
        transfer_data = new complexed[transfer_size];
        swap_indexes = new unsigned[transfer_size];
        destinations = new unsigned[transfer_size];
    }
    unsigned j = 0;
    auto swapped = new bool[PROCESS_VEC_SIZE];
    for (int i = 0; i < PROCESS_VEC_SIZE; i++) {
        swapped[i] = false;
    }
    for (unsigned i = 0; i < PROCESS_VEC_SIZE; i++) {
        if (!swapped[i]) {
            unsigned real_index = i + RANK * PROCESS_VEC_SIZE;
            unsigned reversed_index = reverseBits(real_index, N);
            unsigned dest = reversed_index / PROCESS_VEC_SIZE;
            if (dest == RANK) {
                complexed swap = a[i];
                a[i] = a[reversed_index % PROCESS_VEC_SIZE];
                a[reversed_index % PROCESS_VEC_SIZE] = swap;
                swapped[reversed_index % PROCESS_VEC_SIZE] = true;
            } else {
                transfer_data[j] = a[i];
                swap_indexes[j] = i;
                destinations[j++] = dest;
            }
            swapped[i] = true;
        }
    }
    if (j != transfer_size) {
        std::cout << "Error on 154" << std::endl;
        exit(1);
    }
    if (transfer_size != 0) {
        for (unsigned i = 0; i < transfer_size; i++) {
            unsigned real_index = swap_indexes[i] + RANK * PROCESS_VEC_SIZE;
            unsigned reversed_index = reverseBits(real_index, N);
            MPI_Send(&transfer_data[i], 1, MPI_C_DOUBLE_COMPLEX, destinations[i], reversed_index, MPI_COMM_WORLD);
        }
        for (unsigned i = 0; i < transfer_size; i++) {
            complexed response;
            unsigned real_index = swap_indexes[i] + RANK * PROCESS_VEC_SIZE;
            MPI_Recv(&response, 1, MPI_C_DOUBLE_COMPLEX, destinations[i], real_index, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            a[swap_indexes[i]] = response;
        }
    }

    time = MPI_Wtime() - time;

    if (RANK == ROOT) {
        std::cout << "Executing time: " << time << " sec" << std::endl;
    }

    print_vector(a, N, argv[argc-1]);

    MPI_Finalize();
    return 0;
}
