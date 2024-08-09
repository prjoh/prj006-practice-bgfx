#pragma once


#include <memory>

#include <bx/bx.h>

#include <GeometryBase.h>
#include <MaterialBase.h>


namespace zv
{
	class Object3D
	{
	public:
		Object3D() { bx::mtxIdentity(m_modelMatrix); }
		~Object3D() = default;

	public:
		virtual void cleanup()
		{
			m_pMaterial->cleanup();
			m_pGeometry->cleanup();
		};
		virtual void render() const = 0;

	protected:
		void acquireGeometry(std::unique_ptr<Geometry>& geometry) { m_pGeometry = std::move(geometry); }
		void acquireMaterial(std::unique_ptr<Material>& material) { m_pMaterial = std::move(material); }

	protected:
		std::unique_ptr<Geometry> m_pGeometry{ nullptr };
		std::unique_ptr<Material> m_pMaterial{ nullptr };
		f32 m_modelMatrix[16];
		// Transform / Matrix
		
		// UUID
		// Render order
		// Children ?
	};
}