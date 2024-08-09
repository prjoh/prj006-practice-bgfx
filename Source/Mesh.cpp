#include <Mesh.h>


#include <bgfx/bgfx.h>


namespace zv
{
	Mesh::Mesh(std::unique_ptr<Geometry>&& geometry, std::unique_ptr<Material>&& material)
	{
		acquireGeometry(geometry);
		acquireMaterial(material);
	}

	void Mesh::render() const
	{
		m_pMaterial->updateUniforms();

		bgfx::setTransform(m_modelMatrix);

		m_pGeometry->bindBuffers();

		m_pMaterial->bindTextures();

		// Set render states.
		bgfx::setState(0
			| BGFX_STATE_WRITE_RGB
			| BGFX_STATE_WRITE_A
			| BGFX_STATE_WRITE_Z
			| BGFX_STATE_DEPTH_TEST_LESS
			| BGFX_STATE_MSAA
		);

		m_pMaterial->bindProgram();
	}
}