/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"

#include "GEO_mesh_merge_by_distance.hh"

#include "BLI_math_vector.h"

#include "ply_import_mesh.hh"

namespace blender::io::ply {
Mesh *convert_ply_to_mesh(PlyData &data, Mesh *mesh, const PLYImportParams &params)
{

  /* Add vertices to the mesh. */
  mesh->totvert = int(data.vertices.size());
  CustomData_add_layer_named(
      &mesh->vdata, CD_PROP_FLOAT3, CD_CONSTRUCT, nullptr, mesh->totvert, "position");
  MutableSpan<float3> verts = mesh->vert_positions_for_write();
  verts.copy_from(data.vertices);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  if (!data.edges.is_empty()) {
    mesh->totedge = int(data.edges.size());
    CustomData_add_layer(&mesh->edata, CD_MEDGE, CD_SET_DEFAULT, nullptr, mesh->totedge);
    MutableSpan<MEdge> edges = mesh->edges_for_write();
    for (int i = 0; i < mesh->totedge; i++) {
      edges[i].v1 = data.edges[i].first;
      edges[i].v2 = data.edges[i].second;
    }
  }

  /* Add faces and edges to the mesh. */
  if (!data.faces.is_empty()) {
    /* Specify amount of total faces. */
    mesh->totpoly = int(data.faces.size());
    mesh->totloop = 0;
    for (int i = 0; i < data.faces.size(); i++) {
      /* Add number of edges to the amount of edges. */
      mesh->totloop += data.faces[i].size();
    }
    CustomData_add_layer(&mesh->pdata, CD_MPOLY, CD_SET_DEFAULT, nullptr, mesh->totpoly);
    CustomData_add_layer(&mesh->ldata, CD_MLOOP, CD_SET_DEFAULT, nullptr, mesh->totloop);
    MutableSpan<MPoly> polys = mesh->polys_for_write();
    MutableSpan<MLoop> loops = mesh->loops_for_write();

    int offset = 0;
    /* Iterate over amount of faces. */
    for (int i = 0; i < mesh->totpoly; i++) {
      int size = int(data.faces[i].size());
      /* Set the index from where this face starts and specify the amount of edges it has. */
      polys[i].loopstart = offset;
      polys[i].totloop = size;

      for (int j = 0; j < size; j++) {
        /* Set the vertex index of the loop to the one in PlyData. */
        loops[offset + j].v = data.faces[i][j];
      }
      offset += size;
    }
  }

  /* Vertex colors */
  if (!data.vertex_colors.is_empty()) {
    /* Create a data layer for vertex colors and set them. */
    bke::SpanAttributeWriter<ColorGeometry4f> colors =
        attributes.lookup_or_add_for_write_span<ColorGeometry4f>("Col", ATTR_DOMAIN_POINT);
    for (int i = 0; i < data.vertex_colors.size(); i++) {
      copy_v4_v4(colors.span[i], data.vertex_colors[i]);
    }
    colors.finish();
    BKE_id_attributes_active_color_set(&mesh->id, "Col");
  }

  /* Uvmap */
  if (!data.UV_coordinates.is_empty()) {
    bke::SpanAttributeWriter<float2> Uv = attributes.lookup_or_add_for_write_only_span<float2>(
        "UVMap", ATTR_DOMAIN_CORNER);
    int counter = 0;
    for (int i = 0; i < data.faces.size(); i++) {
      for (int j = 0; j < data.faces[i].size(); j++) {
        copy_v2_v2(Uv.span[counter], data.UV_coordinates[data.faces[i][j]]);
        counter++;
      }
    }
    Uv.finish();
  }

  /* Calculate edges from the rest of the mesh. */
  BKE_mesh_calc_edges(mesh, true, false);

  /* Note: This is important to do after initializing the loops. */
  if (!data.vertex_normals.is_empty()) {
    float(*vertex_normals)[3] = static_cast<float(*)[3]>(
        MEM_malloc_arrayN(data.vertex_normals.size(), sizeof(float[3]), __func__));

    /* Below code is necessary to access vertex normals within Blender.
     * Until Blender supports custom vertex normals, this is a workaround. */
    bke::SpanAttributeWriter<float3> normals;
    if (params.import_normals_as_attribute) {
      normals = attributes.lookup_or_add_for_write_span<float3>("Normal", ATTR_DOMAIN_POINT);
    }

    for (int i = 0; i < data.vertex_normals.size(); i++) {
      copy_v3_v3(vertex_normals[i], data.vertex_normals[i]);
      if (normals) {
        copy_v3_v3(normals.span[i], data.vertex_normals[i]);
      }
    }
    normals.finish();
    BKE_mesh_set_custom_normals_from_verts(mesh, vertex_normals);
    MEM_freeN(vertex_normals);
  }

  /* Merge all vertices on the same location. */
  if (params.merge_verts) {
    std::optional<Mesh *> return_value = blender::geometry::mesh_merge_by_distance_all(
        *mesh, IndexMask(mesh->totvert), 0.0001f);
    if (return_value.has_value()) {
      mesh = return_value.value();
    }
  }

  return mesh;
}
}  // namespace blender::io::ply
