#ifndef XCGD_COMMONS_H
#define XCGD_COMMONS_H

#include "a2dcore.h"

/**
 * @brief The abstract base class for a Galerkin (finite element or Galerkin
 * difference) mesh
 */
template <typename T, int spatial_dim_, int nodes_per_element_>
class MeshBase {
 public:
  static constexpr int spatial_dim = spatial_dim_;
  static constexpr int nodes_per_element = nodes_per_element_;

  virtual int get_num_nodes() const = 0;
  virtual int get_num_elements() const = 0;
  virtual void get_node_xloc(int node, T* xloc) const = 0;
  virtual void get_elem_dof_nodes(int elem, int* nodes) const = 0;
};

/**
 * @brief The abstract base class for a Galerkin (finite element or Galerkin
 * difference) basis
 */
template <typename T, class Mesh_>
class BasisBase {
 public:
  using Mesh = Mesh_;
  static constexpr int spatial_dim = Mesh::spatial_dim;
  static constexpr int nodes_per_element = Mesh::nodes_per_element;

  BasisBase(Mesh& mesh) : mesh(mesh) {}

  virtual void eval_basis_grad(int elem, const T* pt, T* N, T* Nxi) = 0;

  inline int get_num_nodes() const { return mesh.get_num_nodes(); };
  inline int get_num_elements() const { return mesh.get_num_elements(); };
  inline void get_node_xloc(int node, T* xloc) const {
    mesh.get_node_xloc(node, xloc);
  };
  inline void get_elem_dof_nodes(int elem, int* nodes) const {
    mesh.get_elem_dof_nodes(elem, nodes);
  };

 protected:
  Mesh& mesh;
};

template <typename T, class Basis, int dim>
static void eval_grad(Basis& basis, int elem, const T pt[], const T dof[],
                      A2D::Mat<T, dim, Basis::spatial_dim>& grad) {
  static constexpr int spatial_dim = Basis::spatial_dim;
  static constexpr int nodes_per_element = Basis::nodes_per_element;

  T Nxi[spatial_dim * nodes_per_element];
  basis.eval_basis_grad(elem, pt, (T*)nullptr, Nxi);

  for (int k = 0; k < spatial_dim * dim; k++) {
    grad[k] = 0.0;
  }

  for (int i = 0; i < nodes_per_element; i++) {
    for (int k = 0; k < dim; k++) {
      for (int j = 0; j < spatial_dim; j++) {
        grad(k, j) += Nxi[spatial_dim * i + j] * dof[dim * i + k];
      }
    }
  }
}

template <typename T, class Basis, int dim>
static void eval_grad(Basis& basis, int elem, const T pt[], const T dof[],
                      A2D::Vec<T, dim>& vals,
                      A2D::Mat<T, dim, Basis::spatial_dim>& grad) {
  static constexpr int spatial_dim = Basis::spatial_dim;
  static constexpr int nodes_per_element = Basis::nodes_per_element;

  T N[nodes_per_element];
  T Nxi[spatial_dim * nodes_per_element];
  basis.eval_basis_grad(elem, pt, N, Nxi);

  for (int k = 0; k < dim; k++) {
    vals(k) = 0.0;
  }

  for (int k = 0; k < spatial_dim * dim; k++) {
    grad[k] = 0.0;
  }

  for (int i = 0; i < nodes_per_element; i++) {
    for (int k = 0; k < dim; k++) {
      vals(k) += N[i] * dof[dim * i + k];
      for (int j = 0; j < spatial_dim; j++) {
        grad(k, j) += Nxi[spatial_dim * i + j] * dof[dim * i + k];
      }
    }
  }
}

template <typename T, class Basis, int dim>
static void add_grad(Basis& basis, int elem, const T pt[],
                     const A2D::Vec<T, dim>& coef_vals,
                     const A2D::Mat<T, dim, Basis::spatial_dim>& coef_grad,
                     T elem_res[]) {
  static constexpr int spatial_dim = Basis::spatial_dim;
  static constexpr int nodes_per_element = Basis::nodes_per_element;

  T N[nodes_per_element];
  T Nxi[spatial_dim * nodes_per_element];
  basis.eval_basis_grad(elem, pt, N, Nxi);

  for (int i = 0; i < nodes_per_element; i++) {
    for (int k = 0; k < dim; k++) {
      elem_res[dim * i + k] += coef_vals(k) * N[i];
      for (int j = 0; j < spatial_dim; j++) {
        elem_res[dim * i + k] += (coef_grad(k, j) * Nxi[spatial_dim * i + j]);
      }
    }
  }
}

template <typename T, class Basis, int dim>
static void add_matrix(Basis& basis, int elem, const T pt[],
                       const A2D::Mat<T, dim, dim>& coef_vals,
                       const A2D::Mat<T, dim * Basis::spatial_dim,
                                      dim * Basis::spatial_dim>& coef_grad,
                       T elem_jac[]) {
  static constexpr int spatial_dim = Basis::spatial_dim;
  static constexpr int nodes_per_element = Basis::nodes_per_element;

  T N[nodes_per_element];
  T Nxi[spatial_dim * nodes_per_element];
  basis.eval_basis_grad(elem, pt, N, Nxi);

  constexpr int dof_per_element = dim * nodes_per_element;

  for (int i = 0; i < nodes_per_element; i++) {
    T ni = N[i];
    std::vector<T> nxi(&Nxi[spatial_dim * i],
                       &Nxi[spatial_dim * i] + spatial_dim);

    for (int j = 0; j < nodes_per_element; j++) {
      T nj = N[j];
      std::vector<T> nxj(&Nxi[spatial_dim * j],
                         &Nxi[spatial_dim * j] + spatial_dim);

      for (int ii = 0; ii < dim; ii++) {
        int row = dim * i + ii;
        for (int jj = 0; jj < dim; jj++) {
          int col = dim * j + jj;

          T val = 0.0;
          for (int kk = 0; kk < spatial_dim; kk++) {
            for (int ll = 0; ll < spatial_dim; ll++) {
              val += coef_grad(spatial_dim * ii + kk, spatial_dim * jj + ll) *
                     nxi[kk] * nxj[ll];
            }
          }
          elem_jac[col + row * dof_per_element] +=
              val + coef_vals(ii, jj) * ni * nj;
        }
      }
    }
  }
}

#endif  // XCGD_COMMONS_H