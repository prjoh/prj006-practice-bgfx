/*
 * Copyright 2016-2021 The Brenwill Workshop Ltd.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * At your option, you may choose to accept this material under either:
 *  1. The Apache License, Version 2.0, found at <http://www.apache.org/licenses/LICENSE-2.0>, or
 *  2. The MIT License, found at <http://opensource.org/licenses/MIT>.
 */

#include "spirv_msl.hpp"
#include "GLSL.std.450.h"

#include <algorithm>
#include <assert.h>
#include <numeric>

using namespace spv;
using namespace SPIRV_CROSS_NAMESPACE;
using namespace std;

static const uint32_t k_unknown_location = ~0u;
static const uint32_t k_unknown_component = ~0u;
static const char *force_inline = "static inline __attribute__((always_inline))";

CompilerMSL::CompilerMSL(std::vector<uint32_t> spirv_)
    : CompilerGLSL(std::move(spirv_))
{
}

CompilerMSL::CompilerMSL(const uint32_t *ir_, size_t word_count)
    : CompilerGLSL(ir_, word_count)
{
}

CompilerMSL::CompilerMSL(const ParsedIR &ir_)
    : CompilerGLSL(ir_)
{
}

CompilerMSL::CompilerMSL(ParsedIR &&ir_)
    : CompilerGLSL(std::move(ir_))
{
}

void CompilerMSL::add_msl_shader_input(const MSLShaderInterfaceVariable &si)
{
	inputs_by_location[{si.location, si.component}] = si;
	if (si.builtin != BuiltInMax && !inputs_by_builtin.count(si.builtin))
		inputs_by_builtin[si.builtin] = si;
}

void CompilerMSL::add_msl_shader_output(const MSLShaderInterfaceVariable &so)
{
	outputs_by_location[{so.location, so.component}] = so;
	if (so.builtin != BuiltInMax && !outputs_by_builtin.count(so.builtin))
		outputs_by_builtin[so.builtin] = so;
}

void CompilerMSL::add_msl_resource_binding(const MSLResourceBinding &binding)
{
	StageSetBinding tuple = { binding.stage, binding.desc_set, binding.binding };
	resource_bindings[tuple] = { binding, false };

	// If we might need to pad argument buffer members to positionally align
	// arg buffer indexes, also maintain a lookup by argument buffer index.
	if (msl_options.pad_argument_buffer_resources)
	{
		StageSetBinding arg_idx_tuple = { binding.stage, binding.desc_set, k_unknown_component };

#define ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP(rez) \
	arg_idx_tuple.binding = binding.msl_##rez; \
	resource_arg_buff_idx_to_binding_number[arg_idx_tuple] = binding.binding

		switch (binding.basetype)
		{
		case SPIRType::Void:
		case SPIRType::Boolean:
		case SPIRType::SByte:
		case SPIRType::UByte:
		case SPIRType::Short:
		case SPIRType::UShort:
		case SPIRType::Int:
		case SPIRType::UInt:
		case SPIRType::Int64:
		case SPIRType::UInt64:
		case SPIRType::AtomicCounter:
		case SPIRType::Half:
		case SPIRType::Float:
		case SPIRType::Double:
			ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP(buffer);
			break;
		case SPIRType::Image:
			ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP(texture);
			break;
		case SPIRType::Sampler:
			ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP(sampler);
			break;
		case SPIRType::SampledImage:
			ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP(texture);
			ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP(sampler);
			break;
		default:
			SPIRV_CROSS_THROW("Unexpected argument buffer resource base type. When padding argument buffer elements, "
			                  "all descriptor set resources must be supplied with a base type by the app.");
		}
#undef ADD_ARG_IDX_TO_BINDING_NUM_LOOKUP
	}
}

void CompilerMSL::add_dynamic_buffer(uint32_t desc_set, uint32_t binding, uint32_t index)
{
	SetBindingPair pair = { desc_set, binding };
	buffers_requiring_dynamic_offset[pair] = { index, 0 };
}

void CompilerMSL::add_inline_uniform_block(uint32_t desc_set, uint32_t binding)
{
	SetBindingPair pair = { desc_set, binding };
	inline_uniform_blocks.insert(pair);
}

void CompilerMSL::add_discrete_descriptor_set(uint32_t desc_set)
{
	if (desc_set < kMaxArgumentBuffers)
		argument_buffer_discrete_mask |= 1u << desc_set;
}

void CompilerMSL::set_argument_buffer_device_address_space(uint32_t desc_set, bool device_storage)
{
	if (desc_set < kMaxArgumentBuffers)
	{
		if (device_storage)
			argument_buffer_device_storage_mask |= 1u << desc_set;
		else
			argument_buffer_device_storage_mask &= ~(1u << desc_set);
	}
}

bool CompilerMSL::is_msl_shader_input_used(uint32_t location)
{
	// Don't report internal location allocations to app.
	return location_inputs_in_use.count(location) != 0 &&
	       location_inputs_in_use_fallback.count(location) == 0;
}

bool CompilerMSL::is_msl_shader_output_used(uint32_t location)
{
	// Don't report internal location allocations to app.
	return location_outputs_in_use.count(location) != 0 &&
	       location_outputs_in_use_fallback.count(location) == 0;
}

uint32_t CompilerMSL::get_automatic_builtin_input_location(spv::BuiltIn builtin) const
{
	auto itr = builtin_to_automatic_input_location.find(builtin);
	if (itr == builtin_to_automatic_input_location.end())
		return k_unknown_location;
	else
		return itr->second;
}

uint32_t CompilerMSL::get_automatic_builtin_output_location(spv::BuiltIn builtin) const
{
	auto itr = builtin_to_automatic_output_location.find(builtin);
	if (itr == builtin_to_automatic_output_location.end())
		return k_unknown_location;
	else
		return itr->second;
}

bool CompilerMSL::is_msl_resource_binding_used(ExecutionModel model, uint32_t desc_set, uint32_t binding) const
{
	StageSetBinding tuple = { model, desc_set, binding };
	auto itr = resource_bindings.find(tuple);
	return itr != end(resource_bindings) && itr->second.second;
}

// Returns the size of the array of resources used by the variable with the specified id.
// The returned value is retrieved from the resource binding added using add_msl_resource_binding().
uint32_t CompilerMSL::get_resource_array_size(uint32_t id) const
{
	StageSetBinding tuple = { get_entry_point().model, get_decoration(id, DecorationDescriptorSet),
		                      get_decoration(id, DecorationBinding) };
	auto itr = resource_bindings.find(tuple);
	return itr != end(resource_bindings) ? itr->second.first.count : 0;
}

uint32_t CompilerMSL::get_automatic_msl_resource_binding(uint32_t id) const
{
	return get_extended_decoration(id, SPIRVCrossDecorationResourceIndexPrimary);
}

uint32_t CompilerMSL::get_automatic_msl_resource_binding_secondary(uint32_t id) const
{
	return get_extended_decoration(id, SPIRVCrossDecorationResourceIndexSecondary);
}

uint32_t CompilerMSL::get_automatic_msl_resource_binding_tertiary(uint32_t id) const
{
	return get_extended_decoration(id, SPIRVCrossDecorationResourceIndexTertiary);
}

uint32_t CompilerMSL::get_automatic_msl_resource_binding_quaternary(uint32_t id) const
{
	return get_extended_decoration(id, SPIRVCrossDecorationResourceIndexQuaternary);
}

void CompilerMSL::set_fragment_output_components(uint32_t location, uint32_t components)
{
	fragment_output_components[location] = components;
}

bool CompilerMSL::builtin_translates_to_nonarray(spv::BuiltIn builtin) const
{
	return (builtin == BuiltInSampleMask);
}

void CompilerMSL::build_implicit_builtins()
{
	bool need_sample_pos = active_input_builtins.get(BuiltInSamplePosition);
	bool need_vertex_params = capture_output_to_buffer && get_execution_model() == ExecutionModelVertex &&
	                          !msl_options.vertex_for_tessellation;
	bool need_tesc_params = is_tesc_shader();
	bool need_tese_params = is_tese_shader() && msl_options.raw_buffer_tese_input;
	bool need_subgroup_mask =
	    active_input_builtins.get(BuiltInSubgroupEqMask) || active_input_builtins.get(BuiltInSubgroupGeMask) ||
	    active_input_builtins.get(BuiltInSubgroupGtMask) || active_input_builtins.get(BuiltInSubgroupLeMask) ||
	    active_input_builtins.get(BuiltInSubgroupLtMask);
	bool need_subgroup_ge_mask = !msl_options.is_ios() && (active_input_builtins.get(BuiltInSubgroupGeMask) ||
	                                                       active_input_builtins.get(BuiltInSubgroupGtMask));
	bool need_multiview = get_execution_model() == ExecutionModelVertex && !msl_options.view_index_from_device_index &&
	                      msl_options.multiview_layered_rendering &&
	                      (msl_options.multiview || active_input_builtins.get(BuiltInViewIndex));
	bool need_dispatch_base =
	    msl_options.dispatch_base && get_execution_model() == ExecutionModelGLCompute &&
	    (active_input_builtins.get(BuiltInWorkgroupId) || active_input_builtins.get(BuiltInGlobalInvocationId));
	bool need_grid_params = get_execution_model() == ExecutionModelVertex && msl_options.vertex_for_tessellation;
	bool need_vertex_base_params =
	    need_grid_params &&
	    (active_input_builtins.get(BuiltInVertexId) || active_input_builtins.get(BuiltInVertexIndex) ||
	     active_input_builtins.get(BuiltInBaseVertex) || active_input_builtins.get(BuiltInInstanceId) ||
	     active_input_builtins.get(BuiltInInstanceIndex) || active_input_builtins.get(BuiltInBaseInstance));
	bool need_local_invocation_index = msl_options.emulate_subgroups && active_input_builtins.get(BuiltInSubgroupId);
	bool need_workgroup_size = msl_options.emulate_subgroups && active_input_builtins.get(BuiltInNumSubgroups);

	if (need_subpass_input || need_sample_pos || need_subgroup_mask || need_vertex_params || need_tesc_params ||
	    need_tese_params || need_multiview || need_dispatch_base || need_vertex_base_params || need_grid_params ||
	    needs_sample_id || needs_subgroup_invocation_id || needs_subgroup_size || needs_helper_invocation ||
		has_additional_fixed_sample_mask() || need_local_invocation_index || need_workgroup_size)
	{
		bool has_frag_coord = false;
		bool has_sample_id = false;
		bool has_vertex_idx = false;
		bool has_base_vertex = false;
		bool has_instance_idx = false;
		bool has_base_instance = false;
		bool has_invocation_id = false;
		bool has_primitive_id = false;
		bool has_subgroup_invocation_id = false;
		bool has_subgroup_size = false;
		bool has_view_idx = false;
		bool has_layer = false;
		bool has_helper_invocation = false;
		bool has_local_invocation_index = false;
		bool has_workgroup_size = false;
		uint32_t workgroup_id_type = 0;

		ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
			if (var.storage != StorageClassInput && var.storage != StorageClassOutput)
				return;
			if (!interface_variable_exists_in_entry_point(var.self))
				return;
			if (!has_decoration(var.self, DecorationBuiltIn))
				return;

			BuiltIn builtin = ir.meta[var.self].decoration.builtin_type;

			if (var.storage == StorageClassOutput)
			{
				if (has_additional_fixed_sample_mask() && builtin == BuiltInSampleMask)
				{
					builtin_sample_mask_id = var.self;
					mark_implicit_builtin(StorageClassOutput, BuiltInSampleMask, var.self);
					does_shader_write_sample_mask = true;
				}
			}

			if (var.storage != StorageClassInput)
				return;

			// Use Metal's native frame-buffer fetch API for subpass inputs.
			if (need_subpass_input && (!msl_options.use_framebuffer_fetch_subpasses))
			{
				switch (builtin)
				{
				case BuiltInFragCoord:
					mark_implicit_builtin(StorageClassInput, BuiltInFragCoord, var.self);
					builtin_frag_coord_id = var.self;
					has_frag_coord = true;
					break;
				case BuiltInLayer:
					if (!msl_options.arrayed_subpass_input || msl_options.multiview)
						break;
					mark_implicit_builtin(StorageClassInput, BuiltInLayer, var.self);
					builtin_layer_id = var.self;
					has_layer = true;
					break;
				case BuiltInViewIndex:
					if (!msl_options.multiview)
						break;
					mark_implicit_builtin(StorageClassInput, BuiltInViewIndex, var.self);
					builtin_view_idx_id = var.self;
					has_view_idx = true;
					break;
				default:
					break;
				}
			}

			if ((need_sample_pos || needs_sample_id) && builtin == BuiltInSampleId)
			{
				builtin_sample_id_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInSampleId, var.self);
				has_sample_id = true;
			}

			if (need_vertex_params)
			{
				switch (builtin)
				{
				case BuiltInVertexIndex:
					builtin_vertex_idx_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInVertexIndex, var.self);
					has_vertex_idx = true;
					break;
				case BuiltInBaseVertex:
					builtin_base_vertex_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInBaseVertex, var.self);
					has_base_vertex = true;
					break;
				case BuiltInInstanceIndex:
					builtin_instance_idx_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInInstanceIndex, var.self);
					has_instance_idx = true;
					break;
				case BuiltInBaseInstance:
					builtin_base_instance_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInBaseInstance, var.self);
					has_base_instance = true;
					break;
				default:
					break;
				}
			}

			if (need_tesc_params && builtin == BuiltInInvocationId)
			{
				builtin_invocation_id_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInInvocationId, var.self);
				has_invocation_id = true;
			}

			if ((need_tesc_params || need_tese_params) && builtin == BuiltInPrimitiveId)
			{
				builtin_primitive_id_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInPrimitiveId, var.self);
				has_primitive_id = true;
			}

			if (need_tese_params && builtin == BuiltInTessLevelOuter)
			{
				tess_level_outer_var_id = var.self;
			}

			if (need_tese_params && builtin == BuiltInTessLevelInner)
			{
				tess_level_inner_var_id = var.self;
			}

			if ((need_subgroup_mask || needs_subgroup_invocation_id) && builtin == BuiltInSubgroupLocalInvocationId)
			{
				builtin_subgroup_invocation_id_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInSubgroupLocalInvocationId, var.self);
				has_subgroup_invocation_id = true;
			}

			if ((need_subgroup_ge_mask || needs_subgroup_size) && builtin == BuiltInSubgroupSize)
			{
				builtin_subgroup_size_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInSubgroupSize, var.self);
				has_subgroup_size = true;
			}

			if (need_multiview)
			{
				switch (builtin)
				{
				case BuiltInInstanceIndex:
					// The view index here is derived from the instance index.
					builtin_instance_idx_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInInstanceIndex, var.self);
					has_instance_idx = true;
					break;
				case BuiltInBaseInstance:
					// If a non-zero base instance is used, we need to adjust for it when calculating the view index.
					builtin_base_instance_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInBaseInstance, var.self);
					has_base_instance = true;
					break;
				case BuiltInViewIndex:
					builtin_view_idx_id = var.self;
					mark_implicit_builtin(StorageClassInput, BuiltInViewIndex, var.self);
					has_view_idx = true;
					break;
				default:
					break;
				}
			}

			if (needs_helper_invocation && builtin == BuiltInHelperInvocation)
			{
				builtin_helper_invocation_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInHelperInvocation, var.self);
				has_helper_invocation = true;
			}

			if (need_local_invocation_index && builtin == BuiltInLocalInvocationIndex)
			{
				builtin_local_invocation_index_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInLocalInvocationIndex, var.self);
				has_local_invocation_index = true;
			}

			if (need_workgroup_size && builtin == BuiltInLocalInvocationId)
			{
				builtin_workgroup_size_id = var.self;
				mark_implicit_builtin(StorageClassInput, BuiltInWorkgroupSize, var.self);
				has_workgroup_size = true;
			}

			// The base workgroup needs to have the same type and vector size
			// as the workgroup or invocation ID, so keep track of the type that
			// was used.
			if (need_dispatch_base && workgroup_id_type == 0 &&
			    (builtin == BuiltInWorkgroupId || builtin == BuiltInGlobalInvocationId))
				workgroup_id_type = var.basetype;
		});

		// Use Metal's native frame-buffer fetch API for subpass inputs.
		if ((!has_frag_coord || (msl_options.multiview && !has_view_idx) ||
		     (msl_options.arrayed_subpass_input && !msl_options.multiview && !has_layer)) &&
		    (!msl_options.use_framebuffer_fetch_subpasses) && need_subpass_input)
		{
			if (!has_frag_coord)
			{
				uint32_t offset = ir.increase_bound_by(3);
				uint32_t type_id = offset;
				uint32_t type_ptr_id = offset + 1;
				uint32_t var_id = offset + 2;

				// Create gl_FragCoord.
				SPIRType vec4_type;
				vec4_type.basetype = SPIRType::Float;
				vec4_type.width = 32;
				vec4_type.vecsize = 4;
				set<SPIRType>(type_id, vec4_type);

				SPIRType vec4_type_ptr;
				vec4_type_ptr = vec4_type;
				vec4_type_ptr.pointer = true;
				vec4_type_ptr.pointer_depth++;
				vec4_type_ptr.parent_type = type_id;
				vec4_type_ptr.storage = StorageClassInput;
				auto &ptr_type = set<SPIRType>(type_ptr_id, vec4_type_ptr);
				ptr_type.self = type_id;

				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInFragCoord);
				builtin_frag_coord_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInFragCoord, var_id);
			}

			if (!has_layer && msl_options.arrayed_subpass_input && !msl_options.multiview)
			{
				uint32_t offset = ir.increase_bound_by(2);
				uint32_t type_ptr_id = offset;
				uint32_t var_id = offset + 1;

				// Create gl_Layer.
				SPIRType uint_type_ptr;
				uint_type_ptr = get_uint_type();
				uint_type_ptr.pointer = true;
				uint_type_ptr.pointer_depth++;
				uint_type_ptr.parent_type = get_uint_type_id();
				uint_type_ptr.storage = StorageClassInput;
				auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
				ptr_type.self = get_uint_type_id();

				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInLayer);
				builtin_layer_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInLayer, var_id);
			}

			if (!has_view_idx && msl_options.multiview)
			{
				uint32_t offset = ir.increase_bound_by(2);
				uint32_t type_ptr_id = offset;
				uint32_t var_id = offset + 1;

				// Create gl_ViewIndex.
				SPIRType uint_type_ptr;
				uint_type_ptr = get_uint_type();
				uint_type_ptr.pointer = true;
				uint_type_ptr.pointer_depth++;
				uint_type_ptr.parent_type = get_uint_type_id();
				uint_type_ptr.storage = StorageClassInput;
				auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
				ptr_type.self = get_uint_type_id();

				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInViewIndex);
				builtin_view_idx_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInViewIndex, var_id);
			}
		}

		if (!has_sample_id && (need_sample_pos || needs_sample_id))
		{
			uint32_t offset = ir.increase_bound_by(2);
			uint32_t type_ptr_id = offset;
			uint32_t var_id = offset + 1;

			// Create gl_SampleID.
			SPIRType uint_type_ptr;
			uint_type_ptr = get_uint_type();
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = get_uint_type_id();
			uint_type_ptr.storage = StorageClassInput;
			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = get_uint_type_id();

			set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInSampleId);
			builtin_sample_id_id = var_id;
			mark_implicit_builtin(StorageClassInput, BuiltInSampleId, var_id);
		}

		if ((need_vertex_params && (!has_vertex_idx || !has_base_vertex || !has_instance_idx || !has_base_instance)) ||
		    (need_multiview && (!has_instance_idx || !has_base_instance || !has_view_idx)))
		{
			uint32_t type_ptr_id = ir.increase_bound_by(1);

			SPIRType uint_type_ptr;
			uint_type_ptr = get_uint_type();
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = get_uint_type_id();
			uint_type_ptr.storage = StorageClassInput;
			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = get_uint_type_id();

			if (need_vertex_params && !has_vertex_idx)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_VertexIndex.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInVertexIndex);
				builtin_vertex_idx_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInVertexIndex, var_id);
			}

			if (need_vertex_params && !has_base_vertex)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_BaseVertex.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInBaseVertex);
				builtin_base_vertex_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInBaseVertex, var_id);
			}

			if (!has_instance_idx) // Needed by both multiview and tessellation
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_InstanceIndex.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInInstanceIndex);
				builtin_instance_idx_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInInstanceIndex, var_id);
			}

			if (!has_base_instance) // Needed by both multiview and tessellation
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_BaseInstance.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInBaseInstance);
				builtin_base_instance_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInBaseInstance, var_id);
			}

			if (need_multiview)
			{
				// Multiview shaders are not allowed to write to gl_Layer, ostensibly because
				// it is implicitly written from gl_ViewIndex, but we have to do that explicitly.
				// Note that we can't just abuse gl_ViewIndex for this purpose: it's an input, but
				// gl_Layer is an output in vertex-pipeline shaders.
				uint32_t type_ptr_out_id = ir.increase_bound_by(2);
				SPIRType uint_type_ptr_out;
				uint_type_ptr_out = get_uint_type();
				uint_type_ptr_out.pointer = true;
				uint_type_ptr_out.pointer_depth++;
				uint_type_ptr_out.parent_type = get_uint_type_id();
				uint_type_ptr_out.storage = StorageClassOutput;
				auto &ptr_out_type = set<SPIRType>(type_ptr_out_id, uint_type_ptr_out);
				ptr_out_type.self = get_uint_type_id();
				uint32_t var_id = type_ptr_out_id + 1;
				set<SPIRVariable>(var_id, type_ptr_out_id, StorageClassOutput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInLayer);
				builtin_layer_id = var_id;
				mark_implicit_builtin(StorageClassOutput, BuiltInLayer, var_id);
			}

			if (need_multiview && !has_view_idx)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_ViewIndex.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInViewIndex);
				builtin_view_idx_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInViewIndex, var_id);
			}
		}

		if ((need_tesc_params && (msl_options.multi_patch_workgroup || !has_invocation_id || !has_primitive_id)) ||
		    (need_tese_params && !has_primitive_id) || need_grid_params)
		{
			uint32_t type_ptr_id = ir.increase_bound_by(1);

			SPIRType uint_type_ptr;
			uint_type_ptr = get_uint_type();
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = get_uint_type_id();
			uint_type_ptr.storage = StorageClassInput;
			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = get_uint_type_id();

			if ((need_tesc_params && msl_options.multi_patch_workgroup) || need_grid_params)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_GlobalInvocationID.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInGlobalInvocationId);
				builtin_invocation_id_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInGlobalInvocationId, var_id);
			}
			else if (need_tesc_params && !has_invocation_id)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_InvocationID.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInInvocationId);
				builtin_invocation_id_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInInvocationId, var_id);
			}

			if ((need_tesc_params || need_tese_params) && !has_primitive_id)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				// Create gl_PrimitiveID.
				set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
				set_decoration(var_id, DecorationBuiltIn, BuiltInPrimitiveId);
				builtin_primitive_id_id = var_id;
				mark_implicit_builtin(StorageClassInput, BuiltInPrimitiveId, var_id);
			}

			if (need_grid_params)
			{
				uint32_t var_id = ir.increase_bound_by(1);

				set<SPIRVariable>(var_id, build_extended_vector_type(get_uint_type_id(), 3), StorageClassInput);
				set_extended_decoration(var_id, SPIRVCrossDecorationBuiltInStageInputSize);
				get_entry_point().interface_variables.push_back(var_id);
				set_name(var_id, "spvStageInputSize");
				builtin_stage_input_size_id = var_id;
			}
		}

		if (!has_subgroup_invocation_id && (need_subgroup_mask || needs_subgroup_invocation_id))
		{
			uint32_t offset = ir.increase_bound_by(2);
			uint32_t type_ptr_id = offset;
			uint32_t var_id = offset + 1;

			// Create gl_SubgroupInvocationID.
			SPIRType uint_type_ptr;
			uint_type_ptr = get_uint_type();
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = get_uint_type_id();
			uint_type_ptr.storage = StorageClassInput;
			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = get_uint_type_id();

			set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInSubgroupLocalInvocationId);
			builtin_subgroup_invocation_id_id = var_id;
			mark_implicit_builtin(StorageClassInput, BuiltInSubgroupLocalInvocationId, var_id);
		}

		if (!has_subgroup_size && (need_subgroup_ge_mask || needs_subgroup_size))
		{
			uint32_t offset = ir.increase_bound_by(2);
			uint32_t type_ptr_id = offset;
			uint32_t var_id = offset + 1;

			// Create gl_SubgroupSize.
			SPIRType uint_type_ptr;
			uint_type_ptr = get_uint_type();
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = get_uint_type_id();
			uint_type_ptr.storage = StorageClassInput;
			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = get_uint_type_id();

			set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInSubgroupSize);
			builtin_subgroup_size_id = var_id;
			mark_implicit_builtin(StorageClassInput, BuiltInSubgroupSize, var_id);
		}

		if (need_dispatch_base || need_vertex_base_params)
		{
			if (workgroup_id_type == 0)
				workgroup_id_type = build_extended_vector_type(get_uint_type_id(), 3);
			uint32_t var_id;
			if (msl_options.supports_msl_version(1, 2))
			{
				// If we have MSL 1.2, we can (ab)use the [[grid_origin]] builtin
				// to convey this information and save a buffer slot.
				uint32_t offset = ir.increase_bound_by(1);
				var_id = offset;

				set<SPIRVariable>(var_id, workgroup_id_type, StorageClassInput);
				set_extended_decoration(var_id, SPIRVCrossDecorationBuiltInDispatchBase);
				get_entry_point().interface_variables.push_back(var_id);
			}
			else
			{
				// Otherwise, we need to fall back to a good ol' fashioned buffer.
				uint32_t offset = ir.increase_bound_by(2);
				var_id = offset;
				uint32_t type_id = offset + 1;

				SPIRType var_type = get<SPIRType>(workgroup_id_type);
				var_type.storage = StorageClassUniform;
				set<SPIRType>(type_id, var_type);

				set<SPIRVariable>(var_id, type_id, StorageClassUniform);
				// This should never match anything.
				set_decoration(var_id, DecorationDescriptorSet, ~(5u));
				set_decoration(var_id, DecorationBinding, msl_options.indirect_params_buffer_index);
				set_extended_decoration(var_id, SPIRVCrossDecorationResourceIndexPrimary,
				                        msl_options.indirect_params_buffer_index);
			}
			set_name(var_id, "spvDispatchBase");
			builtin_dispatch_base_id = var_id;
		}

		if (has_additional_fixed_sample_mask() && !does_shader_write_sample_mask)
		{
			uint32_t offset = ir.increase_bound_by(2);
			uint32_t var_id = offset + 1;

			// Create gl_SampleMask.
			SPIRType uint_type_ptr_out;
			uint_type_ptr_out = get_uint_type();
			uint_type_ptr_out.pointer = true;
			uint_type_ptr_out.pointer_depth++;
			uint_type_ptr_out.parent_type = get_uint_type_id();
			uint_type_ptr_out.storage = StorageClassOutput;

			auto &ptr_out_type = set<SPIRType>(offset, uint_type_ptr_out);
			ptr_out_type.self = get_uint_type_id();
			set<SPIRVariable>(var_id, offset, StorageClassOutput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInSampleMask);
			builtin_sample_mask_id = var_id;
			mark_implicit_builtin(StorageClassOutput, BuiltInSampleMask, var_id);
		}

		if (!has_helper_invocation && needs_helper_invocation)
		{
			uint32_t offset = ir.increase_bound_by(3);
			uint32_t type_id = offset;
			uint32_t type_ptr_id = offset + 1;
			uint32_t var_id = offset + 2;

			// Create gl_HelperInvocation.
			SPIRType bool_type;
			bool_type.basetype = SPIRType::Boolean;
			bool_type.width = 8;
			bool_type.vecsize = 1;
			set<SPIRType>(type_id, bool_type);

			SPIRType bool_type_ptr_in;
			bool_type_ptr_in = bool_type;
			bool_type_ptr_in.pointer = true;
			bool_type_ptr_in.pointer_depth++;
			bool_type_ptr_in.parent_type = type_id;
			bool_type_ptr_in.storage = StorageClassInput;

			auto &ptr_in_type = set<SPIRType>(type_ptr_id, bool_type_ptr_in);
			ptr_in_type.self = type_id;
			set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInHelperInvocation);
			builtin_helper_invocation_id = var_id;
			mark_implicit_builtin(StorageClassInput, BuiltInHelperInvocation, var_id);
		}

		if (need_local_invocation_index && !has_local_invocation_index)
		{
			uint32_t offset = ir.increase_bound_by(2);
			uint32_t type_ptr_id = offset;
			uint32_t var_id = offset + 1;

			// Create gl_LocalInvocationIndex.
			SPIRType uint_type_ptr;
			uint_type_ptr = get_uint_type();
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = get_uint_type_id();
			uint_type_ptr.storage = StorageClassInput;

			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = get_uint_type_id();
			set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInLocalInvocationIndex);
			builtin_local_invocation_index_id = var_id;
			mark_implicit_builtin(StorageClassInput, BuiltInLocalInvocationIndex, var_id);
		}

		if (need_workgroup_size && !has_workgroup_size)
		{
			uint32_t offset = ir.increase_bound_by(2);
			uint32_t type_ptr_id = offset;
			uint32_t var_id = offset + 1;

			// Create gl_WorkgroupSize.
			uint32_t type_id = build_extended_vector_type(get_uint_type_id(), 3);
			SPIRType uint_type_ptr = get<SPIRType>(type_id);
			uint_type_ptr.pointer = true;
			uint_type_ptr.pointer_depth++;
			uint_type_ptr.parent_type = type_id;
			uint_type_ptr.storage = StorageClassInput;

			auto &ptr_type = set<SPIRType>(type_ptr_id, uint_type_ptr);
			ptr_type.self = type_id;
			set<SPIRVariable>(var_id, type_ptr_id, StorageClassInput);
			set_decoration(var_id, DecorationBuiltIn, BuiltInWorkgroupSize);
			builtin_workgroup_size_id = var_id;
			mark_implicit_builtin(StorageClassInput, BuiltInWorkgroupSize, var_id);
		}
	}

	if (needs_swizzle_buffer_def)
	{
		uint32_t var_id = build_constant_uint_array_pointer();
		set_name(var_id, "spvSwizzleConstants");
		// This should never match anything.
		set_decoration(var_id, DecorationDescriptorSet, kSwizzleBufferBinding);
		set_decoration(var_id, DecorationBinding, msl_options.swizzle_buffer_index);
		set_extended_decoration(var_id, SPIRVCrossDecorationResourceIndexPrimary, msl_options.swizzle_buffer_index);
		swizzle_buffer_id = var_id;
	}

	if (needs_buffer_size_buffer())
	{
		uint32_t var_id = build_constant_uint_array_pointer();
		set_name(var_id, "spvBufferSizeConstants");
		// This should never match anything.
		set_decoration(var_id, DecorationDescriptorSet, kBufferSizeBufferBinding);
		set_decoration(var_id, DecorationBinding, msl_options.buffer_size_buffer_index);
		set_extended_decoration(var_id, SPIRVCrossDecorationResourceIndexPrimary, msl_options.buffer_size_buffer_index);
		buffer_size_buffer_id = var_id;
	}

	if (needs_view_mask_buffer())
	{
		uint32_t var_id = build_constant_uint_array_pointer();
		set_name(var_id, "spvViewMask");
		// This should never match anything.
		set_decoration(var_id, DecorationDescriptorSet, ~(4u));
		set_decoration(var_id, DecorationBinding, msl_options.view_mask_buffer_index);
		set_extended_decoration(var_id, SPIRVCrossDecorationResourceIndexPrimary, msl_options.view_mask_buffer_index);
		view_mask_buffer_id = var_id;
	}

	if (!buffers_requiring_dynamic_offset.empty())
	{
		uint32_t var_id = build_constant_uint_array_pointer();
		set_name(var_id, "spvDynamicOffsets");
		// This should never match anything.
		set_decoration(var_id, DecorationDescriptorSet, ~(5u));
		set_decoration(var_id, DecorationBinding, msl_options.dynamic_offsets_buffer_index);
		set_extended_decoration(var_id, SPIRVCrossDecorationResourceIndexPrimary,
		                        msl_options.dynamic_offsets_buffer_index);
		dynamic_offsets_buffer_id = var_id;
	}

	// If we're returning a struct from a vertex-like entry point, we must return a position attribute.
	bool need_position = (get_execution_model() == ExecutionModelVertex || is_tese_shader()) &&
	                     !capture_output_to_buffer && !get_is_rasterization_disabled() &&
	                     !active_output_builtins.get(BuiltInPosition);

	if (need_position)
	{
		// If we can get away with returning void from entry point, we don't need to care.
		// If there is at least one other stage output, we need to return [[position]],
		// so we need to create one if it doesn't appear in the SPIR-V. Before adding the
		// implicit variable, check if it actually exists already, but just has not been used
		// or initialized, and if so, mark it as active, and do not create the implicit variable.
		bool has_output = false;
		ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
			if (var.storage == StorageClassOutput && interface_variable_exists_in_entry_point(var.self))
			{
				has_output = true;

				// Check if the var is the Position builtin
				if (has_decoration(var.self, DecorationBuiltIn) && get_decoration(var.self, DecorationBuiltIn) == BuiltInPosition)
					active_output_builtins.set(BuiltInPosition);

				// If the var is a struct, check if any members is the Position builtin
				auto &var_type = get_variable_element_type(var);
				if (var_type.basetype == SPIRType::Struct)
				{
					auto mbr_cnt = var_type.member_types.size();
					for (uint32_t mbr_idx = 0; mbr_idx < mbr_cnt; mbr_idx++)
					{
						auto builtin = BuiltInMax;
						bool is_builtin = is_member_builtin(var_type, mbr_idx, &builtin);
						if (is_builtin && builtin == BuiltInPosition)
							active_output_builtins.set(BuiltInPosition);
					}
				}
			}
		});
		need_position = has_output && !active_output_builtins.get(BuiltInPosition);
	}

	if (need_position)
	{
		uint32_t offset = ir.increase_bound_by(3);
		uint32_t type_id = offset;
		uint32_t type_ptr_id = offset + 1;
		uint32_t var_id = offset + 2;

		// Create gl_Position.
		SPIRType vec4_type;
		vec4_type.basetype = SPIRType::Float;
		vec4_type.width = 32;
		vec4_type.vecsize = 4;
		set<SPIRType>(type_id, vec4_type);

		SPIRType vec4_type_ptr;
		vec4_type_ptr = vec4_type;
		vec4_type_ptr.pointer = true;
		vec4_type_ptr.pointer_depth++;
		vec4_type_ptr.parent_type = type_id;
		vec4_type_ptr.storage = StorageClassOutput;
		auto &ptr_type = set<SPIRType>(type_ptr_id, vec4_type_ptr);
		ptr_type.self = type_id;

		set<SPIRVariable>(var_id, type_ptr_id, StorageClassOutput);
		set_decoration(var_id, DecorationBuiltIn, BuiltInPosition);
		mark_implicit_builtin(StorageClassOutput, BuiltInPosition, var_id);
	}
}

// Checks if the specified builtin variable (e.g. gl_InstanceIndex) is marked as active.
// If not, it marks it as active and forces a recompilation.
// This might be used when the optimization of inactive builtins was too optimistic (e.g. when "spvOut" is emitted).
void CompilerMSL::ensure_builtin(spv::StorageClass storage, spv::BuiltIn builtin)
{
	Bitset *active_builtins = nullptr;
	switch (storage)
	{
	case StorageClassInput:
		active_builtins = &active_input_builtins;
		break;

	case StorageClassOutput:
		active_builtins = &active_output_builtins;
		break;

	default:
		break;
	}

	// At this point, the specified builtin variable must have already been declared in the entry point.
	// If not, mark as active and force recompile.
	if (active_builtins != nullptr && !active_builtins->get(builtin))
	{
		active_builtins->set(builtin);
		force_recompile();
	}
}

void CompilerMSL::mark_implicit_builtin(StorageClass storage, BuiltIn builtin, uint32_t id)
{
	Bitset *active_builtins = nullptr;
	switch (storage)
	{
	case StorageClassInput:
		active_builtins = &active_input_builtins;
		break;

	case StorageClassOutput:
		active_builtins = &active_output_builtins;
		break;

	default:
		break;
	}

	assert(active_builtins != nullptr);
	active_builtins->set(builtin);

	auto &var = get_entry_point().interface_variables;
	if (find(begin(var), end(var), VariableID(id)) == end(var))
		var.push_back(id);
}

uint32_t CompilerMSL::build_constant_uint_array_pointer()
{
	uint32_t offset = ir.increase_bound_by(3);
	uint32_t type_ptr_id = offset;
	uint32_t type_ptr_ptr_id = offset + 1;
	uint32_t var_id = offset + 2;

	// Create a buffer to hold extra data, including the swizzle constants.
	SPIRType uint_type_pointer = get_uint_type();
	uint_type_pointer.pointer = true;
	uint_type_pointer.pointer_depth++;
	uint_type_pointer.parent_type = get_uint_type_id();
	uint_type_pointer.storage = StorageClassUniform;
	set<SPIRType>(type_ptr_id, uint_type_pointer);
	set_decoration(type_ptr_id, DecorationArrayStride, 4);

	SPIRType uint_type_pointer2 = uint_type_pointer;
	uint_type_pointer2.pointer_depth++;
	uint_type_pointer2.parent_type = type_ptr_id;
	set<SPIRType>(type_ptr_ptr_id, uint_type_pointer2);

	set<SPIRVariable>(var_id, type_ptr_ptr_id, StorageClassUniformConstant);
	return var_id;
}

static string create_sampler_address(const char *prefix, MSLSamplerAddress addr)
{
	switch (addr)
	{
	case MSL_SAMPLER_ADDRESS_CLAMP_TO_EDGE:
		return join(prefix, "address::clamp_to_edge");
	case MSL_SAMPLER_ADDRESS_CLAMP_TO_ZERO:
		return join(prefix, "address::clamp_to_zero");
	case MSL_SAMPLER_ADDRESS_CLAMP_TO_BORDER:
		return join(prefix, "address::clamp_to_border");
	case MSL_SAMPLER_ADDRESS_REPEAT:
		return join(prefix, "address::repeat");
	case MSL_SAMPLER_ADDRESS_MIRRORED_REPEAT:
		return join(prefix, "address::mirrored_repeat");
	default:
		SPIRV_CROSS_THROW("Invalid sampler addressing mode.");
	}
}

SPIRType &CompilerMSL::get_stage_in_struct_type()
{
	auto &si_var = get<SPIRVariable>(stage_in_var_id);
	return get_variable_data_type(si_var);
}

SPIRType &CompilerMSL::get_stage_out_struct_type()
{
	auto &so_var = get<SPIRVariable>(stage_out_var_id);
	return get_variable_data_type(so_var);
}

SPIRType &CompilerMSL::get_patch_stage_in_struct_type()
{
	auto &si_var = get<SPIRVariable>(patch_stage_in_var_id);
	return get_variable_data_type(si_var);
}

SPIRType &CompilerMSL::get_patch_stage_out_struct_type()
{
	auto &so_var = get<SPIRVariable>(patch_stage_out_var_id);
	return get_variable_data_type(so_var);
}

std::string CompilerMSL::get_tess_factor_struct_name()
{
	if (is_tessellating_triangles())
		return "MTLTriangleTessellationFactorsHalf";
	return "MTLQuadTessellationFactorsHalf";
}

SPIRType &CompilerMSL::get_uint_type()
{
	return get<SPIRType>(get_uint_type_id());
}

uint32_t CompilerMSL::get_uint_type_id()
{
	if (uint_type_id != 0)
		return uint_type_id;

	uint_type_id = ir.increase_bound_by(1);

	SPIRType type;
	type.basetype = SPIRType::UInt;
	type.width = 32;
	set<SPIRType>(uint_type_id, type);
	return uint_type_id;
}

void CompilerMSL::emit_entry_point_declarations()
{
	// FIXME: Get test coverage here ...
	// Constant arrays of non-primitive types (i.e. matrices) won't link properly into Metal libraries
	declare_complex_constant_arrays();

	// Emit constexpr samplers here.
	for (auto &samp : constexpr_samplers_by_id)
	{
		auto &var = get<SPIRVariable>(samp.first);
		auto &type = get<SPIRType>(var.basetype);
		if (type.basetype == SPIRType::Sampler)
			add_resource_name(samp.first);

		SmallVector<string> args;
		auto &s = samp.second;

		if (s.coord != MSL_SAMPLER_COORD_NORMALIZED)
			args.push_back("coord::pixel");

		if (s.min_filter == s.mag_filter)
		{
			if (s.min_filter != MSL_SAMPLER_FILTER_NEAREST)
				args.push_back("filter::linear");
		}
		else
		{
			if (s.min_filter != MSL_SAMPLER_FILTER_NEAREST)
				args.push_back("min_filter::linear");
			if (s.mag_filter != MSL_SAMPLER_FILTER_NEAREST)
				args.push_back("mag_filter::linear");
		}

		switch (s.mip_filter)
		{
		case MSL_SAMPLER_MIP_FILTER_NONE:
			// Default
			break;
		case MSL_SAMPLER_MIP_FILTER_NEAREST:
			args.push_back("mip_filter::nearest");
			break;
		case MSL_SAMPLER_MIP_FILTER_LINEAR:
			args.push_back("mip_filter::linear");
			break;
		default:
			SPIRV_CROSS_THROW("Invalid mip filter.");
		}

		if (s.s_address == s.t_address && s.s_address == s.r_address)
		{
			if (s.s_address != MSL_SAMPLER_ADDRESS_CLAMP_TO_EDGE)
				args.push_back(create_sampler_address("", s.s_address));
		}
		else
		{
			if (s.s_address != MSL_SAMPLER_ADDRESS_CLAMP_TO_EDGE)
				args.push_back(create_sampler_address("s_", s.s_address));
			if (s.t_address != MSL_SAMPLER_ADDRESS_CLAMP_TO_EDGE)
				args.push_back(create_sampler_address("t_", s.t_address));
			if (s.r_address != MSL_SAMPLER_ADDRESS_CLAMP_TO_EDGE)
				args.push_back(create_sampler_address("r_", s.r_address));
		}

		if (s.compare_enable)
		{
			switch (s.compare_func)
			{
			case MSL_SAMPLER_COMPARE_FUNC_ALWAYS:
				args.push_back("compare_func::always");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_NEVER:
				args.push_back("compare_func::never");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_EQUAL:
				args.push_back("compare_func::equal");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_NOT_EQUAL:
				args.push_back("compare_func::not_equal");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_LESS:
				args.push_back("compare_func::less");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_LESS_EQUAL:
				args.push_back("compare_func::less_equal");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_GREATER:
				args.push_back("compare_func::greater");
				break;
			case MSL_SAMPLER_COMPARE_FUNC_GREATER_EQUAL:
				args.push_back("compare_func::greater_equal");
				break;
			default:
				SPIRV_CROSS_THROW("Invalid sampler compare function.");
			}
		}

		if (s.s_address == MSL_SAMPLER_ADDRESS_CLAMP_TO_BORDER || s.t_address == MSL_SAMPLER_ADDRESS_CLAMP_TO_BORDER ||
		    s.r_address == MSL_SAMPLER_ADDRESS_CLAMP_TO_BORDER)
		{
			switch (s.border_color)
			{
			case MSL_SAMPLER_BORDER_COLOR_OPAQUE_BLACK:
				args.push_back("border_color::opaque_black");
				break;
			case MSL_SAMPLER_BORDER_COLOR_OPAQUE_WHITE:
				args.push_back("border_color::opaque_white");
				break;
			case MSL_SAMPLER_BORDER_COLOR_TRANSPARENT_BLACK:
				args.push_back("border_color::transparent_black");
				break;
			default:
				SPIRV_CROSS_THROW("Invalid sampler border color.");
			}
		}

		if (s.anisotropy_enable)
			args.push_back(join("max_anisotropy(", s.max_anisotropy, ")"));
		if (s.lod_clamp_enable)
		{
			args.push_back(join("lod_clamp(", convert_to_string(s.lod_clamp_min, current_locale_radix_character), ", ",
			                    convert_to_string(s.lod_clamp_max, current_locale_radix_character), ")"));
		}

		// If we would emit no arguments, then omit the parentheses entirely. Otherwise,
		// we'll wind up with a "most vexing parse" situation.
		if (args.empty())
			statement("constexpr sampler ",
			          type.basetype == SPIRType::SampledImage ? to_sampler_expression(samp.first) : to_name(samp.first),
			          ";");
		else
			statement("constexpr sampler ",
			          type.basetype == SPIRType::SampledImage ? to_sampler_expression(samp.first) : to_name(samp.first),
			          "(", merge(args), ");");
	}

	// Emit dynamic buffers here.
	for (auto &dynamic_buffer : buffers_requiring_dynamic_offset)
	{
		if (!dynamic_buffer.second.second)
		{
			// Could happen if no buffer was used at requested binding point.
			continue;
		}

		const auto &var = get<SPIRVariable>(dynamic_buffer.second.second);
		uint32_t var_id = var.self;
		const auto &type = get_variable_data_type(var);
		string name = to_name(var.self);
		uint32_t desc_set = get_decoration(var.self, DecorationDescriptorSet);
		uint32_t arg_id = argument_buffer_ids[desc_set];
		uint32_t base_index = dynamic_buffer.second.first;

		if (!type.array.empty())
		{
			// This is complicated, because we need to support arrays of arrays.
			// And it's even worse if the outermost dimension is a runtime array, because now
			// all this complicated goop has to go into the shader itself. (FIXME)
			if (!type.array[type.array.size() - 1])
				SPIRV_CROSS_THROW("Runtime arrays with dynamic offsets are not supported yet.");
			else
			{
				is_using_builtin_array = true;
				statement(get_argument_address_space(var), " ", type_to_glsl(type), "* ", to_restrict(var_id, true), name,
				          type_to_array_glsl(type), " =");

				uint32_t dim = uint32_t(type.array.size());
				uint32_t j = 0;
				for (SmallVector<uint32_t> indices(type.array.size());
				     indices[type.array.size() - 1] < to_array_size_literal(type); j++)
				{
					while (dim > 0)
					{
						begin_scope();
						--dim;
					}

					string arrays;
					for (uint32_t i = uint32_t(type.array.size()); i; --i)
						arrays += join("[", indices[i - 1], "]");
					statement("(", get_argument_address_space(var), " ", type_to_glsl(type), "* ",
					          to_restrict(var_id, false), ")((", get_argument_address_space(var), " char* ",
					          to_restrict(var_id, false), ")", to_name(arg_id), ".", ensure_valid_name(name, "m"),
					          arrays, " + ", to_name(dynamic_offsets_buffer_id), "[", base_index + j, "]),");

					while (++indices[dim] >= to_array_size_literal(type, dim) && dim < type.array.size() - 1)
					{
						end_scope(",");
						indices[dim++] = 0;
					}
				}
				end_scope_decl();
				statement_no_indent("");
				is_using_builtin_array = false;
			}
		}
		else
		{
			statement(get_argument_address_space(var), " auto& ", to_restrict(var_id, true), name, " = *(",
			          get_argument_address_space(var), " ", type_to_glsl(type), "* ", to_restrict(var_id, false), ")((",
			          get_argument_address_space(var), " char* ", to_restrict(var_id, false), ")", to_name(arg_id), ".",
			          ensure_valid_name(name, "m"), " + ", to_name(dynamic_offsets_buffer_id), "[", base_index, "]);");
		}
	}

	// Emit buffer arrays here.
	for (uint32_t array_id : buffer_arrays_discrete)
	{
		const auto &var = get<SPIRVariable>(array_id);
		const auto &type = get_variable_data_type(var);
		const auto &buffer_type = get_variable_element_type(var);
		string name = to_name(array_id);
		statement(get_argument_address_space(var), " ", type_to_glsl(buffer_type), "* ", to_restrict(array_id, true), name,
		          "[] =");
		begin_scope();
		for (uint32_t i = 0; i < to_array_size_literal(type); ++i)
			statement(name, "_", i, ",");
		end_scope_decl();
		statement_no_indent("");
	}
	// Discrete descriptors are processed in entry point emission every compiler iteration.
	buffer_arrays_discrete.clear();

	// Emit buffer aliases here.
	for (auto &var_id : buffer_aliases_discrete)
	{
		const auto &var = get<SPIRVariable>(var_id);
		const auto &type = get_variable_data_type(var);
		auto addr_space = get_argument_address_space(var);
		auto name = to_name(var_id);

		uint32_t desc_set = get_decoration(var_id, DecorationDescriptorSet);
		uint32_t desc_binding = get_decoration(var_id, DecorationBinding);
		auto alias_name = join("spvBufferAliasSet", desc_set, "Binding", desc_binding);

		statement(addr_space, " auto& ", to_restrict(var_id, true),
		          name,
		          " = *(", addr_space, " ", type_to_glsl(type), "*)", alias_name, ";");
	}
	// Discrete descriptors are processed in entry point emission every compiler iteration.
	buffer_aliases_discrete.clear();

	for (auto &var_pair : buffer_aliases_argument)
	{
		uint32_t var_id = var_pair.first;
		uint32_t alias_id = var_pair.second;

		const auto &var = get<SPIRVariable>(var_id);
		const auto &type = get_variable_data_type(var);
		auto addr_space = get_argument_address_space(var);

		if (type.array.empty())
		{
			statement(addr_space, " auto& ", to_restrict(var_id, true), to_name(var_id), " = (", addr_space, " ",
			          type_to_glsl(type), "&)", ir.meta[alias_id].decoration.qualified_alias, ";");
		}
		else
		{
			const char *desc_addr_space = descriptor_address_space(var_id, var.storage, "thread");

			// Esoteric type cast. Reference to array of pointers.
			// Auto here defers to UBO or SSBO. The address space of the reference needs to refer to the
			// address space of the argument buffer itself, which is usually constant, but can be const device for
			// large argument buffers.
			is_using_builtin_array = true;
			statement(desc_addr_space, " auto& ", to_restrict(var_id, true), to_name(var_id), " = (", addr_space, " ",
			          type_to_glsl(type), "* ", desc_addr_space, " (&)",
			          type_to_array_glsl(type), ")", ir.meta[alias_id].decoration.qualified_alias, ";");
			is_using_builtin_array = false;
		}
	}

	// Emit disabled fragment outputs.
	std::sort(disabled_frag_outputs.begin(), disabled_frag_outputs.end());
	for (uint32_t var_id : disabled_frag_outputs)
	{
		auto &var = get<SPIRVariable>(var_id);
		add_local_variable_name(var_id);
		statement(variable_decl(var), ";");
		var.deferred_declaration = false;
	}
}

string CompilerMSL::compile()
{
	replace_illegal_entry_point_names();
	ir.fixup_reserved_names();

	// Do not deal with GLES-isms like precision, older extensions and such.
	options.vulkan_semantics = true;
	options.es = false;
	options.version = 450;
	backend.null_pointer_literal = "nullptr";
	backend.float_literal_suffix = false;
	backend.uint32_t_literal_suffix = true;
	backend.int16_t_literal_suffix = "";
	backend.uint16_t_literal_suffix = "";
	backend.basic_int_type = "int";
	backend.basic_uint_type = "uint";
	backend.basic_int8_type = "char";
	backend.basic_uint8_type = "uchar";
	backend.basic_int16_type = "short";
	backend.basic_uint16_type = "ushort";
	backend.boolean_mix_function = "select";
	backend.swizzle_is_function = false;
	backend.shared_is_implied = false;
	backend.use_initializer_list = true;
	backend.use_typed_initializer_list = true;
	backend.native_row_major_matrix = false;
	backend.unsized_array_supported = false;
	backend.can_declare_arrays_inline = false;
	backend.allow_truncated_access_chain = true;
	backend.comparison_image_samples_scalar = true;
	backend.native_pointers = true;
	backend.nonuniform_qualifier = "";
	backend.support_small_type_sampling_result = true;
	backend.supports_empty_struct = true;
	backend.support_64bit_switch = true;
	backend.boolean_in_struct_remapped_type = SPIRType::Short;

	// Allow Metal to use the array<T> template unless we force it off.
	backend.can_return_array = !msl_options.force_native_arrays;
	backend.array_is_value_type = !msl_options.force_native_arrays;
	// Arrays which are part of buffer objects are never considered to be value types (just plain C-style).
	backend.array_is_value_type_in_buffer_blocks = false;
	backend.support_pointer_to_pointer = true;
	backend.implicit_c_integer_promotion_rules = true;

	capture_output_to_buffer = msl_options.capture_output_to_buffer;
	is_rasterization_disabled = msl_options.disable_rasterization || capture_output_to_buffer;

	// Initialize array here rather than constructor, MSVC 2013 workaround.
	for (auto &id : next_metal_resource_ids)
		id = 0;

	fixup_anonymous_struct_names();
	fixup_type_alias();
	replace_illegal_names();
	sync_entry_point_aliases_and_names();

	build_function_control_flow_graphs_and_analyze();
	update_active_builtins();
	analyze_image_and_sampler_usage();
	analyze_sampled_image_usage();
	analyze_interlocked_resource_usage();
	preprocess_op_codes();
	build_implicit_builtins();

	if (needs_manual_helper_invocation_updates() &&
	    (active_input_builtins.get(BuiltInHelperInvocation) || needs_helper_invocation))
	{
		string discard_expr =
		    join(builtin_to_glsl(BuiltInHelperInvocation, StorageClassInput), " = true, discard_fragment()");
		backend.discard_literal = discard_expr;
		backend.demote_literal = discard_expr;
	}
	else
	{
		backend.discard_literal = "discard_fragment()";
		backend.demote_literal = "discard_fragment()";
	}

	fixup_image_load_store_access();

	set_enabled_interface_variables(get_active_interface_variables());
	if (msl_options.force_active_argument_buffer_resources)
		activate_argument_buffer_resources();

	if (swizzle_buffer_id)
		add_active_interface_variable(swizzle_buffer_id);
	if (buffer_size_buffer_id)
		add_active_interface_variable(buffer_size_buffer_id);
	if (view_mask_buffer_id)
		add_active_interface_variable(view_mask_buffer_id);
	if (dynamic_offsets_buffer_id)
		add_active_interface_variable(dynamic_offsets_buffer_id);
	if (builtin_layer_id)
		add_active_interface_variable(builtin_layer_id);
	if (builtin_dispatch_base_id && !msl_options.supports_msl_version(1, 2))
		add_active_interface_variable(builtin_dispatch_base_id);
	if (builtin_sample_mask_id)
		add_active_interface_variable(builtin_sample_mask_id);

	// Create structs to hold input, output and uniform variables.
	// Do output first to ensure out. is declared at top of entry function.
	qual_pos_var_name = "";
	stage_out_var_id = add_interface_block(StorageClassOutput);
	patch_stage_out_var_id = add_interface_block(StorageClassOutput, true);
	stage_in_var_id = add_interface_block(StorageClassInput);
	if (is_tese_shader())
		patch_stage_in_var_id = add_interface_block(StorageClassInput, true);

	if (is_tesc_shader())
		stage_out_ptr_var_id = add_interface_block_pointer(stage_out_var_id, StorageClassOutput);
	if (is_tessellation_shader())
		stage_in_ptr_var_id = add_interface_block_pointer(stage_in_var_id, StorageClassInput);

	// Metal vertex functions that define no output must disable rasterization and return void.
	if (!stage_out_var_id)
		is_rasterization_disabled = true;

	// Convert the use of global variables to recursively-passed function parameters
	localize_global_variables();
	extract_global_variables_from_functions();

	// Mark any non-stage-in structs to be tightly packed.
	mark_packable_structs();
	reorder_type_alias();

	// Add fixup hooks required by shader inputs and outputs. This needs to happen before
	// the loop, so the hooks aren't added multiple times.
	fix_up_shader_inputs_outputs();

	// If we are using argument buffers, we create argument buffer structures for them here.
	// These buffers will be used in the entry point, not the individual resources.
	if (msl_options.argument_buffers)
	{
		if (!msl_options.supports_msl_version(2, 0))
			SPIRV_CROSS_THROW("Argument buffers can only be used with MSL 2.0 and up.");
		analyze_argument_buffers();
	}

	uint32_t pass_count = 0;
	do
	{
		reset(pass_count);

		// Start bindings at zero.
		next_metal_resource_index_buffer = 0;
		next_metal_resource_index_texture = 0;
		next_metal_resource_index_sampler = 0;
		for (auto &id : next_metal_resource_ids)
			id = 0;

		// Move constructor for this type is broken on GCC 4.9 ...
		buffer.reset();

		emit_header();
		emit_custom_templates();
		emit_custom_functions();
		emit_specialization_constants_and_structs();
		emit_resources();
		emit_function(get<SPIRFunction>(ir.default_entry_point), Bitset());

		pass_count++;
	} while (is_forcing_recompilation());

	return buffer.str();
}

// Register the need to output any custom functions.
void CompilerMSL::preprocess_op_codes()
{
	OpCodePreprocessor preproc(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), preproc);

	suppress_missing_prototypes = preproc.suppress_missing_prototypes;

	if (preproc.uses_atomics)
	{
		add_header_line("#include <metal_atomic>");
		add_pragma_line("#pragma clang diagnostic ignored \"-Wunused-variable\"");
	}

	// Before MSL 2.1 (2.2 for textures), Metal vertex functions that write to
	// resources must disable rasterization and return void.
	if ((preproc.uses_buffer_write && !msl_options.supports_msl_version(2, 1)) ||
	    (preproc.uses_image_write && !msl_options.supports_msl_version(2, 2)))
		is_rasterization_disabled = true;

	// Tessellation control shaders are run as compute functions in Metal, and so
	// must capture their output to a buffer.
	if (is_tesc_shader() || (get_execution_model() == ExecutionModelVertex && msl_options.vertex_for_tessellation))
	{
		is_rasterization_disabled = true;
		capture_output_to_buffer = true;
	}

	if (preproc.needs_subgroup_invocation_id)
		needs_subgroup_invocation_id = true;
	if (preproc.needs_subgroup_size)
		needs_subgroup_size = true;
	// build_implicit_builtins() hasn't run yet, and in fact, this needs to execute
	// before then so that gl_SampleID will get added; so we also need to check if
	// that function would add gl_FragCoord.
	if (preproc.needs_sample_id || msl_options.force_sample_rate_shading ||
	    (is_sample_rate() && (active_input_builtins.get(BuiltInFragCoord) ||
	                          (need_subpass_input_ms && !msl_options.use_framebuffer_fetch_subpasses))))
		needs_sample_id = true;
	if (preproc.needs_helper_invocation)
		needs_helper_invocation = true;

	// OpKill is removed by the parser, so we need to identify those by inspecting
	// blocks.
	ir.for_each_typed_id<SPIRBlock>([&preproc](uint32_t, SPIRBlock &block) {
		if (block.terminator == SPIRBlock::Kill)
			preproc.uses_discard = true;
	});

	// Fragment shaders that both write to storage resources and discard fragments
	// need checks on the writes, to work around Metal allowing these writes despite
	// the fragment being dead.
	if (msl_options.check_discarded_frag_stores && preproc.uses_discard &&
	    (preproc.uses_buffer_write || preproc.uses_image_write))
	{
		frag_shader_needs_discard_checks = true;
		needs_helper_invocation = true;
		// Fragment discard store checks imply manual HelperInvocation updates.
		msl_options.manual_helper_invocation_updates = true;
	}

	if (is_intersection_query())
	{
		add_header_line("#if __METAL_VERSION__ >= 230");
		add_header_line("#include <metal_raytracing>");
		add_header_line("using namespace metal::raytracing;");
		add_header_line("#endif");
	}
}

// Move the Private and Workgroup global variables to the entry function.
// Non-constant variables cannot have global scope in Metal.
void CompilerMSL::localize_global_variables()
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	auto iter = global_variables.begin();
	while (iter != global_variables.end())
	{
		uint32_t v_id = *iter;
		auto &var = get<SPIRVariable>(v_id);
		if (var.storage == StorageClassPrivate || var.storage == StorageClassWorkgroup)
		{
			if (!variable_is_lut(var))
				entry_func.add_local_variable(v_id);
			iter = global_variables.erase(iter);
		}
		else
			iter++;
	}
}

// For any global variable accessed directly by a function,
// extract that variable and add it as an argument to that function.
void CompilerMSL::extract_global_variables_from_functions()
{
	// Uniforms
	unordered_set<uint32_t> global_var_ids;
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		// Some builtins resolve directly to a function call which does not need any declared variables.
		// Skip these.
		if (var.storage == StorageClassInput && has_decoration(var.self, DecorationBuiltIn))
		{
			auto bi_type = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
			if (bi_type == BuiltInHelperInvocation && !needs_manual_helper_invocation_updates())
				return;
			if (bi_type == BuiltInHelperInvocation && needs_manual_helper_invocation_updates())
			{
				if (msl_options.is_ios() && !msl_options.supports_msl_version(2, 3))
					SPIRV_CROSS_THROW("simd_is_helper_thread() requires version 2.3 on iOS.");
				else if (msl_options.is_macos() && !msl_options.supports_msl_version(2, 1))
					SPIRV_CROSS_THROW("simd_is_helper_thread() requires version 2.1 on macOS.");
				// Make sure this is declared and initialized.
				// Force this to have the proper name.
				set_name(var.self, builtin_to_glsl(BuiltInHelperInvocation, StorageClassInput));
				auto &entry_func = this->get<SPIRFunction>(ir.default_entry_point);
				entry_func.add_local_variable(var.self);
				vars_needing_early_declaration.push_back(var.self);
				entry_func.fixup_hooks_in.push_back([this, &var]()
				                                    { statement(to_name(var.self), " = simd_is_helper_thread();"); });
			}
		}

		if (var.storage == StorageClassInput || var.storage == StorageClassOutput ||
		    var.storage == StorageClassUniform || var.storage == StorageClassUniformConstant ||
		    var.storage == StorageClassPushConstant || var.storage == StorageClassStorageBuffer)
		{
			global_var_ids.insert(var.self);
		}
	});

	// Local vars that are declared in the main function and accessed directly by a function
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	for (auto &var : entry_func.local_variables)
		if (get<SPIRVariable>(var).storage != StorageClassFunction)
			global_var_ids.insert(var);

	std::set<uint32_t> added_arg_ids;
	unordered_set<uint32_t> processed_func_ids;
	extract_global_variables_from_function(ir.default_entry_point, added_arg_ids, global_var_ids, processed_func_ids);
}

// MSL does not support the use of global variables for shader input content.
// For any global variable accessed directly by the specified function, extract that variable,
// add it as an argument to that function, and the arg to the added_arg_ids collection.
void CompilerMSL::extract_global_variables_from_function(uint32_t func_id, std::set<uint32_t> &added_arg_ids,
                                                         unordered_set<uint32_t> &global_var_ids,
                                                         unordered_set<uint32_t> &processed_func_ids)
{
	// Avoid processing a function more than once
	if (processed_func_ids.find(func_id) != processed_func_ids.end())
	{
		// Return function global variables
		added_arg_ids = function_global_vars[func_id];
		return;
	}

	processed_func_ids.insert(func_id);

	auto &func = get<SPIRFunction>(func_id);

	// Recursively establish global args added to functions on which we depend.
	for (auto block : func.blocks)
	{
		auto &b = get<SPIRBlock>(block);
		for (auto &i : b.ops)
		{
			auto ops = stream(i);
			auto op = static_cast<Op>(i.op);

			switch (op)
			{
			case OpLoad:
			case OpInBoundsAccessChain:
			case OpAccessChain:
			case OpPtrAccessChain:
			case OpArrayLength:
			{
				uint32_t base_id = ops[2];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);

				// Use Metal's native frame-buffer fetch API for subpass inputs.
				auto &type = get<SPIRType>(ops[0]);
				if (type.basetype == SPIRType::Image && type.image.dim == DimSubpassData &&
				    (!msl_options.use_framebuffer_fetch_subpasses))
				{
					// Implicitly reads gl_FragCoord.
					assert(builtin_frag_coord_id != 0);
					added_arg_ids.insert(builtin_frag_coord_id);
					if (msl_options.multiview)
					{
						// Implicitly reads gl_ViewIndex.
						assert(builtin_view_idx_id != 0);
						added_arg_ids.insert(builtin_view_idx_id);
					}
					else if (msl_options.arrayed_subpass_input)
					{
						// Implicitly reads gl_Layer.
						assert(builtin_layer_id != 0);
						added_arg_ids.insert(builtin_layer_id);
					}
				}

				break;
			}

			case OpFunctionCall:
			{
				// First see if any of the function call args are globals
				for (uint32_t arg_idx = 3; arg_idx < i.length; arg_idx++)
				{
					uint32_t arg_id = ops[arg_idx];
					if (global_var_ids.find(arg_id) != global_var_ids.end())
						added_arg_ids.insert(arg_id);
				}

				// Then recurse into the function itself to extract globals used internally in the function
				uint32_t inner_func_id = ops[2];
				std::set<uint32_t> inner_func_args;
				extract_global_variables_from_function(inner_func_id, inner_func_args, global_var_ids,
				                                       processed_func_ids);
				added_arg_ids.insert(inner_func_args.begin(), inner_func_args.end());
				break;
			}

			case OpStore:
			{
				uint32_t base_id = ops[0];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);

				uint32_t rvalue_id = ops[1];
				if (global_var_ids.find(rvalue_id) != global_var_ids.end())
					added_arg_ids.insert(rvalue_id);

				if (needs_frag_discard_checks())
					added_arg_ids.insert(builtin_helper_invocation_id);

				break;
			}

			case OpSelect:
			{
				uint32_t base_id = ops[3];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);
				base_id = ops[4];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);
				break;
			}

			case OpAtomicExchange:
			case OpAtomicCompareExchange:
			case OpAtomicStore:
			case OpAtomicIIncrement:
			case OpAtomicIDecrement:
			case OpAtomicIAdd:
			case OpAtomicFAddEXT:
			case OpAtomicISub:
			case OpAtomicSMin:
			case OpAtomicUMin:
			case OpAtomicSMax:
			case OpAtomicUMax:
			case OpAtomicAnd:
			case OpAtomicOr:
			case OpAtomicXor:
			case OpImageWrite:
				if (needs_frag_discard_checks())
					added_arg_ids.insert(builtin_helper_invocation_id);
				break;

			// Emulate texture2D atomic operations
			case OpImageTexelPointer:
			{
				// When using the pointer, we need to know which variable it is actually loaded from.
				uint32_t base_id = ops[2];
				auto *var = maybe_get_backing_variable(base_id);
				if (var && atomic_image_vars.count(var->self))
				{
					if (global_var_ids.find(base_id) != global_var_ids.end())
						added_arg_ids.insert(base_id);
				}
				break;
			}

			case OpExtInst:
			{
				uint32_t extension_set = ops[2];
				if (get<SPIRExtension>(extension_set).ext == SPIRExtension::GLSL)
				{
					auto op_450 = static_cast<GLSLstd450>(ops[3]);
					switch (op_450)
					{
					case GLSLstd450InterpolateAtCentroid:
					case GLSLstd450InterpolateAtSample:
					case GLSLstd450InterpolateAtOffset:
					{
						// For these, we really need the stage-in block. It is theoretically possible to pass the
						// interpolant object, but a) doing so would require us to create an entirely new variable
						// with Interpolant type, and b) if we have a struct or array, handling all the members and
						// elements could get unwieldy fast.
						added_arg_ids.insert(stage_in_var_id);
						break;
					}

					case GLSLstd450Modf:
					case GLSLstd450Frexp:
					{
						uint32_t base_id = ops[5];
						if (global_var_ids.find(base_id) != global_var_ids.end())
							added_arg_ids.insert(base_id);
						break;
					}

					default:
						break;
					}
				}
				break;
			}

			case OpGroupNonUniformInverseBallot:
			{
				added_arg_ids.insert(builtin_subgroup_invocation_id_id);
				break;
			}

			case OpGroupNonUniformBallotFindLSB:
			case OpGroupNonUniformBallotFindMSB:
			{
				added_arg_ids.insert(builtin_subgroup_size_id);
				break;
			}

			case OpGroupNonUniformBallotBitCount:
			{
				auto operation = static_cast<GroupOperation>(ops[3]);
				switch (operation)
				{
				case GroupOperationReduce:
					added_arg_ids.insert(builtin_subgroup_size_id);
					break;
				case GroupOperationInclusiveScan:
				case GroupOperationExclusiveScan:
					added_arg_ids.insert(builtin_subgroup_invocation_id_id);
					break;
				default:
					break;
				}
				break;
			}

			case OpDemoteToHelperInvocation:
				if (needs_manual_helper_invocation_updates() &&
				    (active_input_builtins.get(BuiltInHelperInvocation) || needs_helper_invocation))
					added_arg_ids.insert(builtin_helper_invocation_id);
				break;

			case OpIsHelperInvocationEXT:
				if (needs_manual_helper_invocation_updates())
					added_arg_ids.insert(builtin_helper_invocation_id);
				break;

			case OpRayQueryInitializeKHR:
			case OpRayQueryProceedKHR:
			case OpRayQueryTerminateKHR:
			case OpRayQueryGenerateIntersectionKHR:
			case OpRayQueryConfirmIntersectionKHR:
			{
				// Ray query accesses memory directly, need check pass down object if using Private storage class.
				uint32_t base_id = ops[0];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);
				break;
			}

			case OpRayQueryGetRayTMinKHR:
			case OpRayQueryGetRayFlagsKHR:
			case OpRayQueryGetWorldRayOriginKHR:
			case OpRayQueryGetWorldRayDirectionKHR:
			case OpRayQueryGetIntersectionCandidateAABBOpaqueKHR:
			case OpRayQueryGetIntersectionTypeKHR:
			case OpRayQueryGetIntersectionTKHR:
			case OpRayQueryGetIntersectionInstanceCustomIndexKHR:
			case OpRayQueryGetIntersectionInstanceIdKHR:
			case OpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR:
			case OpRayQueryGetIntersectionGeometryIndexKHR:
			case OpRayQueryGetIntersectionPrimitiveIndexKHR:
			case OpRayQueryGetIntersectionBarycentricsKHR:
			case OpRayQueryGetIntersectionFrontFaceKHR:
			case OpRayQueryGetIntersectionObjectRayDirectionKHR:
			case OpRayQueryGetIntersectionObjectRayOriginKHR:
			case OpRayQueryGetIntersectionObjectToWorldKHR:
			case OpRayQueryGetIntersectionWorldToObjectKHR:
			{
				// Ray query accesses memory directly, need check pass down object if using Private storage class.
				uint32_t base_id = ops[2];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);
				break;
			}

			default:
				break;
			}

			if (needs_manual_helper_invocation_updates() && b.terminator == SPIRBlock::Kill &&
			    (active_input_builtins.get(BuiltInHelperInvocation) || needs_helper_invocation))
				added_arg_ids.insert(builtin_helper_invocation_id);

			// TODO: Add all other operations which can affect memory.
			// We should consider a more unified system here to reduce boiler-plate.
			// This kind of analysis is done in several places ...
		}
	}

	function_global_vars[func_id] = added_arg_ids;

	// Add the global variables as arguments to the function
	if (func_id != ir.default_entry_point)
	{
		bool control_point_added_in = false;
		bool control_point_added_out = false;
		bool patch_added_in = false;
		bool patch_added_out = false;

		for (uint32_t arg_id : added_arg_ids)
		{
			auto &var = get<SPIRVariable>(arg_id);
			uint32_t type_id = var.basetype;
			auto *p_type = &get<SPIRType>(type_id);
			BuiltIn bi_type = BuiltIn(get_decoration(arg_id, DecorationBuiltIn));

			bool is_patch = has_decoration(arg_id, DecorationPatch) || is_patch_block(*p_type);
			bool is_block = has_decoration(p_type->self, DecorationBlock);
			bool is_control_point_storage =
			    !is_patch && ((is_tessellation_shader() && var.storage == StorageClassInput) ||
			                  (is_tesc_shader() && var.storage == StorageClassOutput));
			bool is_patch_block_storage = is_patch && is_block && var.storage == StorageClassOutput;
			bool is_builtin = is_builtin_variable(var);
			bool variable_is_stage_io =
					!is_builtin || bi_type == BuiltInPosition || bi_type == BuiltInPointSize ||
					bi_type == BuiltInClipDistance || bi_type == BuiltInCullDistance ||
					p_type->basetype == SPIRType::Struct;
			bool is_redirected_to_global_stage_io = (is_control_point_storage || is_patch_block_storage) &&
			                                        variable_is_stage_io;

			// If output is masked it is not considered part of the global stage IO interface.
			if (is_redirected_to_global_stage_io && var.storage == StorageClassOutput)
				is_redirected_to_global_stage_io = !is_stage_output_variable_masked(var);

			if (is_redirected_to_global_stage_io)
			{
				// Tessellation control shaders see inputs and per-point outputs as arrays.
				// Similarly, tessellation evaluation shaders see per-point inputs as arrays.
				// We collected them into a structure; we must pass the array of this
				// structure to the function.
				std::string name;
				if (is_patch)
					name = var.storage == StorageClassInput ? patch_stage_in_var_name : patch_stage_out_var_name;
				else
					name = var.storage == StorageClassInput ? "gl_in" : "gl_out";

				if (var.storage == StorageClassOutput && has_decoration(p_type->self, DecorationBlock))
				{
					// If we're redirecting a block, we might still need to access the original block
					// variable if we're masking some members.
					for (uint32_t mbr_idx = 0; mbr_idx < uint32_t(p_type->member_types.size()); mbr_idx++)
					{
						if (is_stage_output_block_member_masked(var, mbr_idx, true))
						{
							func.add_parameter(var.basetype, var.self, true);
							break;
						}
					}
				}

				if (var.storage == StorageClassInput)
				{
					auto &added_in = is_patch ? patch_added_in : control_point_added_in;
					if (added_in)
						continue;
					arg_id = is_patch ? patch_stage_in_var_id : stage_in_ptr_var_id;
					added_in = true;
				}
				else if (var.storage == StorageClassOutput)
				{
					auto &added_out = is_patch ? patch_added_out : control_point_added_out;
					if (added_out)
						continue;
					arg_id = is_patch ? patch_stage_out_var_id : stage_out_ptr_var_id;
					added_out = true;
				}

				type_id = get<SPIRVariable>(arg_id).basetype;
				uint32_t next_id = ir.increase_bound_by(1);
				func.add_parameter(type_id, next_id, true);
				set<SPIRVariable>(next_id, type_id, StorageClassFunction, 0, arg_id);

				set_name(next_id, name);
				if (is_tese_shader() && msl_options.raw_buffer_tese_input && var.storage == StorageClassInput)
					set_decoration(next_id, DecorationNonWritable);
			}
			else if (is_builtin && has_decoration(p_type->self, DecorationBlock))
			{
				// Get the pointee type
				type_id = get_pointee_type_id(type_id);
				p_type = &get<SPIRType>(type_id);

				uint32_t mbr_idx = 0;
				for (auto &mbr_type_id : p_type->member_types)
				{
					BuiltIn builtin = BuiltInMax;
					is_builtin = is_member_builtin(*p_type, mbr_idx, &builtin);
					if (is_builtin && has_active_builtin(builtin, var.storage))
					{
						// Add a arg variable with the same type and decorations as the member
						uint32_t next_ids = ir.increase_bound_by(2);
						uint32_t ptr_type_id = next_ids + 0;
						uint32_t var_id = next_ids + 1;

						// Make sure we have an actual pointer type,
						// so that we will get the appropriate address space when declaring these builtins.
						auto &ptr = set<SPIRType>(ptr_type_id, get<SPIRType>(mbr_type_id));
						ptr.self = mbr_type_id;
						ptr.storage = var.storage;
						ptr.pointer = true;
						ptr.pointer_depth++;
						ptr.parent_type = mbr_type_id;

						func.add_parameter(mbr_type_id, var_id, true);
						set<SPIRVariable>(var_id, ptr_type_id, StorageClassFunction);
						ir.meta[var_id].decoration = ir.meta[type_id].members[mbr_idx];
					}
					mbr_idx++;
				}
			}
			else
			{
				uint32_t next_id = ir.increase_bound_by(1);
				func.add_parameter(type_id, next_id, true);
				set<SPIRVariable>(next_id, type_id, StorageClassFunction, 0, arg_id);

				// Ensure the new variable has all the same meta info
				ir.meta[next_id] = ir.meta[arg_id];
			}
		}
	}
}

// For all variables that are some form of non-input-output interface block, mark that all the structs
// that are recursively contained within the type referenced by that variable should be packed tightly.
void CompilerMSL::mark_packable_structs()
{
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		if (var.storage != StorageClassFunction && !is_hidden_variable(var))
		{
			auto &type = this->get<SPIRType>(var.basetype);
			if (type.pointer &&
			    (type.storage == StorageClassUniform || type.storage == StorageClassUniformConstant ||
			     type.storage == StorageClassPushConstant || type.storage == StorageClassStorageBuffer) &&
			    (has_decoration(type.self, DecorationBlock) || has_decoration(type.self, DecorationBufferBlock)))
				mark_as_packable(type);
		}

		if (var.storage == StorageClassWorkgroup)
		{
			auto *type = &this->get<SPIRType>(var.basetype);
			if (type->basetype == SPIRType::Struct)
				mark_as_workgroup_struct(*type);
		}
	});

	// Physical storage buffer pointers can appear outside of the context of a variable, if the address
	// is calculated from a ulong or uvec2 and cast to a pointer, so check if they need to be packed too.
	ir.for_each_typed_id<SPIRType>([&](uint32_t, SPIRType &type) {
		if (type.basetype == SPIRType::Struct && type.pointer && type.storage == StorageClassPhysicalStorageBuffer)
			mark_as_packable(type);
	});
}

// If the specified type is a struct, it and any nested structs
// are marked as packable with the SPIRVCrossDecorationBufferBlockRepacked decoration,
void CompilerMSL::mark_as_packable(SPIRType &type)
{
	// If this is not the base type (eg. it's a pointer or array), tunnel down
	if (type.parent_type)
	{
		mark_as_packable(get<SPIRType>(type.parent_type));
		return;
	}

	// Handle possible recursion when a struct contains a pointer to its own type nested somewhere.
	if (type.basetype == SPIRType::Struct && !has_extended_decoration(type.self, SPIRVCrossDecorationBufferBlockRepacked))
	{
		set_extended_decoration(type.self, SPIRVCrossDecorationBufferBlockRepacked);

		// Recurse
		uint32_t mbr_cnt = uint32_t(type.member_types.size());
		for (uint32_t mbr_idx = 0; mbr_idx < mbr_cnt; mbr_idx++)
		{
			uint32_t mbr_type_id = type.member_types[mbr_idx];
			auto &mbr_type = get<SPIRType>(mbr_type_id);
			mark_as_packable(mbr_type);
			if (mbr_type.type_alias)
			{
				auto &mbr_type_alias = get<SPIRType>(mbr_type.type_alias);
				mark_as_packable(mbr_type_alias);
			}
		}
	}
}

// If the specified type is a struct, it and any nested structs
// are marked as used with workgroup storage using the SPIRVCrossDecorationWorkgroupStruct decoration.
void CompilerMSL::mark_as_workgroup_struct(SPIRType &type)
{
	// If this is not the base type (eg. it's a pointer or array), tunnel down
	if (type.parent_type)
	{
		mark_as_workgroup_struct(get<SPIRType>(type.parent_type));
		return;
	}

	// Handle possible recursion when a struct contains a pointer to its own type nested somewhere.
	if (type.basetype == SPIRType::Struct && !has_extended_decoration(type.self, SPIRVCrossDecorationWorkgroupStruct))
	{
		set_extended_decoration(type.self, SPIRVCrossDecorationWorkgroupStruct);

		// Recurse
		uint32_t mbr_cnt = uint32_t(type.member_types.size());
		for (uint32_t mbr_idx = 0; mbr_idx < mbr_cnt; mbr_idx++)
		{
			uint32_t mbr_type_id = type.member_types[mbr_idx];
			auto &mbr_type = get<SPIRType>(mbr_type_id);
			mark_as_workgroup_struct(mbr_type);
			if (mbr_type.type_alias)
			{
				auto &mbr_type_alias = get<SPIRType>(mbr_type.type_alias);
				mark_as_workgroup_struct(mbr_type_alias);
			}
		}
	}
}

// If a shader input exists at the location, it is marked as being used by this shader
void CompilerMSL::mark_location_as_used_by_shader(uint32_t location, const SPIRType &type,
                                                  StorageClass storage, bool fallback)
{
	uint32_t count = type_to_location_count(type);
	switch (storage)
	{
	case StorageClassInput:
		for (uint32_t i = 0; i < count; i++)
		{
			location_inputs_in_use.insert(location + i);
			if (fallback)
				location_inputs_in_use_fallback.insert(location + i);
		}
		break;
	case StorageClassOutput:
		for (uint32_t i = 0; i < count; i++)
		{
			location_outputs_in_use.insert(location + i);
			if (fallback)
				location_outputs_in_use_fallback.insert(location + i);
		}
		break;
	default:
		return;
	}
}

uint32_t CompilerMSL::get_target_components_for_fragment_location(uint32_t location) const
{
	auto itr = fragment_output_components.find(location);
	if (itr == end(fragment_output_components))
		return 4;
	else
		return itr->second;
}

uint32_t CompilerMSL::build_extended_vector_type(uint32_t type_id, uint32_t components, SPIRType::BaseType basetype)
{
	uint32_t new_type_id = ir.increase_bound_by(1);
	auto &old_type = get<SPIRType>(type_id);
	auto *type = &set<SPIRType>(new_type_id, old_type);
	type->vecsize = components;
	if (basetype != SPIRType::Unknown)
		type->basetype = basetype;
	type->self = new_type_id;
	type->parent_type = type_id;
	type->array.clear();
	type->array_size_literal.clear();
	type->pointer = false;

	if (is_array(old_type))
	{
		uint32_t array_type_id = ir.increase_bound_by(1);
		type = &set<SPIRType>(array_type_id, *type);
		type->parent_type = new_type_id;
		type->array = old_type.array;
		type->array_size_literal = old_type.array_size_literal;
		new_type_id = array_type_id;
	}

	if (old_type.pointer)
	{
		uint32_t ptr_type_id = ir.increase_bound_by(1);
		type = &set<SPIRType>(ptr_type_id, *type);
		type->self = new_type_id;
		type->parent_type = new_type_id;
		type->storage = old_type.storage;
		type->pointer = true;
		type->pointer_depth++;
		new_type_id = ptr_type_id;
	}

	return new_type_id;
}

uint32_t CompilerMSL::build_msl_interpolant_type(uint32_t type_id, bool is_noperspective)
{
	uint32_t new_type_id = ir.increase_bound_by(1);
	SPIRType &type = set<SPIRType>(new_type_id, get<SPIRType>(type_id));
	type.basetype = SPIRType::Interpolant;
	type.parent_type = type_id;
	// In Metal, the pull-model interpolant type encodes perspective-vs-no-perspective in the type itself.
	// Add this decoration so we know which argument to pass to the template.
	if (is_noperspective)
		set_decoration(new_type_id, DecorationNoPerspective);
	return new_type_id;
}

bool CompilerMSL::add_component_variable_to_interface_block(spv::StorageClass storage, const std::string &ib_var_ref,
                                                            SPIRVariable &var,
                                                            const SPIRType &type,
                                                            InterfaceBlockMeta &meta)
{
	// Deal with Component decorations.
	const InterfaceBlockMeta::LocationMeta *location_meta = nullptr;
	uint32_t location = ~0u;
	if (has_decoration(var.self, DecorationLocation))
	{
		location = get_decoration(var.self, DecorationLocation);
		auto location_meta_itr = meta.location_meta.find(location);
		if (location_meta_itr != end(meta.location_meta))
			location_meta = &location_meta_itr->second;
	}

	// Check if we need to pad fragment output to match a certain number of components.
	if (location_meta)
	{
		bool pad_fragment_output = has_decoration(var.self, DecorationLocation) &&
		                           msl_options.pad_fragment_output_components &&
		                           get_entry_point().model == ExecutionModelFragment && storage == StorageClassOutput;

		auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
		uint32_t start_component = get_decoration(var.self, DecorationComponent);
		uint32_t type_components = type.vecsize;
		uint32_t num_components = location_meta->num_components;

		if (pad_fragment_output)
		{
			uint32_t locn = get_decoration(var.self, DecorationLocation);
			num_components = max<uint32_t>(num_components, get_target_components_for_fragment_location(locn));
		}

		// We have already declared an IO block member as m_location_N.
		// Just emit an early-declared variable and fixup as needed.
		// Arrays need to be unrolled here since each location might need a different number of components.
		entry_func.add_local_variable(var.self);
		vars_needing_early_declaration.push_back(var.self);

		if (var.storage == StorageClassInput)
		{
			entry_func.fixup_hooks_in.push_back([=, &type, &var]() {
				if (!type.array.empty())
				{
					uint32_t array_size = to_array_size_literal(type);
					for (uint32_t loc_off = 0; loc_off < array_size; loc_off++)
					{
						statement(to_name(var.self), "[", loc_off, "]", " = ", ib_var_ref,
						          ".m_location_", location + loc_off,
						          vector_swizzle(type_components, start_component), ";");
					}
				}
				else
				{
					statement(to_name(var.self), " = ", ib_var_ref, ".m_location_", location,
					          vector_swizzle(type_components, start_component), ";");
				}
			});
		}
		else
		{
			entry_func.fixup_hooks_out.push_back([=, &type, &var]() {
				if (!type.array.empty())
				{
					uint32_t array_size = to_array_size_literal(type);
					for (uint32_t loc_off = 0; loc_off < array_size; loc_off++)
					{
						statement(ib_var_ref, ".m_location_", location + loc_off,
						          vector_swizzle(type_components, start_component), " = ",
						          to_name(var.self), "[", loc_off, "];");
					}
				}
				else
				{
					statement(ib_var_ref, ".m_location_", location,
					          vector_swizzle(type_components, start_component), " = ", to_name(var.self), ";");
				}
			});
		}
		return true;
	}
	else
		return false;
}

void CompilerMSL::add_plain_variable_to_interface_block(StorageClass storage, const string &ib_var_ref,
                                                        SPIRType &ib_type, SPIRVariable &var, InterfaceBlockMeta &meta)
{
	bool is_builtin = is_builtin_variable(var);
	BuiltIn builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
	bool is_flat = has_decoration(var.self, DecorationFlat);
	bool is_noperspective = has_decoration(var.self, DecorationNoPerspective);
	bool is_centroid = has_decoration(var.self, DecorationCentroid);
	bool is_sample = has_decoration(var.self, DecorationSample);

	// Add a reference to the variable type to the interface struct.
	uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());
	uint32_t type_id = ensure_correct_builtin_type(var.basetype, builtin);
	var.basetype = type_id;

	type_id = get_pointee_type_id(var.basetype);
	if (meta.strip_array && is_array(get<SPIRType>(type_id)))
		type_id = get<SPIRType>(type_id).parent_type;
	auto &type = get<SPIRType>(type_id);
	uint32_t target_components = 0;
	uint32_t type_components = type.vecsize;

	bool padded_output = false;
	bool padded_input = false;
	uint32_t start_component = 0;

	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);

	if (add_component_variable_to_interface_block(storage, ib_var_ref, var, type, meta))
		return;

	bool pad_fragment_output = has_decoration(var.self, DecorationLocation) &&
	                           msl_options.pad_fragment_output_components &&
	                           get_entry_point().model == ExecutionModelFragment && storage == StorageClassOutput;

	if (pad_fragment_output)
	{
		uint32_t locn = get_decoration(var.self, DecorationLocation);
		target_components = get_target_components_for_fragment_location(locn);
		if (type_components < target_components)
		{
			// Make a new type here.
			type_id = build_extended_vector_type(type_id, target_components);
			padded_output = true;
		}
	}

	if (storage == StorageClassInput && pull_model_inputs.count(var.self))
		ib_type.member_types.push_back(build_msl_interpolant_type(type_id, is_noperspective));
	else
		ib_type.member_types.push_back(type_id);

	// Give the member a name
	string mbr_name = ensure_valid_name(to_expression(var.self), "m");
	set_member_name(ib_type.self, ib_mbr_idx, mbr_name);

	// Update the original variable reference to include the structure reference
	string qual_var_name = ib_var_ref + "." + mbr_name;
	// If using pull-model interpolation, need to add a call to the correct interpolation method.
	if (storage == StorageClassInput && pull_model_inputs.count(var.self))
	{
		if (is_centroid)
			qual_var_name += ".interpolate_at_centroid()";
		else if (is_sample)
			qual_var_name += join(".interpolate_at_sample(", to_expression(builtin_sample_id_id), ")");
		else
			qual_var_name += ".interpolate_at_center()";
	}

	if (padded_output || padded_input)
	{
		entry_func.add_local_variable(var.self);
		vars_needing_early_declaration.push_back(var.self);

		if (padded_output)
		{
			entry_func.fixup_hooks_out.push_back([=, &var]() {
				statement(qual_var_name, vector_swizzle(type_components, start_component), " = ", to_name(var.self),
				          ";");
			});
		}
		else
		{
			entry_func.fixup_hooks_in.push_back([=, &var]() {
				statement(to_name(var.self), " = ", qual_var_name, vector_swizzle(type_components, start_component),
				          ";");
			});
		}
	}
	else if (!meta.strip_array)
		ir.meta[var.self].decoration.qualified_alias = qual_var_name;

	if (var.storage == StorageClassOutput && var.initializer != ID(0))
	{
		if (padded_output || padded_input)
		{
			entry_func.fixup_hooks_in.push_back(
			    [=, &var]() { statement(to_name(var.self), " = ", to_expression(var.initializer), ";"); });
		}
		else
		{
			if (meta.strip_array)
			{
				entry_func.fixup_hooks_in.push_back([=, &var]() {
					uint32_t index = get_extended_decoration(var.self, SPIRVCrossDecorationInterfaceMemberIndex);
					auto invocation = to_tesc_invocation_id();
					statement(to_expression(stage_out_ptr_var_id), "[",
					          invocation, "].",
					          to_member_name(ib_type, index), " = ", to_expression(var.initializer), "[",
					          invocation, "];");
				});
			}
			else
			{
				entry_func.fixup_hooks_in.push_back([=, &var]() {
					statement(qual_var_name, " = ", to_expression(var.initializer), ";");
				});
			}
		}
	}

	// Copy the variable location from the original variable to the member
	if (get_decoration_bitset(var.self).get(DecorationLocation))
	{
		uint32_t locn = get_decoration(var.self, DecorationLocation);
		uint32_t comp = get_decoration(var.self, DecorationComponent);
		if (storage == StorageClassInput)
		{
			type_id = ensure_correct_input_type(var.basetype, locn, comp, 0, meta.strip_array);
			var.basetype = type_id;

			type_id = get_pointee_type_id(type_id);
			if (meta.strip_array && is_array(get<SPIRType>(type_id)))
				type_id = get<SPIRType>(type_id).parent_type;
			if (pull_model_inputs.count(var.self))
				ib_type.member_types[ib_mbr_idx] = build_msl_interpolant_type(type_id, is_noperspective);
			else
				ib_type.member_types[ib_mbr_idx] = type_id;
		}
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
		if (comp)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationComponent, comp);
		mark_location_as_used_by_shader(locn, get<SPIRType>(type_id), storage);
	}
	else if (is_builtin && is_tessellation_shader() && storage == StorageClassInput && inputs_by_builtin.count(builtin))
	{
		uint32_t locn = inputs_by_builtin[builtin].location;
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
		mark_location_as_used_by_shader(locn, type, storage);
	}
	else if (is_builtin && capture_output_to_buffer && storage == StorageClassOutput && outputs_by_builtin.count(builtin))
	{
		uint32_t locn = outputs_by_builtin[builtin].location;
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
		mark_location_as_used_by_shader(locn, type, storage);
	}

	if (get_decoration_bitset(var.self).get(DecorationComponent))
	{
		uint32_t component = get_decoration(var.self, DecorationComponent);
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationComponent, component);
	}

	if (get_decoration_bitset(var.self).get(DecorationIndex))
	{
		uint32_t index = get_decoration(var.self, DecorationIndex);
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationIndex, index);
	}

	// Mark the member as builtin if needed
	if (is_builtin)
	{
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);
		if (builtin == BuiltInPosition && storage == StorageClassOutput)
			qual_pos_var_name = qual_var_name;
	}

	// Copy interpolation decorations if needed
	if (storage != StorageClassInput || !pull_model_inputs.count(var.self))
	{
		if (is_flat)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationFlat);
		if (is_noperspective)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationNoPerspective);
		if (is_centroid)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationCentroid);
		if (is_sample)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationSample);
	}

	set_extended_member_decoration(ib_type.self, ib_mbr_idx, SPIRVCrossDecorationInterfaceOrigID, var.self);
}

void CompilerMSL::add_composite_variable_to_interface_block(StorageClass storage, const string &ib_var_ref,
                                                            SPIRType &ib_type, SPIRVariable &var,
                                                            InterfaceBlockMeta &meta)
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	auto &var_type = meta.strip_array ? get_variable_element_type(var) : get_variable_data_type(var);
	uint32_t elem_cnt = 0;

	if (add_component_variable_to_interface_block(storage, ib_var_ref, var, var_type, meta))
		return;

	if (is_matrix(var_type))
	{
		if (is_array(var_type))
			SPIRV_CROSS_THROW("MSL cannot emit arrays-of-matrices in input and output variables.");

		elem_cnt = var_type.columns;
	}
	else if (is_array(var_type))
	{
		if (var_type.array.size() != 1)
			SPIRV_CROSS_THROW("MSL cannot emit arrays-of-arrays in input and output variables.");

		elem_cnt = to_array_size_literal(var_type);
	}

	bool is_builtin = is_builtin_variable(var);
	BuiltIn builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
	bool is_flat = has_decoration(var.self, DecorationFlat);
	bool is_noperspective = has_decoration(var.self, DecorationNoPerspective);
	bool is_centroid = has_decoration(var.self, DecorationCentroid);
	bool is_sample = has_decoration(var.self, DecorationSample);

	auto *usable_type = &var_type;
	if (usable_type->pointer)
		usable_type = &get<SPIRType>(usable_type->parent_type);
	while (is_array(*usable_type) || is_matrix(*usable_type))
		usable_type = &get<SPIRType>(usable_type->parent_type);

	// If a builtin, force it to have the proper name.
	if (is_builtin)
		set_name(var.self, builtin_to_glsl(builtin, StorageClassFunction));

	bool flatten_from_ib_var = false;
	string flatten_from_ib_mbr_name;

	if (storage == StorageClassOutput && is_builtin && builtin == BuiltInClipDistance)
	{
		// Also declare [[clip_distance]] attribute here.
		uint32_t clip_array_mbr_idx = uint32_t(ib_type.member_types.size());
		ib_type.member_types.push_back(get_variable_data_type_id(var));
		set_member_decoration(ib_type.self, clip_array_mbr_idx, DecorationBuiltIn, BuiltInClipDistance);

		flatten_from_ib_mbr_name = builtin_to_glsl(BuiltInClipDistance, StorageClassOutput);
		set_member_name(ib_type.self, clip_array_mbr_idx, flatten_from_ib_mbr_name);

		// When we flatten, we flatten directly from the "out" struct,
		// not from a function variable.
		flatten_from_ib_var = true;

		if (!msl_options.enable_clip_distance_user_varying)
			return;
	}
	else if (!meta.strip_array)
	{
		// Only flatten/unflatten IO composites for non-tessellation cases where arrays are not stripped.
		entry_func.add_local_variable(var.self);
		// We need to declare the variable early and at entry-point scope.
		vars_needing_early_declaration.push_back(var.self);
	}

	for (uint32_t i = 0; i < elem_cnt; i++)
	{
		// Add a reference to the variable type to the interface struct.
		uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());

		uint32_t target_components = 0;
		bool padded_output = false;
		uint32_t type_id = usable_type->self;

		// Check if we need to pad fragment output to match a certain number of components.
		if (get_decoration_bitset(var.self).get(DecorationLocation) && msl_options.pad_fragment_output_components &&
		    get_entry_point().model == ExecutionModelFragment && storage == StorageClassOutput)
		{
			uint32_t locn = get_decoration(var.self, DecorationLocation) + i;
			target_components = get_target_components_for_fragment_location(locn);
			if (usable_type->vecsize < target_components)
			{
				// Make a new type here.
				type_id = build_extended_vector_type(usable_type->self, target_components);
				padded_output = true;
			}
		}

		if (storage == StorageClassInput && pull_model_inputs.count(var.self))
			ib_type.member_types.push_back(build_msl_interpolant_type(get_pointee_type_id(type_id), is_noperspective));
		else
			ib_type.member_types.push_back(get_pointee_type_id(type_id));

		// Give the member a name
		string mbr_name = ensure_valid_name(join(to_expression(var.self), "_", i), "m");
		set_member_name(ib_type.self, ib_mbr_idx, mbr_name);

		// There is no qualified alias since we need to flatten the internal array on return.
		if (get_decoration_bitset(var.self).get(DecorationLocation))
		{
			uint32_t locn = get_decoration(var.self, DecorationLocation) + i;
			uint32_t comp = get_decoration(var.self, DecorationComponent);
			if (storage == StorageClassInput)
			{
				var.basetype = ensure_correct_input_type(var.basetype, locn, comp, 0, meta.strip_array);
				uint32_t mbr_type_id = ensure_correct_input_type(usable_type->self, locn, comp, 0, meta.strip_array);
				if (storage == StorageClassInput && pull_model_inputs.count(var.self))
					ib_type.member_types[ib_mbr_idx] = build_msl_interpolant_type(mbr_type_id, is_noperspective);
				else
					ib_type.member_types[ib_mbr_idx] = mbr_type_id;
			}
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
			if (comp)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationComponent, comp);
			mark_location_as_used_by_shader(locn, *usable_type, storage);
		}
		else if (is_builtin && is_tessellation_shader() && storage == StorageClassInput && inputs_by_builtin.count(builtin))
		{
			uint32_t locn = inputs_by_builtin[builtin].location + i;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
			mark_location_as_used_by_shader(locn, *usable_type, storage);
		}
		else if (is_builtin && capture_output_to_buffer && storage == StorageClassOutput && outputs_by_builtin.count(builtin))
		{
			uint32_t locn = outputs_by_builtin[builtin].location + i;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
			mark_location_as_used_by_shader(locn, *usable_type, storage);
		}
		else if (is_builtin && (builtin == BuiltInClipDistance || builtin == BuiltInCullDistance))
		{
			// Declare the Clip/CullDistance as [[user(clip/cullN)]].
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationIndex, i);
		}

		if (get_decoration_bitset(var.self).get(DecorationIndex))
		{
			uint32_t index = get_decoration(var.self, DecorationIndex);
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationIndex, index);
		}

		if (storage != StorageClassInput || !pull_model_inputs.count(var.self))
		{
			// Copy interpolation decorations if needed
			if (is_flat)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationFlat);
			if (is_noperspective)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationNoPerspective);
			if (is_centroid)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationCentroid);
			if (is_sample)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationSample);
		}

		set_extended_member_decoration(ib_type.self, ib_mbr_idx, SPIRVCrossDecorationInterfaceOrigID, var.self);

		// Only flatten/unflatten IO composites for non-tessellation cases where arrays are not stripped.
		if (!meta.strip_array)
		{
			switch (storage)
			{
			case StorageClassInput:
				entry_func.fixup_hooks_in.push_back([=, &var]() {
					if (pull_model_inputs.count(var.self))
					{
						string lerp_call;
						if (is_centroid)
							lerp_call = ".interpolate_at_centroid()";
						else if (is_sample)
							lerp_call = join(".interpolate_at_sample(", to_expression(builtin_sample_id_id), ")");
						else
							lerp_call = ".interpolate_at_center()";
						statement(to_name(var.self), "[", i, "] = ", ib_var_ref, ".", mbr_name, lerp_call, ";");
					}
					else
					{
						statement(to_name(var.self), "[", i, "] = ", ib_var_ref, ".", mbr_name, ";");
					}
				});
				break;

			case StorageClassOutput:
				entry_func.fixup_hooks_out.push_back([=, &var]() {
					if (padded_output)
					{
						auto &padded_type = this->get<SPIRType>(type_id);
						statement(
						    ib_var_ref, ".", mbr_name, " = ",
						    remap_swizzle(padded_type, usable_type->vecsize, join(to_name(var.self), "[", i, "]")),
						    ";");
					}
					else if (flatten_from_ib_var)
						statement(ib_var_ref, ".", mbr_name, " = ", ib_var_ref, ".", flatten_from_ib_mbr_name, "[", i,
						          "];");
					else
						statement(ib_var_ref, ".", mbr_name, " = ", to_name(var.self), "[", i, "];");
				});
				break;

			default:
				break;
			}
		}
	}
}

void CompilerMSL::add_composite_member_variable_to_interface_block(StorageClass storage,
                                                                   const string &ib_var_ref, SPIRType &ib_type,
                                                                   SPIRVariable &var, SPIRType &var_type,
                                                                   uint32_t mbr_idx, InterfaceBlockMeta &meta,
                                                                   const string &mbr_name_qual,
                                                                   const string &var_chain_qual,
                                                                   uint32_t &location, uint32_t &var_mbr_idx)
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);

	BuiltIn builtin = BuiltInMax;
	bool is_builtin = is_member_builtin(var_type, mbr_idx, &builtin);
	bool is_flat =
	    has_member_decoration(var_type.self, mbr_idx, DecorationFlat) || has_decoration(var.self, DecorationFlat);
	bool is_noperspective = has_member_decoration(var_type.self, mbr_idx, DecorationNoPerspective) ||
	                        has_decoration(var.self, DecorationNoPerspective);
	bool is_centroid = has_member_decoration(var_type.self, mbr_idx, DecorationCentroid) ||
	                   has_decoration(var.self, DecorationCentroid);
	bool is_sample =
	    has_member_decoration(var_type.self, mbr_idx, DecorationSample) || has_decoration(var.self, DecorationSample);

	uint32_t mbr_type_id = var_type.member_types[mbr_idx];
	auto &mbr_type = get<SPIRType>(mbr_type_id);

	bool mbr_is_indexable = false;
	uint32_t elem_cnt = 1;
	if (is_matrix(mbr_type))
	{
		if (is_array(mbr_type))
			SPIRV_CROSS_THROW("MSL cannot emit arrays-of-matrices in input and output variables.");

		mbr_is_indexable = true;
		elem_cnt = mbr_type.columns;
	}
	else if (is_array(mbr_type))
	{
		if (mbr_type.array.size() != 1)
			SPIRV_CROSS_THROW("MSL cannot emit arrays-of-arrays in input and output variables.");

		mbr_is_indexable = true;
		elem_cnt = to_array_size_literal(mbr_type);
	}

	auto *usable_type = &mbr_type;
	if (usable_type->pointer)
		usable_type = &get<SPIRType>(usable_type->parent_type);
	while (is_array(*usable_type) || is_matrix(*usable_type))
		usable_type = &get<SPIRType>(usable_type->parent_type);

	bool flatten_from_ib_var = false;
	string flatten_from_ib_mbr_name;

	if (storage == StorageClassOutput && is_builtin && builtin == BuiltInClipDistance)
	{
		// Also declare [[clip_distance]] attribute here.
		uint32_t clip_array_mbr_idx = uint32_t(ib_type.member_types.size());
		ib_type.member_types.push_back(mbr_type_id);
		set_member_decoration(ib_type.self, clip_array_mbr_idx, DecorationBuiltIn, BuiltInClipDistance);

		flatten_from_ib_mbr_name = builtin_to_glsl(BuiltInClipDistance, StorageClassOutput);
		set_member_name(ib_type.self, clip_array_mbr_idx, flatten_from_ib_mbr_name);

		// When we flatten, we flatten directly from the "out" struct,
		// not from a function variable.
		flatten_from_ib_var = true;

		if (!msl_options.enable_clip_distance_user_varying)
			return;
	}

	// Recursively handle nested structures.
	if (mbr_type.basetype == SPIRType::Struct)
	{
		for (uint32_t i = 0; i < elem_cnt; i++)
		{
			string mbr_name = append_member_name(mbr_name_qual, var_type, mbr_idx) + (mbr_is_indexable ? join("_", i) : "");
			string var_chain = join(var_chain_qual, ".", to_member_name(var_type, mbr_idx), (mbr_is_indexable ? join("[", i, "]") : ""));
			uint32_t sub_mbr_cnt = uint32_t(mbr_type.member_types.size());
			for (uint32_t sub_mbr_idx = 0; sub_mbr_idx < sub_mbr_cnt; sub_mbr_idx++)
			{
				add_composite_member_variable_to_interface_block(storage, ib_var_ref, ib_type,
																 var, mbr_type, sub_mbr_idx,
																 meta, mbr_name, var_chain,
																 location, var_mbr_idx);
				// FIXME: Recursive structs and tessellation breaks here.
				var_mbr_idx++;
			}
		}
		return;
	}

	for (uint32_t i = 0; i < elem_cnt; i++)
	{
		// Add a reference to the variable type to the interface struct.
		uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());
		if (storage == StorageClassInput && pull_model_inputs.count(var.self))
			ib_type.member_types.push_back(build_msl_interpolant_type(usable_type->self, is_noperspective));
		else
			ib_type.member_types.push_back(usable_type->self);

		// Give the member a name
		string mbr_name = ensure_valid_name(append_member_name(mbr_name_qual, var_type, mbr_idx) + (mbr_is_indexable ? join("_", i) : ""), "m");
		set_member_name(ib_type.self, ib_mbr_idx, mbr_name);

		// Once we determine the location of the first member within nested structures,
		// from a var of the topmost structure, the remaining flattened members of
		// the nested structures will have consecutive location values. At this point,
		// we've recursively tunnelled into structs, arrays, and matrices, and are
		// down to a single location for each member now.
		if (!is_builtin && location != UINT32_MAX)
		{
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
			mark_location_as_used_by_shader(location, *usable_type, storage);
			location++;
		}
		else if (has_member_decoration(var_type.self, mbr_idx, DecorationLocation))
		{
			location = get_member_decoration(var_type.self, mbr_idx, DecorationLocation) + i;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
			mark_location_as_used_by_shader(location, *usable_type, storage);
			location++;
		}
		else if (has_decoration(var.self, DecorationLocation))
		{
			location = get_accumulated_member_location(var, mbr_idx, meta.strip_array) + i;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
			mark_location_as_used_by_shader(location, *usable_type, storage);
			location++;
		}
		else if (is_builtin && is_tessellation_shader() && storage == StorageClassInput && inputs_by_builtin.count(builtin))
		{
			location = inputs_by_builtin[builtin].location + i;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
			mark_location_as_used_by_shader(location, *usable_type, storage);
			location++;
		}
		else if (is_builtin && capture_output_to_buffer && storage == StorageClassOutput && outputs_by_builtin.count(builtin))
		{
			location = outputs_by_builtin[builtin].location + i;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
			mark_location_as_used_by_shader(location, *usable_type, storage);
			location++;
		}
		else if (is_builtin && (builtin == BuiltInClipDistance || builtin == BuiltInCullDistance))
		{
			// Declare the Clip/CullDistance as [[user(clip/cullN)]].
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationIndex, i);
		}

		if (has_member_decoration(var_type.self, mbr_idx, DecorationComponent))
			SPIRV_CROSS_THROW("DecorationComponent on matrices and arrays is not supported.");

		if (storage != StorageClassInput || !pull_model_inputs.count(var.self))
		{
			// Copy interpolation decorations if needed
			if (is_flat)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationFlat);
			if (is_noperspective)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationNoPerspective);
			if (is_centroid)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationCentroid);
			if (is_sample)
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationSample);
		}

		set_extended_member_decoration(ib_type.self, ib_mbr_idx, SPIRVCrossDecorationInterfaceOrigID, var.self);
		set_extended_member_decoration(ib_type.self, ib_mbr_idx, SPIRVCrossDecorationInterfaceMemberIndex, var_mbr_idx);

		// Unflatten or flatten from [[stage_in]] or [[stage_out]] as appropriate.
		if (!meta.strip_array && meta.allow_local_declaration)
		{
			string var_chain = join(var_chain_qual, ".", to_member_name(var_type, mbr_idx), (mbr_is_indexable ? join("[", i, "]") : ""));
			switch (storage)
			{
			case StorageClassInput:
				entry_func.fixup_hooks_in.push_back([=, &var]() {
					string lerp_call;
					if (pull_model_inputs.count(var.self))
					{
						if (is_centroid)
							lerp_call = ".interpolate_at_centroid()";
						else if (is_sample)
							lerp_call = join(".interpolate_at_sample(", to_expression(builtin_sample_id_id), ")");
						else
							lerp_call = ".interpolate_at_center()";
					}
					statement(var_chain, " = ", ib_var_ref, ".", mbr_name, lerp_call, ";");
				});
				break;

			case StorageClassOutput:
				entry_func.fixup_hooks_out.push_back([=]() {
					if (flatten_from_ib_var)
						statement(ib_var_ref, ".", mbr_name, " = ", ib_var_ref, ".", flatten_from_ib_mbr_name, "[", i, "];");
					else
						statement(ib_var_ref, ".", mbr_name, " = ", var_chain, ";");
				});
				break;

			default:
				break;
			}
		}
	}
}

void CompilerMSL::add_plain_member_variable_to_interface_block(StorageClass storage,
                                                               const string &ib_var_ref, SPIRType &ib_type,
                                                               SPIRVariable &var, SPIRType &var_type,
                                                               uint32_t mbr_idx, InterfaceBlockMeta &meta,
                                                               const string &mbr_name_qual,
                                                               const string &var_chain_qual,
                                                               uint32_t &location, uint32_t &var_mbr_idx)
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);

	BuiltIn builtin = BuiltInMax;
	bool is_builtin = is_member_builtin(var_type, mbr_idx, &builtin);
	bool is_flat =
	    has_member_decoration(var_type.self, mbr_idx, DecorationFlat) || has_decoration(var.self, DecorationFlat);
	bool is_noperspective = has_member_decoration(var_type.self, mbr_idx, DecorationNoPerspective) ||
	                        has_decoration(var.self, DecorationNoPerspective);
	bool is_centroid = has_member_decoration(var_type.self, mbr_idx, DecorationCentroid) ||
	                   has_decoration(var.self, DecorationCentroid);
	bool is_sample =
	    has_member_decoration(var_type.self, mbr_idx, DecorationSample) || has_decoration(var.self, DecorationSample);

	// Add a reference to the member to the interface struct.
	uint32_t mbr_type_id = var_type.member_types[mbr_idx];
	uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());
	mbr_type_id = ensure_correct_builtin_type(mbr_type_id, builtin);
	var_type.member_types[mbr_idx] = mbr_type_id;
	if (storage == StorageClassInput && pull_model_inputs.count(var.self))
		ib_type.member_types.push_back(build_msl_interpolant_type(mbr_type_id, is_noperspective));
	else
		ib_type.member_types.push_back(mbr_type_id);

	// Give the member a name
	string mbr_name = ensure_valid_name(append_member_name(mbr_name_qual, var_type, mbr_idx), "m");
	set_member_name(ib_type.self, ib_mbr_idx, mbr_name);

	// Update the original variable reference to include the structure reference
	string qual_var_name = ib_var_ref + "." + mbr_name;
	// If using pull-model interpolation, need to add a call to the correct interpolation method.
	if (storage == StorageClassInput && pull_model_inputs.count(var.self))
	{
		if (is_centroid)
			qual_var_name += ".interpolate_at_centroid()";
		else if (is_sample)
			qual_var_name += join(".interpolate_at_sample(", to_expression(builtin_sample_id_id), ")");
		else
			qual_var_name += ".interpolate_at_center()";
	}

	bool flatten_stage_out = false;
	string var_chain = var_chain_qual + "." + to_member_name(var_type, mbr_idx);
	if (is_builtin && !meta.strip_array)
	{
		// For the builtin gl_PerVertex, we cannot treat it as a block anyways,
		// so redirect to qualified name.
		set_member_qualified_name(var_type.self, mbr_idx, qual_var_name);
	}
	else if (!meta.strip_array && meta.allow_local_declaration)
	{
		// Unflatten or flatten from [[stage_in]] or [[stage_out]] as appropriate.
		switch (storage)
		{
		case StorageClassInput:
			entry_func.fixup_hooks_in.push_back([=]() {
				statement(var_chain, " = ", qual_var_name, ";");
			});
			break;

		case StorageClassOutput:
			flatten_stage_out = true;
			entry_func.fixup_hooks_out.push_back([=]() {
				statement(qual_var_name, " = ", var_chain, ";");
			});
			break;

		default:
			break;
		}
	}

	// Once we determine the location of the first member within nested structures,
	// from a var of the topmost structure, the remaining flattened members of
	// the nested structures will have consecutive location values. At this point,
	// we've recursively tunnelled into structs, arrays, and matrices, and are
	// down to a single location for each member now.
	if (!is_builtin && location != UINT32_MAX)
	{
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
		mark_location_as_used_by_shader(location, get<SPIRType>(mbr_type_id), storage);
		location += type_to_location_count(get<SPIRType>(mbr_type_id));
	}
	else if (has_member_decoration(var_type.self, mbr_idx, DecorationLocation))
	{
		location = get_member_decoration(var_type.self, mbr_idx, DecorationLocation);
		uint32_t comp = get_member_decoration(var_type.self, mbr_idx, DecorationComponent);
		if (storage == StorageClassInput)
		{
			mbr_type_id = ensure_correct_input_type(mbr_type_id, location, comp, 0, meta.strip_array);
			var_type.member_types[mbr_idx] = mbr_type_id;
			if (storage == StorageClassInput && pull_model_inputs.count(var.self))
				ib_type.member_types[ib_mbr_idx] = build_msl_interpolant_type(mbr_type_id, is_noperspective);
			else
				ib_type.member_types[ib_mbr_idx] = mbr_type_id;
		}
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
		mark_location_as_used_by_shader(location, get<SPIRType>(mbr_type_id), storage);
		location += type_to_location_count(get<SPIRType>(mbr_type_id));
	}
	else if (has_decoration(var.self, DecorationLocation))
	{
		location = get_accumulated_member_location(var, mbr_idx, meta.strip_array);
		if (storage == StorageClassInput)
		{
			mbr_type_id = ensure_correct_input_type(mbr_type_id, location, 0, 0, meta.strip_array);
			var_type.member_types[mbr_idx] = mbr_type_id;
			if (storage == StorageClassInput && pull_model_inputs.count(var.self))
				ib_type.member_types[ib_mbr_idx] = build_msl_interpolant_type(mbr_type_id, is_noperspective);
			else
				ib_type.member_types[ib_mbr_idx] = mbr_type_id;
		}
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
		mark_location_as_used_by_shader(location, get<SPIRType>(mbr_type_id), storage);
		location += type_to_location_count(get<SPIRType>(mbr_type_id));
	}
	else if (is_builtin && is_tessellation_shader() && storage == StorageClassInput && inputs_by_builtin.count(builtin))
	{
		location = inputs_by_builtin[builtin].location;
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
		mark_location_as_used_by_shader(location, get<SPIRType>(mbr_type_id), storage);
		location += type_to_location_count(get<SPIRType>(mbr_type_id));
	}
	else if (is_builtin && capture_output_to_buffer && storage == StorageClassOutput && outputs_by_builtin.count(builtin))
	{
		location = outputs_by_builtin[builtin].location;
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
		mark_location_as_used_by_shader(location, get<SPIRType>(mbr_type_id), storage);
		location += type_to_location_count(get<SPIRType>(mbr_type_id));
	}

	// Copy the component location, if present.
	if (has_member_decoration(var_type.self, mbr_idx, DecorationComponent))
	{
		uint32_t comp = get_member_decoration(var_type.self, mbr_idx, DecorationComponent);
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationComponent, comp);
	}

	// Mark the member as builtin if needed
	if (is_builtin)
	{
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);
		if (builtin == BuiltInPosition && storage == StorageClassOutput)
			qual_pos_var_name = qual_var_name;
	}

	const SPIRConstant *c = nullptr;
	if (!flatten_stage_out && var.storage == StorageClassOutput &&
	    var.initializer != ID(0) && (c = maybe_get<SPIRConstant>(var.initializer)))
	{
		if (meta.strip_array)
		{
			entry_func.fixup_hooks_in.push_back([=, &var]() {
				auto &type = this->get<SPIRType>(var.basetype);
				uint32_t index = get_extended_member_decoration(var.self, mbr_idx, SPIRVCrossDecorationInterfaceMemberIndex);

				auto invocation = to_tesc_invocation_id();
				auto constant_chain = join(to_expression(var.initializer), "[", invocation, "]");
				statement(to_expression(stage_out_ptr_var_id), "[",
				          invocation, "].",
				          to_member_name(ib_type, index), " = ",
				          constant_chain, ".", to_member_name(type, mbr_idx), ";");
			});
		}
		else
		{
			entry_func.fixup_hooks_in.push_back([=]() {
				statement(qual_var_name, " = ", constant_expression(
						this->get<SPIRConstant>(c->subconstants[mbr_idx])), ";");
			});
		}
	}

	if (storage != StorageClassInput || !pull_model_inputs.count(var.self))
	{
		// Copy interpolation decorations if needed
		if (is_flat)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationFlat);
		if (is_noperspective)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationNoPerspective);
		if (is_centroid)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationCentroid);
		if (is_sample)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationSample);
	}

	set_extended_member_decoration(ib_type.self, ib_mbr_idx, SPIRVCrossDecorationInterfaceOrigID, var.self);
	set_extended_member_decoration(ib_type.self, ib_mbr_idx, SPIRVCrossDecorationInterfaceMemberIndex, var_mbr_idx);
}

// In Metal, the tessellation levels are stored as tightly packed half-precision floating point values.
// But, stage-in attribute offsets and strides must be multiples of four, so we can't pass the levels
// individually. Therefore, we must pass them as vectors. Triangles get a single float4, with the outer
// levels in 'xyz' and the inner level in 'w'. Quads get a float4 containing the outer levels and a
// float2 containing the inner levels.
void CompilerMSL::add_tess_level_input_to_interface_block(const std::string &ib_var_ref, SPIRType &ib_type,
                                                          SPIRVariable &var)
{
	auto &var_type = get_variable_element_type(var);

	BuiltIn builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
	bool triangles = is_tessellating_triangles();
	string mbr_name;

	// Add a reference to the variable type to the interface struct.
	uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());

	const auto mark_locations = [&](const SPIRType &new_var_type) {
		if (get_decoration_bitset(var.self).get(DecorationLocation))
		{
			uint32_t locn = get_decoration(var.self, DecorationLocation);
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
			mark_location_as_used_by_shader(locn, new_var_type, StorageClassInput);
		}
		else if (inputs_by_builtin.count(builtin))
		{
			uint32_t locn = inputs_by_builtin[builtin].location;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);
			mark_location_as_used_by_shader(locn, new_var_type, StorageClassInput);
		}
	};

	if (triangles)
	{
		// Triangles are tricky, because we want only one member in the struct.
		mbr_name = "gl_TessLevel";

		// If we already added the other one, we can skip this step.
		if (!added_builtin_tess_level)
		{
			uint32_t type_id = build_extended_vector_type(var_type.self, 4);

			ib_type.member_types.push_back(type_id);

			// Give the member a name
			set_member_name(ib_type.self, ib_mbr_idx, mbr_name);

			// We cannot decorate both, but the important part is that
			// it's marked as builtin so we can get automatic attribute assignment if needed.
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);

			mark_locations(var_type);
			added_builtin_tess_level = true;
		}
	}
	else
	{
		mbr_name = builtin_to_glsl(builtin, StorageClassFunction);

		uint32_t type_id = build_extended_vector_type(var_type.self, builtin == BuiltInTessLevelOuter ? 4 : 2);

		uint32_t ptr_type_id = ir.increase_bound_by(1);
		auto &new_var_type = set<SPIRType>(ptr_type_id, get<SPIRType>(type_id));
		new_var_type.pointer = true;
		new_var_type.pointer_depth++;
		new_var_type.storage = StorageClassInput;
		new_var_type.parent_type = type_id;

		ib_type.member_types.push_back(type_id);

		// Give the member a name
		set_member_name(ib_type.self, ib_mbr_idx, mbr_name);
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);

		mark_locations(new_var_type);
	}

	add_tess_level_input(ib_var_ref, mbr_name, var);
}

void CompilerMSL::add_tess_level_input(const std::string &base_ref, const std::string &mbr_name, SPIRVariable &var)
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	BuiltIn builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));

	// Force the variable to have the proper name.
	string var_name = builtin_to_glsl(builtin, StorageClassFunction);
	set_name(var.self, var_name);

	// We need to declare the variable early and at entry-point scope.
	entry_func.add_local_variable(var.self);
	vars_needing_early_declaration.push_back(var.self);
	bool triangles = is_tessellating_triangles();

	if (builtin == BuiltInTessLevelOuter)
	{
		entry_func.fixup_hooks_in.push_back(
		    [=]()
		    {
			    statement(var_name, "[0] = ", base_ref, ".", mbr_name, "[0];");
			    statement(var_name, "[1] = ", base_ref, ".", mbr_name, "[1];");
			    statement(var_name, "[2] = ", base_ref, ".", mbr_name, "[2];");
			    if (!triangles)
				    statement(var_name, "[3] = ", base_ref, ".", mbr_name, "[3];");
		    });
	}
	else
	{
		entry_func.fixup_hooks_in.push_back([=]() {
			if (triangles)
			{
				if (msl_options.raw_buffer_tese_input)
					statement(var_name, "[0] = ", base_ref, ".", mbr_name, ";");
				else
					statement(var_name, "[0] = ", base_ref, ".", mbr_name, "[3];");
			}
			else
			{
				statement(var_name, "[0] = ", base_ref, ".", mbr_name, "[0];");
				statement(var_name, "[1] = ", base_ref, ".", mbr_name, "[1];");
			}
		});
	}
}

bool CompilerMSL::variable_storage_requires_stage_io(spv::StorageClass storage) const
{
	if (storage == StorageClassOutput)
		return !capture_output_to_buffer;
	else if (storage == StorageClassInput)
		return !(is_tesc_shader() && msl_options.multi_patch_workgroup) &&
		       !(is_tese_shader() && msl_options.raw_buffer_tese_input);
	else
		return false;
}

string CompilerMSL::to_tesc_invocation_id()
{
	if (msl_options.multi_patch_workgroup)
	{
		// n.b. builtin_invocation_id_id here is the dispatch global invocation ID,
		// not the TC invocation ID.
		return join(to_expression(builtin_invocation_id_id), ".x % ", get_entry_point().output_vertices);
	}
	else
		return builtin_to_glsl(BuiltInInvocationId, StorageClassInput);
}

void CompilerMSL::emit_local_masked_variable(const SPIRVariable &masked_var, bool strip_array)
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	bool threadgroup_storage = variable_decl_is_remapped_storage(masked_var, StorageClassWorkgroup);

	if (threadgroup_storage && msl_options.multi_patch_workgroup)
	{
		// We need one threadgroup block per patch, so fake this.
		entry_func.fixup_hooks_in.push_back([this, &masked_var]() {
			auto &type = get_variable_data_type(masked_var);
			add_local_variable_name(masked_var.self);

			bool old_is_builtin = is_using_builtin_array;
			is_using_builtin_array = true;

			const uint32_t max_control_points_per_patch = 32u;
			uint32_t max_num_instances =
					(max_control_points_per_patch + get_entry_point().output_vertices - 1u) /
					get_entry_point().output_vertices;
			statement("threadgroup ", type_to_glsl(type), " ",
			          "spvStorage", to_name(masked_var.self), "[", max_num_instances, "]",
			          type_to_array_glsl(type), ";");

			// Assign a threadgroup slice to each PrimitiveID.
			// We assume here that workgroup size is rounded to 32,
			// since that's the maximum number of control points per patch.
			// We cannot size the array based on fixed dispatch parameters,
			// since Metal does not allow that. :(
			// FIXME: We will likely need an option to support passing down target workgroup size,
			// so we can emit appropriate size here.
			statement("threadgroup ", type_to_glsl(type), " ",
			          "(&", to_name(masked_var.self), ")",
			          type_to_array_glsl(type), " = spvStorage", to_name(masked_var.self), "[",
			          "(", to_expression(builtin_invocation_id_id), ".x / ",
			          get_entry_point().output_vertices, ") % ",
			          max_num_instances, "];");

			is_using_builtin_array = old_is_builtin;
		});
	}
	else
	{
		entry_func.add_local_variable(masked_var.self);
	}

	if (!threadgroup_storage)
	{
		vars_needing_early_declaration.push_back(masked_var.self);
	}
	else if (masked_var.initializer)
	{
		// Cannot directly initialize threadgroup variables. Need fixup hooks.
		ID initializer = masked_var.initializer;
		if (strip_array)
		{
			entry_func.fixup_hooks_in.push_back([this, &masked_var, initializer]() {
				auto invocation = to_tesc_invocation_id();
				statement(to_expression(masked_var.self), "[",
				          invocation, "] = ",
				          to_expression(initializer), "[",
				          invocation, "];");
			});
		}
		else
		{
			entry_func.fixup_hooks_in.push_back([this, &masked_var, initializer]() {
				statement(to_expression(masked_var.self), " = ", to_expression(initializer), ";");
			});
		}
	}
}

void CompilerMSL::add_variable_to_interface_block(StorageClass storage, const string &ib_var_ref, SPIRType &ib_type,
                                                  SPIRVariable &var, InterfaceBlockMeta &meta)
{
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	// Tessellation control I/O variables and tessellation evaluation per-point inputs are
	// usually declared as arrays. In these cases, we want to add the element type to the
	// interface block, since in Metal it's the interface block itself which is arrayed.
	auto &var_type = meta.strip_array ? get_variable_element_type(var) : get_variable_data_type(var);
	bool is_builtin = is_builtin_variable(var);
	auto builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
	bool is_block = has_decoration(var_type.self, DecorationBlock);

	// If stage variables are masked out, emit them as plain variables instead.
	// For builtins, we query them one by one later.
	// IO blocks are not masked here, we need to mask them per-member instead.
	if (storage == StorageClassOutput && is_stage_output_variable_masked(var))
	{
		// If we ignore an output, we must still emit it, since it might be used by app.
		// Instead, just emit it as early declaration.
		emit_local_masked_variable(var, meta.strip_array);
		return;
	}

	if (storage == StorageClassInput && has_decoration(var.self, DecorationPerVertexKHR))
		SPIRV_CROSS_THROW("PerVertexKHR decoration is not supported in MSL.");

	// If variable names alias, they will end up with wrong names in the interface struct, because
	// there might be aliases in the member name cache and there would be a mismatch in fixup_in code.
	// Make sure to register the variables as unique resource names ahead of time.
	// This would normally conflict with the name cache when emitting local variables,
	// but this happens in the setup stage, before we hit compilation loops.
	// The name cache is cleared before we actually emit code, so this is safe.
	add_resource_name(var.self);

	if (var_type.basetype == SPIRType::Struct)
	{
		bool block_requires_flattening =
		    variable_storage_requires_stage_io(storage) || (is_block && var_type.array.empty());
		bool needs_local_declaration = !is_builtin && block_requires_flattening && meta.allow_local_declaration;

		if (needs_local_declaration)
		{
			// For I/O blocks or structs, we will need to pass the block itself around
			// to functions if they are used globally in leaf functions.
			// Rather than passing down member by member,
			// we unflatten I/O blocks while running the shader,
			// and pass the actual struct type down to leaf functions.
			// We then unflatten inputs, and flatten outputs in the "fixup" stages.
			emit_local_masked_variable(var, meta.strip_array);
		}

		if (!block_requires_flattening)
		{
			// In Metal tessellation shaders, the interface block itself is arrayed. This makes things
			// very complicated, since stage-in structures in MSL don't support nested structures.
			// Luckily, for stage-out when capturing output, we can avoid this and just add
			// composite members directly, because the stage-out structure is stored to a buffer,
			// not returned.
			add_plain_variable_to_interface_block(storage, ib_var_ref, ib_type, var, meta);
		}
		else
		{
			bool masked_block = false;
			uint32_t location = UINT32_MAX;
			uint32_t var_mbr_idx = 0;
			uint32_t elem_cnt = 1;
			if (is_matrix(var_type))
			{
				if (is_array(var_type))
					SPIRV_CROSS_THROW("MSL cannot emit arrays-of-matrices in input and output variables.");

				elem_cnt = var_type.columns;
			}
			else if (is_array(var_type))
			{
				if (var_type.array.size() != 1)
					SPIRV_CROSS_THROW("MSL cannot emit arrays-of-arrays in input and output variables.");

				elem_cnt = to_array_size_literal(var_type);
			}

			for (uint32_t elem_idx = 0; elem_idx < elem_cnt; elem_idx++)
			{
				// Flatten the struct members into the interface struct
				for (uint32_t mbr_idx = 0; mbr_idx < uint32_t(var_type.member_types.size()); mbr_idx++)
				{
					builtin = BuiltInMax;
					is_builtin = is_member_builtin(var_type, mbr_idx, &builtin);
					auto &mbr_type = get<SPIRType>(var_type.member_types[mbr_idx]);

					if (storage == StorageClassOutput && is_stage_output_block_member_masked(var, mbr_idx, meta.strip_array))
					{
						location = UINT32_MAX; // Skip this member and resolve location again on next var member

						if (is_block)
							masked_block = true;

						// Non-builtin block output variables are just ignored, since they will still access
						// the block variable as-is. They're just not flattened.
						if (is_builtin && !meta.strip_array)
						{
							// Emit a fake variable instead.
							uint32_t ids = ir.increase_bound_by(2);
							uint32_t ptr_type_id = ids + 0;
							uint32_t var_id = ids + 1;

							auto ptr_type = mbr_type;
							ptr_type.pointer = true;
							ptr_type.pointer_depth++;
							ptr_type.parent_type = var_type.member_types[mbr_idx];
							ptr_type.storage = StorageClassOutput;

							uint32_t initializer = 0;
							if (var.initializer)
								if (auto *c = maybe_get<SPIRConstant>(var.initializer))
									initializer = c->subconstants[mbr_idx];

							set<SPIRType>(ptr_type_id, ptr_type);
							set<SPIRVariable>(var_id, ptr_type_id, StorageClassOutput, initializer);
							entry_func.add_local_variable(var_id);
							vars_needing_early_declaration.push_back(var_id);
							set_name(var_id, builtin_to_glsl(builtin, StorageClassOutput));
							set_decoration(var_id, DecorationBuiltIn, builtin);
						}
					}
					else if (!is_builtin || has_active_builtin(builtin, storage))
					{
						bool is_composite_type = is_matrix(mbr_type) || is_array(mbr_type) || mbr_type.basetype == SPIRType::Struct;
						bool attribute_load_store =
								storage == StorageClassInput && get_execution_model() != ExecutionModelFragment;
						bool storage_is_stage_io = variable_storage_requires_stage_io(storage);

						// Clip/CullDistance always need to be declared as user attributes.
						if (builtin == BuiltInClipDistance || builtin == BuiltInCullDistance)
							is_builtin = false;

						const string var_name = to_name(var.self);
						string mbr_name_qual = var_name;
						string var_chain_qual = var_name;
						if (elem_cnt > 1)
						{
							mbr_name_qual += join("_", elem_idx);
							var_chain_qual += join("[", elem_idx, "]");
						}

						if ((!is_builtin || attribute_load_store) && storage_is_stage_io && is_composite_type)
						{
							add_composite_member_variable_to_interface_block(storage, ib_var_ref, ib_type,
							                                                 var, var_type, mbr_idx, meta,
							                                                 mbr_name_qual, var_chain_qual,
							                                                 location, var_mbr_idx);
						}
						else
						{
							add_plain_member_variable_to_interface_block(storage, ib_var_ref, ib_type,
							                                             var, var_type, mbr_idx, meta,
							                                             mbr_name_qual, var_chain_qual,
							                                             location, var_mbr_idx);
						}
					}
					var_mbr_idx++;
				}
			}

			// If we're redirecting a block, we might still need to access the original block
			// variable if we're masking some members.
			if (masked_block && !needs_local_declaration && (!is_builtin_variable(var) || is_tesc_shader()))
			{
				if (is_builtin_variable(var))
				{
					// Ensure correct names for the block members if we're actually going to
					// declare gl_PerVertex.
					for (uint32_t mbr_idx = 0; mbr_idx < uint32_t(var_type.member_types.size()); mbr_idx++)
					{
						set_member_name(var_type.self, mbr_idx, builtin_to_glsl(
								BuiltIn(get_member_decoration(var_type.self, mbr_idx, DecorationBuiltIn)),
								StorageClassOutput));
					}

					set_name(var_type.self, "gl_PerVertex");
					set_name(var.self, "gl_out_masked");
					stage_out_masked_builtin_type_id = var_type.self;
				}
				emit_local_masked_variable(var, meta.strip_array);
			}
		}
	}
	else if (is_tese_shader() && storage == StorageClassInput && !meta.strip_array && is_builtin &&
	         (builtin == BuiltInTessLevelOuter || builtin == BuiltInTessLevelInner))
	{
		add_tess_level_input_to_interface_block(ib_var_ref, ib_type, var);
	}
	else if (var_type.basetype == SPIRType::Boolean || var_type.basetype == SPIRType::Char ||
	         type_is_integral(var_type) || type_is_floating_point(var_type))
	{
		if (!is_builtin || has_active_builtin(builtin, storage))
		{
			bool is_composite_type = is_matrix(var_type) || is_array(var_type);
			bool storage_is_stage_io = variable_storage_requires_stage_io(storage);
			bool attribute_load_store = storage == StorageClassInput && get_execution_model() != ExecutionModelFragment;

			// Clip/CullDistance always needs to be declared as user attributes.
			if (builtin == BuiltInClipDistance || builtin == BuiltInCullDistance)
				is_builtin = false;

			// MSL does not allow matrices or arrays in input or output variables, so need to handle it specially.
			if ((!is_builtin || attribute_load_store) && storage_is_stage_io && is_composite_type)
			{
				add_composite_variable_to_interface_block(storage, ib_var_ref, ib_type, var, meta);
			}
			else
			{
				add_plain_variable_to_interface_block(storage, ib_var_ref, ib_type, var, meta);
			}
		}
	}
}

// Fix up the mapping of variables to interface member indices, which is used to compile access chains
// for per-vertex variables in a tessellation control shader.
void CompilerMSL::fix_up_interface_member_indices(StorageClass storage, uint32_t ib_type_id)
{
	// Only needed for tessellation shaders and pull-model interpolants.
	// Need to redirect interface indices back to variables themselves.
	// For structs, each member of the struct need a separate instance.
	if (!is_tesc_shader() && !(is_tese_shader() && storage == StorageClassInput) &&
	    !(get_execution_model() == ExecutionModelFragment && storage == StorageClassInput &&
	      !pull_model_inputs.empty()))
		return;

	auto mbr_cnt = uint32_t(ir.meta[ib_type_id].members.size());
	for (uint32_t i = 0; i < mbr_cnt; i++)
	{
		uint32_t var_id = get_extended_member_decoration(ib_type_id, i, SPIRVCrossDecorationInterfaceOrigID);
		if (!var_id)
			continue;
		auto &var = get<SPIRVariable>(var_id);

		auto &type = get_variable_element_type(var);

		bool flatten_composites = variable_storage_requires_stage_io(var.storage);
		bool is_block = has_decoration(type.self, DecorationBlock);

		uint32_t mbr_idx = uint32_t(-1);
		if (type.basetype == SPIRType::Struct && (flatten_composites || is_block))
			mbr_idx = get_extended_member_decoration(ib_type_id, i, SPIRVCrossDecorationInterfaceMemberIndex);

		if (mbr_idx != uint32_t(-1))
		{
			// Only set the lowest InterfaceMemberIndex for each variable member.
			// IB struct members will be emitted in-order w.r.t. interface member index.
			if (!has_extended_member_decoration(var_id, mbr_idx, SPIRVCrossDecorationInterfaceMemberIndex))
				set_extended_member_decoration(var_id, mbr_idx, SPIRVCrossDecorationInterfaceMemberIndex, i);
		}
		else
		{
			// Only set the lowest InterfaceMemberIndex for each variable.
			// IB struct members will be emitted in-order w.r.t. interface member index.
			if (!has_extended_decoration(var_id, SPIRVCrossDecorationInterfaceMemberIndex))
				set_extended_decoration(var_id, SPIRVCrossDecorationInterfaceMemberIndex, i);
		}
	}
}

// Add an interface structure for the type of storage, which is either StorageClassInput or StorageClassOutput.
// Returns the ID of the newly added variable, or zero if no variable was added.
uint32_t CompilerMSL::add_interface_block(StorageClass storage, bool patch)
{
	// Accumulate the variables that should appear in the interface struct.
	SmallVector<SPIRVariable *> vars;
	bool incl_builtins = storage == StorageClassOutput || is_tessellation_shader();
	bool has_seen_barycentric = false;

	InterfaceBlockMeta meta;

	// Varying interfaces between stages which use "user()" attribute can be dealt with
	// without explicit packing and unpacking of components. For any variables which link against the runtime
	// in some way (vertex attributes, fragment output, etc), we'll need to deal with it somehow.
	bool pack_components =
	    (storage == StorageClassInput && get_execution_model() == ExecutionModelVertex) ||
	    (storage == StorageClassOutput && get_execution_model() == ExecutionModelFragment) ||
	    (storage == StorageClassOutput && get_execution_model() == ExecutionModelVertex && capture_output_to_buffer);

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t var_id, SPIRVariable &var) {
		if (var.storage != storage)
			return;

		auto &type = this->get<SPIRType>(var.basetype);

		bool is_builtin = is_builtin_variable(var);
		bool is_block = has_decoration(type.self, DecorationBlock);

		auto bi_type = BuiltInMax;
		bool builtin_is_gl_in_out = false;
		if (is_builtin && !is_block)
		{
			bi_type = BuiltIn(get_decoration(var_id, DecorationBuiltIn));
			builtin_is_gl_in_out = bi_type == BuiltInPosition || bi_type == BuiltInPointSize ||
			                       bi_type == BuiltInClipDistance || bi_type == BuiltInCullDistance;
		}

		if (is_builtin && is_block)
			builtin_is_gl_in_out = true;

		uint32_t location = get_decoration(var_id, DecorationLocation);

		bool builtin_is_stage_in_out = builtin_is_gl_in_out ||
		                               bi_type == BuiltInLayer || bi_type == BuiltInViewportIndex ||
		                               bi_type == BuiltInBaryCoordKHR || bi_type == BuiltInBaryCoordNoPerspKHR ||
		                               bi_type == BuiltInFragDepth ||
		                               bi_type == BuiltInFragStencilRefEXT || bi_type == BuiltInSampleMask;

		// These builtins are part of the stage in/out structs.
		bool is_interface_block_builtin =
		    builtin_is_stage_in_out || (is_tese_shader() && !msl_options.raw_buffer_tese_input &&
		                                (bi_type == BuiltInTessLevelOuter || bi_type == BuiltInTessLevelInner));

		bool is_active = interface_variable_exists_in_entry_point(var.self);
		if (is_builtin && is_active)
		{
			// Only emit the builtin if it's active in this entry point. Interface variable list might lie.
			if (is_block)
			{
				// If any builtin is active, the block is active.
				uint32_t mbr_cnt = uint32_t(type.member_types.size());
				for (uint32_t i = 0; !is_active && i < mbr_cnt; i++)
					is_active = has_active_builtin(BuiltIn(get_member_decoration(type.self, i, DecorationBuiltIn)), storage);
			}
			else
			{
				is_active = has_active_builtin(bi_type, storage);
			}
		}

		bool filter_patch_decoration = (has_decoration(var_id, DecorationPatch) || is_patch_block(type)) == patch;

		bool hidden = is_hidden_variable(var, incl_builtins);

		// ClipDistance is never hidden, we need to emulate it when used as an input.
		if (bi_type == BuiltInClipDistance || bi_type == BuiltInCullDistance)
			hidden = false;

		// It's not enough to simply avoid marking fragment outputs if the pipeline won't
		// accept them. We can't put them in the struct at all, or otherwise the compiler
		// complains that the outputs weren't explicitly marked.
		// Frag depth and stencil outputs are incompatible with explicit early fragment tests.
		// In GLSL, depth and stencil outputs are just ignored when explicit early fragment tests are required.
		// In Metal, it's a compilation error, so we need to exclude them from the output struct.
		if (get_execution_model() == ExecutionModelFragment && storage == StorageClassOutput && !patch &&
		    ((is_builtin && ((bi_type == BuiltInFragDepth && (!msl_options.enable_frag_depth_builtin || uses_explicit_early_fragment_test())) ||
		                     (bi_type == BuiltInFragStencilRefEXT && (!msl_options.enable_frag_stencil_ref_builtin || uses_explicit_early_fragment_test())))) ||
		     (!is_builtin && !(msl_options.enable_frag_output_mask & (1 << location)))))
		{
			hidden = true;
			disabled_frag_outputs.push_back(var_id);
			// If a builtin, force it to have the proper name, and mark it as not part of the output struct.
			if (is_builtin)
			{
				set_name(var_id, builtin_to_glsl(bi_type, StorageClassFunction));
				mask_stage_output_by_builtin(bi_type);
			}
		}

		// Barycentric inputs must be emitted in stage-in, because they can have interpolation arguments.
		if (is_active && (bi_type == BuiltInBaryCoordKHR || bi_type == BuiltInBaryCoordNoPerspKHR))
		{
			if (has_seen_barycentric)
				SPIRV_CROSS_THROW("Cannot declare both BaryCoordNV and BaryCoordNoPerspNV in same shader in MSL.");
			has_seen_barycentric = true;
			hidden = false;
		}

		if (is_active && !hidden && type.pointer && filter_patch_decoration &&
		    (!is_builtin || is_interface_block_builtin))
		{
			vars.push_back(&var);

			if (!is_builtin)
			{
				// Need to deal specially with DecorationComponent.
				// Multiple variables can alias the same Location, and try to make sure each location is declared only once.
				// We will swizzle data in and out to make this work.
				// This is only relevant for vertex inputs and fragment outputs.
				// Technically tessellation as well, but it is too complicated to support.
				uint32_t component = get_decoration(var_id, DecorationComponent);
				if (component != 0)
				{
					if (is_tessellation_shader())
						SPIRV_CROSS_THROW("Component decoration is not supported in tessellation shaders.");
					else if (pack_components)
					{
						uint32_t array_size = 1;
						if (!type.array.empty())
							array_size = to_array_size_literal(type);

						for (uint32_t location_offset = 0; location_offset < array_size; location_offset++)
						{
							auto &location_meta = meta.location_meta[location + location_offset];
							location_meta.num_components = max<uint32_t>(location_meta.num_components, component + type.vecsize);

							// For variables sharing location, decorations and base type must match.
							location_meta.base_type_id = type.self;
							location_meta.flat = has_decoration(var.self, DecorationFlat);
							location_meta.noperspective = has_decoration(var.self, DecorationNoPerspective);
							location_meta.centroid = has_decoration(var.self, DecorationCentroid);
							location_meta.sample = has_decoration(var.self, DecorationSample);
						}
					}
				}
			}
		}

		if (is_tese_shader() && msl_options.raw_buffer_tese_input && patch && storage == StorageClassInput &&
		    (bi_type == BuiltInTessLevelOuter || bi_type == BuiltInTessLevelInner))
		{
			// In this case, we won't add the builtin to the interface struct,
			// but we still need the hook to run to populate the arrays.
			string base_ref = join(tess_factor_buffer_var_name, "[", to_expression(builtin_primitive_id_id), "]");
			const char *mbr_name =
			    bi_type == BuiltInTessLevelOuter ? "edgeTessellationFactor" : "insideTessellationFactor";
			add_tess_level_input(base_ref, mbr_name, var);
			if (inputs_by_builtin.count(bi_type))
			{
				uint32_t locn = inputs_by_builtin[bi_type].location;
				mark_location_as_used_by_shader(locn, type, StorageClassInput);
			}
		}
	});

	// If no variables qualify, leave.
	// For patch input in a tessellation evaluation shader, the per-vertex stage inputs
	// are included in a special patch control point array.
	if (vars.empty() &&
	    !(!msl_options.raw_buffer_tese_input && storage == StorageClassInput && patch && stage_in_var_id))
		return 0;

	// Add a new typed variable for this interface structure.
	// The initializer expression is allocated here, but populated when the function
	// declaraion is emitted, because it is cleared after each compilation pass.
	uint32_t next_id = ir.increase_bound_by(3);
	uint32_t ib_type_id = next_id++;
	auto &ib_type = set<SPIRType>(ib_type_id);
	ib_type.basetype = SPIRType::Struct;
	ib_type.storage = storage;
	set_decoration(ib_type_id, DecorationBlock);

	uint32_t ib_var_id = next_id++;
	auto &var = set<SPIRVariable>(ib_var_id, ib_type_id, storage, 0);
	var.initializer = next_id++;

	string ib_var_ref;
	auto &entry_func = get<SPIRFunction>(ir.default_entry_point);
	switch (storage)
	{
	case StorageClassInput:
		ib_var_ref = patch ? patch_stage_in_var_name : stage_in_var_name;
		switch (get_execution_model())
		{
		case ExecutionModelTessellationControl:
			// Add a hook to populate the shared workgroup memory containing the gl_in array.
			entry_func.fixup_hooks_in.push_back([=]() {
				// Can't use PatchVertices, PrimitiveId, or InvocationId yet; the hooks for those may not have run yet.
				if (msl_options.multi_patch_workgroup)
				{
					// n.b. builtin_invocation_id_id here is the dispatch global invocation ID,
					// not the TC invocation ID.
					statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "* gl_in = &",
					          input_buffer_var_name, "[min(", to_expression(builtin_invocation_id_id), ".x / ",
					          get_entry_point().output_vertices,
					          ", spvIndirectParams[1] - 1) * spvIndirectParams[0]];");
				}
				else
				{
					// It's safe to use InvocationId here because it's directly mapped to a
					// Metal builtin, and therefore doesn't need a hook.
					statement("if (", to_expression(builtin_invocation_id_id), " < spvIndirectParams[0])");
					statement("    ", input_wg_var_name, "[", to_expression(builtin_invocation_id_id),
					          "] = ", ib_var_ref, ";");
					statement("threadgroup_barrier(mem_flags::mem_threadgroup);");
					statement("if (", to_expression(builtin_invocation_id_id),
					          " >= ", get_entry_point().output_vertices, ")");
					statement("    return;");
				}
			});
			break;
		case ExecutionModelTessellationEvaluation:
			if (!msl_options.raw_buffer_tese_input)
				break;
			if (patch)
			{
				entry_func.fixup_hooks_in.push_back(
				    [=]()
				    {
					    statement("const device ", to_name(ir.default_entry_point), "_", ib_var_ref, "& ", ib_var_ref,
					              " = ", patch_input_buffer_var_name, "[", to_expression(builtin_primitive_id_id),
					              "];");
				    });
			}
			else
			{
				entry_func.fixup_hooks_in.push_back(
				    [=]()
				    {
					    statement("const device ", to_name(ir.default_entry_point), "_", ib_var_ref, "* gl_in = &",
					              input_buffer_var_name, "[", to_expression(builtin_primitive_id_id), " * ",
					              get_entry_point().output_vertices, "];");
				    });
			}
			break;
		default:
			break;
		}
		break;

	case StorageClassOutput:
	{
		ib_var_ref = patch ? patch_stage_out_var_name : stage_out_var_name;

		// Add the output interface struct as a local variable to the entry function.
		// If the entry point should return the output struct, set the entry function
		// to return the output interface struct, otherwise to return nothing.
		// Watch out for the rare case where the terminator of the last entry point block is a
		// Kill, instead of a Return. Based on SPIR-V's block-domination rules, we assume that
		// any block that has a Kill will also have a terminating Return, except the last block.
		// Indicate the output var requires early initialization.
		bool ep_should_return_output = !get_is_rasterization_disabled();
		uint32_t rtn_id = ep_should_return_output ? ib_var_id : 0;
		if (!capture_output_to_buffer)
		{
			entry_func.add_local_variable(ib_var_id);
			for (auto &blk_id : entry_func.blocks)
			{
				auto &blk = get<SPIRBlock>(blk_id);
				if (blk.terminator == SPIRBlock::Return || (blk.terminator == SPIRBlock::Kill && blk_id == entry_func.blocks.back()))
					blk.return_value = rtn_id;
			}
			vars_needing_early_declaration.push_back(ib_var_id);
		}
		else
		{
			switch (get_execution_model())
			{
			case ExecutionModelVertex:
			case ExecutionModelTessellationEvaluation:
				// Instead of declaring a struct variable to hold the output and then
				// copying that to the output buffer, we'll declare the output variable
				// as a reference to the final output element in the buffer. Then we can
				// avoid the extra copy.
				entry_func.fixup_hooks_in.push_back([=]() {
					if (stage_out_var_id)
					{
						// The first member of the indirect buffer is always the number of vertices
						// to draw.
						// We zero-base the InstanceID & VertexID variables for HLSL emulation elsewhere, so don't do it twice
						if (get_execution_model() == ExecutionModelVertex && msl_options.vertex_for_tessellation)
						{
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "& ", ib_var_ref,
							          " = ", output_buffer_var_name, "[", to_expression(builtin_invocation_id_id),
							          ".y * ", to_expression(builtin_stage_input_size_id), ".x + ",
							          to_expression(builtin_invocation_id_id), ".x];");
						}
						else if (msl_options.enable_base_index_zero)
						{
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "& ", ib_var_ref,
							          " = ", output_buffer_var_name, "[", to_expression(builtin_instance_idx_id),
							          " * spvIndirectParams[0] + ", to_expression(builtin_vertex_idx_id), "];");
						}
						else
						{
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "& ", ib_var_ref,
							          " = ", output_buffer_var_name, "[(", to_expression(builtin_instance_idx_id),
							          " - ", to_expression(builtin_base_instance_id), ") * spvIndirectParams[0] + ",
							          to_expression(builtin_vertex_idx_id), " - ",
							          to_expression(builtin_base_vertex_id), "];");
						}
					}
				});
				break;
			case ExecutionModelTessellationControl:
				if (msl_options.multi_patch_workgroup)
				{
					// We cannot use PrimitiveId here, because the hook may not have run yet.
					if (patch)
					{
						entry_func.fixup_hooks_in.push_back([=]() {
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "& ", ib_var_ref,
							          " = ", patch_output_buffer_var_name, "[", to_expression(builtin_invocation_id_id),
							          ".x / ", get_entry_point().output_vertices, "];");
						});
					}
					else
					{
						entry_func.fixup_hooks_in.push_back([=]() {
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "* gl_out = &",
							          output_buffer_var_name, "[", to_expression(builtin_invocation_id_id), ".x - ",
							          to_expression(builtin_invocation_id_id), ".x % ",
							          get_entry_point().output_vertices, "];");
						});
					}
				}
				else
				{
					if (patch)
					{
						entry_func.fixup_hooks_in.push_back([=]() {
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "& ", ib_var_ref,
							          " = ", patch_output_buffer_var_name, "[", to_expression(builtin_primitive_id_id),
							          "];");
						});
					}
					else
					{
						entry_func.fixup_hooks_in.push_back([=]() {
							statement("device ", to_name(ir.default_entry_point), "_", ib_var_ref, "* gl_out = &",
							          output_buffer_var_name, "[", to_expression(builtin_primitive_id_id), " * ",
							          get_entry_point().output_vertices, "];");
						});
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	}

	default:
		break;
	}

	set_name(ib_type_id, to_name(ir.default_entry_point) + "_" + ib_var_ref);
	set_name(ib_var_id, ib_var_ref);

	for (auto *p_var : vars)
	{
		bool strip_array = (is_tesc_shader() || (is_tese_shader() && storage == StorageClassInput)) && !patch;

		// Fixing up flattened stores in TESC is impossible since the memory is group shared either via
		// device (not masked) or threadgroup (masked) storage classes and it's race condition city.
		meta.strip_array = strip_array;
		meta.allow_local_declaration = !strip_array && !(is_tesc_shader() && storage == StorageClassOutput);
		add_variable_to_interface_block(storage, ib_var_ref, ib_type, *p_var, meta);
	}

	if (((is_tesc_shader() && msl_options.multi_patch_workgroup) ||
	     (is_tese_shader() && msl_options.raw_buffer_tese_input)) &&
	    storage == StorageClassInput)
	{
		// For tessellation inputs, add all outputs from the previous stage to ensure
		// the struct containing them is the correct size and layout.
		for (auto &input : inputs_by_location)
		{
			if (location_inputs_in_use.count(input.first.location) != 0)
				continue;

			if (patch != (input.second.rate == MSL_SHADER_VARIABLE_RATE_PER_PATCH))
				continue;

			// Tessellation levels have their own struct, so there's no need to add them here.
			if (input.second.builtin == BuiltInTessLevelOuter || input.second.builtin == BuiltInTessLevelInner)
				continue;

			// Create a fake variable to put at the location.
			uint32_t offset = ir.increase_bound_by(4);
			uint32_t type_id = offset;
			uint32_t array_type_id = offset + 1;
			uint32_t ptr_type_id = offset + 2;
			uint32_t var_id = offset + 3;

			SPIRType type;
			switch (input.second.format)
			{
			case MSL_SHADER_VARIABLE_FORMAT_UINT16:
			case MSL_SHADER_VARIABLE_FORMAT_ANY16:
				type.basetype = SPIRType::UShort;
				type.width = 16;
				break;
			case MSL_SHADER_VARIABLE_FORMAT_ANY32:
			default:
				type.basetype = SPIRType::UInt;
				type.width = 32;
				break;
			}
			type.vecsize = input.second.vecsize;
			set<SPIRType>(type_id, type);

			type.array.push_back(0);
			type.array_size_literal.push_back(true);
			type.parent_type = type_id;
			set<SPIRType>(array_type_id, type);

			type.pointer = true;
			type.pointer_depth++;
			type.parent_type = array_type_id;
			type.storage = storage;
			auto &ptr_type = set<SPIRType>(ptr_type_id, type);
			ptr_type.self = array_type_id;

			auto &fake_var = set<SPIRVariable>(var_id, ptr_type_id, storage);
			set_decoration(var_id, DecorationLocation, input.first.location);
			if (input.first.component)
				set_decoration(var_id, DecorationComponent, input.first.component);

			meta.strip_array = true;
			meta.allow_local_declaration = false;
			add_variable_to_interface_block(storage, ib_var_ref, ib_type, fake_var, meta);
		}
	}

	if (capture_output_to_buffer && storage == StorageClassOutput)
	{
		// For captured output, add all inputs from the next stage to ensure
		// the struct containing them is the correct size and layout. This is
		// necessary for certain implicit builtins that may nonetheless be read,
		// even when they aren't written.
		for (auto &output : outputs_by_location)
		{
			if (location_outputs_in_use.count(output.first.location) != 0)
				continue;

			// Create a fake variable to put at the location.
			uint32_t offset = ir.increase_bound_by(4);
			uint32_t type_id = offset;
			uint32_t array_type_id = offset + 1;
			uint32_t ptr_type_id = offset + 2;
			uint32_t var_id = offset + 3;

			SPIRType type;
			switch (output.second.format)
			{
			case MSL_SHADER_VARIABLE_FORMAT_UINT16:
			case MSL_SHADER_VARIABLE_FORMAT_ANY16:
				type.basetype = SPIRType::UShort;
				type.width = 16;
				break;
			case MSL_SHADER_VARIABLE_FORMAT_ANY32:
			default:
				type.basetype = SPIRType::UInt;
				type.width = 32;
				break;
			}
			type.vecsize = output.second.vecsize;
			set<SPIRType>(type_id, type);

			if (is_tesc_shader())
			{
				type.array.push_back(0);
				type.array_size_literal.push_back(true);
				type.parent_type = type_id;
				set<SPIRType>(array_type_id, type);
			}

			type.pointer = true;
			type.pointer_depth++;
			type.parent_type = is_tesc_shader() ? array_type_id : type_id;
			type.storage = storage;
			auto &ptr_type = set<SPIRType>(ptr_type_id, type);
			ptr_type.self = type.parent_type;

			auto &fake_var = set<SPIRVariable>(var_id, ptr_type_id, storage);
			set_decoration(var_id, DecorationLocation, output.first.location);
			if (output.first.component)
				set_decoration(var_id, DecorationComponent, output.first.component);

			meta.strip_array = true;
			meta.allow_local_declaration = false;
			add_variable_to_interface_block(storage, ib_var_ref, ib_type, fake_var, meta);
		}
	}

	// When multiple variables need to access same location,
	// unroll locations one by one and we will flatten output or input as necessary.
	for (auto &loc : meta.location_meta)
	{
		uint32_t location = loc.first;
		auto &location_meta = loc.second;

		uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());
		uint32_t type_id = build_extended_vector_type(location_meta.base_type_id, location_meta.num_components);
		ib_type.member_types.push_back(type_id);

		set_member_name(ib_type.self, ib_mbr_idx, join("m_location_", location));
		set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, location);
		mark_location_as_used_by_shader(location, get<SPIRType>(type_id), storage);

		if (location_meta.flat)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationFlat);
		if (location_meta.noperspective)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationNoPerspective);
		if (location_meta.centroid)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationCentroid);
		if (location_meta.sample)
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationSample);
	}

	// Sort the members of the structure by their locations.
	MemberSorter member_sorter(ib_type, ir.meta[ib_type_id], MemberSorter::LocationThenBuiltInType);
	member_sorter.sort();

	// The member indices were saved to the original variables, but after the members
	// were sorted, those indices are now likely incorrect. Fix those up now.
	fix_up_interface_member_indices(storage, ib_type_id);

	// For patch inputs, add one more member, holding the array of control point data.
	if (is_tese_shader() && !msl_options.raw_buffer_tese_input && storage == StorageClassInput && patch &&
	    stage_in_var_id)
	{
		uint32_t pcp_type_id = ir.increase_bound_by(1);
		auto &pcp_type = set<SPIRType>(pcp_type_id, ib_type);
		pcp_type.basetype = SPIRType::ControlPointArray;
		pcp_type.parent_type = pcp_type.type_alias = get_stage_in_struct_type().self;
		pcp_type.storage = storage;
		ir.meta[pcp_type_id] = ir.meta[ib_type.self];
		uint32_t mbr_idx = uint32_t(ib_type.member_types.size());
		ib_type.member_types.push_back(pcp_type_id);
		set_member_name(ib_type.self, mbr_idx, "gl_in");
	}

	if (storage == StorageClassInput)
		set_decoration(ib_var_id, DecorationNonWritable);

	return ib_var_id;
}

uint32_t CompilerMSL::add_interface_block_pointer(uint32_t ib_var_id, StorageClass storage)
{
	if (!ib_var_id)
		return 0;

	uint32_t ib_ptr_var_id;
	uint32_t next_id = ir.increase_bound_by(3);
	auto &ib_type = expression_type(ib_var_id);
	if (is_tesc_shader() || (is_tese_shader() && msl_options.raw_buffer_tese_input))
	{
		// Tessellation control per-vertex I/O is presented as an array, so we must
		// do the same with our struct here.
		uint32_t ib_ptr_type_id = next_id++;
		auto &ib_ptr_type = set<SPIRType>(ib_ptr_type_id, ib_type);
		ib_ptr_type.parent_type = ib_ptr_type.type_alias = ib_type.self;
		ib_ptr_type.pointer = true;
		ib_ptr_type.pointer_depth++;
		ib_ptr_type.storage = storage == StorageClassInput ?
		                          ((is_tesc_shader() && msl_options.multi_patch_workgroup) ||
		                                   (is_tese_shader() && msl_options.raw_buffer_tese_input) ?
		                               StorageClassStorageBuffer :
		                               StorageClassWorkgroup) :
		                          StorageClassStorageBuffer;
		ir.meta[ib_ptr_type_id] = ir.meta[ib_type.self];
		// To ensure that get_variable_data_type() doesn't strip off the pointer,
		// which we need, use another pointer.
		uint32_t ib_ptr_ptr_type_id = next_id++;
		auto &ib_ptr_ptr_type = set<SPIRType>(ib_ptr_ptr_type_id, ib_ptr_type);
		ib_ptr_ptr_type.parent_type = ib_ptr_type_id;
		ib_ptr_ptr_type.type_alias = ib_type.self;
		ib_ptr_ptr_type.storage = StorageClassFunction;
		ir.meta[ib_ptr_ptr_type_id] = ir.meta[ib_type.self];

		ib_ptr_var_id = next_id;
		set<SPIRVariable>(ib_ptr_var_id, ib_ptr_ptr_type_id, StorageClassFunction, 0);
		set_name(ib_ptr_var_id, storage == StorageClassInput ? "gl_in" : "gl_out");
		if (storage == StorageClassInput)
			set_decoration(ib_ptr_var_id, DecorationNonWritable);
	}
	else
	{
		// Tessellation evaluation per-vertex inputs are also presented as arrays.
		// But, in Metal, this array uses a very special type, 'patch_control_point<T>',
		// which is a container that can be used to access the control point data.
		// To represent this, a special 'ControlPointArray' type has been added to the
		// SPIRV-Cross type system. It should only be generated by and seen in the MSL
		// backend (i.e. this one).
		uint32_t pcp_type_id = next_id++;
		auto &pcp_type = set<SPIRType>(pcp_type_id, ib_type);
		pcp_type.basetype = SPIRType::ControlPointArray;
		pcp_type.parent_type = pcp_type.type_alias = ib_type.self;
		pcp_type.storage = storage;
		ir.meta[pcp_type_id] = ir.meta[ib_type.self];

		ib_ptr_var_id = next_id;
		set<SPIRVariable>(ib_ptr_var_id, pcp_type_id, storage, 0);
		set_name(ib_ptr_var_id, "gl_in");
		ir.meta[ib_ptr_var_id].decoration.qualified_alias = join(patch_stage_in_var_name, ".gl_in");
	}
	return ib_ptr_var_id;
}

// Ensure that the type is compatible with the builtin.
// If it is, simply return the given type ID.
// Otherwise, create a new type, and return it's ID.
uint32_t CompilerMSL::ensure_correct_builtin_type(uint32_t type_id, BuiltIn builtin)
{
	auto &type = get<SPIRType>(type_id);

	if ((builtin == BuiltInSampleMask && is_array(type)) ||
	    ((builtin == BuiltInLayer || builtin == BuiltInViewportIndex || builtin == BuiltInFragStencilRefEXT) &&
	     type.basetype != SPIRType::UInt))
	{
		uint32_t next_id = ir.increase_bound_by(type.pointer ? 2 : 1);
		uint32_t base_type_id = next_id++;
		auto &base_type = set<SPIRType>(base_type_id);
		base_type.basetype = SPIRType::UInt;
		base_type.width = 32;

		if (!type.pointer)
			return base_type_id;

		uint32_t ptr_type_id = next_id++;
		auto &ptr_type = set<SPIRType>(ptr_type_id);
		ptr_type = base_type;
		ptr_type.pointer = true;
		ptr_type.pointer_depth++;
		ptr_type.storage = type.storage;
		ptr_type.parent_type = base_type_id;
		return ptr_type_id;
	}

	return type_id;
}

// Ensure that the type is compatible with the shader input.
// If it is, simply return the given type ID.
// Otherwise, create a new type, and return its ID.
uint32_t CompilerMSL::ensure_correct_input_type(uint32_t type_id, uint32_t location, uint32_t component, uint32_t num_components, bool strip_array)
{
	auto &type = get<SPIRType>(type_id);

	uint32_t max_array_dimensions = strip_array ? 1 : 0;

	// Struct and array types must match exactly.
	if (type.basetype == SPIRType::Struct || type.array.size() > max_array_dimensions)
		return type_id;

	auto p_va = inputs_by_location.find({location, component});
	if (p_va == end(inputs_by_location))
	{
		if (num_components > type.vecsize)
			return build_extended_vector_type(type_id, num_components);
		else
			return type_id;
	}

	if (num_components == 0)
		num_components = p_va->second.vecsize;

	switch (p_va->second.format)
	{
	case MSL_SHADER_VARIABLE_FORMAT_UINT8:
	{
		switch (type.basetype)
		{
		case SPIRType::UByte:
		case SPIRType::UShort:
		case SPIRType::UInt:
			if (num_components > type.vecsize)
				return build_extended_vector_type(type_id, num_components);
			else
				return type_id;

		case SPIRType::Short:
			return build_extended_vector_type(type_id, num_components > type.vecsize ? num_components : type.vecsize,
			                                  SPIRType::UShort);
		case SPIRType::Int:
			return build_extended_vector_type(type_id, num_components > type.vecsize ? num_components : type.vecsize,
			                                  SPIRType::UInt);

		default:
			SPIRV_CROSS_THROW("Vertex attribute type mismatch between host and shader");
		}
	}

	case MSL_SHADER_VARIABLE_FORMAT_UINT16:
	{
		switch (type.basetype)
		{
		case SPIRType::UShort:
		case SPIRType::UInt:
			if (num_components > type.vecsize)
				return build_extended_vector_type(type_id, num_components);
			else
				return type_id;

		case SPIRType::Int:
			return build_extended_vector_type(type_id, num_components > type.vecsize ? num_components : type.vecsize,
			                                  SPIRType::UInt);

		default:
			SPIRV_CROSS_THROW("Vertex attribute type mismatch between host and shader");
		}
	}

	default:
		if (num_components > type.vecsize)
			type_id = build_extended_vector_type(type_id, num_components);
		break;
	}

	return type_id;
}

void CompilerMSL::mark_struct_members_packed(const SPIRType &type)
{
	// Handle possible recursion when a struct contains a pointer to its own type nested somewhere.
	if (has_extended_decoration(type.self, SPIRVCrossDecorationPhysicalTypePacked))
		return;

	set_extended_decoration(type.self, SPIRVCrossDecorationPhysicalTypePacked);

	// Problem case! Struct needs to be placed at an awkward alignment.
	// Mark every member of the child struct as packed.
	uint32_t mbr_cnt = uint32_t(type.member_types.size());
	for (uint32_t i = 0; i < mbr_cnt; i++)
	{
		auto &mbr_type = get<SPIRType>(type.member_types[i]);
		if (mbr_type.basetype == SPIRType::Struct)
		{
			// Recursively mark structs as packed.
			auto *struct_type = &mbr_type;
			while (!struct_type->array.empty())
				struct_type = &get<SPIRType>(struct_type->parent_type);
			mark_struct_members_packed(*struct_type);
		}
		else if (!is_scalar(mbr_type))
			set_extended_member_decoration(type.self, i, SPIRVCrossDecorationPhysicalTypePacked);
	}
}

void CompilerMSL::mark_scalar_layout_structs(const SPIRType &type)
{
	uint32_t mbr_cnt = uint32_t(type.member_types.size());
	for (uint32_t i = 0; i < mbr_cnt; i++)
	{
		// Handle possible recursion when a struct contains a pointer to its own type nested somewhere.
		auto &mbr_type = get<SPIRType>(type.member_types[i]);
		if (mbr_type.basetype == SPIRType::Struct && !(mbr_type.pointer && mbr_type.storage == StorageClassPhysicalStorageBuffer))
		{
			auto *struct_type = &mbr_type;
			while (!struct_type->array.empty())
				struct_type = &get<SPIRType>(struct_type->parent_type);

			if (has_extended_decoration(struct_type->self, SPIRVCrossDecorationPhysicalTypePacked))
				continue;

			uint32_t msl_alignment = get_declared_struct_member_alignment_msl(type, i);
			uint32_t msl_size = get_declared_struct_member_size_msl(type, i);
			uint32_t spirv_offset = type_struct_member_offset(type, i);
			uint32_t spirv_offset_next;
			if (i + 1 < mbr_cnt)
				spirv_offset_next = type_struct_member_offset(type, i + 1);
			else
				spirv_offset_next = spirv_offset + msl_size;

			// Both are complicated cases. In scalar layout, a struct of float3 might just consume 12 bytes,
			// and the next member will be placed at offset 12.
			bool struct_is_misaligned = (spirv_offset % msl_alignment) != 0;
			bool struct_is_too_large = spirv_offset + msl_size > spirv_offset_next;
			uint32_t array_stride = 0;
			bool struct_needs_explicit_padding = false;

			// Verify that if a struct is used as an array that ArrayStride matches the effective size of the struct.
			if (!mbr_type.array.empty())
			{
				array_stride = type_struct_member_array_stride(type, i);
				uint32_t dimensions = uint32_t(mbr_type.array.size() - 1);
				for (uint32_t dim = 0; dim < dimensions; dim++)
				{
					uint32_t array_size = to_array_size_literal(mbr_type, dim);
					array_stride /= max<uint32_t>(array_size, 1u);
				}

				// Set expected struct size based on ArrayStride.
				struct_needs_explicit_padding = true;

				// If struct size is larger than array stride, we might be able to fit, if we tightly pack.
				if (get_declared_struct_size_msl(*struct_type) > array_stride)
					struct_is_too_large = true;
			}

			if (struct_is_misaligned || struct_is_too_large)
				mark_struct_members_packed(*struct_type);
			mark_scalar_layout_structs(*struct_type);

			if (struct_needs_explicit_padding)
			{
				msl_size = get_declared_struct_size_msl(*struct_type, true, true);
				if (array_stride < msl_size)
				{
					SPIRV_CROSS_THROW("Cannot express an array stride smaller than size of struct type.");
				}
				else
				{
					if (has_extended_decoration(struct_type->self, SPIRVCrossDecorationPaddingTarget))
					{
						if (array_stride !=
						    get_extended_decoration(struct_type->self, SPIRVCrossDecorationPaddingTarget))
							SPIRV_CROSS_THROW(
							    "A struct is used with different array strides. Cannot express this in MSL.");
					}
					else
						set_extended_decoration(struct_type->self, SPIRVCrossDecorationPaddingTarget, array_stride);
				}
			}
		}
	}
}

// Sort the members of the struct type by offset, and pack and then pad members where needed
// to align MSL members with SPIR-V offsets. The struct members are iterated twice. Packing
// occurs first, followed by padding, because packing a member reduces both its size and its
// natural alignment, possibly requiring a padding member to be added ahead of it.
void CompilerMSL::align_struct(SPIRType &ib_type, unordered_set<uint32_t> &aligned_structs)
{
	// We align structs recursively, so stop any redundant work.
	ID &ib_type_id = ib_type.self;
	if (aligned_structs.count(ib_type_id))
		return;
	aligned_structs.insert(ib_type_id);

	// Sort the members of the interface structure by their offset.
	// They should already be sorted per SPIR-V spec anyway.
	MemberSorter member_sorter(ib_type, ir.meta[ib_type_id], MemberSorter::Offset);
	member_sorter.sort();

	auto mbr_cnt = uint32_t(ib_type.member_types.size());

	for (uint32_t mbr_idx = 0; mbr_idx < mbr_cnt; mbr_idx++)
	{
		// Pack any dependent struct types before we pack a parent struct.
		auto &mbr_type = get<SPIRType>(ib_type.member_types[mbr_idx]);
		if (mbr_type.basetype == SPIRType::Struct)
			align_struct(mbr_type, aligned_structs);
	}

	// Test the alignment of each member, and if a member should be closer to the previous
	// member than the default spacing expects, it is likely that the previous member is in
	// a packed format. If so, and the previous member is packable, pack it.
	// For example ... this applies to any 3-element vector that is followed by a scalar.
	uint32_t msl_offset = 0;
	for (uint32_t mbr_idx = 0; mbr_idx < mbr_cnt; mbr_idx++)
	{
		// This checks the member in isolation, if the member needs some kind of type remapping to conform to SPIR-V
		// offsets, array strides and matrix strides.
		ensure_member_packing_rules_msl(ib_type, mbr_idx);

		// Align current offset to the current member's default alignment. If the member was packed, it will observe
		// the updated alignment here.
		uint32_t msl_align_mask = get_declared_struct_member_alignment_msl(ib_type, mbr_idx) - 1;
		uint32_t aligned_msl_offset = (msl_offset + msl_align_mask) & ~msl_align_mask;

		// Fetch the member offset as declared in the SPIRV.
		uint32_t spirv_mbr_offset = get_member_decoration(ib_type_id, mbr_idx, DecorationOffset);
		if (spirv_mbr_offset > aligned_msl_offset)
		{
			// Since MSL and SPIR-V have slightly different struct member alignment and
			// size rules, we'll pad to standard C-packing rules with a char[] array. If the member is farther
			// away than C-packing, expects, add an inert padding member before the the member.
			uint32_t padding_bytes = spirv_mbr_offset - aligned_msl_offset;
			set_extended_member_decoration(ib_type_id, mbr_idx, SPIRVCrossDecorationPaddingTarget, padding_bytes);

			// Re-align as a sanity check that aligning post-padding matches up.
			msl_offset += padding_bytes;
			aligned_msl_offset = (msl_offset + msl_align_mask) & ~msl_align_mask;
		}
		else if (spirv_mbr_offset < aligned_msl_offset)
		{
			// This should not happen, but deal with unexpected scenarios.
			// It *might* happen if a sub-struct has a larger alignment requirement in MSL than SPIR-V.
			SPIRV_CROSS_THROW("Cannot represent buffer block correctly in MSL.");
		}

		assert(aligned_msl_offset == spirv_mbr_offset);

		// Increment the current offset to be positioned immediately after the current member.
		// Don't do this for the last member since it can be unsized, and it is not relevant for padding purposes here.
		if (mbr_idx + 1 < mbr_cnt)
			msl_offset = aligned_msl_offset + get_declared_struct_member_size_msl(ib_type, mbr_idx);
	}
}

bool CompilerMSL::validate_member_packing_rules_msl(const SPIRType &type, uint32_t index) const
{
	auto &mbr_type = get<SPIRType>(type.member_types[index]);
	uint32_t spirv_offset = get_member_decoration(type.self, index, DecorationOffset);

	if (index + 1 < type.member_types.size())
	{
		// First, we will check offsets. If SPIR-V offset + MSL size > SPIR-V offset of next member,
		// we *must* perform some kind of remapping, no way getting around it.
		// We can always pad after this member if necessary, so that case is fine.
		uint32_t spirv_offset_next = get_member_decoration(type.self, index + 1, DecorationOffset);
		assert(spirv_offset_next >= spirv_offset);
		uint32_t maximum_size = spirv_offset_next - spirv_offset;
		uint32_t msl_mbr_size = get_declared_struct_member_size_msl(type, index);
		if (msl_mbr_size > maximum_size)
			return false;
	}

	if (!mbr_type.array.empty())
	{
		// If we have an array type, array stride must match exactly with SPIR-V.

		// An exception to this requirement is if we have one array element.
		// This comes from DX scalar layout workaround.
		// If app tries to be cheeky and access the member out of bounds, this will not work, but this is the best we can do.
		// In OpAccessChain with logical memory models, access chains must be in-bounds in SPIR-V specification.
		bool relax_array_stride = mbr_type.array.back() == 1 && mbr_type.array_size_literal.back();

		if (!relax_array_stride)
		{
			uint32_t spirv_array_stride = type_struct_member_array_stride(type, index);
			uint32_t msl_array_stride = get_declared_struct_member_array_stride_msl(type, index);
			if (spirv_array_stride != msl_array_stride)
				return false;
		}
	}

	if (is_matrix(mbr_type))
	{
		// Need to check MatrixStride as well.
		uint32_t spirv_matrix_stride = type_struct_member_matrix_stride(type, index);
		uint32_t msl_matrix_stride = get_declared_struct_member_matrix_stride_msl(type, index);
		if (spirv_matrix_stride != msl_matrix_stride)
			return false;
	}

	// Now, we check alignment.
	uint32_t msl_alignment = get_declared_struct_member_alignment_msl(type, index);
	if ((spirv_offset % msl_alignment) != 0)
		return false;

	// We're in the clear.
	return true;
}

// Here we need to verify that the member type we declare conforms to Offset, ArrayStride or MatrixStride restrictions.
// If there is a mismatch, we need to emit remapped types, either normal types, or "packed_X" types.
// In odd cases we need to emit packed and remapped types, for e.g. weird matrices or arrays with weird array strides.
void CompilerMSL::ensure_member_packing_rules_msl(SPIRType &ib_type, uint32_t index)
{
	if (validate_member_packing_rules_msl(ib_type, index))
		return;

	// We failed validation.
	// This case will be nightmare-ish to deal with. This could possibly happen if struct alignment does not quite
	// match up with what we want. Scalar block layout comes to mind here where we might have to work around the rule
	// that struct alignment == max alignment of all members and struct size depends on this alignment.
	// Can't repack structs, but can repack pointers to structs.
	auto &mbr_type = get<SPIRType>(ib_type.member_types[index]);
	bool is_buff_ptr = mbr_type.pointer && mbr_type.storage == StorageClassPhysicalStorageBuffer;
	if (mbr_type.basetype == SPIRType::Struct && !is_buff_ptr)
		SPIRV_CROSS_THROW("Cannot perform any repacking for structs when it is used as a member of another struct.");

	// Perform remapping here.
	// There is nothing to be gained by using packed scalars, so don't attempt it.
	if (!is_scalar(ib_type))
		set_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypePacked);

	// Try validating again, now with packed.
	if (validate_member_packing_rules_msl(ib_type, index))
		return;

	// We're in deep trouble, and we need to create a new PhysicalType which matches up with what we expect.
	// A lot of work goes here ...
	// We will need remapping on Load and Store to translate the types between Logical and Physical.

	// First, we check if we have small vector std140 array.
	// We detect this if we have an array of vectors, and array stride is greater than number of elements.
	if (!mbr_type.array.empty() && !is_matrix(mbr_type))
	{
		uint32_t array_stride = type_struct_member_array_stride(ib_type, index);

		// Hack off array-of-arrays until we find the array stride per element we must have to make it work.
		uint32_t dimensions = uint32_t(mbr_type.array.size() - 1);
		for (uint32_t dim = 0; dim < dimensions; dim++)
			array_stride /= max<uint32_t>(to_array_size_literal(mbr_type, dim), 1u);

		// Pointers are 8 bytes
		uint32_t mbr_width_in_bytes = is_buff_ptr ? 8 : (mbr_type.width / 8);
		uint32_t elems_per_stride = array_stride / mbr_width_in_bytes;

		if (elems_per_stride == 3)
			SPIRV_CROSS_THROW("Cannot use ArrayStride of 3 elements in remapping scenarios.");
		else if (elems_per_stride > 4)
			SPIRV_CROSS_THROW("Cannot represent vectors with more than 4 elements in MSL.");

		auto physical_type = mbr_type;
		physical_type.vecsize = elems_per_stride;
		physical_type.parent_type = 0;

		// If this is a physical buffer pointer, replace type with a ulongn vector.
		if (is_buff_ptr)
		{
			physical_type.width = 64;
			physical_type.basetype = to_unsigned_basetype(physical_type.width);
			physical_type.pointer = false;
			physical_type.pointer_depth = false;
			physical_type.forward_pointer = false;
		}

		uint32_t type_id = ir.increase_bound_by(1);
		set<SPIRType>(type_id, physical_type);
		set_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypeID, type_id);
		set_decoration(type_id, DecorationArrayStride, array_stride);

		// Remove packed_ for vectors of size 1, 2 and 4.
		unset_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypePacked);
	}
	else if (is_matrix(mbr_type))
	{
		// MatrixStride might be std140-esque.
		uint32_t matrix_stride = type_struct_member_matrix_stride(ib_type, index);

		uint32_t elems_per_stride = matrix_stride / (mbr_type.width / 8);

		if (elems_per_stride == 3)
			SPIRV_CROSS_THROW("Cannot use ArrayStride of 3 elements in remapping scenarios.");
		else if (elems_per_stride > 4)
			SPIRV_CROSS_THROW("Cannot represent vectors with more than 4 elements in MSL.");

		bool row_major = has_member_decoration(ib_type.self, index, DecorationRowMajor);

		auto physical_type = mbr_type;
		physical_type.parent_type = 0;
		if (row_major)
			physical_type.columns = elems_per_stride;
		else
			physical_type.vecsize = elems_per_stride;
		uint32_t type_id = ir.increase_bound_by(1);
		set<SPIRType>(type_id, physical_type);
		set_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypeID, type_id);

		// Remove packed_ for vectors of size 1, 2 and 4.
		unset_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypePacked);
	}
	else
		SPIRV_CROSS_THROW("Found a buffer packing case which we cannot represent in MSL.");

	// Try validating again, now with physical type remapping.
	if (validate_member_packing_rules_msl(ib_type, index))
		return;

	// We might have a particular odd scalar layout case where the last element of an array
	// does not take up as much space as the ArrayStride or MatrixStride. This can happen with DX cbuffers.
	// The "proper" workaround for this is extremely painful and essentially impossible in the edge case of float3[],
	// so we hack around it by declaring the offending array or matrix with one less array size/col/row,
	// and rely on padding to get the correct value. We will technically access arrays out of bounds into the padding region,
	// but it should spill over gracefully without too much trouble. We rely on behavior like this for unsized arrays anyways.

	// E.g. we might observe a physical layout of:
	// { float2 a[2]; float b; } in cbuffer layout where ArrayStride of a is 16, but offset of b is 24, packed right after a[1] ...
	uint32_t type_id = get_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypeID);
	auto &type = get<SPIRType>(type_id);

	// Modify the physical type in-place. This is safe since each physical type workaround is a copy.
	if (is_array(type))
	{
		if (type.array.back() > 1)
		{
			if (!type.array_size_literal.back())
				SPIRV_CROSS_THROW("Cannot apply scalar layout workaround with spec constant array size.");
			type.array.back() -= 1;
		}
		else
		{
			// We have an array of size 1, so we cannot decrement that. Our only option now is to
			// force a packed layout instead, and drop the physical type remap since ArrayStride is meaningless now.
			unset_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypeID);
			set_extended_member_decoration(ib_type.self, index, SPIRVCrossDecorationPhysicalTypePacked);
		}
	}
	else if (is_matrix(type))
	{
		bool row_major = has_member_decoration(ib_type.self, index, DecorationRowMajor);
		if (!row_major)
		{
			// Slice off one column. If we only have 2 columns, this might turn the matrix into a vector with one array element instead.
			if (type.columns > 2)
			{
				type.columns--;
			}
			else if (type.columns == 2)
			{
				type.columns = 1;
				assert(type.array.empty());
				type.array.push_back(1);
				type.array_size_literal.push_back(true);
			}
		}
		else
		{
			// Slice off one row. If we only have 2 rows, this might turn the matrix into a vector with one array element instead.
			if (type.vecsize > 2)
			{
				type.vecsize--;
			}
			else if (type.vecsize == 2)
			{
				type.vecsize = type.columns;
				type.columns = 1;
				assert(type.array.empty());
				type.array.push_back(1);
				type.array_size_literal.push_back(true);
			}
		}
	}

	// This better validate now, or we must fail gracefully.
	if (!validate_member_packing_rules_msl(ib_type, index))
		SPIRV_CROSS_THROW("Found a buffer packing case which we cannot represent in MSL.");
}

void CompilerMSL::emit_store_statement(uint32_t lhs_expression, uint32_t rhs_expression)
{
	auto &type = expression_type(rhs_expression);

	bool lhs_remapped_type = has_extended_decoration(lhs_expression, SPIRVCrossDecorationPhysicalTypeID);
	bool lhs_packed_type = has_extended_decoration(lhs_expression, SPIRVCrossDecorationPhysicalTypePacked);
	auto *lhs_e = maybe_get<SPIRExpression>(lhs_expression);
	auto *rhs_e = maybe_get<SPIRExpression>(rhs_expression);

	bool transpose = lhs_e && lhs_e->need_transpose;

	// No physical type remapping, and no packed type, so can just emit a store directly.
	if (!lhs_remapped_type && !lhs_packed_type)
	{
		// We might not be dealing with remapped physical types or packed types,
		// but we might be doing a clean store to a row-major matrix.
		// In this case, we just flip transpose states, and emit the store, a transpose must be in the RHS expression, if any.
		if (is_matrix(type) && lhs_e && lhs_e->need_transpose)
		{
			lhs_e->need_transpose = false;

			if (rhs_e && rhs_e->need_transpose)
			{
				// Direct copy, but might need to unpack RHS.
				// Skip the transpose, as we will transpose when writing to LHS and transpose(transpose(T)) == T.
				rhs_e->need_transpose = false;
				statement(to_expression(lhs_expression), " = ", to_unpacked_row_major_matrix_expression(rhs_expression),
				          ";");
				rhs_e->need_transpose = true;
			}
			else
				statement(to_expression(lhs_expression), " = transpose(", to_unpacked_expression(rhs_expression), ");");

			lhs_e->need_transpose = true;
			register_write(lhs_expression);
		}
		else if (lhs_e && lhs_e->need_transpose)
		{
			lhs_e->need_transpose = false;

			// Storing a column to a row-major matrix. Unroll the write.
			for (uint32_t c = 0; c < type.vecsize; c++)
			{
				auto lhs_expr = to_dereferenced_expression(lhs_expression);
				auto column_index = lhs_expr.find_last_of('[');
				if (column_index != string::npos)
				{
					statement(lhs_expr.insert(column_index, join('[', c, ']')), " = ",
					          to_extract_component_expression(rhs_expression, c), ";");
				}
			}
			lhs_e->need_transpose = true;
			register_write(lhs_expression);
		}
		else
			CompilerGLSL::emit_store_statement(lhs_expression, rhs_expression);
	}
	else if (!lhs_remapped_type && !is_matrix(type) && !transpose)
	{
		// Even if the target type is packed, we can directly store to it. We cannot store to packed matrices directly,
		// since they are declared as array of vectors instead, and we need the fallback path below.
		CompilerGLSL::emit_store_statement(lhs_expression, rhs_expression);
	}
	else
	{
		// Special handling when storing to a remapped physical type.
		// This is mostly to deal with std140 padded matrices or vectors.

		TypeID physical_type_id = lhs_remapped_type ?
		                              ID(get_extended_decoration(lhs_expression, SPIRVCrossDecorationPhysicalTypeID)) :
		                              type.self;

		auto &physical_type = get<SPIRType>(physical_type_id);

		string cast_addr_space = "thread";
		auto *p_var_lhs = maybe_get_backing_variable(lhs_expression);
		if (p_var_lhs)
			cast_addr_space = get_type_address_space(get<SPIRType>(p_var_lhs->basetype), lhs_expression);

		if (is_matrix(type))
		{
			const char *packed_pfx = lhs_packed_type ? "packed_" : "";

			// Packed matrices are stored as arrays of packed vectors, so we need
			// to assign the vectors one at a time.
			// For row-major matrices, we need to transpose the *right-hand* side,
			// not the left-hand side.

			// Lots of cases to cover here ...

			bool rhs_transpose = rhs_e && rhs_e->need_transpose;
			SPIRType write_type = type;
			string cast_expr;

			// We're dealing with transpose manually.
			if (rhs_transpose)
				rhs_e->need_transpose = false;

			if (transpose)
			{
				// We're dealing with transpose manually.
				lhs_e->need_transpose = false;
				write_type.vecsize = type.columns;
				write_type.columns = 1;

				if (physical_type.columns != type.columns)
					cast_expr = join("(", cast_addr_space, " ", packed_pfx, type_to_glsl(write_type), "&)");

				if (rhs_transpose)
				{
					// If RHS is also transposed, we can just copy row by row.
					for (uint32_t i = 0; i < type.vecsize; i++)
					{
						statement(cast_expr, to_enclosed_expression(lhs_expression), "[", i, "]", " = ",
						          to_unpacked_row_major_matrix_expression(rhs_expression), "[", i, "];");
					}
				}
				else
				{
					auto vector_type = expression_type(rhs_expression);
					vector_type.vecsize = vector_type.columns;
					vector_type.columns = 1;

					// Transpose on the fly. Emitting a lot of full transpose() ops and extracting lanes seems very bad,
					// so pick out individual components instead.
					for (uint32_t i = 0; i < type.vecsize; i++)
					{
						string rhs_row = type_to_glsl_constructor(vector_type) + "(";
						for (uint32_t j = 0; j < vector_type.vecsize; j++)
						{
							rhs_row += join(to_enclosed_unpacked_expression(rhs_expression), "[", j, "][", i, "]");
							if (j + 1 < vector_type.vecsize)
								rhs_row += ", ";
						}
						rhs_row += ")";

						statement(cast_expr, to_enclosed_expression(lhs_expression), "[", i, "]", " = ", rhs_row, ";");
					}
				}

				// We're dealing with transpose manually.
				lhs_e->need_transpose = true;
			}
			else
			{
				write_type.columns = 1;

				if (physical_type.vecsize != type.vecsize)
					cast_expr = join("(", cast_addr_space, " ", packed_pfx, type_to_glsl(write_type), "&)");

				if (rhs_transpose)
				{
					auto vector_type = expression_type(rhs_expression);
					vector_type.columns = 1;

					// Transpose on the fly. Emitting a lot of full transpose() ops and extracting lanes seems very bad,
					// so pick out individual components instead.
					for (uint32_t i = 0; i < type.columns; i++)
					{
						string rhs_row = type_to_glsl_constructor(vector_type) + "(";
						for (uint32_t j = 0; j < vector_type.vecsize; j++)
						{
							// Need to explicitly unpack expression since we've mucked with transpose state.
							auto unpacked_expr = to_unpacked_row_major_matrix_expression(rhs_expression);
							rhs_row += join(unpacked_expr, "[", j, "][", i, "]");
							if (j + 1 < vector_type.vecsize)
								rhs_row += ", ";
						}
						rhs_row += ")";

						statement(cast_expr, to_enclosed_expression(lhs_expression), "[", i, "]", " = ", rhs_row, ";");
					}
				}
				else
				{
					// Copy column-by-column.
					for (uint32_t i = 0; i < type.columns; i++)
					{
						statement(cast_expr, to_enclosed_expression(lhs_expression), "[", i, "]", " = ",
						          to_enclosed_unpacked_expression(rhs_expression), "[", i, "];");
					}
				}
			}

			// We're dealing with transpose manually.
			if (rhs_transpose)
				rhs_e->need_transpose = true;
		}
		else if (transpose)
		{
			lhs_e->need_transpose = false;

			SPIRType write_type = type;
			write_type.vecsize = 1;
			write_type.columns = 1;

			// Storing a column to a row-major matrix. Unroll the write.
			for (uint32_t c = 0; c < type.vecsize; c++)
			{
				auto lhs_expr = to_enclosed_expression(lhs_expression);
				auto column_index = lhs_expr.find_last_of('[');
				if (column_index != string::npos)
				{
					statement("((", cast_addr_space, " ", type_to_glsl(write_type), "*)&",
					          lhs_expr.insert(column_index, join('[', c, ']', ")")), " = ",
					          to_extract_component_expression(rhs_expression, c), ";");
				}
			}

			lhs_e->need_transpose = true;
		}
		else if ((is_matrix(physical_type) || is_array(physical_type)) && physical_type.vecsize > type.vecsize)
		{
			assert(type.vecsize >= 1 && type.vecsize <= 3);

			// If we have packed types, we cannot use swizzled stores.
			// We could technically unroll the store for each element if needed.
			// When remapping to a std140 physical type, we always get float4,
			// and the packed decoration should always be removed.
			assert(!lhs_packed_type);

			string lhs = to_dereferenced_expression(lhs_expression);
			string rhs = to_pointer_expression(rhs_expression);

			// Unpack the expression so we can store to it with a float or float2.
			// It's still an l-value, so it's fine. Most other unpacking of expressions turn them into r-values instead.
			lhs = join("(", cast_addr_space, " ", type_to_glsl(type), "&)", enclose_expression(lhs));
			if (!optimize_read_modify_write(expression_type(rhs_expression), lhs, rhs))
				statement(lhs, " = ", rhs, ";");
		}
		else if (!is_matrix(type))
		{
			string lhs = to_dereferenced_expression(lhs_expression);
			string rhs = to_pointer_expression(rhs_expression);
			if (!optimize_read_modify_write(expression_type(rhs_expression), lhs, rhs))
				statement(lhs, " = ", rhs, ";");
		}

		register_write(lhs_expression);
	}
}

static bool expression_ends_with(const string &expr_str, const std::string &ending)
{
	if (expr_str.length() >= ending.length())
		return (expr_str.compare(expr_str.length() - ending.length(), ending.length(), ending) == 0);
	else
		return false;
}

// Converts the format of the current expression from packed to unpacked,
// by wrapping the expression in a constructor of the appropriate type.
// Also, handle special physical ID remapping scenarios, similar to emit_store_statement().
string CompilerMSL::unpack_expression_type(string expr_str, const SPIRType &type, uint32_t physical_type_id,
                                           bool packed, bool row_major)
{
	// Trivial case, nothing to do.
	if (physical_type_id == 0 && !packed)
		return expr_str;

	const SPIRType *physical_type = nullptr;
	if (physical_type_id)
		physical_type = &get<SPIRType>(physical_type_id);

	static const char *swizzle_lut[] = {
		".x",
		".xy",
		".xyz",
	};

	if (physical_type && is_vector(*physical_type) && is_array(*physical_type) &&
	    physical_type->vecsize > type.vecsize && !expression_ends_with(expr_str, swizzle_lut[type.vecsize - 1]))
	{
		// std140 array cases for vectors.
		assert(type.vecsize >= 1 && type.vecsize <= 3);
		return enclose_expression(expr_str) + swizzle_lut[type.vecsize - 1];
	}
	else if (physical_type && is_matrix(*physical_type) && is_vector(type) && physical_type->vecsize > type.vecsize)
	{
		// Extract column from padded matrix.
		assert(type.vecsize >= 1 && type.vecsize <= 3);
		return enclose_expression(expr_str) + swizzle_lut[type.vecsize - 1];
	}
	else if (is_matrix(type))
	{
		// Packed matrices are stored as arrays of packed vectors. Unfortunately,
		// we can't just pass the array straight to the matrix constructor. We have to
		// pass each vector individually, so that they can be unpacked to normal vectors.
		if (!physical_type)
			physical_type = &type;

		uint32_t vecsize = type.vecsize;
		uint32_t columns = type.columns;
		if (row_major)
			swap(vecsize, columns);

		uint32_t physical_vecsize = row_major ? physical_type->columns : physical_type->vecsize;

		const char *base_type = type.width == 16 ? "half" : "float";
		string unpack_expr = join(base_type, columns, "x", vecsize, "(");

		const char *load_swiz = "";

		if (physical_vecsize != vecsize)
			load_swiz = swizzle_lut[vecsize - 1];

		for (uint32_t i = 0; i < columns; i++)
		{
			if (i > 0)
				unpack_expr += ", ";

			if (packed)
				unpack_expr += join(base_type, physical_vecsize, "(", expr_str, "[", i, "]", ")", load_swiz);
			else
				unpack_expr += join(expr_str, "[", i, "]", load_swiz);
		}

		unpack_expr += ")";
		return unpack_expr;
	}
	else
	{
		return join(type_to_glsl(type), "(", expr_str, ")");
	}
}

// Emits the file header info
void CompilerMSL::emit_header()
{
	// This particular line can be overridden during compilation, so make it a flag and not a pragma line.
	if (suppress_missing_prototypes)
		statement("#pragma clang diagnostic ignored \"-Wmissing-prototypes\"");

	// Disable warning about missing braces for array<T> template to make arrays a value type
	if (spv_function_implementations.count(SPVFuncImplUnsafeArray) != 0)
		statement("#pragma clang diagnostic ignored \"-Wmissing-braces\"");

	for (auto &pragma : pragma_lines)
		statement(pragma);

	if (!pragma_lines.empty() || suppress_missing_prototypes)
		statement("");

	statement("#include <metal_stdlib>");
	statement("#include <simd/simd.h>");

	for (auto &header : header_lines)
		statement(header);

	statement("");
	statement("using namespace metal;");
	statement("");

	for (auto &td : typedef_lines)
		statement(td);

	if (!typedef_lines.empty())
		statement("");
}

void CompilerMSL::add_pragma_line(const string &line)
{
	auto rslt = pragma_lines.insert(line);
	if (rslt.second)
		force_recompile();
}

void CompilerMSL::add_typedef_line(const string &line)
{
	auto rslt = typedef_lines.insert(line);
	if (rslt.second)
		force_recompile();
}

// Template struct like spvUnsafeArray<> need to be declared *before* any resources are declared
void CompilerMSL::emit_custom_templates()
{
	static const char * const address_spaces[] = {
		"thread", "constant", "device", "threadgroup", "threadgroup_imageblock", "ray_data", "object_data"
	};

	for (const auto &spv_func : spv_function_implementations)
	{
		switch (spv_func)
		{
		case SPVFuncImplUnsafeArray:
			statement("template<typename T, size_t Num>");
			statement("struct spvUnsafeArray");
			begin_scope();
			statement("T elements[Num ? Num : 1];");
			statement("");
			statement("thread T& operator [] (size_t pos) thread");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			statement("constexpr const thread T& operator [] (size_t pos) const thread");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			statement("");
			statement("device T& operator [] (size_t pos) device");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			statement("constexpr const device T& operator [] (size_t pos) const device");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			statement("");
			statement("constexpr const constant T& operator [] (size_t pos) const constant");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			statement("");
			statement("threadgroup T& operator [] (size_t pos) threadgroup");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			statement("constexpr const threadgroup T& operator [] (size_t pos) const threadgroup");
			begin_scope();
			statement("return elements[pos];");
			end_scope();
			end_scope_decl();
			statement("");
			break;

		case SPVFuncImplStorageMatrix:
			statement("template<typename T, int Cols, int Rows=Cols>");
			statement("struct spvStorageMatrix");
			begin_scope();
			statement("vec<T, Rows> columns[Cols];");
			statement("");
			for (size_t method_idx = 0; method_idx < sizeof(address_spaces) / sizeof(address_spaces[0]); ++method_idx)
			{
				// Some address spaces require particular features.
				if (method_idx == 4) // threadgroup_imageblock
					statement("#ifdef __HAVE_IMAGEBLOCKS__");
				else if (method_idx == 5) // ray_data
					statement("#ifdef __HAVE_RAYTRACING__");
				else if (method_idx == 6) // object_data
					statement("#ifdef __HAVE_MESH__");
				const string &method_as = address_spaces[method_idx];
				statement("spvStorageMatrix() ", method_as, " = default;");
				if (method_idx != 1) // constant
				{
					statement(method_as, " spvStorageMatrix& operator=(initializer_list<vec<T, Rows>> cols) ",
					          method_as);
					begin_scope();
					statement("size_t i;");
					statement("thread vec<T, Rows>* col;");
					statement("for (i = 0, col = cols.begin(); i < Cols; ++i, ++col)");
					statement("    columns[i] = *col;");
					statement("return *this;");
					end_scope();
				}
				statement("");
				for (size_t param_idx = 0; param_idx < sizeof(address_spaces) / sizeof(address_spaces[0]); ++param_idx)
				{
					if (param_idx != method_idx)
					{
						if (param_idx == 4) // threadgroup_imageblock
							statement("#ifdef __HAVE_IMAGEBLOCKS__");
						else if (param_idx == 5) // ray_data
							statement("#ifdef __HAVE_RAYTRACING__");
						else if (param_idx == 6) // object_data
							statement("#ifdef __HAVE_MESH__");
					}
					const string &param_as = address_spaces[param_idx];
					statement("spvStorageMatrix(const ", param_as, " matrix<T, Cols, Rows>& m) ", method_as);
					begin_scope();
					statement("for (size_t i = 0; i < Cols; ++i)");
					statement("    columns[i] = m.columns[i];");
					end_scope();
					statement("spvStorageMatrix(const ", param_as, " spvStorageMatrix& m) ", method_as, " = default;");
					if (method_idx != 1) // constant
					{
						statement(method_as, " spvStorageMatrix& operator=(const ", param_as,
						          " matrix<T, Cols, Rows>& m) ", method_as);
						begin_scope();
						statement("for (size_t i = 0; i < Cols; ++i)");
						statement("    columns[i] = m.columns[i];");
						statement("return *this;");
						end_scope();
						statement(method_as, " spvStorageMatrix& operator=(const ", param_as, " spvStorageMatrix& m) ",
						          method_as, " = default;");
					}
					if (param_idx != method_idx && param_idx >= 4)
						statement("#endif");
					statement("");
				}
				statement("operator matrix<T, Cols, Rows>() const ", method_as);
				begin_scope();
				statement("matrix<T, Cols, Rows> m;");
				statement("for (int i = 0; i < Cols; ++i)");
				statement("    m.columns[i] = columns[i];");
				statement("return m;");
				end_scope();
				statement("");
				statement("vec<T, Rows> operator[](size_t idx) const ", method_as);
				begin_scope();
				statement("return columns[idx];");
				end_scope();
				if (method_idx != 1) // constant
				{
					statement(method_as, " vec<T, Rows>& operator[](size_t idx) ", method_as);
					begin_scope();
					statement("return columns[idx];");
					end_scope();
				}
				if (method_idx >= 4)
					statement("#endif");
				statement("");
			}
			end_scope_decl();
			statement("");
			statement("template<typename T, int Cols, int Rows>");
			statement("matrix<T, Rows, Cols> transpose(spvStorageMatrix<T, Cols, Rows> m)");
			begin_scope();
			statement("return transpose(matrix<T, Cols, Rows>(m));");
			end_scope();
			statement("");
			statement("typedef spvStorageMatrix<half, 2, 2> spvStorage_half2x2;");
			statement("typedef spvStorageMatrix<half, 2, 3> spvStorage_half2x3;");
			statement("typedef spvStorageMatrix<half, 2, 4> spvStorage_half2x4;");
			statement("typedef spvStorageMatrix<half, 3, 2> spvStorage_half3x2;");
			statement("typedef spvStorageMatrix<half, 3, 3> spvStorage_half3x3;");
			statement("typedef spvStorageMatrix<half, 3, 4> spvStorage_half3x4;");
			statement("typedef spvStorageMatrix<half, 4, 2> spvStorage_half4x2;");
			statement("typedef spvStorageMatrix<half, 4, 3> spvStorage_half4x3;");
			statement("typedef spvStorageMatrix<half, 4, 4> spvStorage_half4x4;");
			statement("typedef spvStorageMatrix<float, 2, 2> spvStorage_float2x2;");
			statement("typedef spvStorageMatrix<float, 2, 3> spvStorage_float2x3;");
			statement("typedef spvStorageMatrix<float, 2, 4> spvStorage_float2x4;");
			statement("typedef spvStorageMatrix<float, 3, 2> spvStorage_float3x2;");
			statement("typedef spvStorageMatrix<float, 3, 3> spvStorage_float3x3;");
			statement("typedef spvStorageMatrix<float, 3, 4> spvStorage_float3x4;");
			statement("typedef spvStorageMatrix<float, 4, 2> spvStorage_float4x2;");
			statement("typedef spvStorageMatrix<float, 4, 3> spvStorage_float4x3;");
			statement("typedef spvStorageMatrix<float, 4, 4> spvStorage_float4x4;");
			statement("");
			break;

		default:
			break;
		}
	}
}

// Emits any needed custom function bodies.
// Metal helper functions must be static force-inline, i.e. static inline __attribute__((always_inline))
// otherwise they will cause problems when linked together in a single Metallib.
void CompilerMSL::emit_custom_functions()
{
	for (uint32_t i = kArrayCopyMultidimMax; i >= 2; i--)
		if (spv_function_implementations.count(static_cast<SPVFuncImpl>(SPVFuncImplArrayCopyMultidimBase + i)))
			spv_function_implementations.insert(static_cast<SPVFuncImpl>(SPVFuncImplArrayCopyMultidimBase + i - 1));

	if (spv_function_implementations.count(SPVFuncImplDynamicImageSampler))
	{
		// Unfortunately, this one needs a lot of the other functions to compile OK.
		if (!msl_options.supports_msl_version(2))
			SPIRV_CROSS_THROW(
			    "spvDynamicImageSampler requires default-constructible texture objects, which require MSL 2.0.");
		spv_function_implementations.insert(SPVFuncImplForwardArgs);
		spv_function_implementations.insert(SPVFuncImplTextureSwizzle);
		if (msl_options.swizzle_texture_samples)
			spv_function_implementations.insert(SPVFuncImplGatherSwizzle);
		for (uint32_t i = SPVFuncImplChromaReconstructNearest2Plane;
		     i <= SPVFuncImplChromaReconstructLinear420XMidpointYMidpoint3Plane; i++)
			spv_function_implementations.insert(static_cast<SPVFuncImpl>(i));
		spv_function_implementations.insert(SPVFuncImplExpandITUFullRange);
		spv_function_implementations.insert(SPVFuncImplExpandITUNarrowRange);
		spv_function_implementations.insert(SPVFuncImplConvertYCbCrBT709);
		spv_function_implementations.insert(SPVFuncImplConvertYCbCrBT601);
		spv_function_implementations.insert(SPVFuncImplConvertYCbCrBT2020);
	}

	for (uint32_t i = SPVFuncImplChromaReconstructNearest2Plane;
	     i <= SPVFuncImplChromaReconstructLinear420XMidpointYMidpoint3Plane; i++)
		if (spv_function_implementations.count(static_cast<SPVFuncImpl>(i)))
			spv_function_implementations.insert(SPVFuncImplForwardArgs);

	if (spv_function_implementations.count(SPVFuncImplTextureSwizzle) ||
	    spv_function_implementations.count(SPVFuncImplGatherSwizzle) ||
	    spv_function_implementations.count(SPVFuncImplGatherCompareSwizzle))
	{
		spv_function_implementations.insert(SPVFuncImplForwardArgs);
		spv_function_implementations.insert(SPVFuncImplGetSwizzle);
	}

	for (const auto &spv_func : spv_function_implementations)
	{
		switch (spv_func)
		{
		case SPVFuncImplMod:
			statement("// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()");
			statement("template<typename Tx, typename Ty>");
			statement("inline Tx mod(Tx x, Ty y)");
			begin_scope();
			statement("return x - y * floor(x / y);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplRadians:
			statement("// Implementation of the GLSL radians() function");
			statement("template<typename T>");
			statement("inline T radians(T d)");
			begin_scope();
			statement("return d * T(0.01745329251);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplDegrees:
			statement("// Implementation of the GLSL degrees() function");
			statement("template<typename T>");
			statement("inline T degrees(T r)");
			begin_scope();
			statement("return r * T(57.2957795131);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplFindILsb:
			statement("// Implementation of the GLSL findLSB() function");
			statement("template<typename T>");
			statement("inline T spvFindLSB(T x)");
			begin_scope();
			statement("return select(ctz(x), T(-1), x == T(0));");
			end_scope();
			statement("");
			break;

		case SPVFuncImplFindUMsb:
			statement("// Implementation of the unsigned GLSL findMSB() function");
			statement("template<typename T>");
			statement("inline T spvFindUMSB(T x)");
			begin_scope();
			statement("return select(clz(T(0)) - (clz(x) + T(1)), T(-1), x == T(0));");
			end_scope();
			statement("");
			break;

		case SPVFuncImplFindSMsb:
			statement("// Implementation of the signed GLSL findMSB() function");
			statement("template<typename T>");
			statement("inline T spvFindSMSB(T x)");
			begin_scope();
			statement("T v = select(x, T(-1) - x, x < T(0));");
			statement("return select(clz(T(0)) - (clz(v) + T(1)), T(-1), v == T(0));");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSSign:
			statement("// Implementation of the GLSL sign() function for integer types");
			statement("template<typename T, typename E = typename enable_if<is_integral<T>::value>::type>");
			statement("inline T sign(T x)");
			begin_scope();
			statement("return select(select(select(x, T(0), x == T(0)), T(1), x > T(0)), T(-1), x < T(0));");
			end_scope();
			statement("");
			break;

		case SPVFuncImplArrayCopy:
		case SPVFuncImplArrayOfArrayCopy2Dim:
		case SPVFuncImplArrayOfArrayCopy3Dim:
		case SPVFuncImplArrayOfArrayCopy4Dim:
		case SPVFuncImplArrayOfArrayCopy5Dim:
		case SPVFuncImplArrayOfArrayCopy6Dim:
		{
			// Unfortunately we cannot template on the address space, so combinatorial explosion it is.
			static const char *function_name_tags[] = {
				"FromConstantToStack",     "FromConstantToThreadGroup", "FromStackToStack",
				"FromStackToThreadGroup",  "FromThreadGroupToStack",    "FromThreadGroupToThreadGroup",
				"FromDeviceToDevice",      "FromConstantToDevice",      "FromStackToDevice",
				"FromThreadGroupToDevice", "FromDeviceToStack",         "FromDeviceToThreadGroup",
			};

			static const char *src_address_space[] = {
				"constant",          "constant",          "thread const", "thread const",
				"threadgroup const", "threadgroup const", "device const", "constant",
				"thread const",      "threadgroup const", "device const", "device const",
			};

			static const char *dst_address_space[] = {
				"thread", "threadgroup", "thread", "threadgroup", "thread", "threadgroup",
				"device", "device",      "device", "device",      "thread", "threadgroup",
			};

			for (uint32_t variant = 0; variant < 12; variant++)
			{
				uint8_t dimensions = spv_func - SPVFuncImplArrayCopyMultidimBase;
				string tmp = "template<typename T";
				for (uint8_t i = 0; i < dimensions; i++)
				{
					tmp += ", uint ";
					tmp += 'A' + i;
				}
				tmp += ">";
				statement(tmp);

				string array_arg;
				for (uint8_t i = 0; i < dimensions; i++)
				{
					array_arg += "[";
					array_arg += 'A' + i;
					array_arg += "]";
				}

				statement("inline void spvArrayCopy", function_name_tags[variant], dimensions, "(",
				          dst_address_space[variant], " T (&dst)", array_arg, ", ", src_address_space[variant],
				          " T (&src)", array_arg, ")");

				begin_scope();
				statement("for (uint i = 0; i < A; i++)");
				begin_scope();

				if (dimensions == 1)
					statement("dst[i] = src[i];");
				else
					statement("spvArrayCopy", function_name_tags[variant], dimensions - 1, "(dst[i], src[i]);");
				end_scope();
				end_scope();
				statement("");
			}
			break;
		}

		// Support for Metal 2.1's new texture_buffer type.
		case SPVFuncImplTexelBufferCoords:
		{
			if (msl_options.texel_buffer_texture_width > 0)
			{
				string tex_width_str = convert_to_string(msl_options.texel_buffer_texture_width);
				statement("// Returns 2D texture coords corresponding to 1D texel buffer coords");
				statement(force_inline);
				statement("uint2 spvTexelBufferCoord(uint tc)");
				begin_scope();
				statement(join("return uint2(tc % ", tex_width_str, ", tc / ", tex_width_str, ");"));
				end_scope();
				statement("");
			}
			else
			{
				statement("// Returns 2D texture coords corresponding to 1D texel buffer coords");
				statement(
				    "#define spvTexelBufferCoord(tc, tex) uint2((tc) % (tex).get_width(), (tc) / (tex).get_width())");
				statement("");
			}
			break;
		}

		// Emulate texture2D atomic operations
		case SPVFuncImplImage2DAtomicCoords:
		{
			if (msl_options.supports_msl_version(1, 2))
			{
				statement("// The required alignment of a linear texture of R32Uint format.");
				statement("constant uint spvLinearTextureAlignmentOverride [[function_constant(",
				          msl_options.r32ui_alignment_constant_id, ")]];");
				statement("constant uint spvLinearTextureAlignment = ",
				          "is_function_constant_defined(spvLinearTextureAlignmentOverride) ? ",
				          "spvLinearTextureAlignmentOverride : ", msl_options.r32ui_linear_texture_alignment, ";");
			}
			else
			{
				statement("// The required alignment of a linear texture of R32Uint format.");
				statement("constant uint spvLinearTextureAlignment = ", msl_options.r32ui_linear_texture_alignment,
				          ";");
			}
			statement("// Returns buffer coords corresponding to 2D texture coords for emulating 2D texture atomics");
			statement("#define spvImage2DAtomicCoord(tc, tex) (((((tex).get_width() + ",
			          " spvLinearTextureAlignment / 4 - 1) & ~(",
			          " spvLinearTextureAlignment / 4 - 1)) * (tc).y) + (tc).x)");
			statement("");
			break;
		}

		// "fadd" intrinsic support
		case SPVFuncImplFAdd:
			statement("template<typename T>");
			statement("[[clang::optnone]] T spvFAdd(T l, T r)");
			begin_scope();
			statement("return fma(T(1), l, r);");
			end_scope();
			statement("");
			break;

		// "fsub" intrinsic support
		case SPVFuncImplFSub:
			statement("template<typename T>");
			statement("[[clang::optnone]] T spvFSub(T l, T r)");
			begin_scope();
			statement("return fma(T(-1), r, l);");
			end_scope();
			statement("");
			break;

		// "fmul' intrinsic support
		case SPVFuncImplFMul:
			statement("template<typename T>");
			statement("[[clang::optnone]] T spvFMul(T l, T r)");
			begin_scope();
			statement("return fma(l, r, T(0));");
			end_scope();
			statement("");

			statement("template<typename T, int Cols, int Rows>");
			statement("[[clang::optnone]] vec<T, Cols> spvFMulVectorMatrix(vec<T, Rows> v, matrix<T, Cols, Rows> m)");
			begin_scope();
			statement("vec<T, Cols> res = vec<T, Cols>(0);");
			statement("for (uint i = Rows; i > 0; --i)");
			begin_scope();
			statement("vec<T, Cols> tmp(0);");
			statement("for (uint j = 0; j < Cols; ++j)");
			begin_scope();
			statement("tmp[j] = m[j][i - 1];");
			end_scope();
			statement("res = fma(tmp, vec<T, Cols>(v[i - 1]), res);");
			end_scope();
			statement("return res;");
			end_scope();
			statement("");

			statement("template<typename T, int Cols, int Rows>");
			statement("[[clang::optnone]] vec<T, Rows> spvFMulMatrixVector(matrix<T, Cols, Rows> m, vec<T, Cols> v)");
			begin_scope();
			statement("vec<T, Rows> res = vec<T, Rows>(0);");
			statement("for (uint i = Cols; i > 0; --i)");
			begin_scope();
			statement("res = fma(m[i - 1], vec<T, Rows>(v[i - 1]), res);");
			end_scope();
			statement("return res;");
			end_scope();
			statement("");

			statement("template<typename T, int LCols, int LRows, int RCols, int RRows>");
			statement("[[clang::optnone]] matrix<T, RCols, LRows> spvFMulMatrixMatrix(matrix<T, LCols, LRows> l, matrix<T, RCols, RRows> r)");
			begin_scope();
			statement("matrix<T, RCols, LRows> res;");
			statement("for (uint i = 0; i < RCols; i++)");
			begin_scope();
			statement("vec<T, RCols> tmp(0);");
			statement("for (uint j = 0; j < LCols; j++)");
			begin_scope();
			statement("tmp = fma(vec<T, RCols>(r[i][j]), l[j], tmp);");
			end_scope();
			statement("res[i] = tmp;");
			end_scope();
			statement("return res;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplQuantizeToF16:
			// Ensure fast-math is disabled to match Vulkan results.
			// SpvHalfTypeSelector is used to match the half* template type to the float* template type.
			// Depending on GPU, MSL does not always flush converted subnormal halfs to zero,
			// as required by OpQuantizeToF16, so check for subnormals and flush them to zero.
			statement("template <typename F> struct SpvHalfTypeSelector;");
			statement("template <> struct SpvHalfTypeSelector<float> { public: using H = half; };");
			statement("template<uint N> struct SpvHalfTypeSelector<vec<float, N>> { using H = vec<half, N>; };");
			statement("template<typename F, typename H = typename SpvHalfTypeSelector<F>::H>");
			statement("[[clang::optnone]] F spvQuantizeToF16(F fval)");
			begin_scope();
			statement("H hval = H(fval);");
			statement("hval = select(copysign(H(0), hval), hval, isnormal(hval) || isinf(hval) || isnan(hval));");
			statement("return F(hval);");
			end_scope();
			statement("");
			break;

		// Emulate texturecube_array with texture2d_array for iOS where this type is not available
		case SPVFuncImplCubemapTo2DArrayFace:
			statement(force_inline);
			statement("float3 spvCubemapTo2DArrayFace(float3 P)");
			begin_scope();
			statement("float3 Coords = abs(P.xyz);");
			statement("float CubeFace = 0;");
			statement("float ProjectionAxis = 0;");
			statement("float u = 0;");
			statement("float v = 0;");
			statement("if (Coords.x >= Coords.y && Coords.x >= Coords.z)");
			begin_scope();
			statement("CubeFace = P.x >= 0 ? 0 : 1;");
			statement("ProjectionAxis = Coords.x;");
			statement("u = P.x >= 0 ? -P.z : P.z;");
			statement("v = -P.y;");
			end_scope();
			statement("else if (Coords.y >= Coords.x && Coords.y >= Coords.z)");
			begin_scope();
			statement("CubeFace = P.y >= 0 ? 2 : 3;");
			statement("ProjectionAxis = Coords.y;");
			statement("u = P.x;");
			statement("v = P.y >= 0 ? P.z : -P.z;");
			end_scope();
			statement("else");
			begin_scope();
			statement("CubeFace = P.z >= 0 ? 4 : 5;");
			statement("ProjectionAxis = Coords.z;");
			statement("u = P.z >= 0 ? P.x : -P.x;");
			statement("v = -P.y;");
			end_scope();
			statement("u = 0.5 * (u/ProjectionAxis + 1);");
			statement("v = 0.5 * (v/ProjectionAxis + 1);");
			statement("return float3(u, v, CubeFace);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplInverse4x4:
			statement("// Returns the determinant of a 2x2 matrix.");
			statement(force_inline);
			statement("float spvDet2x2(float a1, float a2, float b1, float b2)");
			begin_scope();
			statement("return a1 * b2 - b1 * a2;");
			end_scope();
			statement("");

			statement("// Returns the determinant of a 3x3 matrix.");
			statement(force_inline);
			statement("float spvDet3x3(float a1, float a2, float a3, float b1, float b2, float b3, float c1, "
			          "float c2, float c3)");
			begin_scope();
			statement("return a1 * spvDet2x2(b2, b3, c2, c3) - b1 * spvDet2x2(a2, a3, c2, c3) + c1 * spvDet2x2(a2, a3, "
			          "b2, b3);");
			end_scope();
			statement("");
			statement("// Returns the inverse of a matrix, by using the algorithm of calculating the classical");
			statement("// adjoint and dividing by the determinant. The contents of the matrix are changed.");
			statement(force_inline);
			statement("float4x4 spvInverse4x4(float4x4 m)");
			begin_scope();
			statement("float4x4 adj;	// The adjoint matrix (inverse after dividing by determinant)");
			statement_no_indent("");
			statement("// Create the transpose of the cofactors, as the classical adjoint of the matrix.");
			statement("adj[0][0] =  spvDet3x3(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], "
			          "m[3][3]);");
			statement("adj[0][1] = -spvDet3x3(m[0][1], m[0][2], m[0][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], "
			          "m[3][3]);");
			statement("adj[0][2] =  spvDet3x3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[3][1], m[3][2], "
			          "m[3][3]);");
			statement("adj[0][3] = -spvDet3x3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], "
			          "m[2][3]);");
			statement_no_indent("");
			statement("adj[1][0] = -spvDet3x3(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], "
			          "m[3][3]);");
			statement("adj[1][1] =  spvDet3x3(m[0][0], m[0][2], m[0][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], "
			          "m[3][3]);");
			statement("adj[1][2] = -spvDet3x3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[3][0], m[3][2], "
			          "m[3][3]);");
			statement("adj[1][3] =  spvDet3x3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], "
			          "m[2][3]);");
			statement_no_indent("");
			statement("adj[2][0] =  spvDet3x3(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], "
			          "m[3][3]);");
			statement("adj[2][1] = -spvDet3x3(m[0][0], m[0][1], m[0][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], "
			          "m[3][3]);");
			statement("adj[2][2] =  spvDet3x3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[3][0], m[3][1], "
			          "m[3][3]);");
			statement("adj[2][3] = -spvDet3x3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], "
			          "m[2][3]);");
			statement_no_indent("");
			statement("adj[3][0] = -spvDet3x3(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], "
			          "m[3][2]);");
			statement("adj[3][1] =  spvDet3x3(m[0][0], m[0][1], m[0][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], "
			          "m[3][2]);");
			statement("adj[3][2] = -spvDet3x3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[3][0], m[3][1], "
			          "m[3][2]);");
			statement("adj[3][3] =  spvDet3x3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], "
			          "m[2][2]);");
			statement_no_indent("");
			statement("// Calculate the determinant as a combination of the cofactors of the first row.");
			statement("float det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]) + (adj[0][2] * m[2][0]) + (adj[0][3] "
			          "* m[3][0]);");
			statement_no_indent("");
			statement("// Divide the classical adjoint matrix by the determinant.");
			statement("// If determinant is zero, matrix is not invertable, so leave it unchanged.");
			statement("return (det != 0.0f) ? (adj * (1.0f / det)) : m;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplInverse3x3:
			if (spv_function_implementations.count(SPVFuncImplInverse4x4) == 0)
			{
				statement("// Returns the determinant of a 2x2 matrix.");
				statement(force_inline);
				statement("float spvDet2x2(float a1, float a2, float b1, float b2)");
				begin_scope();
				statement("return a1 * b2 - b1 * a2;");
				end_scope();
				statement("");
			}

			statement("// Returns the inverse of a matrix, by using the algorithm of calculating the classical");
			statement("// adjoint and dividing by the determinant. The contents of the matrix are changed.");
			statement(force_inline);
			statement("float3x3 spvInverse3x3(float3x3 m)");
			begin_scope();
			statement("float3x3 adj;	// The adjoint matrix (inverse after dividing by determinant)");
			statement_no_indent("");
			statement("// Create the transpose of the cofactors, as the classical adjoint of the matrix.");
			statement("adj[0][0] =  spvDet2x2(m[1][1], m[1][2], m[2][1], m[2][2]);");
			statement("adj[0][1] = -spvDet2x2(m[0][1], m[0][2], m[2][1], m[2][2]);");
			statement("adj[0][2] =  spvDet2x2(m[0][1], m[0][2], m[1][1], m[1][2]);");
			statement_no_indent("");
			statement("adj[1][0] = -spvDet2x2(m[1][0], m[1][2], m[2][0], m[2][2]);");
			statement("adj[1][1] =  spvDet2x2(m[0][0], m[0][2], m[2][0], m[2][2]);");
			statement("adj[1][2] = -spvDet2x2(m[0][0], m[0][2], m[1][0], m[1][2]);");
			statement_no_indent("");
			statement("adj[2][0] =  spvDet2x2(m[1][0], m[1][1], m[2][0], m[2][1]);");
			statement("adj[2][1] = -spvDet2x2(m[0][0], m[0][1], m[2][0], m[2][1]);");
			statement("adj[2][2] =  spvDet2x2(m[0][0], m[0][1], m[1][0], m[1][1]);");
			statement_no_indent("");
			statement("// Calculate the determinant as a combination of the cofactors of the first row.");
			statement("float det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]) + (adj[0][2] * m[2][0]);");
			statement_no_indent("");
			statement("// Divide the classical adjoint matrix by the determinant.");
			statement("// If determinant is zero, matrix is not invertable, so leave it unchanged.");
			statement("return (det != 0.0f) ? (adj * (1.0f / det)) : m;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplInverse2x2:
			statement("// Returns the inverse of a matrix, by using the algorithm of calculating the classical");
			statement("// adjoint and dividing by the determinant. The contents of the matrix are changed.");
			statement(force_inline);
			statement("float2x2 spvInverse2x2(float2x2 m)");
			begin_scope();
			statement("float2x2 adj;	// The adjoint matrix (inverse after dividing by determinant)");
			statement_no_indent("");
			statement("// Create the transpose of the cofactors, as the classical adjoint of the matrix.");
			statement("adj[0][0] =  m[1][1];");
			statement("adj[0][1] = -m[0][1];");
			statement_no_indent("");
			statement("adj[1][0] = -m[1][0];");
			statement("adj[1][1] =  m[0][0];");
			statement_no_indent("");
			statement("// Calculate the determinant as a combination of the cofactors of the first row.");
			statement("float det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]);");
			statement_no_indent("");
			statement("// Divide the classical adjoint matrix by the determinant.");
			statement("// If determinant is zero, matrix is not invertable, so leave it unchanged.");
			statement("return (det != 0.0f) ? (adj * (1.0f / det)) : m;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplForwardArgs:
			statement("template<typename T> struct spvRemoveReference { typedef T type; };");
			statement("template<typename T> struct spvRemoveReference<thread T&> { typedef T type; };");
			statement("template<typename T> struct spvRemoveReference<thread T&&> { typedef T type; };");
			statement("template<typename T> inline constexpr thread T&& spvForward(thread typename "
			          "spvRemoveReference<T>::type& x)");
			begin_scope();
			statement("return static_cast<thread T&&>(x);");
			end_scope();
			statement("template<typename T> inline constexpr thread T&& spvForward(thread typename "
			          "spvRemoveReference<T>::type&& x)");
			begin_scope();
			statement("return static_cast<thread T&&>(x);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplGetSwizzle:
			statement("enum class spvSwizzle : uint");
			begin_scope();
			statement("none = 0,");
			statement("zero,");
			statement("one,");
			statement("red,");
			statement("green,");
			statement("blue,");
			statement("alpha");
			end_scope_decl();
			statement("");
			statement("template<typename T>");
			statement("inline T spvGetSwizzle(vec<T, 4> x, T c, spvSwizzle s)");
			begin_scope();
			statement("switch (s)");
			begin_scope();
			statement("case spvSwizzle::none:");
			statement("    return c;");
			statement("case spvSwizzle::zero:");
			statement("    return 0;");
			statement("case spvSwizzle::one:");
			statement("    return 1;");
			statement("case spvSwizzle::red:");
			statement("    return x.r;");
			statement("case spvSwizzle::green:");
			statement("    return x.g;");
			statement("case spvSwizzle::blue:");
			statement("    return x.b;");
			statement("case spvSwizzle::alpha:");
			statement("    return x.a;");
			end_scope();
			end_scope();
			statement("");
			break;

		case SPVFuncImplTextureSwizzle:
			statement("// Wrapper function that swizzles texture samples and fetches.");
			statement("template<typename T>");
			statement("inline vec<T, 4> spvTextureSwizzle(vec<T, 4> x, uint s)");
			begin_scope();
			statement("if (!s)");
			statement("    return x;");
			statement("return vec<T, 4>(spvGetSwizzle(x, x.r, spvSwizzle((s >> 0) & 0xFF)), "
			          "spvGetSwizzle(x, x.g, spvSwizzle((s >> 8) & 0xFF)), spvGetSwizzle(x, x.b, spvSwizzle((s >> 16) "
			          "& 0xFF)), "
			          "spvGetSwizzle(x, x.a, spvSwizzle((s >> 24) & 0xFF)));");
			end_scope();
			statement("");
			statement("template<typename T>");
			statement("inline T spvTextureSwizzle(T x, uint s)");
			begin_scope();
			statement("return spvTextureSwizzle(vec<T, 4>(x, 0, 0, 1), s).x;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplGatherSwizzle:
			statement("// Wrapper function that swizzles texture gathers.");
			statement("template<typename T, template<typename, access = access::sample, typename = void> class Tex, "
			          "typename... Ts>");
			statement("inline vec<T, 4> spvGatherSwizzle(const thread Tex<T>& t, sampler s, "
			          "uint sw, component c, Ts... params) METAL_CONST_ARG(c)");
			begin_scope();
			statement("if (sw)");
			begin_scope();
			statement("switch (spvSwizzle((sw >> (uint(c) * 8)) & 0xFF))");
			begin_scope();
			statement("case spvSwizzle::none:");
			statement("    break;");
			statement("case spvSwizzle::zero:");
			statement("    return vec<T, 4>(0, 0, 0, 0);");
			statement("case spvSwizzle::one:");
			statement("    return vec<T, 4>(1, 1, 1, 1);");
			statement("case spvSwizzle::red:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::x);");
			statement("case spvSwizzle::green:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::y);");
			statement("case spvSwizzle::blue:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::z);");
			statement("case spvSwizzle::alpha:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::w);");
			end_scope();
			end_scope();
			// texture::gather insists on its component parameter being a constant
			// expression, so we need this silly workaround just to compile the shader.
			statement("switch (c)");
			begin_scope();
			statement("case component::x:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::x);");
			statement("case component::y:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::y);");
			statement("case component::z:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::z);");
			statement("case component::w:");
			statement("    return t.gather(s, spvForward<Ts>(params)..., component::w);");
			end_scope();
			end_scope();
			statement("");
			break;

		case SPVFuncImplGatherCompareSwizzle:
			statement("// Wrapper function that swizzles depth texture gathers.");
			statement("template<typename T, template<typename, access = access::sample, typename = void> class Tex, "
			          "typename... Ts>");
			statement("inline vec<T, 4> spvGatherCompareSwizzle(const thread Tex<T>& t, sampler "
			          "s, uint sw, Ts... params) ");
			begin_scope();
			statement("if (sw)");
			begin_scope();
			statement("switch (spvSwizzle(sw & 0xFF))");
			begin_scope();
			statement("case spvSwizzle::none:");
			statement("case spvSwizzle::red:");
			statement("    break;");
			statement("case spvSwizzle::zero:");
			statement("case spvSwizzle::green:");
			statement("case spvSwizzle::blue:");
			statement("case spvSwizzle::alpha:");
			statement("    return vec<T, 4>(0, 0, 0, 0);");
			statement("case spvSwizzle::one:");
			statement("    return vec<T, 4>(1, 1, 1, 1);");
			end_scope();
			end_scope();
			statement("return t.gather_compare(s, spvForward<Ts>(params)...);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBroadcast:
			// Metal doesn't allow broadcasting boolean values directly, but we can work around that by broadcasting
			// them as integers.
			statement("template<typename T>");
			statement("inline T spvSubgroupBroadcast(T value, ushort lane)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_broadcast(value, lane);");
			else
				statement("return simd_broadcast(value, lane);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupBroadcast(bool value, ushort lane)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return !!quad_broadcast((ushort)value, lane);");
			else
				statement("return !!simd_broadcast((ushort)value, lane);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvSubgroupBroadcast(vec<bool, N> value, ushort lane)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return (vec<bool, N>)quad_broadcast((vec<ushort, N>)value, lane);");
			else
				statement("return (vec<bool, N>)simd_broadcast((vec<ushort, N>)value, lane);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBroadcastFirst:
			statement("template<typename T>");
			statement("inline T spvSubgroupBroadcastFirst(T value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_broadcast_first(value);");
			else
				statement("return simd_broadcast_first(value);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupBroadcastFirst(bool value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return !!quad_broadcast_first((ushort)value);");
			else
				statement("return !!simd_broadcast_first((ushort)value);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvSubgroupBroadcastFirst(vec<bool, N> value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return (vec<bool, N>)quad_broadcast_first((vec<ushort, N>)value);");
			else
				statement("return (vec<bool, N>)simd_broadcast_first((vec<ushort, N>)value);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBallot:
			statement("inline uint4 spvSubgroupBallot(bool value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
			{
				statement("return uint4((quad_vote::vote_t)quad_ballot(value), 0, 0, 0);");
			}
			else if (msl_options.is_ios())
			{
				// The current simd_vote on iOS uses a 32-bit integer-like object.
				statement("return uint4((simd_vote::vote_t)simd_ballot(value), 0, 0, 0);");
			}
			else
			{
				statement("simd_vote vote = simd_ballot(value);");
				statement("// simd_ballot() returns a 64-bit integer-like object, but");
				statement("// SPIR-V callers expect a uint4. We must convert.");
				statement("// FIXME: This won't include higher bits if Apple ever supports");
				statement("// 128 lanes in an SIMD-group.");
				statement("return uint4(as_type<uint2>((simd_vote::vote_t)vote), 0, 0);");
			}
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBallotBitExtract:
			statement("inline bool spvSubgroupBallotBitExtract(uint4 ballot, uint bit)");
			begin_scope();
			statement("return !!extract_bits(ballot[bit / 32], bit % 32, 1);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBallotFindLSB:
			statement("inline uint spvSubgroupBallotFindLSB(uint4 ballot, uint gl_SubgroupSize)");
			begin_scope();
			if (msl_options.is_ios())
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, gl_SubgroupSize), uint3(0));");
			}
			else
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupSize, 32u)), "
				          "extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupSize - 32, 0)), uint2(0));");
			}
			statement("ballot &= mask;");
			statement("return select(ctz(ballot.x), select(32 + ctz(ballot.y), select(64 + ctz(ballot.z), select(96 + "
			          "ctz(ballot.w), uint(-1), ballot.w == 0), ballot.z == 0), ballot.y == 0), ballot.x == 0);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBallotFindMSB:
			statement("inline uint spvSubgroupBallotFindMSB(uint4 ballot, uint gl_SubgroupSize)");
			begin_scope();
			if (msl_options.is_ios())
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, gl_SubgroupSize), uint3(0));");
			}
			else
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupSize, 32u)), "
				          "extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupSize - 32, 0)), uint2(0));");
			}
			statement("ballot &= mask;");
			statement("return select(128 - (clz(ballot.w) + 1), select(96 - (clz(ballot.z) + 1), select(64 - "
			          "(clz(ballot.y) + 1), select(32 - (clz(ballot.x) + 1), uint(-1), ballot.x == 0), ballot.y == 0), "
			          "ballot.z == 0), ballot.w == 0);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupBallotBitCount:
			statement("inline uint spvPopCount4(uint4 ballot)");
			begin_scope();
			statement("return popcount(ballot.x) + popcount(ballot.y) + popcount(ballot.z) + popcount(ballot.w);");
			end_scope();
			statement("");
			statement("inline uint spvSubgroupBallotBitCount(uint4 ballot, uint gl_SubgroupSize)");
			begin_scope();
			if (msl_options.is_ios())
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, gl_SubgroupSize), uint3(0));");
			}
			else
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupSize, 32u)), "
				          "extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupSize - 32, 0)), uint2(0));");
			}
			statement("return spvPopCount4(ballot & mask);");
			end_scope();
			statement("");
			statement("inline uint spvSubgroupBallotInclusiveBitCount(uint4 ballot, uint gl_SubgroupInvocationID)");
			begin_scope();
			if (msl_options.is_ios())
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, gl_SubgroupInvocationID + 1), uint3(0));");
			}
			else
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupInvocationID + 1, 32u)), "
				          "extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupInvocationID + 1 - 32, 0)), "
				          "uint2(0));");
			}
			statement("return spvPopCount4(ballot & mask);");
			end_scope();
			statement("");
			statement("inline uint spvSubgroupBallotExclusiveBitCount(uint4 ballot, uint gl_SubgroupInvocationID)");
			begin_scope();
			if (msl_options.is_ios())
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, gl_SubgroupInvocationID), uint2(0));");
			}
			else
			{
				statement("uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupInvocationID, 32u)), "
				          "extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupInvocationID - 32, 0)), uint2(0));");
			}
			statement("return spvPopCount4(ballot & mask);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupAllEqual:
			// Metal doesn't provide a function to evaluate this directly. But, we can
			// implement this by comparing every thread's value to one thread's value
			// (in this case, the value of the first active thread). Then, by the transitive
			// property of equality, if all comparisons return true, then they are all equal.
			statement("template<typename T>");
			statement("inline bool spvSubgroupAllEqual(T value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_all(all(value == quad_broadcast_first(value)));");
			else
				statement("return simd_all(all(value == simd_broadcast_first(value)));");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupAllEqual(bool value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_all(value) || !quad_any(value);");
			else
				statement("return simd_all(value) || !simd_any(value);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline bool spvSubgroupAllEqual(vec<bool, N> value)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_all(all(value == (vec<bool, N>)quad_broadcast_first((vec<ushort, N>)value)));");
			else
				statement("return simd_all(all(value == (vec<bool, N>)simd_broadcast_first((vec<ushort, N>)value)));");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupShuffle:
			statement("template<typename T>");
			statement("inline T spvSubgroupShuffle(T value, ushort lane)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_shuffle(value, lane);");
			else
				statement("return simd_shuffle(value, lane);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupShuffle(bool value, ushort lane)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return !!quad_shuffle((ushort)value, lane);");
			else
				statement("return !!simd_shuffle((ushort)value, lane);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvSubgroupShuffle(vec<bool, N> value, ushort lane)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return (vec<bool, N>)quad_shuffle((vec<ushort, N>)value, lane);");
			else
				statement("return (vec<bool, N>)simd_shuffle((vec<ushort, N>)value, lane);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupShuffleXor:
			statement("template<typename T>");
			statement("inline T spvSubgroupShuffleXor(T value, ushort mask)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_shuffle_xor(value, mask);");
			else
				statement("return simd_shuffle_xor(value, mask);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupShuffleXor(bool value, ushort mask)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return !!quad_shuffle_xor((ushort)value, mask);");
			else
				statement("return !!simd_shuffle_xor((ushort)value, mask);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvSubgroupShuffleXor(vec<bool, N> value, ushort mask)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return (vec<bool, N>)quad_shuffle_xor((vec<ushort, N>)value, mask);");
			else
				statement("return (vec<bool, N>)simd_shuffle_xor((vec<ushort, N>)value, mask);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupShuffleUp:
			statement("template<typename T>");
			statement("inline T spvSubgroupShuffleUp(T value, ushort delta)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_shuffle_up(value, delta);");
			else
				statement("return simd_shuffle_up(value, delta);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupShuffleUp(bool value, ushort delta)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return !!quad_shuffle_up((ushort)value, delta);");
			else
				statement("return !!simd_shuffle_up((ushort)value, delta);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvSubgroupShuffleUp(vec<bool, N> value, ushort delta)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return (vec<bool, N>)quad_shuffle_up((vec<ushort, N>)value, delta);");
			else
				statement("return (vec<bool, N>)simd_shuffle_up((vec<ushort, N>)value, delta);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplSubgroupShuffleDown:
			statement("template<typename T>");
			statement("inline T spvSubgroupShuffleDown(T value, ushort delta)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return quad_shuffle_down(value, delta);");
			else
				statement("return simd_shuffle_down(value, delta);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvSubgroupShuffleDown(bool value, ushort delta)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return !!quad_shuffle_down((ushort)value, delta);");
			else
				statement("return !!simd_shuffle_down((ushort)value, delta);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvSubgroupShuffleDown(vec<bool, N> value, ushort delta)");
			begin_scope();
			if (msl_options.use_quadgroup_operation())
				statement("return (vec<bool, N>)quad_shuffle_down((vec<ushort, N>)value, delta);");
			else
				statement("return (vec<bool, N>)simd_shuffle_down((vec<ushort, N>)value, delta);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplQuadBroadcast:
			statement("template<typename T>");
			statement("inline T spvQuadBroadcast(T value, uint lane)");
			begin_scope();
			statement("return quad_broadcast(value, lane);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvQuadBroadcast(bool value, uint lane)");
			begin_scope();
			statement("return !!quad_broadcast((ushort)value, lane);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvQuadBroadcast(vec<bool, N> value, uint lane)");
			begin_scope();
			statement("return (vec<bool, N>)quad_broadcast((vec<ushort, N>)value, lane);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplQuadSwap:
			// We can implement this easily based on the following table giving
			// the target lane ID from the direction and current lane ID:
			//        Direction
			//      | 0 | 1 | 2 |
			//   ---+---+---+---+
			// L 0  | 1   2   3
			// a 1  | 0   3   2
			// n 2  | 3   0   1
			// e 3  | 2   1   0
			// Notice that target = source ^ (direction + 1).
			statement("template<typename T>");
			statement("inline T spvQuadSwap(T value, uint dir)");
			begin_scope();
			statement("return quad_shuffle_xor(value, dir + 1);");
			end_scope();
			statement("");
			statement("template<>");
			statement("inline bool spvQuadSwap(bool value, uint dir)");
			begin_scope();
			statement("return !!quad_shuffle_xor((ushort)value, dir + 1);");
			end_scope();
			statement("");
			statement("template<uint N>");
			statement("inline vec<bool, N> spvQuadSwap(vec<bool, N> value, uint dir)");
			begin_scope();
			statement("return (vec<bool, N>)quad_shuffle_xor((vec<ushort, N>)value, dir + 1);");
			end_scope();
			statement("");
			break;

		case SPVFuncImplReflectScalar:
			// Metal does not support scalar versions of these functions.
			// Ensure fast-math is disabled to match Vulkan results.
			statement("template<typename T>");
			statement("[[clang::optnone]] T spvReflect(T i, T n)");
			begin_scope();
			statement("return i - T(2) * i * n * n;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplRefractScalar:
			// Metal does not support scalar versions of these functions.
			statement("template<typename T>");
			statement("inline T spvRefract(T i, T n, T eta)");
			begin_scope();
			statement("T NoI = n * i;");
			statement("T NoI2 = NoI * NoI;");
			statement("T k = T(1) - eta * eta * (T(1) - NoI2);");
			statement("if (k < T(0))");
			begin_scope();
			statement("return T(0);");
			end_scope();
			statement("else");
			begin_scope();
			statement("return eta * i - (eta * NoI + sqrt(k)) * n;");
			end_scope();
			end_scope();
			statement("");
			break;

		case SPVFuncImplFaceForwardScalar:
			// Metal does not support scalar versions of these functions.
			statement("template<typename T>");
			statement("inline T spvFaceForward(T n, T i, T nref)");
			begin_scope();
			statement("return i * nref < T(0) ? n : -n;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructNearest2Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructNearest(texture2d<T> plane0, texture2d<T> plane1, sampler "
			          "samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("ycbcr.br = plane1.sample(samp, coord, spvForward<LodOptions>(options)...).rg;");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructNearest3Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructNearest(texture2d<T> plane0, texture2d<T> plane1, "
			          "texture2d<T> plane2, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("ycbcr.b = plane1.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("ycbcr.r = plane2.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear422CositedEven2Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear422CositedEven(texture2d<T> plane0, texture2d<T> "
			          "plane1, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("if (fract(coord.x * plane1.get_width()) != 0.0)");
			begin_scope();
			statement("ycbcr.br = vec<T, 2>(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), 0.5).rg);");
			end_scope();
			statement("else");
			begin_scope();
			statement("ycbcr.br = plane1.sample(samp, coord, spvForward<LodOptions>(options)...).rg;");
			end_scope();
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear422CositedEven3Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear422CositedEven(texture2d<T> plane0, texture2d<T> "
			          "plane1, texture2d<T> plane2, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("if (fract(coord.x * plane1.get_width()) != 0.0)");
			begin_scope();
			statement("ycbcr.b = T(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), 0.5).r);");
			statement("ycbcr.r = T(mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), 0.5).r);");
			end_scope();
			statement("else");
			begin_scope();
			statement("ycbcr.b = plane1.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("ycbcr.r = plane2.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			end_scope();
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear422Midpoint2Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear422Midpoint(texture2d<T> plane0, texture2d<T> "
			          "plane1, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("int2 offs = int2(fract(coord.x * plane1.get_width()) != 0.0 ? 1 : -1, 0);");
			statement("ycbcr.br = vec<T, 2>(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., offs), 0.25).rg);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear422Midpoint3Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear422Midpoint(texture2d<T> plane0, texture2d<T> "
			          "plane1, texture2d<T> plane2, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("int2 offs = int2(fract(coord.x * plane1.get_width()) != 0.0 ? 1 : -1, 0);");
			statement("ycbcr.b = T(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., offs), 0.25).r);");
			statement("ycbcr.r = T(mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., offs), 0.25).r);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XCositedEvenYCositedEven2Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear420XCositedEvenYCositedEven(texture2d<T> plane0, "
			          "texture2d<T> plane1, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("float2 ab = fract(round(coord * float2(plane0.get_width(), plane0.get_height())) * 0.5);");
			statement("ycbcr.br = vec<T, 2>(mix(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).rg);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XCositedEvenYCositedEven3Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear420XCositedEvenYCositedEven(texture2d<T> plane0, "
			          "texture2d<T> plane1, texture2d<T> plane2, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("float2 ab = fract(round(coord * float2(plane0.get_width(), plane0.get_height())) * 0.5);");
			statement("ycbcr.b = T(mix(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).r);");
			statement("ycbcr.r = T(mix(mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).r);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XMidpointYCositedEven2Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear420XMidpointYCositedEven(texture2d<T> plane0, "
			          "texture2d<T> plane1, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("float2 ab = fract((round(coord * float2(plane0.get_width(), plane0.get_height())) - float2(0.5, "
			          "0)) * 0.5);");
			statement("ycbcr.br = vec<T, 2>(mix(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).rg);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XMidpointYCositedEven3Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear420XMidpointYCositedEven(texture2d<T> plane0, "
			          "texture2d<T> plane1, texture2d<T> plane2, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("float2 ab = fract((round(coord * float2(plane0.get_width(), plane0.get_height())) - float2(0.5, "
			          "0)) * 0.5);");
			statement("ycbcr.b = T(mix(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).r);");
			statement("ycbcr.r = T(mix(mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).r);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XCositedEvenYMidpoint2Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear420XCositedEvenYMidpoint(texture2d<T> plane0, "
			          "texture2d<T> plane1, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("float2 ab = fract((round(coord * float2(plane0.get_width(), plane0.get_height())) - float2(0, "
			          "0.5)) * 0.5);");
			statement("ycbcr.br = vec<T, 2>(mix(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).rg);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XCositedEvenYMidpoint3Plane:
			statement("template<typename T, typename... LodOptions>");
			statement("inline vec<T, 4> spvChromaReconstructLinear420XCositedEvenYMidpoint(texture2d<T> plane0, "
			          "texture2d<T> plane1, texture2d<T> plane2, sampler samp, float2 coord, LodOptions... options)");
			begin_scope();
			statement("vec<T, 4> ycbcr = vec<T, 4>(0, 0, 0, 1);");
			statement("ycbcr.g = plane0.sample(samp, coord, spvForward<LodOptions>(options)...).r;");
			statement("float2 ab = fract((round(coord * float2(plane0.get_width(), plane0.get_height())) - float2(0, "
			          "0.5)) * 0.5);");
			statement("ycbcr.b = T(mix(mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane1.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).r);");
			statement("ycbcr.r = T(mix(mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)...), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 0)), ab.x), "
			          "mix(plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(0, 1)), "
			          "plane2.sample(samp, coord, spvForward<LodOptions>(options)..., int2(1, 1)), ab.x), ab.y).r);");
			statement("return ycbcr;");
			end_scope();
			statement("");
			break;

		case SPVFuncImplChromaReconstructLinear420XMidpointYMidpoint2Plane:
			statement("template<typename T, typename... LodOptions>");
			statem                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        