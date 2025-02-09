/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <string>
#include <type_traits>

#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "ply_file_buffer.hh"

/* SEP macro from BLI path utils clashes with SEP symbol in fmt headers. */
#undef SEP
#define FMT_HEADER_ONLY
#include <bitset>
#include <fmt/format.h>

namespace blender::io::ply {
class FileBufferBinary : public FileBuffer {
 public:
  using FileBuffer::FileBuffer;

  void write_vertex(float x, float y, float z) override
  {
    float3 vector(x, y, z);
    char *bits = reinterpret_cast<char *>(&vector);
    Span<char> span(bits, sizeof(float3));

    write_bytes(span);
  }

  void write_UV(float u, float v) override
  {
    float2 vector(u, v);
    char *bits = reinterpret_cast<char *>(&vector);
    Span<char> span(bits, sizeof(float2));

    write_bytes(span);
  }

  void write_vertex_normal(float nx, float ny, float nz) override
  {
    float3 vector(nx, ny, nz);
    char *bits = reinterpret_cast<char *>(&vector);
    Span<char> span(bits, sizeof(float3));

    write_bytes(span);
  }

  void write_vertex_color(uchar r, uchar g, uchar b, uchar a) override
  {
    uchar4 vector(r, g, b, a);
    char *bits = reinterpret_cast<char *>(&vector);
    Span<char> span(bits, sizeof(uchar4));

    write_bytes(span);
  }

  void write_vertex_end() override
  {
    /* In binary, there is no end to a vertex. */
  }

  void write_face(char size, Vector<uint32_t> const &vertex_indices) override
  {
    /* Pre allocate memory so no further allocation has to be done for typical faces. */
    Vector<char, 128> data;
    data.append(size);
    for (auto &&vertexIndex : vertex_indices) {
      uint32_t x = vertexIndex;
      auto *vtxbits = static_cast<char *>(static_cast<void *>(&x));
      data.insert(data.end(), vtxbits, vtxbits + sizeof(uint32_t));
    }

    Span<char> span(data);
    write_bytes(span);
  }

  void write_edge(int first, int second) override
  {
    int2 vector(first, second);
    char *bits = reinterpret_cast<char *>(&vector);
    Span<char> span(bits, sizeof(int2));

    write_bytes(span);
  }
};
}  // namespace blender::io::ply
