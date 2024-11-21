#ifndef XCGD_LINALG_H
#define XCGD_LINALG_H

#include <stdexcept>
#include <vector>

#include "sparse_utils/lapack_helpers.h"
#include "sparse_utils/sparse_matrix.h"
#include "utils/misc.h"

template <typename T>
double matrix_norm(char norm, int m, int n, T A[]) {
  if (!(norm == 'M' or norm == 'm' or norm == '1' or norm == 'O' or
        norm == 'o' or norm == 'I' or norm == 'i' or norm == 'F' or
        norm == 'f' or norm == 'E' or norm == 'e')) {
    char msg[256];
    std::snprintf(msg, 256, "not supported norm: %c", norm);
    throw std::runtime_error(msg);
  }
  return SparseUtils::LAPACKlange(norm, m, n, A, m);
}

/**
 * @brief Solve Ax = b for x
 *
 * @param n number of rows/columns
 * @param A matrix stored column by column
 * @param b right hand side, stores solution on exit
 */
template <typename T>
void direct_solve(int n, T A[], T b[]) {
  std::vector<int> ipiv(n);
  int info = -1;
  SparseUtils::LAPACKgetrf(n, n, A, n, ipiv.data(), &info);
  int nrhs = 1;
  SparseUtils::LAPACKgetrs('N', n, nrhs, A, n, ipiv.data(), b, n, &info);
  if (info != 0) {
    char msg[256];
    std::snprintf(msg, 256, "direct inverse failed with exit code %d", info);
    throw std::runtime_error(msg);
  }
}

/**
 * @brief compute inv(A)
 *
 * @param n [in] number of rows/columns
 * @param A [in, out] matrix stored column by column, stores inv(A) on exit
 * @param rcond [out] reciprocal condition number
 * @param norm [in] specifies which norm to use for computing condition number
 * @return info = 0 successful exit, otherwise fail
 */
template <typename T>
void direct_inverse(int n, T A[], double *rcond = nullptr, char norm = '1') {
  double Anorm = -1.0;
  if (rcond) {
    if (!(norm == '1' or norm == 'O' or norm == 'I')) {
      char msg[256];
      std::snprintf(msg, 256, "not supported norm: %c", norm);
      throw std::runtime_error(msg);
    }
    Anorm = matrix_norm(norm, n, n, A);
  }

  std::vector<int> ipiv(n);
  int info = -1;
  SparseUtils::LAPACKgetrf(n, n, A, n, ipiv.data(), &info);

  // Optionally compute the reciprocal of the condition number
  if (rcond) {
    assert(Anorm > 0.0);
    SparseUtils::LAPACKgecon(norm, n, A, n, Anorm, rcond, &info);
    if (info != 0) {
      char msg[256];
      std::snprintf(
          msg, 256,
          "direct_inverse() failed to evaluate the rcond with exit code %d",
          info);
      throw std::runtime_error(msg);
    }
  }

  // First we let LAPACK determine the optimal lwork value
  std::vector<T> work(1);
  int lwork = -1;
  SparseUtils::LAPACKgetri(n, A, n, ipiv.data(), work.data(), lwork, &info);
  if (info != 0) {
    char msg[256];
    std::snprintf(msg, 256,
                  "direct_inverse() failed to determine the optimal lwork with "
                  "exit code %d",
                  info);
    throw std::runtime_error(msg);
  }
  lwork = int(freal(work[0]));
  if (lwork < 1) {
    lwork = 1;
  }

  // Compute the inverse of the matrix using LU factorization
  work.resize(lwork);
  SparseUtils::LAPACKgetri(n, A, n, ipiv.data(), work.data(), lwork, &info);
  if (info != 0) {
    char msg[256];
    std::snprintf(msg, 256, "direct_inverse() failed with exit code %d", info);
    throw std::runtime_error(msg);
  }
}

// A squared specialization of the general block sparse row matrix tailored for
// the Galerkin method applications (i.e. finite element method and Galerkin
// difference method)
template <typename T, int M>
class GalerkinBSRMat final : public SparseUtils::BSRMat<T, M, M> {
 public:
  GalerkinBSRMat(int nbrows, int nnz, const int *rowp_, const int *cols_,
                 const T *vals_ = nullptr)
      : SparseUtils::BSRMat<T, M, M>(nbrows, nbrows, nnz, rowp_, cols_, vals_) {
  }

  template <class Mesh>
  void add_block_values(int elem, const Mesh &mesh, T mat[]) {
    constexpr int N = M;
    int nodes[Mesh::max_nnodes_per_element];
    int nnodes = mesh.get_elem_dof_nodes(elem, nodes);

    for (int ii = 0; ii < nnodes; ii++) {
      int block_row = nodes[ii];

      for (int jj = 0; jj < nnodes; jj++) {
        int block_col = nodes[jj];

        int jp = this->find_value_index(block_row, block_col);
        if (jp != SparseUtils::NO_INDEX) {
          for (int local_row = 0; local_row < M; local_row++) {
            for (int local_col = 0; local_col < N; local_col++) {
              this->vals[M * N * jp + N * local_row + local_col] +=
                  mat[(M * ii + local_row) * Mesh::max_nnodes_per_element * N +
                      N * jj + local_col];
            }
          }
        }
      }
    }
  }

  template <int max_nnodes_per_element>
  void add_block_values(int nnodes, int *nodes, T mat[]) {
    constexpr int N = M;

    for (int ii = 0; ii < nnodes; ii++) {
      int block_row = nodes[ii];

      for (int jj = 0; jj < nnodes; jj++) {
        int block_col = nodes[jj];

        int jp = this->find_value_index(block_row, block_col);
        if (jp != SparseUtils::NO_INDEX) {
          for (int local_row = 0; local_row < M; local_row++) {
            for (int local_col = 0; local_col < N; local_col++) {
              this->vals[M * N * jp + N * local_row + local_col] +=
                  mat[(M * ii + local_row) * max_nnodes_per_element * N +
                      N * jj + local_col];
            }
          }
        }
      }
    }
  }
};

#endif  // XCGD_LINALG_H
