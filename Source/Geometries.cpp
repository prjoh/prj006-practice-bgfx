#include <Geometries.h>

#include <Utils.h>


namespace zv
{
    PlaneGeometry::PlaneGeometry(f32 width, f32 height, u32 widthSegments, u32 heightSegments)
    {
        Vertex::init();

        f32 widthHalf = width * 0.5f;
        f32 heightHalf = height * 0.5f;

        u32 gridX = widthSegments;
        u32 gridY = heightSegments;

        u32 gridX1 = gridX + 1;
        u32 gridY1 = gridY + 1;

        f32 segment_width = (f32)width / (f32)gridX;
        f32 segment_height = (f32)height / (f32)gridY;

        for (s16 iy = 0; iy < gridY1; iy++) {

            f32 y = iy * segment_height - heightHalf;

            for (s16 ix = 0; ix < gridX1; ix++) {

                f32 x = ix * segment_width - widthHalf;

                s16 u = (s16)(ix / gridX * 0x7fff);  // TODO: What is 0x7fff?
                s16 v = (s16)((1 - (iy / gridY)) * 0x7fff);  // TODO: What is 0x7fff?

                m_vertices.emplace_back(
                    Vertex{
                        x, -y, 0.0f,
                        utils::encodeNormalRgba8(0, 0, -1),  // TODO: This depends on the initial placement of plane + handedness
                        0,
                        u, v
                    }
                );
            }
        }

        for (u32 iy = 0; iy < gridY; iy++) {

            for (u32 ix = 0; ix < gridX; ix++) {

                u16 a = ix + gridX1 * iy;
                u16 b = ix + gridX1 * (iy + 1);
                u16 c = (ix + 1) + gridX1 * (iy + 1);
                u16 d = (ix + 1) + gridX1 * iy;

                m_indices.insert(m_indices.end(), { a, b, d });
                m_indices.insert(m_indices.end(), { b, c, d });

            }
        }

        utils::calcTangents(
            m_vertices.data(),
            m_vertices.size(),
            Vertex::s_Layout,
            m_indices.data(),
            m_indices.size()
        );

        initializeBuffers();
    }

	CubeGeometry::CubeGeometry(f32 width, f32 height, f32 depth, u32 widthSegments, u32 heightSegments, u32 depthSegments)
	{
        Vertex::init();

		u16 numVertices = 0;

		buildPlane(2, 1, 0, -1, -1, depth, height, width, depthSegments, heightSegments, m_vertices, m_indices, &numVertices); // px
		buildPlane(0, 2, 1, 1, 1, width, depth, height, widthSegments, depthSegments, m_vertices, m_indices, &numVertices); // py
		buildPlane(0, 1, 2, 1, -1, width, height, depth, widthSegments, heightSegments, m_vertices, m_indices, &numVertices); // pz

		buildPlane(2, 1, 0, 1, -1, depth, height, -width, depthSegments, heightSegments, m_vertices, m_indices, &numVertices); // nx
		buildPlane(0, 2, 1, 1, -1, width, depth, -height, widthSegments, depthSegments, m_vertices, m_indices, &numVertices); // ny
		buildPlane(0, 1, 2, -1, -1, width, height, -depth, widthSegments, heightSegments, m_vertices, m_indices, &numVertices); // nz

        utils::calcTangents(
            m_vertices.data(),
            m_vertices.size(),
            Vertex::s_Layout,
            m_indices.data(),
            m_indices.size()
        );

        initializeBuffers();
    }

	void CubeGeometry::buildPlane(
		s32 u, s32 v, s32 w, 
		s32 uDir, s32 vDir, 
		f32 width, f32 height, 
		f32 depth, s16 gridX, s16 gridY,
		std::vector<Vertex>& vertices, std::vector<u16>& indices, u16* numVertices)
	{
        f32 segmentWidth = width / (f32)gridX;
        f32 segmentHeight = height / (f32)gridY;

        f32 widthHalf = width * 0.5f;
        f32 heightHalf = height * 0.5f;
        f32 depthHalf = depth * 0.5f;

        s16 gridX1 = gridX + 1;
        s16 gridY1 = gridY + 1;

        u32 vertexCounter = 0;

        f32 position[3];
        f32 normal[3];

        // generate vertices, normals and uvs

        for (s16 iy = 0; iy < gridY1; iy++) {

            f32 y = (f32)iy * segmentHeight - heightHalf;

            for (s16 ix = 0; ix < gridX1; ix++) {

                f32 x = (f32)ix * segmentWidth - widthHalf;

                // set values to correct vector component

                position[u] = x * (f32)uDir;
                position[v] = y * (f32)vDir;
                position[w] = depthHalf;

                // set values to correct vector component

                normal[u] = 0;
                normal[v] = 0;
                normal[w] = depth > 0 ? 1 : -1;

                vertices.emplace_back(
                    Vertex{
                        position[0], 
                        position[1], 
                        position[2],
                        utils::encodeNormalRgba8(normal[0], normal[1], normal[2]),
                        0,  // TODO: Calculate tangent?
                        (s16)(ix / gridX * 0x7fff),  // TODO: What is 0x7fff?
                        (s16)((1 - (iy / gridY)) * 0x7fff),  // TODO: What is 0x7fff?
                    }
                );

                vertexCounter += 1;
            }

        }

        // indices

        // 1. you need three indices to draw a single face
        // 2. a single segment consists of two faces
        // 3. so we need to generate six (2*3) indices per segment

        for (u32 iy = 0; iy < gridY; iy++) {

            for (u32 ix = 0; ix < gridX; ix++) {

                u16 a = *numVertices + ix + gridX1 * iy;
                u16 b = *numVertices + ix + gridX1 * (iy + 1);
                u16 c = *numVertices + (ix + 1) + gridX1 * (iy + 1);
                u16 d = *numVertices + (ix + 1) + gridX1 * iy;

                // faces
                indices.insert(indices.end(), { a, b, d });
                indices.insert(indices.end(), { b, c, d });

            }

        }

        *numVertices += vertexCounter;
	}

    CylinderGeometry::CylinderGeometry(f32 radiusTop, f32 radiusBottom, f32 height, u32 radialSegments, u32 heightSegments, f32 thetaStart, f32 thetaLength)
    {
        u16 index = 0;
        std::vector<std::vector<u16>> indexArray;

        auto generateTorso = [](
            f32 radiusTop, f32 radiusBottom, f32 height, 
            u32 radialSegments, u32 heightSegments, 
            f32 thetaStart, f32 thetaLength, 
            std::vector<Vertex>& vertices, std::vector<u16>& indices, 
            u16* index, std::vector<std::vector<u16>>& indexArray)
        {
            f32 halfHeight = height * 0.5f;

            // this will be used to calculate the normal
            f32 slope = (radiusBottom - radiusTop) / height;

            // generate vertices, normals and uvs
            for (u32 iy = 0; iy <= heightSegments; iy++) {

                std::vector<u16> indexRow;

                s16 v = iy / heightSegments;

                // calculate the radius of the current row
                f32 radius = (f32)v * (radiusBottom - radiusTop) + radiusTop;

                for (u32 ix = 0; ix <= radialSegments; ix++) {

                    s16 u = ix / radialSegments;

                    f32 theta = (f32)u * thetaLength + thetaStart;

                    f32 sinTheta = sin(theta);
                    f32 cosTheta = cos(theta);

                    // vertex
                    f32 x = radius * sinTheta;
                    f32 y = -(f32)v * height + halfHeight;
                    f32 z = radius * cosTheta;

                    // normal
                    vec3 normal = bx::normalize({ sinTheta, slope, cosTheta });

                    vertices.emplace_back(
                        Vertex{
                            x, y, z,
                            utils::encodeNormalRgba8(normal.x, normal.y, normal.z),
                            0,  // TODO: Calculate tangent?
                            (s16)(u * 0x7fff),  // TODO: What is 0x7fff?
                            (s16)((1 - v) * 0x7fff),  // TODO: What is 0x7fff?
                        }
                    );

                    // save index of vertex in respective row
                    indexRow.push_back((*index)++);
                }

                // now save vertices of the row in our index array
                indexArray.emplace_back(indexRow);

            }

            // generate indices
            for (u16 x = 0; x < radialSegments; x++) {

                for (u16 y = 0; y < heightSegments; y++) {

                    // we use the index array to access the correct indices
                    u16 a = indexArray[y][x];
                    u16 b = indexArray[y + 1][x];
                    u16 c = indexArray[y + 1][x + 1];
                    u16 d = indexArray[y][x + 1];

                    // faces
                    indices.insert(indices.end(), { a, b, d });
                    indices.insert(indices.end(), { b, c, d });
                }
            }
        };

        auto generateCap = [](bool top, f32 radiusTop, f32 radiusBottom, f32 height, u32 radialSegments, f32 thetaStart, f32 thetaLength, std::vector<Vertex>& vertices, std::vector<u16>& indices, u16* index)
        {
            f32 halfHeight = height * 0.5f;
            
            // save the index of the first center vertex
            u16 centerIndexStart = *index;

            f32 radius = top ? radiusTop : radiusBottom;
            f32 sign = top ? 1.0f : -1.0f;

            // first we generate the center vertex data of the cap.
            // because the geometry needs one set of uvs per face,
            // we must generate a center vertex per face/segment

            for (u32 ix = 1; ix <= radialSegments; ix++)
            {
                vertices.emplace_back(
                    Vertex{
                        0, halfHeight* sign, 0,
                        utils::encodeNormalRgba8(0, sign, 0),
                        0,  // TODO: Calculate tangent?
                        (s16)(0.5f * 0x7fff),  // TODO: What is 0x7fff?
                        (s16)(0.5f * 0x7fff),  // TODO: What is 0x7fff?
                    }
                );

                // increase index
                (*index)++;
            }

            // save the index of the last center vertex
            u16 centerIndexEnd = *index;

            // now we generate the surrounding vertices, normals and uvs
            for (u32 ix = 0; ix <= radialSegments; ix++) {

                f32 theta = (f32)ix / (f32)radialSegments * thetaLength + thetaStart;

                f32 cosTheta = cos(theta);
                f32 sinTheta = sin(theta);

                // vertex
                f32 x = radius * sinTheta;
                f32 y = halfHeight * sign;
                f32 z = radius * cosTheta;

                // uv
                s16 u = (cosTheta * 0.5f) + 0.5f;
                s16 v = (sinTheta * 0.5f * sign) + 0.5f;

                vertices.emplace_back(
                    Vertex{
                        x, y, z,
                        utils::encodeNormalRgba8(0, sign, 0),
                        0,  // TODO: Calculate tangent?
                        (s16)(u * 0x7fff),  // TODO: What is 0x7fff?
                        (s16)(v * 0x7fff),  // TODO: What is 0x7fff?
                    }
                );

                // increase index
                (*index)++;
            }

            // generate indices
            for (u16 ix = 0; ix < radialSegments; ix++) {

                u16 c = centerIndexStart + ix;
                u16 i = centerIndexEnd + ix;

                if (top) {
                    // face top
                    indices.insert(indices.end(), { i, (u16)(i + 1), c });
                }
                else {
                    // face bottom
                    indices.insert(indices.end(), { (u16)(i + 1), i, c });
                }
            }
        };

        generateTorso(
            radiusTop, radiusBottom, height,
            radialSegments, heightSegments,
            thetaStart, thetaLength,
            m_vertices, m_indices,
            &index, indexArray);
        if (radiusTop > 0.0f) generateCap(true, radiusTop, radiusBottom, height, radialSegments, thetaStart, thetaLength, m_vertices, m_indices, &index);
        if (radiusBottom > 0.0f) generateCap(false, radiusTop, radiusBottom, height, radialSegments, thetaStart, thetaLength, m_vertices, m_indices, &index);

        utils::calcTangents(
            m_vertices.data(),
            m_vertices.size(),
            Vertex::s_Layout,
            m_indices.data(),
            m_indices.size()
        );

        initializeBuffers();
    }
}