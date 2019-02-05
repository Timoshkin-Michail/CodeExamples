#include "mpi.h"
#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>


struct Matrix {
    double *Data;
    int R, C;
    Matrix() {
        R = 0;
        C = 0;
        Data = NULL;
    }
    Matrix(int rows, int columns) {
        this->R = rows;
        this->C = columns;
        Data = new double [rows * columns];
        for (int i = 0; i < rows * columns; i++) {
            Data[i] = 0;
        }
    }
    Matrix(const Matrix & matrix) {
        R = matrix.R;
        C = matrix.C;
        Data = new double [R * C];
        for (int i = 0; i < R * C; i++) {
            Data[i] = matrix.Data[i];
        }
    }
    Matrix & operator=(const Matrix & matrix) {
        delete [] Data;
        R = matrix.R;
        C = matrix.C;
        Data = new double [R * C];
        for (int i = 0; i < R * C; i++) {
            Data[i] = matrix.Data[i];
        }
        return *this;
    }
    double * operator[](int index) {
        return Data + index * C;
    }
    ~Matrix() {
        delete [] Data;
    }
};


void Read(Matrix &matrix, MPI_Comm &square,const int *cube_dims,const int *proc_coordinates,
          char *filename, int &m, int &n) {
    MPI_File file;
    MPI_File_open(square, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    char type;
    MPI_File_read_all(file, &type, 1, MPI_CHAR, NULL);
    MPI_File_read_all(file, &m, 1, MPI_INT, NULL);
    MPI_File_read_all(file, &n, 1, MPI_INT, NULL);
    int block_m, block_n;
    block_m =  m / cube_dims[0] ;
    MPI_Offset offset = sizeof(type) + sizeof(m) + sizeof(n) +
                        (proc_coordinates[0] * block_m) * n * sizeof(double);
    MPI_Datatype filetype;
    int array_size = n;
    block_n = n / cube_dims[1];
    int start_array = proc_coordinates[1] * block_n;
    MPI_Type_create_subarray(1, &array_size, &block_n, &start_array, MPI_ORDER_C, MPI_DOUBLE, &filetype);
    MPI_Type_commit(&filetype);
    MPI_File_set_view(file, offset, MPI_DOUBLE, filetype, "native", MPI_INFO_NULL);
    matrix = Matrix(block_m, block_n);
    MPI_File_read_all(file, matrix[0], block_m * block_n, MPI_DOUBLE, MPI_STATUS_IGNORE);
    MPI_Type_free(&filetype);
    MPI_File_close(&file);
}

void Write(Matrix &matrix, MPI_Comm &square, const int *cube_dims, const int *cube_coords,
           char *filename, int m, int n) {
    MPI_File file;
    MPI_File_open(square, filename, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &file);
    char type = 'd';
    if (cube_coords[0] == 0 && cube_coords[1] == 0) {
        MPI_File_write(file, &type, 1, MPI_CHAR, MPI_STATUS_IGNORE);
        MPI_File_write(file, &m, 1, MPI_INT, MPI_STATUS_IGNORE);
        MPI_File_write(file, &n, 1, MPI_INT, MPI_STATUS_IGNORE);
    }
    MPI_Offset offset = sizeof(type) + sizeof(m) + sizeof(n) +
                        (cube_coords[0] * m / cube_dims[0]) * n * sizeof(double);
    MPI_Datatype filetype;
    int array_size = n;
    int start_array = cube_coords[1] * n / cube_dims[1];
    int block_m = matrix.R;
    int block_n = matrix.C;
    MPI_Type_create_subarray(1, &array_size, &block_n, &start_array, MPI_ORDER_C,
                             MPI_DOUBLE, &filetype);
    MPI_Type_commit(&filetype);
    MPI_File_set_view(file, offset, MPI_DOUBLE, filetype, "native", MPI_INFO_NULL);
    MPI_File_write_all(file, matrix[0], block_m * block_n, MPI_DOUBLE, MPI_STATUS_IGNORE);
    MPI_Type_free(&filetype);
    MPI_File_close(&file);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm cube, square, line_i, line_j, line_k;
    
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    int cube_rank;
    int cube_dims[] = { 0, 0, 0 };
    int cube_periods[] = { 0, 0, 0 };
    int proc_coordinates[] = { 0, 0, 0 };
    
    MPI_Dims_create(size, 3, cube_dims);
    MPI_Cart_create(MPI_COMM_WORLD, 3, cube_dims, cube_periods, 0, &cube);
    MPI_Comm_rank(cube, &cube_rank);
    
    MPI_Cart_coords(cube, cube_rank, 3, proc_coordinates);
    int square_dims[] = { 1, 1, 0 };
    MPI_Cart_sub(cube, square_dims, &square);
    int line_dims[] = { 1, 0, 0 };
    MPI_Cart_sub(cube, line_dims, &line_i);
    line_dims[0] = 0;
    line_dims[1] = 1;
    MPI_Cart_sub(cube, line_dims, &line_j);
    line_dims[1] = 0;
    line_dims[2] = 1;
    MPI_Cart_sub(cube, line_dims, &line_k);
    Matrix A, B, C;
    int C_m = 0, C_n = 0;

    double time, all_time, max_time, io_time, i_time, o_time;

    i_time = MPI_Wtime();
    MPI_Barrier(cube);
    //load A matrix
    if (proc_coordinates[2] == 0) { //bottom
        int m, n;
        Read(A, square, cube_dims, proc_coordinates, argv[1], m, n);
        C_m = m;
        if (proc_coordinates[1] != 0) { //(i,j,0), j = 1 .. cube_dims[1]
            int dest_coord = proc_coordinates[1];
            int dest_rank;
            MPI_Cart_rank(line_k, &dest_coord, &dest_rank);
            int block_sizes[] = { A.R, A.C };
            MPI_Send(block_sizes, 2, MPI_INT, dest_rank, 2, line_k);
            MPI_Send(A[0], A.R * A.C, MPI_DOUBLE, dest_rank, 1, line_k);// sending to (i,j,j)
            A.C = n / cube_dims[1];
            dest_coord = 0;
            MPI_Cart_rank(line_j, &dest_coord, &dest_rank);
            MPI_Bcast(A[0], A.R * A.C, MPI_DOUBLE, dest_rank, line_j);// receiving from (i,0,0)
        } else { // (i,0,0)
            int root_rank;
            MPI_Comm_rank(line_j, &root_rank);
            MPI_Bcast(A[0], A.R * A.C, MPI_DOUBLE, root_rank, line_j);// sending to (i,j,0)
        }
    } else if (proc_coordinates[1] == proc_coordinates[2]) {// (i,j,j)
        int block_sizes[2];
        int source_coord = 0;
        int source_rank;
        MPI_Cart_rank(line_k, &source_coord, &source_rank);
        MPI_Recv(block_sizes, 2, MPI_INT, source_rank, 2, line_k, MPI_STATUS_IGNORE);
        A = Matrix(block_sizes[0], block_sizes[1]);
        MPI_Recv(A[0], A.R * A.C, MPI_DOUBLE, source_rank, 1, line_k, MPI_STATUS_IGNORE);//receiving from (i,j,0)
        MPI_Comm_rank(line_j, &source_rank);
        MPI_Bcast(block_sizes, 2, MPI_INT, source_rank, line_j);//sending to (i,j_,j)
        MPI_Bcast(A[0], A.R * A.C, MPI_DOUBLE, source_rank, line_j);
    } else {
        int block_sizes[2];
        int root_coord = proc_coordinates[2];
        int root_rank;
        MPI_Cart_rank(line_j, &root_coord, &root_rank);
        MPI_Bcast(block_sizes, 2, MPI_INT, root_rank, line_j);// receiving from (i,j,j)
        A = Matrix(block_sizes[0], block_sizes[1]);
        MPI_Bcast(A[0], A.R * A.C, MPI_DOUBLE, root_rank, line_j);
    }

    //load B matrix
    if (proc_coordinates[2] == 0) { //bottom
        int m, n;
        Read(B, square, cube_dims, proc_coordinates, argv[2], m, n);
        C_n = n;
        if (proc_coordinates[0] != 0) { // (i,j,0), i = 1 .. cube_dims
            int dest_coord = proc_coordinates[0];
            int dest_rank;
            MPI_Cart_rank(line_k, &dest_coord, &dest_rank);
            int block_sizes[] = { B.R, B.C };
            MPI_Send(block_sizes, 2, MPI_INT, dest_rank, 2, line_k);
            MPI_Send(B[0], B.R * B.C, MPI_DOUBLE, dest_rank, 1, line_k);// sending to (i,j,i)
            B.R = m / cube_dims[0];
            dest_coord = 0;
            MPI_Cart_rank(line_i, &dest_coord, &dest_rank);
            MPI_Bcast(B[0], B.R * B.C, MPI_DOUBLE, dest_rank, line_i);// receiving from (0,j,0)
        } else { // (0,j,0)
            int root_rank;
            MPI_Comm_rank(line_i, &root_rank);
            MPI_Bcast(B[0], B.R * B.C, MPI_DOUBLE, root_rank, line_i); // sending to (i,j,0)
        }
    } else if (proc_coordinates[0] == proc_coordinates[2]) {//(i,j,i)
        int block_sizes[2];
        int source_coord = 0;
        int source_rank;
        MPI_Cart_rank(line_k, &source_coord, &source_rank);
        MPI_Recv(block_sizes, 2, MPI_INT, source_rank, 2, line_k, MPI_STATUS_IGNORE);
        B = Matrix(block_sizes[0], block_sizes[1]);
        MPI_Recv(B[0], B.R * B.C, MPI_DOUBLE, source_rank, 1, line_k, MPI_STATUS_IGNORE);//receiving from (i,j,0)
        MPI_Comm_rank(line_i, &source_rank);
        MPI_Bcast(block_sizes, 2, MPI_INT, source_rank, line_i);
        MPI_Bcast(B[0], B.R * B.C, MPI_DOUBLE, source_rank, line_i);// sending to (i_,j,i)
    } else {
        int block_sizes[2];
        int root_coord = proc_coordinates[2];
        int root_rank;
        MPI_Cart_rank(line_i, &root_coord, &root_rank);
        MPI_Bcast(block_sizes, 2, MPI_INT, root_rank, line_i);
        B = Matrix(block_sizes[0], block_sizes[1]);
        MPI_Bcast(B[0], B.R * B.C, MPI_DOUBLE, root_rank, line_i); // receiving from (i,j,i)
    }
    i_time = MPI_Wtime()- i_time;


    if (A.C != B.R) {
        std::cout << "Wrong block sizes on ( "<< proc_coordinates[0] << ", "<< proc_coordinates[1] <<", "<< proc_coordinates[2] << ") node"<< std::endl;
        exit(1);
    }


    MPI_Barrier(cube);
    time = MPI_Wtime();
    C = Matrix(A.R, B.C);
    for (int i = 0; i < A.R; ++i) {
        for (int k = 0; k < A.C; ++k) {
            for (int j = 0; j < B.C; ++j) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    time = MPI_Wtime() - time;
    MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, cube);
    MPI_Reduce(&time, &all_time, 1, MPI_DOUBLE, MPI_SUM, 0, cube);
    int root_coord = 0;
    int root_rank;
    MPI_Cart_rank(line_k, &root_coord, &root_rank);
    Matrix C_sum(C.R, C.C);
    MPI_Reduce(C[0], C_sum[0], C.R * C.C, MPI_DOUBLE, MPI_SUM, root_rank, line_k);

    o_time = MPI_Wtime();
    if (proc_coordinates[2] == 0) {
        MPI_Barrier(square);
        Write(C_sum, square, cube_dims, proc_coordinates, argv[3], C_m, C_n);
    }
    o_time = MPI_Wtime() - o_time;

    time = i_time + o_time;
    MPI_Reduce(&time, &io_time, 1, MPI_DOUBLE, MPI_MAX, 0, cube);
    if (!cube_rank) {
        std::cout << "Max time: " << max_time << std::endl;
        std::cout << "All time: " << all_time << std::endl;
        std::cout << "I/O time: " << io_time << std::endl;
    }
    MPI_Finalize();
}
