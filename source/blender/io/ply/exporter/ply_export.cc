/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_layer.h"

#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "BLI_memory_utils.hh"

#include "ply_data.hh"
#include "ply_export.hh"
#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_export_load_plydata.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"

namespace blender::io::ply {

void exporter_main(bContext *C, const PLYExportParams &export_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  exporter_main(bmain, scene, view_layer, C, export_params);
}

void exporter_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   bContext *C,
                   const PLYExportParams &export_params)
{
  /* Load mesh data into PlyData struct. */
  std::unique_ptr<blender::io::ply::PlyData> plyData = std::make_unique<PlyData>();
  load_plydata(*plyData, CTX_data_ensure_evaluated_depsgraph(C), export_params);

  /* Get filebuffer. */
  std::unique_ptr<FileBuffer> buffer;

  if (export_params.ascii_format) {
    buffer = std::make_unique<FileBufferAscii>(export_params.filepath);
  }
  else {
    buffer = std::make_unique<FileBufferBinary>(export_params.filepath);
  }

  /* Generate and write header. */
  write_header(buffer, plyData, export_params);

  /* Generate and write vertices. */
  write_vertices(buffer, plyData);

  /* Generate and write faces. */
  write_faces(buffer, plyData);

  /* Generate and write edges. */
  write_edges(buffer, plyData);

  /* Clean up. */
  buffer->close_file();
}
}  // namespace blender::io::ply
