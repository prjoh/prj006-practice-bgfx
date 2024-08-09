#pragma once


#include <bgfx/bgfx.h>


namespace zv
{
	enum class eTextureType {
		Diffuse = 0,
		Normal = 1,
	};

	class Material
	{
	public:
		Material() = default;
		~Material() = default;

	public:
		virtual void updateUniforms() = 0;
		virtual void cleanup();

		void bindTextures() const;
		void bindProgram() const;

	protected:
		void setProgram(const bgfx::ProgramHandle& programHandle) { m_hProgram = programHandle; }
		void setTexture(eTextureType type, const bgfx::TextureHandle& textureHandle);

	protected:
		bgfx::ProgramHandle m_hProgram{ bgfx::kInvalidHandle };

		bgfx::TextureHandle m_hTextureDiffuse{ bgfx::kInvalidHandle };
		bgfx::TextureHandle m_hTextureNormal{ bgfx::kInvalidHandle };

	private:
		bgfx::UniformHandle m_hUTextureDiffuse = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
		bgfx::UniformHandle m_hUTextureNormal = bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);
	};
}