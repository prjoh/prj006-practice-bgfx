#pragma once


#include <MaterialBase.h>
#include <Types.h>


namespace zv
{
	class TestMaterial : public Material
	{
		using base_type = Material;

	public:
		TestMaterial(const bgfx::ProgramHandle& programHandle, const bgfx::TextureHandle& diffuseTexture, const bgfx::TextureHandle& normalTexture, f32* time);
		~TestMaterial() = default;

		TestMaterial() = delete;

	public:
		void cleanup() override;

		void updateUniforms() override;

	private:
		const u16 NumLights = 4;

		f32* m_time{ nullptr };

		bgfx::UniformHandle m_hULightPosRadius = bgfx::createUniform("u_lightPosRadius", bgfx::UniformType::Vec4, NumLights);
		bgfx::UniformHandle m_hULightRgbInnerR = bgfx::createUniform("u_lightRgbInnerR", bgfx::UniformType::Vec4, NumLights);
	};
}