#include <Materials.h>


namespace zv
{
    TestMaterial::TestMaterial(const bgfx::ProgramHandle& programHandle, const bgfx::TextureHandle& diffuseTexture, const bgfx::TextureHandle& normalTexture, f32* time)
        : m_time(time)
    {
        setProgram(programHandle);

        setTexture(eTextureType::Diffuse, diffuseTexture);
        setTexture(eTextureType::Normal, normalTexture);
    }

    void TestMaterial::cleanup()
    {
        bgfx::destroy(m_hULightPosRadius);
        bgfx::destroy(m_hULightRgbInnerR);

        base_type::cleanup();
    }

    void TestMaterial::updateUniforms()
	{
        f32 lightPosRadius[4][4];
        for (u32 ii = 0; ii < NumLights; ++ii)
        {
            lightPosRadius[ii][0] = bx::sin((*m_time * (0.1f + ii * 0.17f) + ii * bx::kPiHalf * 1.37f)) * 3.0f;
            lightPosRadius[ii][1] = bx::cos((*m_time * (0.2f + ii * 0.29f) + ii * bx::kPiHalf * 1.49f)) * 3.0f;
            lightPosRadius[ii][2] = -2.5f;
            lightPosRadius[ii][3] = 3.0f;
        }

        bgfx::setUniform(m_hULightPosRadius, lightPosRadius, NumLights);

        f32 lightRgbInnerR[4][4] =
        {
            { 1.0f, 0.7f, 0.2f, 0.8f },
            { 0.7f, 0.2f, 1.0f, 0.8f },
            { 0.2f, 1.0f, 0.7f, 0.8f },
            { 1.0f, 0.4f, 0.2f, 0.8f },
        };

        bgfx::setUniform(m_hULightRgbInnerR, lightRgbInnerR, NumLights);
	}
}