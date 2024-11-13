#pragma once

#include "ad/a2dmattrace.h"
#include "physics_commons.h"

template <typename T>
class LinearElasticity2DVonMisesStressAggregation final
    : public PhysicsBase<T, 2, 0, 2> {
 private:
  using PhysicsBase = PhysicsBase<T, 2, 0, 2>;

 public:
  using PhysicsBase::data_per_node;
  using PhysicsBase::dof_per_node;
  using PhysicsBase::spatial_dim;

  LinearElasticity2DVonMisesStressAggregation(double ksrho, T E, T nu, T vm_max)
      : ksrho(ksrho),
        mu(0.5 * E / (1.0 + nu)),
        lambda(E * nu / ((1.0 + nu) * (1.0 - nu))) {}

  T energy(T weight, T _, A2D::Vec<T, spatial_dim>& __,
           A2D::Vec<T, spatial_dim>& ___,
           A2D::Mat<T, spatial_dim, spatial_dim>& J,
           A2D::Vec<T, dof_per_node>& ____,
           A2D::Mat<T, dof_per_node, spatial_dim>& grad) const {
    T detJ, trS, detS, von_mises;
    A2D::SymMat<T, spatial_dim> E, S;
    A2D::MatDet(J, detJ);
    A2D::MatGreenStrain<A2D::GreenStrainType::LINEAR>(grad, E);
    A2D::SymIsotropic(mu, lambda, E, S);
    A2D::MatTrace(S, trS);
    A2D::MatDet(S, detS);
    von_mises = sqrt(trS * trS - 3.0 * detS * detS);
    return von_mises;
  }

  void residual(T weight, T _, A2D::Vec<T, spatial_dim>& xloc,
                A2D::Vec<T, spatial_dim>& __,
                A2D::Mat<T, spatial_dim, spatial_dim>& J,
                A2D::Vec<T, dof_per_node>& u,
                A2D::Mat<T, dof_per_node, spatial_dim>& grad,
                A2D::Vec<T, dof_per_node>& coef_u,
                A2D::Mat<T, dof_per_node, spatial_dim>& coef_grad) const {}

 private:
  double ksrho;  // KS aggregation parameter
  T mu, lambda;  // Lame parameters
  T vs_max;      // maximum Von Mises stress
};
