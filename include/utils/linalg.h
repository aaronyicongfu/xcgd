#ifndef XCGD_LINALG_H
#define XCGD_LINALG_H

#include <vector>

#include "sparse_utils/lapack.h"
#include "utils/misc.h"

/**
 * @brief Solve Ax = b for x
 *
 * @param n number of rows/columns
 * @param A matrix stored column by column
 * @param b right hand side, stores solution on exit
 * @return info = 0 successful exit, otherwise fail
 */
template <typename T>
int direct_solve(int n, T A[], T b[]) {
  std::vector<int> ipiv(n);
  int info = -1;
  SparseUtils::LAPACKgetrf(&n, &n, A, &n, ipiv.data(), &info);
  int nrhs = 1;
  SparseUtils::LAPACKgetrs("N", &n, &nrhs, A, &n, ipiv.data(), b, &n, &info);
  return info;
}

/**
 * @brief compute inv(A)
 *
 * @param n number of rows/columns
 * @param A matrix stored column by column, stores inv(A) on exit
 * @return info = 0 successful exit, otherwise fail
 */
template <typename T>
int direct_inverse(int n, T A[]) {
  std::vector<int> ipiv(n);
  int info = -1;
  SparseUtils::LAPACKgetrf(&n, &n, A, &n, ipiv.data(), &info);
  std::vector<T> work(n);
  // Get optimal lwork
  int lwork = -1;
  SparseUtils::LAPACKgetri(&n, A, &n, ipiv.data(), work.data(), &lwork, &info);
  lwork = int(freal(work[0]));
  std::printf("optimal lwork for n = %d: %d\n", n, lwork);
  SparseUtils::LAPACKgetri(&n, A, &n, ipiv.data(), work.data(), &lwork, &info);
  return info;
}

#endif  // XCGD_LINALG_H