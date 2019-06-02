#include "library.h"
#include <omp.h>
#include <mpi.h>
#include <complex>
#include <iostream>
#include <cstdlib>
#include <cstdio>

typedef std::complex<double> complexed;

void OneQubitEvolution(unsigned k, const complexed *in, complexed* out, complexed u[2][2], size_t rank, size_t process_vector_size) {
    auto b = new complexed[process_vector_size];
    unsigned mask_k = 1u << k;
    unsigned index = rank * process_vector_size;
    unsigned k_bit = (index & mask_k) >> k;
    unsigned dest = (index ^ mask_k) / process_vector_size;
    if (dest != rank) {
        MPI_Sendrecv(in, process_vector_size, MPI_C_DOUBLE_COMPLEX, dest, k_bit,
                b, process_vector_size, MPI_C_DOUBLE_COMPLEX, dest, 1-k_bit, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        #pragma omp parallel for shared(in, b)
        for (unsigned i = 0; i < process_vector_size; i++) {
            unsigned real_index = i + rank * process_vector_size;
            b[i] = in[(real_index ^ mask_k) % process_vector_size];
        }
    }
    #pragma omp parallel for shared(in, b, out)
    for (unsigned i = 0; i < process_vector_size; i++) {
        unsigned real_index = i + rank * process_vector_size;
        unsigned matrix_index = (real_index & mask_k) >> k;
        out[i] = u[matrix_index][matrix_index] * in[i] + u[matrix_index][1 - matrix_index] * b[i];
    }
    delete [] b;
}

void TwoQubitsEvolution(unsigned k, unsigned l, const complexed* in, complexed* out, complexed u[4][4], size_t rank, size_t process_vector_size) {
    unsigned mask_l = (unsigned)1 << l;
    unsigned mask_k = (unsigned)1 << k;
    unsigned rank_index = rank * process_vector_size;
    unsigned rank00 = (rank_index & ~mask_k & ~mask_l) / process_vector_size;
    unsigned rank01 = ((rank_index & ~mask_k) | mask_l) / process_vector_size;
    unsigned rank10 = ((rank_index | mask_k) & ~mask_l) / process_vector_size;
    unsigned rank11 = (rank_index | mask_k | mask_l) / process_vector_size;
    int post_list[] = {rank00, rank01, rank10, rank11};
    int type;
    if (rank == rank00) {
        type = 0;
    } else if (rank == rank01) {
        type = 1;
    } else if (rank == rank10) {
        type = 2;
    } else if (rank == rank11) {
        type = 3;
    } else {
        type = -1;
    }
    post_list[type] = -1;
    complexed* tmp[4];
    int del[4] = {0, 1, 2, 3};

    if (rank00 == rank01 && rank01 == rank10 && rank10 == rank11) {
        post_list[0] = post_list[1] = post_list[2] = post_list[3] = -1;
        tmp[0] = new complexed[process_vector_size];
        tmp[1] = tmp[2] = tmp[3] = tmp[0];
        del[1] = del[2] = del[3] = -1;
    } else if (rank00 == rank01 && rank10 == rank11) {
        post_list[1] = post_list[3] = -1;
        tmp[0] = new complexed[process_vector_size];
        tmp[2] = new complexed[process_vector_size];
        tmp[1] = tmp[0];
        tmp[3] = tmp[2];
        del[1] = del[3] = -1;
    } else if (rank00 == rank10 && rank01 == rank11) {
        post_list[2] = post_list[3] = -1;
        tmp[0] = new complexed[process_vector_size];
        tmp[1] = new complexed[process_vector_size];
        tmp[2] = tmp[0];
        tmp[3] = tmp[1];
        del[2] = del[3] = -1;
    } else {
        tmp[0] = new complexed[process_vector_size];
        tmp[1] = new complexed[process_vector_size];
        tmp[2] = new complexed[process_vector_size];
        tmp[3] = new complexed[process_vector_size];
    }
    for (unsigned j = 0; j < process_vector_size; j++) {
        tmp[type][j] = in[j];
    }
    for (unsigned i = 0; i < 4; i++) {
        if (post_list[i] >= 0) {
            MPI_Sendrecv(tmp[type], process_vector_size, MPI_C_DOUBLE_COMPLEX, post_list[i], type,
                tmp[i], process_vector_size, MPI_C_DOUBLE_COMPLEX, post_list[i], i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    #pragma omp parallel for shared(out, tmp)
    for (unsigned i = 0; i < process_vector_size; ++i) {
        unsigned real_index = i + rank * process_vector_size;
        unsigned index00 = (real_index & ~mask_k & ~mask_l) % process_vector_size;
        unsigned index01 = ((real_index & ~mask_k) | mask_l) % process_vector_size;
        unsigned index10 = ((real_index | mask_k) & ~mask_l) % process_vector_size;
        unsigned index11 = (real_index | mask_k | mask_l) % process_vector_size;
        unsigned cubit_k = (real_index & mask_k) >> k;
        unsigned cubit_l = (real_index & mask_l) >> l;
        unsigned index_m = (cubit_k << 1u) + cubit_l;
        out[i] = u[index_m][0b00] * tmp[0][index00] +
                 u[index_m][0b01] * tmp[1][index01] +
                 u[index_m][0b10] * tmp[2][index10] +
                 u[index_m][0b11] * tmp[3][index11];
    }
    for (int i = 0; i < 4; i++) {
        if (del[i] >= 0) {
            delete [] tmp[i];
        }
    }
}

void Hadamar(unsigned k, const complexed *in, complexed* out, size_t rank, size_t process_vector_size) {
    complexed u[2][2];
    complexed el = complexed(pow(2, -0.5), 0);
    u[0][0] = el; u[0][1] = el;
    u[1][0] = el; u[1][1] = -el;
    OneQubitEvolution(k, in, out, u, rank, process_vector_size);
}

void nHadamar(unsigned n, const complexed *in, complexed *out, size_t rank, size_t process_vector_size) {
    auto tmp = new complexed[process_vector_size];
    for (int i = 0; i < process_vector_size; i++) {
        tmp[i] = in[i];
    }
    auto tmp2 = out;
    for (unsigned  int i = 0; i < n; i++) {
        Hadamar(i, tmp, tmp2, rank, process_vector_size);
        auto t = tmp;
        tmp = tmp2;
        tmp2 = t;
    }
    for (int i = 0; i < process_vector_size; i++) {
        out[i] = tmp[i];
    }
}

void RW(unsigned k, double phi, const complexed *in, complexed* out, size_t rank, size_t process_vector_size) {
    complexed u[2][2];
    u[0][0] = 1; u[0][1] = 0;
    u[1][0] = 0; u[1][1] = exp(complexed(0, phi));
    OneQubitEvolution(k, in, out, u, rank, process_vector_size);
}

void NOT(unsigned k, const complexed *in, complexed* out, size_t rank, size_t process_vector_size) {
    complexed u[2][2];
    u[0][0] = 0; u[0][1] = 1;
    u[1][0] = 1; u[1][1] = 0;
    OneQubitEvolution(k, in, out, u, rank, process_vector_size);
}

void CNOT(unsigned k, unsigned l, const complexed *in, complexed* out, size_t rank, size_t process_vector_size) {
    complexed u[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            u[i][j] = (i == j)? 1: 0;
        }
    }
    u[2][2] = u[3][3] = 0;
    u[2][3] = u[3][2] = 1;
    TwoQubitsEvolution(k, l, in, out, u, rank, process_vector_size);
}

void CRW(unsigned k, unsigned l, double phi, const complexed *in, complexed* out, size_t rank, size_t process_vector_size) {
    complexed u[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            u[i][j] = (i == j)? 1: 0;
        }
    }
    u[3][3] = exp(complexed(0, phi));
    TwoQubitsEvolution(k, l, in, out, u, rank, process_vector_size);
}
