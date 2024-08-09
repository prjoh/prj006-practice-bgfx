#pragma once


#include <bgfx/bgfx.h>

#include <Transform.h>
#include <Types.h>


namespace zv
{
	class Camera
	{
	public:
        Camera() = delete;
        Camera(const vec3& position,
               const vec3& center = { 0.0f, 0.0f, 0.0f },
               const vec3& up = g_WorldUp,
               f32 aspect = 800.0f / 600.0f,
               f32 fov = 45.0f,
               f32 zNear = 0.01f,
               f32 zFar = 1000.0f);
        ~Camera() = default;

    public:
        //const vec3 position() { return m_transform.position(); }
        const f32* viewMatrix(bool updateMatrix = true)
        {
            if (updateMatrix)
                bx::mtxLookAt(m_viewMatrix, m_position, bx::add(m_position, forward()));

            //f32 translation[16];
            //bx::mtxIdentity(translation);
            //vec3 translate = m_position;
            //bx::mtxTranslate(translation, translate.x, translate.y, translate.z);

            //f32 rotation[16];
            //bx::mtxFromQuaternion(rotation, m_orientation);

            //bx::mtxInverse(translation, translation);
            //bx::mtxInverse(rotation, rotation);

            //bx::mtxMul(m_viewMatrix, translation, rotation);

            //bx::mtxLookAt(m_viewMatrix, m_transform.position(), bx::add(m_transform.position(), m_transform.front()));
            return m_viewMatrix;
        }
        const f32* projectionMatrix(bool updateMatrix = true)
        {
            if (updateMatrix)
                bx::mtxProj(m_projectionMatrix, m_fov, m_aspect, m_zNear, m_zFar, bgfx::getCaps()->homogeneousDepth);

            return m_projectionMatrix;
        }

        void update(f32 elapsedTimeS);

        vec3 forward() const { 
            return bx::normalize(
                bx::mul(
            	    g_WorldForward,
            	    m_orientation
                )
            );
        }

    private:
        //Transform m_transform;  // TODO: Make this pointer >> A Camera is a Component which requires a Transform Component to be present on the current Entity!

        // TODO: Remove and do all calcs inside matrices!
        vec3 m_position;
        quat m_orientation;

        f32 m_viewMatrix[16];
        f32 m_projectionMatrix[16];

        f32 m_fov;
        f32 m_aspect;
        f32 m_zNear;
        f32 m_zFar;

        //bool inverted_pitch = true;
	};
}
