#ifndef XCGD_ANALYSIS_H
#define XCGD_ANALYSIS_H

#include "a2dcore.h"
#include "elements/commons.h"
#include "sparse_utils/sparse_matrix.h"

template <typename T, class Basis, class Quadrature, class Physics>
class FEAnalysis {
 public:
  // Static data taken from the element basis
  static const int spatial_dim = Basis::spatial_dim;
  static const int nodes_per_element = Basis::nodes_per_element;

  // Static data from the qaudrature
  static const int num_quadrature_pts = Quadrature::num_quadrature_pts;

  // Static data taken from the physics
  static const int dof_per_node = Physics::dof_per_node;

  // Derived static data
  static const int dof_per_element = dof_per_node * nodes_per_element;

  template <int dim>
  static void get_element_dof(const int element_nodes[], const T dof[],
                              T element_dof[]) {
    for (int j = 0; j < nodes_per_element; j++) {
      int node = element_nodes[j];
      for (int k = 0; k < dim; k++, element_dof++) {
        element_dof[0] = dof[dim * node + k];
      }
    }
  }

  template <int dim, class Func>
  static void get_element_dof_new(const Func& get_element_nodes, int e,
                                  const T dof[], T element_dof[]) {
    for (int j = 0; j < nodes_per_element; j++) {
      int node = get_element_nodes(e, j);
      for (int k = 0; k < dim; k++, element_dof++) {
        element_dof[0] = dof[dim * node + k];
      }
    }
  }

  template <int dim>
  static void add_element_res(const int nodes[], const T element_res[],
                              T res[]) {
    for (int j = 0; j < nodes_per_element; j++) {
      int node = nodes[j];
      for (int k = 0; k < dim; k++, element_res++) {
        res[dim * node + k] += element_res[0];
      }
    }
  }

  template <class Func>
  static T energy_new(Physics& phys, int num_elements,
                      const Func& get_element_nodes, const T xloc[],
                      const T dof[]) {
    T total_energy = 0.0;

    for (int i = 0; i < num_elements; i++) {
      // Get the element node locations
      T element_xloc[spatial_dim * nodes_per_element];
      get_element_dof_new<spatial_dim>(get_element_nodes, i, xloc,
                                       element_xloc);

      // Get the element degrees of freedom
      T element_dof[dof_per_element];
      get_element_dof_new<dof_per_node>(get_element_nodes, i, dof, element_dof);

      for (int j = 0; j < num_quadrature_pts; j++) {
        T pt[spatial_dim];
        T weight = Quadrature::template get_quadrature_pt<T>(j, pt);

        // Evaluate the derivative of the spatial dof in the computational
        // coordinates
        A2D::Mat<T, spatial_dim, spatial_dim> J;
        eval_grad<T, Basis, spatial_dim>(pt, element_xloc, J);

        // Evaluate the derivative of the dof in the computational coordinates
        A2D::Vec<T, dof_per_node> vals;
        A2D::Mat<T, dof_per_node, spatial_dim> grad;
        eval_grad<T, Basis, dof_per_node>(pt, element_dof, vals, grad);

        // Add the energy contributions
        total_energy += phys.energy(weight, J, vals, grad);
      }
    }

    return total_energy;
  }

  static T energy(Physics& phys, int num_elements, const int element_nodes[],
                  const T xloc[], const T dof[]) {
    T total_energy = 0.0;

    for (int i = 0; i < num_elements; i++) {
      // Get the element node locations
      T element_xloc[spatial_dim * nodes_per_element];
      get_element_dof<spatial_dim>(&element_nodes[nodes_per_element * i], xloc,
                                   element_xloc);

      // Get the element degrees of freedom
      T element_dof[dof_per_element];
      get_element_dof<dof_per_node>(&element_nodes[nodes_per_element * i], dof,
                                    element_dof);

      for (int j = 0; j < num_quadrature_pts; j++) {
        T pt[spatial_dim];
        T weight = Quadrature::template get_quadrature_pt<T>(j, pt);

        // Evaluate the derivative of the spatial dof in the computational
        // coordinates
        A2D::Mat<T, spatial_dim, spatial_dim> J;
        eval_grad<T, Basis, spatial_dim>(pt, element_xloc, J);

        // Evaluate the derivative of the dof in the computational coordinates
        A2D::Vec<T, dof_per_node> vals;
        A2D::Mat<T, dof_per_node, spatial_dim> grad;
        eval_grad<T, Basis, dof_per_node>(pt, element_dof, vals, grad);

        // Add the energy contributions
        total_energy += phys.energy(weight, J, vals, grad);
      }
    }

    return total_energy;
  }

  static void residual(Physics& phys, int num_elements,
                       const int element_nodes[], const T xloc[], const T dof[],
                       T res[]) {
    for (int i = 0; i < num_elements; i++) {
      // Get the element node locations
      T element_xloc[spatial_dim * nodes_per_element];
      get_element_dof<spatial_dim>(&element_nodes[nodes_per_element * i], xloc,
                                   element_xloc);

      // Get the element degrees of freedom
      T element_dof[dof_per_element];
      get_element_dof<dof_per_node>(&element_nodes[nodes_per_element * i], dof,
                                    element_dof);

      // Create the element residual
      T element_res[dof_per_element];
      for (int j = 0; j < dof_per_element; j++) {
        element_res[j] = 0.0;
      }

      for (int j = 0; j < num_quadrature_pts; j++) {
        T pt[spatial_dim];
        T weight = Quadrature::template get_quadrature_pt<T>(j, pt);

        // Evaluate the derivative of the spatial dof in the computational
        // coordinates
        A2D::Mat<T, spatial_dim, spatial_dim> J;
        eval_grad<T, Basis, spatial_dim>(pt, element_xloc, J);

        // Evaluate the derivative of the dof in the computational coordinates
        A2D::Vec<T, dof_per_node> vals;
        A2D::Mat<T, dof_per_node, spatial_dim> grad;
        eval_grad<T, Basis, dof_per_node>(pt, element_dof, vals, grad);

        // Evaluate the residuals at the quadrature points
        A2D::Vec<T, dof_per_node> coef_vals;
        A2D::Mat<T, dof_per_node, spatial_dim> coef_grad;
        phys.residual(weight, J, vals, grad, coef_vals, coef_grad);

        // Add the contributions to the element residual
        add_grad<T, Basis, dof_per_node>(pt, coef_vals, coef_grad, element_res);
      }

      add_element_res<dof_per_node>(&element_nodes[nodes_per_element * i],
                                    element_res, res);
    }
  }

  static void jacobian_product(Physics& phys, int num_elements,
                               const int element_nodes[], const T xloc[],
                               const T dof[], const T direct[], T res[]) {
    for (int i = 0; i < num_elements; i++) {
      // Get the element node locations
      T element_xloc[spatial_dim * nodes_per_element];
      get_element_dof<spatial_dim>(&element_nodes[nodes_per_element * i], xloc,
                                   element_xloc);

      // Get the element degrees of freedom
      T element_dof[dof_per_element];
      get_element_dof<dof_per_node>(&element_nodes[nodes_per_element * i], dof,
                                    element_dof);

      // Get the element directions for the Jacobian-vector product
      T element_direct[dof_per_element];
      get_element_dof<dof_per_node>(&element_nodes[nodes_per_element * i],
                                    direct, element_direct);

      // Create the element residual
      T element_res[dof_per_element];
      for (int j = 0; j < dof_per_element; j++) {
        element_res[j] = 0.0;
      }

      for (int j = 0; j < num_quadrature_pts; j++) {
        T pt[spatial_dim];
        T weight = Quadrature::template get_quadrature_pt<T>(j, pt);

        // Evaluate the derivative of the spatial dof in the computational
        // coordinates
        A2D::Mat<T, spatial_dim, spatial_dim> J;
        eval_grad<T, Basis, spatial_dim>(pt, element_xloc, J);

        // Evaluate the derivative of the dof in the computational coordinates
        A2D::Vec<T, dof_per_node> vals;
        A2D::Mat<T, dof_per_node, spatial_dim> grad;
        eval_grad<T, Basis, dof_per_node>(pt, element_dof, vals, grad);

        // Evaluate the derivative of the direction in the computational
        // coordinates
        A2D::Vec<T, dof_per_node> direct_vals;
        A2D::Mat<T, dof_per_node, spatial_dim> direct_grad;
        eval_grad<T, Basis, dof_per_node>(pt, element_direct, direct_vals,
                                          direct_grad);

        // Evaluate the residuals at the quadrature points
        A2D::Vec<T, dof_per_node> coef_vals;
        A2D::Mat<T, dof_per_node, spatial_dim> coef_grad;
        phys.jacobian_product(weight, J, vals, grad, direct_vals, direct_grad,
                              coef_vals, coef_grad);

        // Add the contributions to the element residual
        add_grad<T, Basis, dof_per_node>(pt, coef_vals, coef_grad, element_res);
      }

      add_element_res<dof_per_node>(&element_nodes[nodes_per_element * i],
                                    element_res, res);
    }
  }

  static void jacobian(
      Physics& phys, int num_elements, const int element_nodes[],
      const T xloc[], const T dof[],
      SparseUtils::BSRMat<T, dof_per_node, dof_per_node>* mat) {
    for (int i = 0; i < num_elements; i++) {
      // Get the element node locations
      T element_xloc[spatial_dim * nodes_per_element];
      get_element_dof<spatial_dim>(&element_nodes[nodes_per_element * i], xloc,
                                   element_xloc);

      // Get the element degrees of freedom
      T element_dof[dof_per_element];
      get_element_dof<dof_per_node>(&element_nodes[nodes_per_element * i], dof,
                                    element_dof);

      // Create the element Jacobian
      T element_jac[dof_per_element * dof_per_element];
      for (int j = 0; j < dof_per_element * dof_per_element; j++) {
        element_jac[j] = 0.0;
      }

      for (int j = 0; j < num_quadrature_pts; j++) {
        T pt[spatial_dim];
        T weight = Quadrature::template get_quadrature_pt<T>(j, pt);

        // Evaluate the derivative of the spatial dof in the computational
        // coordinates
        A2D::Mat<T, spatial_dim, spatial_dim> J;
        eval_grad<T, Basis, spatial_dim>(pt, element_xloc, J);

        // Evaluate the derivative of the dof in the computational coordinates
        A2D::Vec<T, dof_per_node> vals;
        A2D::Mat<T, dof_per_node, spatial_dim> grad;
        eval_grad<T, Basis, dof_per_node>(pt, element_dof, vals, grad);

        // Evaluate the residuals at the quadrature points
        A2D::Mat<T, dof_per_node, dof_per_node> coef_vals;
        A2D::Mat<T, dof_per_node * spatial_dim, dof_per_node * spatial_dim>
            coef_grad;
        phys.jacobian(weight, J, vals, grad, coef_vals, coef_grad);

        // Add the contributions to the element residual
        add_matrix<T, Basis, dof_per_node>(pt, coef_vals, coef_grad,
                                           element_jac);
      }

      mat->add_block_values(nodes_per_element,
                            &element_nodes[nodes_per_element * i],
                            nodes_per_element,
                            &element_nodes[nodes_per_element * i], element_jac);
    }
  }
};

#endif  // XCGD_ANALYSIS_H