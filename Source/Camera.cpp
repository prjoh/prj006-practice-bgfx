#include <Camera.h>


#include <Input.h>


namespace zv
{
	Camera::Camera(const vec3& position, const vec3& center, const vec3& up, f32 aspect, f32 fov, f32 zNear, f32 zFar)
		: /*m_transform(position),*/ m_position(position), m_orientation(bx::InitIdentity), m_aspect(aspect), m_fov(fov), m_zNear(zNear), m_zFar(zFar)
	{
		////m_transform.lookAt(center, up);
		//vec3 forward = bx::normalize(bx::sub(center, position));
		//f32 dot = bx::dot(g_WorldForward, forward);

		//if (bx::abs(dot - (-1.0f)) < 0.000001f)
		//{
		//	m_orientation = { g_WorldUp.x, g_WorldUp.y, g_WorldUp.z, 3.1415926535897932f };
		//}
		//else if (bx::abs(dot - (1.0f)) < 0.000001f)
		//{
		//	m_orientation = { bx::InitIdentity };
		//}
		//else
		//{
		//	f32 rotAngle = bx::acos(dot);
		//	vec3 rotAxis = bx::cross(g_WorldForward, forward);
		//	rotAxis = bx::normalize(rotAxis);
		//	m_orientation = bx::fromAxisAngle(rotAxis, rotAngle);
		//}

		//bx::mtxIdentity(m_projectionMatrix);
		//bx::mtxIdentity(m_viewMatrix);
		bx::mtxProj(m_projectionMatrix, m_fov, m_aspect, m_zNear, m_zFar, bgfx::getCaps()->homogeneousDepth);
	}

	void Camera::update(f32 elapsedTimeS)
	{
		//// TODO: Mouse button down + delta not working!
		////if (Input::mouseButtonDown(SDL_BUTTON_RIGHT))
		////{
		//	const f32 sensitivity = 0.05f;

		//	// TODO: Why negative???

		//	f32 pitch = -1.0f * (f32)Input::mouseDeltaY() * sensitivity;
		//	f32 yaw = (f32)Input::mouseDeltaX() * sensitivity;
		//	quat q_pitch = bx::fromAxisAngle(g_XAxis, bx::toRad(pitch));
		//	quat q_yaw = bx::fromAxisAngle(g_YAxis, bx::toRad(yaw));

		//	if (yaw > 0.0f)
		//		int x = 0;

		//	//m_transform.orientation(bx::normalize(bx::mul(bx::mul(q_pitch, m_transform.orientation()), q_yaw)));
		//	m_orientation = bx::normalize(bx::mul(bx::mul(q_pitch, m_orientation), q_yaw));
		////}

		//const f32 speed = 2.5f * elapsedTimeS;

		//const vec3 forward = bx::normalize(
		//	bx::mul(
		//		g_WorldForward,
		//		m_orientation
		//	)
		//);
		//const vec3 right = bx::normalize(
		//	bx::mul(
		//		g_WorldRight,
		//		m_orientation
		//	)
		//);

		//if (Input::keyPressed(SDLK_w))
		//	m_position = bx::add(m_position, bx::mul(forward, speed));
		//if (Input::keyPressed(SDLK_s))
		//	m_position = bx::sub(m_position, bx::mul(forward, speed));
		//if (Input::keyPressed(SDLK_a))
		//	m_position = bx::sub(m_position, bx::mul(right, speed));
		//if (Input::keyPressed(SDLK_d))
		//	m_position = bx::add(m_position, bx::mul(right, speed));
	}
}
