/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_multires.h"
#include "BKE_subdiv_eval.h"

#include "DEG_depsgraph_query.h"

void multires_reshape_apply_base_update_mesh_coords(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;
  float(*base_positions)[3] = BKE_mesh_vert_positions_for_write(base_mesh);
  /* Update the context in case the vertices were duplicated. */
  reshape_context->base_positions = base_positions;

  const MLoop *mloop = reshape_context->base_loops;
  for (int loop_index = 0; loop_index < base_mesh->totloop; ++loop_index) {
    const MLoop *loop = &mloop[loop_index];

    GridCoord grid_coord;
    grid_coord.grid_index = loop_index;
    grid_coord.u = 1.0f;
    grid_coord.v = 1.0f;

    float P[3];
    float tangent_matrix[3][3];
    multires_reshape_evaluate_limit_at_grid(reshape_context, &grid_coord, P, tangent_matrix);

    ReshapeConstGridElement grid_element = multires_reshape_orig_grid_element_for_grid_coord(
        reshape_context, &grid_coord);
    float D[3];
    mul_v3_m3v3(D, tangent_matrix, grid_element.displacement);

    add_v3_v3v3(base_positions[loop->v], P, D);
  }
}

/* Assumes no is normalized; return value's sign is negative if v is on the other side of the
 * plane. */
static float v3_dist_from_plane(const float v[3], const float center[3], const float no[3])
{
  float s[3];
  sub_v3_v3v3(s, v, center);
  return dot_v3v3(s, no);
}

void multires_reshape_apply_base_refit_base_mesh(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;
  float(*base_positions)[3] = BKE_mesh_vert_positions_for_write(base_mesh);
  /* Update the context in case the vertices were duplicated. */
  reshape_context->base_positions = base_positions;
  MeshElemMap *pmap;
  int *pmap_mem;
  BKE_mesh_vert_poly_map_create(&pmap,
                                &pmap_mem,
                                reshape_context->base_polys,
                                reshape_context->base_loops,
                                base_mesh->totvert,
                                base_mesh->totpoly,
                                base_mesh->totloop);

  float(*origco)[3] = static_cast<float(*)[3]>(
      MEM_calloc_arrayN(base_mesh->totvert, sizeof(float[3]), __func__));
  for (int i = 0; i < base_mesh->totvert; i++) {
    copy_v3_v3(origco[i], base_positions[i]);
  }

  for (int i = 0; i < base_mesh->totvert; i++) {
    float avg_no[3] = {0, 0, 0}, center[3] = {0, 0, 0}, push[3];

    /* Don't adjust vertices not used by at least one poly. */
    if (!pmap[i].count) {
      continue;
    }

    /* Find center. */
    int tot = 0;
    for (int j = 0; j < pmap[i].count; j++) {
      const MPoly *p = &reshape_context->base_polys[pmap[i].indices[j]];

      /* This double counts, not sure if that's bad or good. */
      for (int k = 0; k < p->totloop; k++) {
        const int vndx = reshape_context->base_loops[p->loopstart + k].v;
        if (vndx != i) {
          add_v3_v3(center, origco[vndx]);
          tot++;
        }
      }
    }
    mul_v3_fl(center, 1.0f / tot);

    /* Find normal. */
    for (int j = 0; j < pmap[i].count; j++) {
      const MPoly *p = &reshape_context->base_polys[pmap[i].indices[j]];
      MPoly fake_poly;
      float no[3];

      /* Set up poly, loops, and coords in order to call BKE_mesh_calc_poly_normal(). */
      fake_poly.totloop = p->totloop;
      fake_poly.loopstart = 0;
      MLoop *fake_loops = static_cast<MLoop *>(
          MEM_malloc_arrayN(p->totloop, sizeof(MLoop), __func__));
      float(*fake_co)[3] = static_cast<float(*)[3]>(
          MEM_malloc_arrayN(p->totloop, sizeof(float[3]), __func__));

      for (int k = 0; k < p->totloop; k++) {
        const int vndx = reshape_context->base_loops[p->loopstart + k].v;

        fake_loops[k].v = k;

        if (vndx == i) {
          copy_v3_v3(fake_co[k], center);
        }
        else {
          copy_v3_v3(fake_co[k], origco[vndx]);
        }
      }

      BKE_mesh_calc_poly_normal(&fake_poly, fake_loops, (const float(*)[3])fake_co, no);
      MEM_freeN(fake_loops);
      MEM_freeN(fake_co);

      add_v3_v3(avg_no, no);
    }
    normalize_v3(avg_no);

    /* Push vertex away from the plane. */
    const float dist = v3_dist_from_plane(base_positions[i], center, avg_no);
    copy_v3_v3(push, avg_no);
    mul_v3_fl(push, dist);
    add_v3_v3(base_positions[i], push);
  }

  MEM_freeN(origco);
  MEM_freeN(pmap);
  MEM_freeN(pmap_mem);

  /* Vertices were moved around, need to update normals after all the vertices are updated
   * Probably this is possible to do in the loop above, but this is rather tricky because
   * we don't know all needed vertices' coordinates there yet. */
  BKE_mesh_tag_coords_changed(base_mesh);
}

void multires_reshape_apply_base_refine_from_base(MultiresReshapeContext *reshape_context)
{
  BKE_subdiv_eval_refine_from_mesh(reshape_context->subdiv, reshape_context->base_mesh, nullptr);
}

void multires_reshape_apply_base_refine_from_deform(MultiresReshapeContext *reshape_context)
{
  Depsgraph *depsgraph = reshape_context->depsgraph;
  Object *object = reshape_context->object;
  MultiresModifierData *mmd = reshape_context->mmd;
  BLI_assert(depsgraph != nullptr);
  BLI_assert(object != nullptr);
  BLI_assert(mmd != nullptr);

  float(*deformed_verts)[3] = BKE_multires_create_deformed_base_mesh_vert_coords(
      depsgraph, object, mmd, nullptr);

  BKE_subdiv_eval_refine_from_mesh(
      reshape_context->subdiv, reshape_context->base_mesh, deformed_verts);

  MEM_freeN(deformed_verts);
}
