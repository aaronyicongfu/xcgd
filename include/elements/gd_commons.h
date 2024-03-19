#ifndef XCGD_GD_COMMONS_H
#define XCGD_GD_COMMONS_H

#include <algorithm>
#include <set>
#include <vector>

// The structured ground grid
template <typename T>
class StructuredGrid2D final {
 public:
  static constexpr int spatial_dim = 2;
  static constexpr int nverts_per_cell = 4;

  /**
   * @param nxy_ numbers of cells in x, y directions
   * @param lxy_ lengths of the grid in x, y directions
   * @param lxy_ origin (x, y) of the grid
   */
  StructuredGrid2D(const int* nxy_, const T* lxy_, const T* xy0_ = nullptr) {
    for (int d = 0; d < spatial_dim; d++) {
      nxy[d] = nxy_[d];
      if (xy0_) {
        xy0[d] = xy0_[d];
        lxy[d] = lxy_[d] - xy0_[d];
      } else {
        xy0[d] = 0.0;
        lxy[d] = lxy_[d];
      }
    }
  }

  inline int get_num_verts() const {
    const int* nxy = get_nxy();
    int nnodes = 1;
    for (int d = 0; d < spatial_dim; d++) {
      nnodes *= nxy[d] + 1;
    }
    return nnodes;
  }

  inline int get_num_cells() const {
    const int* nxy = get_nxy();
    int nelems = 1;
    for (int d = 0; d < spatial_dim; d++) {
      nelems *= nxy[d];
    }
    return nelems;
  }

  // coordinates -> vert
  inline int get_coords_vert(int ni, int nj) const {
    return ni + (nxy[0] + 1) * nj;
  }
  inline int get_coords_vert(const int* nij) const {
    return nij[0] + (nxy[0] + 1) * nij[1];
  }

  // vert -> coordinates
  inline void get_vert_coords(int vert, int* nij) const {
    nij[0] = vert % (nxy[0] + 1);
    nij[1] = vert / (nxy[0] + 1);
  }

  // coordinates -> cell
  inline int get_coords_cell(int ei, int ej) const { return ei + nxy[0] * ej; }
  inline int get_coords_cell(const int* eij) const {
    return eij[0] + nxy[0] * eij[1];
  }

  // cell -> coordinates
  inline void get_cell_coords(int cell, int* eij) const {
    eij[0] = cell % nxy[0];
    eij[1] = cell / nxy[0];
  }

  // cell -> verts
  void get_cell_verts(int cell, int* verts) const {
    if (verts) {
      int nij[spatial_dim] = {cell % nxy[0], cell / nxy[1]};
      verts[0] = get_coords_vert(nij);   //  3-------2
      verts[1] = verts[0] + 1;           //  |       |
      verts[2] = verts[1] + nxy[0] + 1;  //  |       |
      verts[3] = verts[2] - 1;           //  0-------1
    }
  }

  void get_cell_vert_ranges(int cell, T* xloc_min, T* xloc_max) const {
    int verts[4];
    get_cell_verts(cell, verts);

    get_vert_xloc(verts[0], xloc_min);
    get_vert_xloc(verts[2], xloc_max);
  }

  void get_vert_xloc(int vert, T* xloc) const {
    if (xloc) {
      int nij[spatial_dim];
      get_vert_coords(vert, nij);
      for (int d = 0; d < spatial_dim; d++) {
        xloc[d] = xy0[d] + lxy[d] * T(nij[d]) / T(nxy[d]);
      }
    }
  }

  // Get the xloc of the centroid of the cell
  void get_cell_xloc(int cell, T* xloc) const {
    if (xloc) {
      int nij[spatial_dim];
      get_cell_coords(cell, nij);
      for (int d = 0; d < spatial_dim; d++) {
        xloc[d] = xy0[d] + lxy[d] * (T(nij[d]) + 0.5) / T(nxy[d]);
      }
    }
  }

  const int* get_nxy() const { return nxy; };
  const T* get_lxy() const { return lxy; };
  const T* get_xy0() const { return xy0; };

 private:
  int nxy[spatial_dim];
  T lxy[spatial_dim];
  T xy0[spatial_dim];
};

template <typename T, int Np_1d>
class GDMesh2D final : public MeshBase<T, 2, Np_1d * Np_1d, 4> {
 private:
  using MeshBase = MeshBase<T, 2, Np_1d * Np_1d, 4>;
  static_assert(Np_1d % 2 == 0);
  using Grid = StructuredGrid2D<T>;

 public:
  using MeshBase::nodes_per_element;
  using MeshBase::spatial_dim;

  /**
   * @brief Construct GD Mesh given grid only
   */
  GDMesh2D(const Grid& grid) : grid(grid), has_lsf(false) {
    check_grid_compatibility(grid);
    num_nodes = grid.get_num_verts();
    num_elements = grid.get_num_cells();
  }

  /**
   * @brief Construct GD Mesh given grid and LSF function
   *
   * @tparam Func functor type
   * @param grid GD grid
   * @param lsf level set function that determining the analysis domain, lsf(T*
   * xyz) where xyz contains x,y,(z) coordinates.
   *
   * Note: Within the analysis domain, lsf <= 0
   */
  template <class Func>
  GDMesh2D(const Grid& grid, const Func& lsf) : grid(grid), has_lsf(true) {
    check_grid_compatibility(grid);
    init_dofs_from_lsf(lsf);
    num_nodes = node_verts.size();
    num_elements = elem_cells.size();
  }

  int get_num_nodes() const { return num_nodes; }
  int get_num_elements() const { return num_elements; }

  inline void get_node_xloc(int node, T* xloc) const {
    if (has_lsf) {
      grid.get_vert_xloc(node_verts[node], xloc);
    } else {
      grid.get_vert_xloc(node, xloc);
    }
  }

  /**
   * @brief For a GD element, get all dof nodes
   *
   * @param elem element index
   * @param nodes dof node indices, length: nodes_per_element
   */
  void get_elem_dof_nodes(int elem, int* nodes) const {
    // if (has_lsf) {
    //   elem = elem_cells[elem];
    // }

    // // Get stencil coordinates for each spatial dimension
    // int coords[spatial_dim * nodes_per_element];
    // get_cell_ground_stencil_coords(elem, coords);

    // if (has_lsf) {
    //   adjust_stencil(elem, coords);
    // }

    // for (int index = 0; index < nodes_per_element; index++) {
    //   nodes[index] = grid.get_coords_vert(&coords[spatial_dim * index]);
    // }

    if (has_lsf) {
      int cell = elem_cells[elem];
      get_cell_ground_stencil(cell, nodes);
      adjust_stencil(cell, nodes);

    } else {
      get_cell_ground_stencil(elem, nodes);
    }
  }

  void get_cell_ground_stencil(int cell, int* nodes) const {
    constexpr int q = Np_1d / 2;
    int eij[spatial_dim];
    grid.get_cell_coords(cell, eij);
    const int* nxy = grid.get_nxy();
    for (int d = 0; d < spatial_dim; d++) {
      if (eij[d] < q - 1) {
        eij[d] = q - 1;
      } else if (eij[d] > nxy[d] - q) {
        eij[d] = nxy[d] - q;
      }
    }

    int index = 0;
    for (int j = 0; j < Np_1d; j++) {
      for (int i = 0; i < Np_1d; i++, index++) {
        nodes[index] =
            grid.get_coords_vert(eij[0] - q + 1 + i, eij[1] - q + 1 + j);
      }
    }
  };

  /**
   * @brief For a GD element, get the stencil associated to the ground grid,
   * regardless the optional boundary defined by the level set function
   *
   * @param elem element index
   * @param vert_coords array of size spatial_dim * nodes_per_element, each
   * entry is the coordinates of a ground stencil vertex
   */
  void get_cell_ground_stencil_coords(int cell, int* vert_coords) const {
    constexpr int q = Np_1d / 2;
    int eij[spatial_dim];
    grid.get_cell_coords(cell, eij);
    const int* nxy = grid.get_nxy();
    for (int d = 0; d < spatial_dim; d++) {
      if (eij[d] < q - 1) {
        eij[d] = q - 1;
      } else if (eij[d] > nxy[d] - q) {
        eij[d] = nxy[d] - q;
      }
    }

    for (int index = 0; index < nodes_per_element; index++) {
      int i = index % Np_1d;
      int j = index / Np_1d;
      vert_coords[spatial_dim * index] = eij[0] - q + 1 + i;
      vert_coords[spatial_dim * index + 1] = eij[1] - q + 1 + j;
    }
  }

  /**
   * @brief Adjust the stencil by pushing the stencil verts that are outside the
   * LSF boundary inward such that all nodes are active nodes
   */
  void adjust_stencil(int cell, int* nodes) const {
    // Get push direction
    int dir = dir_cells[cell];
    int dim = dir / spatial_dim;
    int sign = dir % spatial_dim == 0 ? 1 : -1;

    // Adjust nodes
    for (int index = 0; index < nodes_per_element; index++) {
      int vert = node_verts[nodes[index]];
      if (active_verts_set.count(vert) == 0) {
        int vert_coords[spatial_dim] = {-1, -1};
        grid.get_vert_coords(vert, vert_coords);
        vert_coords[dim] += sign * Np_1d;
        nodes[index] = grid.get_coords_vert(vert_coords);
      }
    }
  }

  inline void get_elem_dof_verts(int elem, int* verts) const {
    grid.get_cell_verts(elem, verts);
  }

  void get_elem_node_ranges(int elem, T* xloc_min, T* xloc_max) const {
    // [x0, ..., xN, y0, ..., yN, ...]
    std::vector<T> coords(spatial_dim * nodes_per_element);

    int nodes[nodes_per_element];
    get_elem_dof_nodes(elem, nodes);

    for (int i = 0; i < nodes_per_element; i++) {
      T xloc[spatial_dim];
      get_node_xloc(nodes[i], xloc);
      for (int d = 0; d < spatial_dim; d++) {
        coords[i + d * nodes_per_element] = xloc[d];
      }
    }

    for (int d = 0; d < spatial_dim; d++) {
      xloc_min[d] =
          *std::min_element(&coords[d * nodes_per_element],
                            &coords[d * nodes_per_element] + nodes_per_element,
                            [](T& a, T& b) { return freal(a) < freal(b); });

      xloc_max[d] =
          *std::max_element(&coords[d * nodes_per_element],
                            &coords[d * nodes_per_element] + nodes_per_element,
                            [](T& a, T& b) { return freal(a) < freal(b); });
    }
  }

  inline void get_elem_vert_ranges(int elem, T* xloc_min, T* xloc_max) const {
    grid.get_cell_vert_ranges(elem, xloc_min, xloc_max);
  }

 private:
  void check_grid_compatibility(const Grid& grid) const {
    const int* nxy = grid.get_nxy();
    for (int d = 0; d < spatial_dim; d++) {
      if (nxy[d] < Np_1d - 1) {
        char msg[256];
        std::snprintf(
            msg, 256,
            "too few elements (%d) for Np_1d (%d) along %d-th dimension",
            nxy[d], Np_1d, d);
        throw std::runtime_error(msg);
      }
    }
  }

  template <class Func>
  void init_dofs_from_lsf(const Func& lsf) {
    // LSF values are always associated with the ground grid verts, unlike the
    // dof values which might only be associated with part of the ground grid
    // verts (i.e. nodes)
    int nverts = grid.get_num_verts();
    std::vector<T> lsf_dof(nverts);

    // Populate lsf values
    std::vector<bool> active_lsf_verts(nverts, false);
    for (int i = 0; i < nverts; i++) {
      T xloc[spatial_dim];
      grid.get_vert_xloc(i, xloc);
      lsf_dof[i] = lsf(xloc);
      if (lsf_dof[i] <= T(0.0)) {
        active_lsf_verts[i] = true;
      }
    }

    // Active cell is a cell with at least one active lsf vert
    int ncells = grid.get_num_cells();
    std::vector<bool> active_cells(ncells, false);

    // Unlike LSF values, dof are associated with nodes, which is a subset of
    // verts. Here we determine which verts are dof nodes
    for (int c = 0; c < ncells; c++) {
      if (active_cells[c]) continue;
      int verts[Grid::nverts_per_cell];
      grid.get_cell_verts(c, verts);
      for (int i = 0; i < Grid::nverts_per_cell; i++) {
        if (active_lsf_verts[verts[i]]) {
          active_cells[c] = true;
          break;
        }
      }
    }

    // Create active dof nodes and the mapping to verts
    for (int c = 0; c < ncells; c++) {
      if (!active_cells[c]) continue;
      elem_cells.push_back(c);
      int verts[Grid::nverts_per_cell];
      grid.get_cell_verts(c, verts);
      for (int i = 0; i < Grid::nverts_per_cell; i++) {
        active_verts_set.insert(verts[i]);
      }
    }

    node_verts.resize(active_verts_set.size());
    std::copy(active_verts_set.begin(), active_verts_set.end(),
              node_verts.begin());

    // For each cell, get the push direction for the outlying ground stencil
    // vertices
    dir_cells = std::vector<int>(ncells, -1);  // val | direction
                                               // 0   | +x
                                               // 1   | -x
                                               // 2   | +y
                                               // 3   | -y
                                               // 4   | +z
                                               // 5   | -z

    for (int c = 0; c < ncells; c++) {
      T xloc[spatial_dim];
      grid.get_cell_coords(c, xloc);
      auto grad = lsf.grad(xloc);

      double tmp = 0.0;
      int dim = -1;
      for (int d = 0; d < spatial_dim; d++) {
        if (fabs(grad[d]) > tmp) {
          tmp = fabs(grad[d]);
          dim = d;
        }
      }
      dir_cells[c] = 2 * dim + grad[dim] > 0 ? 0 : 1;
    }
  }

  const Grid& grid;
  int num_nodes = -1;
  int num_elements = -1;
  bool has_lsf = false;

  // indices of vertices that are dof nodes, i.e. vertices that have active
  // degrees of freedom
  std::set<int> active_verts_set;
  std::vector<int> node_verts;  // node -> vert

  // indices of cells that are dof elements, i.e. cells that have active degrees
  // of freedom
  std::vector<int> elem_cells;  // elem -> cell

  std::vector<int> dir_cells;
};

#endif  // XCGD_GD_COMMONS_H