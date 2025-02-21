#include <string>
#include <iostream>
#include <chrono>
#include "vector3.h"
#include "isotropicremesher.h"
#include "halfedgemesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static void exportObject(const char *filename, const std::vector<Vector3> &vertices, const std::vector<std::vector<size_t>> &faces)
{
    FILE *fp = fopen(filename, "wb");
    for (const auto &it: vertices) {
        fprintf(fp, "v %f %f %f\n", it.x(), it.y(), it.z());
    }
    for (const auto &it: faces) {
        if (it.size() == 2) {
            fprintf(fp, "l");
            for (const auto &v: it)
                fprintf(fp, " %zu", v + 1);
            fprintf(fp, "\n");
            continue;
        }
        fprintf(fp, "f");
        for (const auto &v: it)
            fprintf(fp, " %zu", v + 1);
        fprintf(fp, "\n");
    }
    fclose(fp);
}

static bool loadObj(const std::string &filename, 
    std::vector<Vector3> &outputVertices, 
    std::vector<std::vector<size_t>> &outputTriangles)
{
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    
    bool loadSuccess = tinyobj::LoadObj(&attributes, &shapes, &materials, &warn, &err, filename.c_str());
    if (!warn.empty()) {
        std::cout << "WARN:" << warn.c_str() << std::endl;
    }
    if (!err.empty()) {
        std::cout << err.c_str() << std::endl;
    }
    if (!loadSuccess) {
        return false;
    }
    
    outputVertices.resize(attributes.vertices.size() / 3);
    for (size_t i = 0, j = 0; i < outputVertices.size(); ++i) {
        auto &dest = outputVertices[i];
        dest.setX(attributes.vertices[j++]);
        dest.setY(attributes.vertices[j++]);
        dest.setZ(attributes.vertices[j++]);
    }
    
    outputTriangles.clear();
    for (const auto &shape: shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
            outputTriangles.push_back(std::vector<size_t> {
                (size_t)shape.mesh.indices[i + 0].vertex_index,
                (size_t)shape.mesh.indices[i + 1].vertex_index,
                (size_t)shape.mesh.indices[i + 2].vertex_index
            });
        }
    }
    
    return true;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        std::vector<Vector3> inputVertices;
        std::vector<std::vector<size_t>> inputTriangles;

        std::string file = argv[1];
        size_t extPos = file.find_last_of('.');
        if (extPos != std::string::npos)
        {
            std::string newFile = file.substr(0, extPos) + "_remesh.obj";
            loadObj(file, inputVertices, inputTriangles);

            IsotropicRemesher isotropicRemesher(&inputVertices, &inputTriangles);
            isotropicRemesher.setSharpEdgeIncludedAngle(90);
            isotropicRemesher.setTargetEdgeLength(isotropicRemesher.initialAverageEdgeLength() * 0.5);
            //isotropicRemesher.setTargetTriangleCount(50000);
            isotropicRemesher.remesh(3);

            FILE* fp = fopen(newFile.c_str(), "wb");
            size_t outputIndex = 0;
            HalfedgeMesh* halfedgeMesh = isotropicRemesher.remeshedHalfedgeMesh();
            for (HalfedgeMesh::Vertex* vertex = halfedgeMesh->moveToNextVertex(nullptr);
                nullptr != vertex;
                vertex = halfedgeMesh->moveToNextVertex(vertex)) {
                vertex->outputIndex = outputIndex++;
                fprintf(fp, "v %f %f %f\n",
                    vertex->position[0],
                    vertex->position[1],
                    vertex->position[2]);
            }
            for (HalfedgeMesh::Face* face = halfedgeMesh->moveToNextFace(nullptr);
                nullptr != face;
                face = halfedgeMesh->moveToNextFace(face)) {
                fprintf(fp, "f %d %d %d\n",
                    (int)face->halfedge->previousHalfedge->startVertex->outputIndex + 1,
                    (int)face->halfedge->startVertex->outputIndex + 1,
                    (int)face->halfedge->nextHalfedge->startVertex->outputIndex + 1);
            }
            fclose(fp);
        }
    }
    
    return 0;
}
