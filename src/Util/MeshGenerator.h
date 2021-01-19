/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_UTIL_MESH_GENERATOR_H
#define CNOID_UTIL_MESH_GENERATOR_H

#include "EigenTypes.h"
#include "SceneDrawables.h"
#include "exportdecl.h"

namespace cnoid {

class MeshFilter;

class CNOID_EXPORT MeshGenerator
{
public:
    MeshGenerator();
    MeshGenerator(const MeshGenerator& org);
    ~MeshGenerator();

    void setDivisionNumber(int n);
    int divisionNumber() const { return divisionNumber_; }
    static int defaultDivisionNumber();

    void setExtraDivisionNumber(int n) { extraDivisionNumber_ = n; }
    int extraDivisionNumber(int n) { return extraDivisionNumber_; }

    void setNormalGenerationEnabled(bool on);
    // \deprecated
    void enableNormalGeneration(bool on);
    bool isNormalGenerationEnabled() const;

    void setBoundingBoxUpdateEnabled(bool on);
    bool isBoundingBoxUpdateEnabled() const;

    SgMesh* generateBox(Vector3 size, bool enableTextureCoordinate = false);
    SgMesh* generateSphere(double radius, bool enableTextureCoordinate = false);
    SgMesh* generateCylinder(double radius, double height, bool bottom = true,
            bool top = true, bool side = true, bool enableTextureCoordinate = false);
    SgMesh* generateCone(double radius, double height,
            bool bottom = true, bool side = true, bool enableTextureCoordinate = false);
    SgMesh* generateCapsule(double radius, double height);
    SgMesh* generateDisc(double radius, double innerRadius);
    SgMesh* generateArrow(double cylinderRadius, double cylinderHeight, double coneRadius, double coneHeight);
    SgMesh* generateTorus(double radius, double crossSectionRadius);
    SgMesh* generateTorus(double radius, double crossSectionRadius, double beginAngle, double endAngle);

    typedef std::vector<Vector2, Eigen::aligned_allocator<Vector2>> Vector2Array;
    typedef std::vector<Vector3, Eigen::aligned_allocator<Vector3>> Vector3Array;
    typedef std::vector<AngleAxis, Eigen::aligned_allocator<AngleAxis>> AngleAxisArray;

    struct Extrusion
    {
        Vector2Array crossSection;
        Vector3Array spine;
        AngleAxisArray orientation;
        Vector2Array scale;
        double creaseAngle;
        bool beginCap;
        bool endCap;
        Extrusion() {
            creaseAngle = 0.0;
            beginCap = true;
            endCap = true;
        }
    };

    SgMesh* generateExtrusion(const Extrusion& extrusion, bool enableTextureCoordinate = false);
    SgLineSet* generateExtrusionLineSet(const Extrusion& extrusion, SgMesh* mesh);

    struct ElevationGrid
    {
        int xDimension;
        int zDimension;
        double xSpacing;
        double zSpacing;
        std::vector<double> height;
        bool ccw;
        double creaseAngle;
        ElevationGrid() {
            xDimension = 0;
            zDimension = 0;
            xSpacing = 1.0;
            zSpacing = 1.0;
            ccw = true;
            creaseAngle = 0.0;
        }
    };

    SgMesh* generateElevationGrid(const ElevationGrid& elevationGrid, bool enableTextureCoordinate = false);

    void generateTextureCoordinateForIndexedFaceSet(SgMesh* mesh);

private:
    int divisionNumber_;
    int extraDivisionNumber_;
    bool isNormalGenerationEnabled_;
    bool isBoundingBoxUpdateEnabled_;
    MeshFilter* meshFilter;

    MeshFilter* getOrCreateMeshFilter();
    SgMesh* generateBoxWithExtraTriangles(Vector3 size);
    void generateBoxPlaneMesh(
        const Vector3& size, SgMesh* mesh, SgVertexArray* vertices, int axis1, int axis2, int axis3, int* divisions);
    void generateNormals(SgMesh* mesh, double creaseAngle);
    void generateTextureCoordinateForBox(SgMesh* mesh);
    void generateTextureCoordinateForSphere(SgMesh* mesh);
    void generateTextureCoordinateForCylinder(SgMesh* mesh);
    void generateTextureCoordinateForCone(SgMesh* mesh);
    void generateTextureCoordinateForExtrusion(
        SgMesh* mesh, const Vector2Array& crossSections, const Vector3Array& spinePoints,
        int numTriOfbeginCap, int numTriOfendCap, int indexOfendCap);
    void generateTextureCoordinateForElevationGrid(SgMesh* mesh, const ElevationGrid& grid);
};

}

#endif
