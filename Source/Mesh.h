#pragma once


#include <memory>

#include <Object3D.h>
#include <GeometryBase.h>
#include <MaterialBase.h>


namespace zv
{
	class Mesh : public Object3D
	{
	public:
		Mesh(std::unique_ptr<Geometry>&& geometry, std::unique_ptr<Material>&& material);
		~Mesh() = default;

		Mesh() = delete;

	public:
		void render() const override;
	};
}