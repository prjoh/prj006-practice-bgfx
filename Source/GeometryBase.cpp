#include <GeometryBase.h>


namespace zv
{
	bgfx::VertexLayout Vertex::s_Layout;

	void Geometry::cleanup()
	{
		bgfx::destroy(m_hIndexBuffer);
		bgfx::destroy(m_hVertexBuffer);
	}

	void Geometry::bindBuffers() const
	{
		bgfx::setVertexBuffer(0, m_hVertexBuffer);
		bgfx::setIndexBuffer(m_hIndexBuffer);
	}

	void Geometry::initializeBuffers()
	{
		// Create static vertex buffer.
		m_hVertexBuffer = bgfx::createVertexBuffer(
			bgfx::makeRef(m_vertices.data(), sizeof(Vertex) * m_vertices.size()),
			Vertex::s_Layout
		);

		// Create static index buffer.
		m_hIndexBuffer = bgfx::createIndexBuffer(
			bgfx::makeRef(m_indices.data(), sizeof(u16) * m_indices.size())
		);
	}
}
