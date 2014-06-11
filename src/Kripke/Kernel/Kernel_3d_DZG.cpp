#include<Kripke/Kernel/Kernel_3d_DZG.h>
#include<Kripke/User_Data.h>
#include<Kripke/SubTVec.h>
#include<Kripke/LMat.h>

Kernel_3d_DZG::Kernel_3d_DZG() {

}

Kernel_3d_DZG::~Kernel_3d_DZG() {

}

Nesting_Order Kernel_3d_DZG::nestingPsi(void) const {
  return NEST_DZG;
}

Nesting_Order Kernel_3d_DZG::nestingPhi(void) const {
  return NEST_DZG;
}

void Kernel_3d_DZG::LTimes(Grid_Data *grid_data) {
  // Outer parameters
  double ***ell = grid_data->ell->data;
  int num_zones = grid_data->num_zones;
  int num_moments = grid_data->num_moments;
  int nidx = grid_data->nm_table.size();
  int blk_size = grid_data->L_block;

  grid_data->phi->clear(0.0);

  // Loop over Group Sets
  int num_group_sets = grid_data->gd_sets.size();
  for (int gset = 0; gset < num_group_sets; ++gset) {
    std::vector<Group_Dir_Set> &dir_sets = grid_data->gd_sets[gset];
    int num_dir_sets = dir_sets.size();

    // Loop over Direction Sets
    for (int dset = 0; dset < num_dir_sets; ++dset) {
      Group_Dir_Set &gd_set = dir_sets[dset];

      // Get dimensioning
      int num_local_groups = gd_set.num_groups;
      int group0 = gd_set.group0;
      int num_local_directions = gd_set.num_directions;
      int dir0 = gd_set.direction0;
      //int num_groups_zones = num_local_groups*num_zones;

      /* 3D Cartesian Geometry */
      for(int z_start = 0;z_start < num_zones;z_start += blk_size){
        int z_end = std::min(z_start+blk_size, num_zones);
        for(int nm_offset = 0;nm_offset < nidx;++nm_offset){
          int n = grid_data->nm_table[nm_offset];
          int m = nm_offset - n*n - n;
          double *ell_n_m = ell[n][m + n];


          for (int d = 0; d < num_local_directions; d++) {
            double ell_n_m_d = ell_n_m[d + dir0];

            for (int z = z_start;z < z_end;++ z){
              double * KRESTRICT phi = grid_data->phi->ptr(group0, nm_offset, z);
              double * KRESTRICT psi = gd_set.psi->ptr(0, d, z);

              for(int group = 0;group < num_local_groups; ++ group){
                phi[group] += ell_n_m_d * psi[group];
              }
            }
          }
        }
      }
    } // Direction Set
  } // Group Set
}

void Kernel_3d_DZG::LPlusTimes(Grid_Data *grid_data) {
  // Outer parameters
  double ***ell_plus = grid_data->ell_plus->data;
  int num_zones = grid_data->num_zones;
  int num_moments = grid_data->num_moments;
  int nidx = grid_data->nm_table.size();
  int blk_size = grid_data->L_block;

  // Loop over Group Sets
  int num_group_sets = grid_data->gd_sets.size();
  for (int gset = 0; gset < num_group_sets; ++gset) {
    std::vector<Group_Dir_Set> &dir_sets = grid_data->gd_sets[gset];
    int num_dir_sets = dir_sets.size();

    // Loop over Direction Sets
    for (int dset = 0; dset < num_dir_sets; ++dset) {
      Group_Dir_Set &gd_set = dir_sets[dset];

      // Get dimensioning
      int num_local_groups = gd_set.num_groups;
      int group0 = gd_set.group0;
      int num_local_directions = gd_set.num_directions;
      int dir0 = gd_set.direction0;
      int num_groups_zones = num_local_groups*num_zones;

      // Get Variables
      gd_set.rhs->clear(0.0);

      /* 3D Cartesian Geometry */
      for(int z_start = 0;z_start < num_groups_zones;z_start += blk_size){
        int z_end = std::min(z_start+blk_size, num_zones);
        for (int d = 0; d < num_local_directions; d++) {
          double **ell_plus_d = ell_plus[d + dir0];

          for(int nm_offset = 0;nm_offset < nidx;++nm_offset){
            int n = grid_data->nm_table[nm_offset];
            int m = nm_offset - n*n - n;

            double ell_plus_d_n_m = ell_plus_d[n][n+m];

            for(int z = z_start;z < z_end;++ z){

              double * KRESTRICT phi_out = grid_data->phi_out->ptr(group0, nm_offset, z);
              double * KRESTRICT rhs = gd_set.rhs->ptr(0, d, z);
              for(int group = 0;group < num_local_groups;++ group){
                rhs[group] += ell_plus_d_n_m * phi_out[group];
              }
            }

          }
        }
      }

    } // Direction Set
  } // Group Set
}

/* Sweep routine for Diamond-Difference */
/* Macros for offsets with fluxes on cell faces */
#define I_PLANE_INDEX(j, k) (k)*(local_jmax) + (j)
#define J_PLANE_INDEX(i, k) (k)*(local_imax) + (i)
#define K_PLANE_INDEX(i, j) (j)*(local_imax) + (i)
#define Zonal_INDEX(i, j, k) (i) + (local_imax)*(j) \
  + (local_imax)*(local_jmax)*(k)

void Kernel_3d_DZG::sweep(Grid_Data *grid_data, Group_Dir_Set *gd_set,
    double *i_plane_ptr, double *j_plane_ptr, double *k_plane_ptr) {
  int num_directions = gd_set->num_directions;
  int num_groups = gd_set->num_groups;
  int num_zones = grid_data->num_zones;

  Directions *direction = gd_set->directions;

  int local_imax = grid_data->nzones[0];
  int local_jmax = grid_data->nzones[1];
  int local_kmax = grid_data->nzones[2];
  int local_imax_1 = local_imax + 1;
  int local_jmax_1 = local_jmax + 1;

  double * dx = &grid_data->deltas[0][0];
  double * dy = &grid_data->deltas[1][0];
  double * dz = &grid_data->deltas[2][0];

  // Alias the MPI data with a SubTVec for the face data
  SubTVec i_plane(nestingPsi(), num_groups, num_directions,
      local_jmax * local_kmax, i_plane_ptr);
  SubTVec j_plane(nestingPsi(), num_groups, num_directions,
      local_imax * local_kmax, j_plane_ptr);
  SubTVec k_plane(nestingPsi(), num_groups, num_directions,
      local_imax * local_jmax, k_plane_ptr);

  // All directions have same id,jd,kd, since these are all one Direction Set
  // So pull that information out now
  int octant = direction[0].octant;
  Grid_Sweep_Block const &extent = grid_data->octant_extent[octant];

  std::vector<Grid_Sweep_Block> const &idxset =
      grid_data->octant_indexset[octant];

  std::vector<double> xcos_dxi_all(local_imax);
  std::vector<double> ycos_dyj_all(local_jmax);
  std::vector<double> zcos_dzk_all(local_kmax);

  for (int d = 0; d < num_directions; ++d) {
    double xcos = direction[d].xcos;
    double ycos = direction[d].ycos;
    double zcos = direction[d].zcos;
    for (int i = 0; i < local_imax; ++i) {
       double dxi = dx[i + 1];
       xcos_dxi_all[i] = 2.0 * xcos / dxi;
     }

     for (int j = 0; j < local_jmax; ++j) {
       double dyj = dy[j + 1];
       ycos_dyj_all[j] = 2.0 * ycos / dyj;
     }

     for (int k = 0; k < local_kmax; ++k) {
       double dzk = dz[k + 1];
       zcos_dzk_all[k] = 2.0 * zcos / dzk;
     }


    /*  Perform transport sweep of the grid 1 cell at a time.   */
    for (int block_idx = 0; block_idx < idxset.size(); ++block_idx) {
      Grid_Sweep_Block const &block = idxset[block_idx];
      for (int k = block.start_k; k != block.end_k; k += block.inc_k) {
        double zcos_dzk = zcos_dzk_all[k];
        for (int j = block.start_j; j != block.end_j; j += block.inc_j) {
          double ycos_dyj = ycos_dyj_all[j];
          for (int i = block.start_i; i != block.end_i; i += block.inc_i) {
            double xcos_dxi = xcos_dxi_all[i];

            int z = Zonal_INDEX(i, j, k);
            double * KRESTRICT psi_d_z = gd_set->psi->ptr(0, d, z);
            double * KRESTRICT rhs_d_z = gd_set->rhs->ptr(0, d, z);

            double * KRESTRICT psi_lf_d_z = i_plane.ptr(0, d, I_PLANE_INDEX(j, k));
            double * KRESTRICT psi_fr_d_z = j_plane.ptr(0, d, J_PLANE_INDEX(i, k));
            double * KRESTRICT psi_bo_d_z = k_plane.ptr(0, d, K_PLANE_INDEX(i, j));

            double * KRESTRICT sigt_z = gd_set->sigt->ptr(0, 0, z);

            for (int group = 0; group < num_groups; ++group) {
              /* Calculate new zonal flux */
              double psi_d_z_g = (rhs_d_z[group]
                  + psi_lf_d_z[group] * xcos_dxi
                  + psi_fr_d_z[group] * ycos_dyj
                  + psi_bo_d_z[group] * zcos_dzk)
                  / (xcos_dxi + ycos_dyj + zcos_dzk + sigt_z[group]);

              psi_d_z[group] = psi_d_z_g;

              /* Apply diamond-difference relationships */
              psi_lf_d_z[group] = 2.0 * psi_d_z_g - psi_lf_d_z[group];
              psi_fr_d_z[group] = 2.0 * psi_d_z_g - psi_fr_d_z[group];
              psi_bo_d_z[group] = 2.0 * psi_d_z_g - psi_bo_d_z[group];
            }
          }
        }
      }
    }
  }
}

