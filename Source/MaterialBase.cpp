#include <MaterialBase.h>


namespace zv
{
	void Material::setTexture(eTextureType type, const bgfx::TextureHandle& handle)
	{
		switch (type)
		{
		case eTextureType::Diffuse:
			m_hTextureDiffuse = handle;
			break;
		case eTextureType::Normal:
			m_hTextureNormal = handle;
			break;
		default:
			break;
			// TODO: ERROR
		}
	}

	void Material::cleanup()
	{
		bgfx::destroy(m_hUTextureDiffuse);
		bgfx::destroy(m_hUTextureNormal);
	}

	void Material::bindTextures() const
	{
		if (bgfx::isValid(m_hTextureDiffuse))
			bgfx::setTexture(0, m_hUTextureDiffuse, m_hTextureDiffuse);

		if (bgfx::isValid(m_hTextureNormal))
			bgfx::setTexture(1, m_hUTextureNormal, m_hTextureNormal);
	}

	void Material::bindProgram() const
	{
		// TODO: ViewId, depth, flags
		bgfx::submit(0, m_hProgram);
	}
}