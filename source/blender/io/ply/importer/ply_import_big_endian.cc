#include "ply_import_big_endian.hh"
#include <fstream>

namespace blender::io::ply {
Mesh *import_ply_big_endian(std::ifstream &file, PlyHeader *header)
{
  PlyData *data = load_ply_big_endian(file, header);
  if (data != nullptr) {
    return convert_ply_to_mesh(data);
  }
  return nullptr;
}

Mesh *convert_ply_to_mesh(PlyData *data)
{
  return nullptr;
}

float3 read_float3(std::ifstream &file) {
  float3 currFloat3;

  for (int i = 0; i < 3; i++) {
    float temp;
    file.read((char *)&temp, sizeof(temp));
    if (!file.good()) {
      printf("Error reading data");
      break;
    }
    temp = swap_bits<float>(temp);
    currFloat3[i] = temp;
  }

  return currFloat3;
}

uchar3 read_uchar3(std::ifstream& file) {
  uchar3 currUchar3;

  for (int i = 0; i < 3; i++) {
    uchar temp;
    file.read((char*)&temp, sizeof(temp));
    if (!file.good()) {
      printf("Error reading data");
      break;
    }
    // No swapping of bytes necessary as uchar is only 1 byte
    currUchar3[i] = temp;
  }

  return currUchar3;
}

float3 convert_uchar3_float3(uchar3 input) {
  float3 returnVal;
  for (int i = 0; i < 3; i++) {
    returnVal[i] = input[i] / 255.0f;
  }
  return returnVal;
}

PlyData *load_ply_big_endian(std::ifstream &file, PlyHeader *header)
{
  PlyData data;

  for (int i = 0; i < header->vertex_count; i++) {
    float3 currFloat3;

    for (auto prop : header->properties) {
      if (prop.first == "x") {
        currFloat3 = read_float3(file);
      } else if (prop.first == "z") {
        data.vertices.append(currFloat3);
      } else if (prop.first == "nx") {
        currFloat3 = read_float3(file);
      } else if (prop.first == "nz") {
        data.vertex_normals.append(currFloat3);
      } else if (prop.first == "red") {
        currFloat3 = convert_uchar3_float3(read_uchar3(file));
      } else if (prop.first == "blue") {
        data.vertex_colors.append(currFloat3);
      }
    }
  }

  std::cout << std::endl;

  std::cout << "Vertex count: " << data.vertices.size() << std::endl;
  std::cout << "\tFirst: " << data.vertices.first() << std::endl;
  std::cout << "\tLast: " << data.vertices.last() << std::endl;
  std::cout << "Normals count: " << data.vertex_normals.size() << std::endl;
  std::cout << "\tFirst: " << data.vertex_normals.first() << std::endl;
  std::cout << "\tLast: " << data.vertex_normals.last() << std::endl;
  std::cout << "Colours count: " << data.vertex_colors.size() << std::endl;
  std::cout << "\tFirst: " << data.vertex_colors.first() << std::endl;
  std::cout << "\tLast: " << data.vertex_colors.last() << std::endl;

  return nullptr;
}

}  // namespace blender::io::ply
