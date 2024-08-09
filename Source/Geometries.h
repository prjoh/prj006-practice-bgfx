#pragma once


#include <vector>

#include <GeometryBase.h>
#include <Types.h>
#include <Utils.h>


namespace zv
{
	class PlaneGeometry : public Geometry
	{
	public:
		PlaneGeometry(
			f32 width = 1.0f, f32 height = 1.0f,
			u32 widthSegments = 1, u32 heightSegments = 1);
		~PlaneGeometry() = default;
	};

	class CubeGeometry : public Geometry
	{
	public:
		CubeGeometry(
			f32 width = 1.0f, f32 height = 1.0f, f32 depth = 1.0f, 
			u32 widthSegments = 1, u32 heightSegments = 1, u32 depthSegments = 1);
		~CubeGeometry() = default;

	private:
		void buildPlane(s32 u, s32 v, s32 w, s32 uDir, s32 vDir, f32 width, f32 height, f32 depth, s16 gridX, s16 gridY,
						 std::vector<Vertex>& vertices, std::vector<u16>& indices, u16* num_vertices);
	};

	// TODO: Cylinder, Cone

	class CylinderGeometry : public Geometry
	{
	public:
		CylinderGeometry(f32 radiusTop = 1.0f, f32 radiusBottom = 1, f32 height = 1, u32 radialSegments = 32, u32 heightSegments = 1, f32 thetaStart = 0.0f, f32 thetaLength = bx::kPi * 2.0f);
		~CylinderGeometry() = default;
	};
}