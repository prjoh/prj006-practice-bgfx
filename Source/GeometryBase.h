#pragma once


#include <vector>

#include <bgfx/bgfx.h>

#include <Types.h>


namespace zv
{
    struct Vertex
    {
        f32 x;
        f32 y;
        f32 z;
        u32 normal;
        u32 tangent;
        s16 u;
        s16 v;

        static void init()
        {
            s_Layout
                .begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
                .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Uint8, true, true)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
                .end();
        }

        static bgfx::VertexLayout s_Layout;
    };

	class Geometry
	{
	public:
        Geometry() = default;
		~Geometry() = default;
    
    public:
        virtual void cleanup();
        
        void bindBuffers() const;

	protected:
        void initializeBuffers();

	protected:
        std::vector<Vertex> m_vertices{};
        std::vector<u16> m_indices{};

		bgfx::VertexBufferHandle m_hVertexBuffer{ bgfx::kInvalidHandle };
		bgfx::IndexBufferHandle m_hIndexBuffer{ bgfx::kInvalidHandle };
	};
}