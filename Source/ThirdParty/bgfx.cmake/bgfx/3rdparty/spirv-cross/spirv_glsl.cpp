/*
 * Copyright 2015-2021 Arm Limited
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

#include "spirv_glsl.hpp"
#include "GLSL.std.450.h"
#include "spirv_common.hpp"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <limits>
#include <locale.h>
#include <utility>
#include <array>

#ifndef _WIN32
#include <langinfo.h>
#endif
#include <locale.h>

using namespace spv;
using namespace SPIRV_CROSS_NAMESPACE;
using namespace std;

enum ExtraSubExpressionType
{
	// Create masks above any legal ID range to allow multiple address spaces into the extra_sub_expressions map.
	EXTRA_SUB_EXPRESSION_TYPE_STREAM_OFFSET = 0x10000000,
	EXTRA_SUB_EXPRESSION_TYPE_AUX = 0x20000000
};

static bool is_unsigned_opcode(Op op)
{
	// Don't have to be exhaustive, only relevant for legacy target checking ...
	switch (op)
	{
	case OpShiftRightLogical:
	case OpUGreaterThan:
	case OpUGreaterThanEqual:
	case OpULessThan:
	case OpULessThanEqual:
	case OpUConvert:
	case OpUDiv:
	case OpUMod:
	case OpUMulExtended:
	case OpConvertUToF:
	case OpConvertFToU:
		return true;

	default:
		return false;
	}
}

static bool is_unsigned_glsl_opcode(GLSLstd450 op)
{
	// Don't have to be exhaustive, only relevant for legacy target checking ...
	switch (op)
	{
	case GLSLstd450UClamp:
	case GLSLstd450UMin:
	case GLSLstd450UMax:
	case GLSLstd450FindUMsb:
		return true;

	default:
		return false;
	}
}

static bool packing_is_vec4_padded(BufferPackingStandard packing)
{
	switch (packing)
	{
	case BufferPackingHLSLCbuffer:
	case BufferPackingHLSLCbufferPackOffset:
	case BufferPackingStd140:
	case BufferPackingStd140EnhancedLayout:
		return true;

	default:
		return false;
	}
}

static bool packing_is_hlsl(BufferPackingStandard packing)
{
	switch (packing)
	{
	case BufferPackingHLSLCbuffer:
	case BufferPackingHLSLCbufferPackOffset:
		return true;

	default:
		return false;
	}
}

static bool packing_has_flexible_offset(BufferPackingStandard packing)
{
	switch (packing)
	{
	case BufferPackingStd140:
	case BufferPackingStd430:
	case BufferPackingScalar:
	case BufferPackingHLSLCbuffer:
		return false;

	default:
		return true;
	}
}

static bool packing_is_scalar(BufferPackingStandard packing)
{
	switch (packing)
	{
	case BufferPackingScalar:
	case BufferPackingScalarEnhancedLayout:
		return true;

	default:
		return false;
	}
}

static BufferPackingStandard packing_to_substruct_packing(BufferPackingStandard packing)
{
	switch (packing)
	{
	case BufferPackingStd140EnhancedLayout:
		return BufferPackingStd140;
	case BufferPackingStd430EnhancedLayout:
		return BufferPackingStd430;
	case BufferPackingHLSLCbufferPackOffset:
		return BufferPackingHLSLCbuffer;
	case BufferPackingScalarEnhancedLayout:
		return BufferPackingScalar;
	default:
		return packing;
	}
}

void CompilerGLSL::init()
{
	if (ir.source.known)
	{
		options.es = ir.source.es;
		options.version = ir.source.version;
	}

	// Query the locale to see what the decimal point is.
	// We'll rely on fixing it up ourselves in the rare case we have a comma-as-decimal locale
	// rather than setting locales ourselves. Settings locales in a safe and isolated way is rather
	// tricky.
#ifdef _WIN32
	// On Windows, localeconv uses thread-local storage, so it should be fine.
	const struct lconv *conv = localeconv();
	if (conv && conv->decimal_point)
		current_locale_radix_character = *conv->decimal_point;
#elif defined(__ANDROID__) && __ANDROID_API__ < 26
	// nl_langinfo is not supported on this platform, fall back to the worse alternative.
	const struct lconv *conv = localeconv();
	if (conv && conv->decimal_point)
		current_locale_radix_character = *conv->decimal_point;
#else
	// localeconv, the portable function is not MT safe ...
	const char *decimal_point = nl_langinfo(RADIXCHAR);
	if (decimal_point && *decimal_point != '\0')
		current_locale_radix_character = *decimal_point;
#endif
}

static const char *to_pls_layout(PlsFormat format)
{
	switch (format)
	{
	case PlsR11FG11FB10F:
		return "layout(r11f_g11f_b10f) ";
	case PlsR32F:
		return "layout(r32f) ";
	case PlsRG16F:
		return "layout(rg16f) ";
	case PlsRGB10A2:
		return "layout(rgb10_a2) ";
	case PlsRGBA8:
		return "layout(rgba8) ";
	case PlsRG16:
		return "layout(rg16) ";
	case PlsRGBA8I:
		return "layout(rgba8i)";
	case PlsRG16I:
		return "layout(rg16i) ";
	case PlsRGB10A2UI:
		return "layout(rgb10_a2ui) ";
	case PlsRGBA8UI:
		return "layout(rgba8ui) ";
	case PlsRG16UI:
		return "layout(rg16ui) ";
	case PlsR32UI:
		return "layout(r32ui) ";
	default:
		return "";
	}
}

static SPIRType::BaseType pls_format_to_basetype(PlsFormat format)
{
	switch (format)
	{
	default:
	case PlsR11FG11FB10F:
	case PlsR32F:
	case PlsRG16F:
	case PlsRGB10A2:
	case PlsRGBA8:
	case PlsRG16:
		return SPIRType::Float;

	case PlsRGBA8I:
	case PlsRG16I:
		return SPIRType::Int;

	case PlsRGB10A2UI:
	case PlsRGBA8UI:
	case PlsRG16UI:
	case PlsR32UI:
		return SPIRType::UInt;
	}
}

static uint32_t pls_format_to_components(PlsFormat format)
{
	switch (format)
	{
	default:
	case PlsR32F:
	case PlsR32UI:
		return 1;

	case PlsRG16F:
	case PlsRG16:
	case PlsRG16UI:
	case PlsRG16I:
		return 2;

	case PlsR11FG11FB10F:
		return 3;

	case PlsRGB10A2:
	case PlsRGBA8:
	case PlsRGBA8I:
	case PlsRGB10A2UI:
	case PlsRGBA8UI:
		return 4;
	}
}

const char *CompilerGLSL::vector_swizzle(int vecsize, int index)
{
	static const char *const swizzle[4][4] = {
		{ ".x", ".y", ".z", ".w" },
		{ ".xy", ".yz", ".zw", nullptr },
		{ ".xyz", ".yzw", nullptr, nullptr },
#if defined(__GNUC__) && (__GNUC__ == 9)
		// This works around a GCC 9 bug, see details in https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90947.
		// This array ends up being compiled as all nullptrs, tripping the assertions below.
		{ "", nullptr, nullptr, "$" },
#else
		{ "", nullptr, nullptr, nullptr },
#endif
	};

	assert(vecsize >= 1 && vecsize <= 4);
	assert(index >= 0 && index < 4);
	assert(swizzle[vecsize - 1][index]);

	return swizzle[vecsize - 1][index];
}

void CompilerGLSL::reset(uint32_t iteration_count)
{
	// Sanity check the iteration count to be robust against a certain class of bugs where
	// we keep forcing recompilations without making clear forward progress.
	// In buggy situations we will loop forever, or loop for an unbounded number of iterations.
	// Certain types of recompilations are considered to make forward progress,
	// but in almost all situations, we'll never see more than 3 iterations.
	// It is highly context-sensitive when we need to force recompilation,
	// and it is not practical with the current architecture
	// to resolve everything up front.
	if (iteration_count >= options.force_recompile_max_debug_iterations && !is_force_recompile_forward_progress)
		SPIRV_CROSS_THROW("Maximum compilation loops detected and no forward progress was made. Must be a SPIRV-Cross bug!");

	// We do some speculative optimizations which should pretty much always work out,
	// but just in case the SPIR-V is rather weird, recompile until it's happy.
	// This typically only means one extra pass.
	clear_force_recompile();

	// Clear invalid expression tracking.
	invalid_expressions.clear();
	composite_insert_overwritten.clear();
	current_function = nullptr;

	// Clear temporary usage tracking.
	expression_usage_counts.clear();
	forwarded_temporaries.clear();
	suppressed_usage_tracking.clear();

	// Ensure that we declare phi-variable copies even if the original declaration isn't deferred
	flushed_phi_variables.clear();

	current_emitting_switch_stack.clear();

	reset_name_caches();

	ir.for_each_typed_id<SPIRFunction>([&](uint32_t, SPIRFunction &func) {
		func.active = false;
		func.flush_undeclared = true;
	});

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) { var.dependees.clear(); });

	ir.reset_all_of_type<SPIRExpression>();
	ir.reset_all_of_type<SPIRAccessChain>();

	statement_count = 0;
	indent = 0;
	current_loop_level = 0;
}

void CompilerGLSL::remap_pls_variables()
{
	for (auto &input : pls_inputs)
	{
		auto &var = get<SPIRVariable>(input.id);

		bool input_is_target = false;
		if (var.storage == StorageClassUniformConstant)
		{
			auto &type = get<SPIRType>(var.basetype);
			input_is_target = type.image.dim == DimSubpassData;
		}

		if (var.storage != StorageClassInput && !input_is_target)
			SPIRV_CROSS_THROW("Can only use in and target variables for PLS inputs.");
		var.remapped_variable = true;
	}

	for (auto &output : pls_outputs)
	{
		auto &var = get<SPIRVariable>(output.id);
		if (var.storage != StorageClassOutput)
			SPIRV_CROSS_THROW("Can only use out variables for PLS outputs.");
		var.remapped_variable = true;
	}
}

void CompilerGLSL::remap_ext_framebuffer_fetch(uint32_t input_attachment_index, uint32_t color_location, bool coherent)
{
	subpass_to_framebuffer_fetch_attachment.push_back({ input_attachment_index, color_location });
	inout_color_attachments.push_back({ color_location, coherent });
}

bool CompilerGLSL::location_is_framebuffer_fetch(uint32_t location) const
{
	return std::find_if(begin(inout_color_attachments), end(inout_color_attachments),
	                    [&](const std::pair<uint32_t, bool> &elem) {
		                    return elem.first == location;
	                    }) != end(inout_color_attachments);
}

bool CompilerGLSL::location_is_non_coherent_framebuffer_fetch(uint32_t location) const
{
	return std::find_if(begin(inout_color_attachments), end(inout_color_attachments),
	                    [&](const std::pair<uint32_t, bool> &elem) {
		                    return elem.first == location && !elem.second;
	                    }) != end(inout_color_attachments);
}

void CompilerGLSL::find_static_extensions()
{
	ir.for_each_typed_id<SPIRType>([&](uint32_t, const SPIRType &type) {
		if (type.basetype == SPIRType::Double)
		{
			if (options.es)
				SPIRV_CROSS_THROW("FP64 not supported in ES profile.");
			if (!options.es && options.version < 400)
				require_extension_internal("GL_ARB_gpu_shader_fp64");
		}
		else if (type.basetype == SPIRType::Int64 || type.basetype == SPIRType::UInt64)
		{
			if (options.es && options.version < 310) // GL_NV_gpu_shader5 fallback requires 310.
				SPIRV_CROSS_THROW("64-bit integers not supported in ES profile before version 310.");
			require_extension_internal("GL_ARB_gpu_shader_int64");
		}
		else if (type.basetype == SPIRType::Half)
		{
			require_extension_internal("GL_EXT_shader_explicit_arithmetic_types_float16");
			if (options.vulkan_semantics)
				require_extension_internal("GL_EXT_shader_16bit_storage");
		}
		else if (type.basetype == SPIRType::SByte || type.basetype == SPIRType::UByte)
		{
			require_extension_internal("GL_EXT_shader_explicit_arithmetic_types_int8");
			if (options.vulkan_semantics)
				require_extension_internal("GL_EXT_shader_8bit_storage");
		}
		else if (type.basetype == SPIRType::Short || type.basetype == SPIRType::UShort)
		{
			require_extension_internal("GL_EXT_shader_explicit_arithmetic_types_int16");
			if (options.vulkan_semantics)
				require_extension_internal("GL_EXT_shader_16bit_storage");
		}
	});

	auto &execution = get_entry_point();
	switch (execution.model)
	{
	case ExecutionModelGLCompute:
		if (!options.es && options.version < 430)
			require_extension_internal("GL_ARB_compute_shader");
		if (options.es && options.version < 310)
			SPIRV_CROSS_THROW("At least ESSL 3.10 required for compute shaders.");
		break;

	case ExecutionModelGeometry:
		if (options.es && options.version < 320)
			require_extension_internal("GL_EXT_geometry_shader");
		if (!options.es && options.version < 150)
			require_extension_internal("GL_ARB_geometry_shader4");

		if (execution.flags.get(ExecutionModeInvocations) && execution.invocations != 1)
		{
			// Instanced GS is part of 400 core or this extension.
			if (!options.es && options.version < 400)
				require_extension_internal("GL_ARB_gpu_shader5");
		}
		break;

	case ExecutionModelTessellationEvaluation:
	case ExecutionModelTessellationControl:
		if (options.es && options.version < 320)
			require_extension_internal("GL_EXT_tessellation_shader");
		if (!options.es && options.version < 400)
			require_extension_internal("GL_ARB_tessellation_shader");
		break;

	case ExecutionModelRayGenerationKHR:
	case ExecutionModelIntersectionKHR:
	case ExecutionModelAnyHitKHR:
	case ExecutionModelClosestHitKHR:
	case ExecutionModelMissKHR:
	case ExecutionModelCallableKHR:
		// NV enums are aliases.
		if (options.es || options.version < 460)
			SPIRV_CROSS_THROW("Ray tracing shaders require non-es profile with version 460 or above.");
		if (!options.vulkan_semantics)
			SPIRV_CROSS_THROW("Ray tracing requires Vulkan semantics.");

		// Need to figure out if we should target KHR or NV extension based on capabilities.
		for (auto &cap : ir.declared_capabilities)
		{
			if (cap == CapabilityRayTracingKHR || cap == CapabilityRayQueryKHR ||
			    cap == CapabilityRayTraversalPrimitiveCullingKHR)
			{
				ray_tracing_is_khr = true;
				break;
			}
		}

		if (ray_tracing_is_khr)
		{
			// In KHR ray tracing we pass payloads by pointer instead of location,
			// so make sure we assign locations properly.
			ray_tracing_khr_fixup_locations();
			require_extension_internal("GL_EXT_ray_tracing");
		}
		else
			require_extension_internal("GL_NV_ray_tracing");
		break;

	case ExecutionModelMeshEXT:
	case ExecutionModelTaskEXT:
		if (options.es || options.version < 450)
			SPIRV_CROSS_THROW("Mesh shaders require GLSL 450 or above.");
		if (!options.vulkan_semantics)
			SPIRV_CROSS_THROW("Mesh shaders require Vulkan semantics.");
		require_extension_internal("GL_EXT_mesh_shader");
		break;

	default:
		break;
	}

	if (!pls_inputs.empty() || !pls_outputs.empty())
	{
		if (execution.model != ExecutionModelFragment)
			SPIRV_CROSS_THROW("Can only use GL_EXT_shader_pixel_local_storage in fragment shaders.");
		require_extension_internal("GL_EXT_shader_pixel_local_storage");
	}

	if (!inout_color_attachments.empty())
	{
		if (execution.model != ExecutionModelFragment)
			SPIRV_CROSS_THROW("Can only use GL_EXT_shader_framebuffer_fetch in fragment shaders.");
		if (options.vulkan_semantics)
			SPIRV_CROSS_THROW("Cannot use EXT_shader_framebuffer_fetch in Vulkan GLSL.");

		bool has_coherent = false;
		bool has_incoherent = false;

		for (auto &att : inout_color_attachments)
		{
			if (att.second)
				has_coherent = true;
			else
				has_incoherent = true;
		}

		if (has_coherent)
			require_extension_internal("GL_EXT_shader_framebuffer_fetch");
		if (has_incoherent)
			require_extension_internal("GL_EXT_shader_framebuffer_fetch_non_coherent");
	}

	if (options.separate_shader_objects && !options.es && options.version < 410)
		require_extension_internal("GL_ARB_separate_shader_objects");

	if (ir.addressing_model == AddressingModelPhysicalStorageBuffer64EXT)
	{
		if (!options.vulkan_semantics)
			SPIRV_CROSS_THROW("GL_EXT_buffer_reference is only supported in Vulkan GLSL.");
		if (options.es && options.version < 320)
			SPIRV_CROSS_THROW("GL_EXT_buffer_reference requires ESSL 320.");
		else if (!options.es && options.version < 450)
			SPIRV_CROSS_THROW("GL_EXT_buffer_reference requires GLSL 450.");
		require_extension_internal("GL_EXT_buffer_reference");
	}
	else if (ir.addressing_model != AddressingModelLogical)
	{
		SPIRV_CROSS_THROW("Only Logical and PhysicalStorageBuffer64EXT addressing models are supported.");
	}

	// Check for nonuniform qualifier and passthrough.
	// Instead of looping over all decorations to find this, just look at capabilities.
	for (auto &cap : ir.declared_capabilities)
	{
		switch (cap)
		{
		case CapabilityShaderNonUniformEXT:
			if (!options.vulkan_semantics)
				require_extension_internal("GL_NV_gpu_shader5");
			else
				require_extension_internal("GL_EXT_nonuniform_qualifier");
			break;
		case CapabilityRuntimeDescriptorArrayEXT:
			if (!options.vulkan_semantics)
				SPIRV_CROSS_THROW("GL_EXT_nonuniform_qualifier is only supported in Vulkan GLSL.");
			require_extension_internal("GL_EXT_nonuniform_qualifier");
			break;

		case CapabilityGeometryShaderPassthroughNV:
			if (execution.model == ExecutionModelGeometry)
			{
				require_extension_internal("GL_NV_geometry_shader_passthrough");
				execution.geometry_passthrough = true;
			}
			break;

		case CapabilityVariablePointers:
		case CapabilityVariablePointersStorageBuffer:
			SPIRV_CROSS_THROW("VariablePointers capability is not supported in GLSL.");

		case CapabilityMultiView:
			if (options.vulkan_semantics)
				require_extension_internal("GL_EXT_multiview");
			else
			{
				require_extension_internal("GL_OVR_multiview2");
				if (options.ovr_multiview_view_count == 0)
					SPIRV_CROSS_THROW("ovr_multiview_view_count must be non-zero when using GL_OVR_multiview2.");
				if (get_execution_model() != ExecutionModelVertex)
					SPIRV_CROSS_THROW("OVR_multiview2 can only be used with Vertex shaders.");
			}
			break;

		case CapabilityRayQueryKHR:
			if (options.es || options.version < 460 || !options.vulkan_semantics)
				SPIRV_CROSS_THROW("RayQuery requires Vulkan GLSL 460.");
			require_extension_internal("GL_EXT_ray_query");
			ray_tracing_is_khr = true;
			break;

		case CapabilityRayTraversalPrimitiveCullingKHR:
			if (options.es || options.version < 460 || !options.vulkan_semantics)
				SPIRV_CROSS_THROW("RayQuery requires Vulkan GLSL 460.");
			require_extension_internal("GL_EXT_ray_flags_primitive_culling");
			ray_tracing_is_khr = true;
			break;

		default:
			break;
		}
	}

	if (options.ovr_multiview_view_count)
	{
		if (options.vulkan_semantics)
			SPIRV_CROSS_THROW("OVR_multiview2 cannot be used with Vulkan semantics.");
		if (get_execution_model() != ExecutionModelVertex)
			SPIRV_CROSS_THROW("OVR_multiview2 can only be used with Vertex shaders.");
		require_extension_internal("GL_OVR_multiview2");
	}

	// KHR one is likely to get promoted at some point, so if we don't see an explicit SPIR-V extension, assume KHR.
	for (auto &ext : ir.declared_extensions)
		if (ext == "SPV_NV_fragment_shader_barycentric")
			barycentric_is_nv = true;
}

void CompilerGLSL::require_polyfill(Polyfill polyfill, bool relaxed)
{
	uint32_t &polyfills = (relaxed && options.es) ? required_polyfills_relaxed : required_polyfills;

	if ((polyfills & polyfill) == 0)
	{
		polyfills |= polyfill;
		force_recompile();
	}
}

void CompilerGLSL::ray_tracing_khr_fixup_locations()
{
	uint32_t location = 0;
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		// Incoming payload storage can also be used for tracing.
		if (var.storage != StorageClassRayPayloadKHR && var.storage != StorageClassCallableDataKHR &&
		    var.storage != StorageClassIncomingRayPayloadKHR && var.storage != StorageClassIncomingCallableDataKHR)
			return;
		if (is_hidden_variable(var))
			return;
		set_decoration(var.self, DecorationLocation, location++);
	});
}

string CompilerGLSL::compile()
{
	ir.fixup_reserved_names();

	if (!options.vulkan_semantics)
	{
		// only NV_gpu_shader5 supports divergent indexing on OpenGL, and it does so without extra qualifiers
		backend.nonuniform_qualifier = "";
		backend.needs_row_major_load_workaround = options.enable_row_major_load_workaround;
	}
	backend.allow_precision_qualifiers = options.vulkan_semantics || options.es;
	backend.force_gl_in_out_block = true;
	backend.supports_extensions = true;
	backend.use_array_constructor = true;
	backend.workgroup_size_is_hidden = true;
	backend.requires_relaxed_precision_analysis = options.es || options.vulkan_semantics;
	backend.support_precise_qualifier =
			(!options.es && options.version >= 400) || (options.es && options.version >= 320);

	if (is_legacy_es())
		backend.support_case_fallthrough = false;

	// Scan the SPIR-V to find trivial uses of extensions.
	fixup_anonymous_struct_names();
	fixup_type_alias();
	reorder_type_alias();
	build_function_control_flow_graphs_and_analyze();
	find_static_extensions();
	fixup_image_load_store_access();
	update_active_builtins();
	analyze_image_and_sampler_usage();
	analyze_interlocked_resource_usage();
	if (!inout_color_attachments.empty())
		emit_inout_fragment_outputs_copy_to_subpass_inputs();

	// Shaders might cast unrelated data to pointers of non-block types.
	// Find all such instances and make sure we can cast the pointers to a synthesized block type.
	if (ir.addressing_model == AddressingModelPhysicalStorageBuffer64EXT)
		analyze_non_block_pointer_types();

	uint32_t pass_count = 0;
	do
	{
		reset(pass_count);

		buffer.reset();

		emit_header();
		emit_resources();
		emit_extension_workarounds(get_execution_model());

		if (required_polyfills != 0)
			emit_polyfills(required_polyfills, false);
		if (options.es && required_polyfills_relaxed != 0)
			emit_polyfills(required_polyfills_relaxed, true);

		emit_function(get<SPIRFunction>(ir.default_entry_point), Bitset());

		pass_count++;
	} while (is_forcing_recompilation());

	// Implement the interlocked wrapper function at the end.
	// The body was implemented in lieu of main().
	if (interlocked_is_complex)
	{
		statement("void main()");
		begin_scope();
		statement("// Interlocks were used in a way not compatible with GLSL, this is very slow.");
		statement("SPIRV_Cross_beginInvocationInterlock();");
		statement("spvMainInterlockedBody();");
		statement("SPIRV_Cross_endInvocationInterlock();");
		end_scope();
	}

	// Entry point in GLSL is always main().
	get_entry_point().name = "main";

	return buffer.str();
}

std::string CompilerGLSL::get_partial_source()
{
	return buffer.str();
}

void CompilerGLSL::build_workgroup_size(SmallVector<string> &arguments, const SpecializationConstant &wg_x,
                                        const SpecializationConstant &wg_y, const SpecializationConstant &wg_z)
{
	auto &execution = get_entry_point();
	bool builtin_workgroup = execution.workgroup_size.constant != 0;
	bool use_local_size_id = !builtin_workgroup && execution.flags.get(ExecutionModeLocalSizeId);

	if (wg_x.id)
	{
		if (options.vulkan_semantics)
			arguments.push_back(join("local_size_x_id = ", wg_x.constant_id));
		else
			arguments.push_back(join("local_size_x = ", get<SPIRConstant>(wg_x.id).specialization_constant_macro_name));
	}
	else if (use_local_size_id && execution.workgroup_size.id_x)
		arguments.push_back(join("local_size_x = ", get<SPIRConstant>(execution.workgroup_size.id_x).scalar()));
	else
		arguments.push_back(join("local_size_x = ", execution.workgroup_size.x));

	if (wg_y.id)
	{
		if (options.vulkan_semantics)
			arguments.push_back(join("local_size_y_id = ", wg_y.constant_id));
		else
			arguments.push_back(join("local_size_y = ", get<SPIRConstant>(wg_y.id).specialization_constant_macro_name));
	}
	else if (use_local_size_id && execution.workgroup_size.id_y)
		arguments.push_back(join("local_size_y = ", get<SPIRConstant>(execution.workgroup_size.id_y).scalar()));
	else
		arguments.push_back(join("local_size_y = ", execution.workgroup_size.y));

	if (wg_z.id)
	{
		if (options.vulkan_semantics)
			arguments.push_back(join("local_size_z_id = ", wg_z.constant_id));
		else
			arguments.push_back(join("local_size_z = ", get<SPIRConstant>(wg_z.id).specialization_constant_macro_name));
	}
	else if (use_local_size_id && execution.workgroup_size.id_z)
		arguments.push_back(join("local_size_z = ", get<SPIRConstant>(execution.workgroup_size.id_z).scalar()));
	else
		arguments.push_back(join("local_size_z = ", execution.workgroup_size.z));
}

void CompilerGLSL::request_subgroup_feature(ShaderSubgroupSupportHelper::Feature feature)
{
	if (options.vulkan_semantics)
	{
		auto khr_extension = ShaderSubgroupSupportHelper::get_KHR_extension_for_feature(feature);
		require_extension_internal(ShaderSubgroupSupportHelper::get_extension_name(khr_extension));
	}
	else
	{
		if (!shader_subgroup_supporter.is_feature_requested(feature))
			force_recompile();
		shader_subgroup_supporter.request_feature(feature);
	}
}

void CompilerGLSL::emit_header()
{
	auto &execution = get_entry_point();
	statement("#version ", options.version, options.es && options.version > 100 ? " es" : "");

	if (!options.es && options.version < 420)
	{
		// Needed for binding = # on UBOs, etc.
		if (options.enable_420pack_extension)
		{
			statement("#ifdef GL_ARB_shading_language_420pack");
			statement("#extension GL_ARB_shading_language_420pack : require");
			statement("#endif");
		}
		// Needed for: layout(early_fragment_tests) in;
		if (execution.flags.get(ExecutionModeEarlyFragmentTests))
			require_extension_internal("GL_ARB_shader_image_load_store");
	}

	// Needed for: layout(post_depth_coverage) in;
	if (execution.flags.get(ExecutionModePostDepthCoverage))
		require_extension_internal("GL_ARB_post_depth_coverage");

	// Needed for: layout({pixel,sample}_interlock_[un]ordered) in;
	bool interlock_used = execution.flags.get(ExecutionModePixelInterlockOrderedEXT) ||
	                      execution.flags.get(ExecutionModePixelInterlockUnorderedEXT) ||
	                      execution.flags.get(ExecutionModeSampleInterlockOrderedEXT) ||
	                      execution.flags.get(ExecutionModeSampleInterlockUnorderedEXT);

	if (interlock_used)
	{
		if (options.es)
		{
			if (options.version < 310)
				SPIRV_CROSS_THROW("At least ESSL 3.10 required for fragment shader interlock.");
			require_extension_internal("GL_NV_fragment_shader_interlock");
		}
		else
		{
			if (options.version < 420)
				require_extension_internal("GL_ARB_shader_image_load_store");
			require_extension_internal("GL_ARB_fragment_shader_interlock");
		}
	}

	for (auto &ext : forced_extensions)
	{
		if (ext == "GL_ARB_gpu_shader_int64")
		{
			statement("#if defined(GL_ARB_gpu_shader_int64)");
			statement("#extension GL_ARB_gpu_shader_int64 : require");
			if (!options.vulkan_semantics || options.es)
			{
				statement("#elif defined(GL_NV_gpu_shader5)");
				statement("#extension GL_NV_gpu_shader5 : require");
			}
			statement("#else");
			statement("#error No extension available for 64-bit integers.");
			statement("#endif");
		}
		else if (ext == "GL_EXT_shader_explicit_arithmetic_types_float16")
		{
			// Special case, this extension has a potential fallback to another vendor extension in normal GLSL.
			// GL_AMD_gpu_shader_half_float is a superset, so try that first.
			statement("#if defined(GL_AMD_gpu_shader_half_float)");
			statement("#extension GL_AMD_gpu_shader_half_float : require");
			if (!options.vulkan_semantics)
			{
				statement("#elif defined(GL_NV_gpu_shader5)");
				statement("#extension GL_NV_gpu_shader5 : require");
			}
			else
			{
				statement("#elif defined(GL_EXT_shader_explicit_arithmetic_types_float16)");
				statement("#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require");
			}
			statement("#else");
			statement("#error No extension available for FP16.");
			statement("#endif");
		}
		else if (ext == "GL_EXT_shader_explicit_arithmetic_types_int8")
		{
			if (options.vulkan_semantics)
				statement("#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require");
			else
			{
				statement("#if defined(GL_EXT_shader_explicit_arithmetic_types_int8)");
				statement("#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require");
				statement("#elif defined(GL_NV_gpu_shader5)");
				statement("#extension GL_NV_gpu_shader5 : require");
				statement("#else");
				statement("#error No extension available for Int8.");
				statement("#endif");
			}
		}
		else if (ext == "GL_EXT_shader_explicit_arithmetic_types_int16")
		{
			if (options.vulkan_semantics)
				statement("#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require");
			else
			{
				statement("#if defined(GL_EXT_shader_explicit_arithmetic_types_int16)");
				statement("#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require");
				statement("#elif defined(GL_AMD_gpu_shader_int16)");
				statement("#extension GL_AMD_gpu_shader_int16 : require");
				statement("#elif defined(GL_NV_gpu_shader5)");
				statement("#extension GL_NV_gpu_shader5 : require");
				statement("#else");
				statement("#error No extension available for Int16.");
				statement("#endif");
			}
		}
		else if (ext == "GL_ARB_post_depth_coverage")
		{
			if (options.es)
				statement("#extension GL_EXT_post_depth_coverage : require");
			else
			{
				statement("#if defined(GL_ARB_post_depth_coverge)");
				statement("#extension GL_ARB_post_depth_coverage : require");
				statement("#else");
				statement("#extension GL_EXT_post_depth_coverage : require");
				statement("#endif");
			}
		}
		else if (!options.vulkan_semantics && ext == "GL_ARB_shader_draw_parameters")
		{
			// Soft-enable this extension on plain GLSL.
			statement("#ifdef ", ext);
			statement("#extension ", ext, " : enable");
			statement("#endif");
		}
		else if (ext == "GL_EXT_control_flow_attributes")
		{
			// These are just hints so we can conditionally enable and fallback in the shader.
			statement("#if defined(GL_EXT_control_flow_attributes)");
			statement("#extension GL_EXT_control_flow_attributes : require");
			statement("#define SPIRV_CROSS_FLATTEN [[flatten]]");
			statement("#define SPIRV_CROSS_BRANCH [[dont_flatten]]");
			statement("#define SPIRV_CROSS_UNROLL [[unroll]]");
			statement("#define SPIRV_CROSS_LOOP [[dont_unroll]]");
			statement("#else");
			statement("#define SPIRV_CROSS_FLATTEN");
			statement("#define SPIRV_CROSS_BRANCH");
			statement("#define SPIRV_CROSS_UNROLL");
			statement("#define SPIRV_CROSS_LOOP");
			statement("#endif");
		}
		else if (ext == "GL_NV_fragment_shader_interlock")
		{
			statement("#extension GL_NV_fragment_shader_interlock : require");
			statement("#define SPIRV_Cross_beginInvocationInterlock() beginInvocationInterlockNV()");
			statement("#define SPIRV_Cross_endInvocationInterlock() endInvocationInterlockNV()");
		}
		else if (ext == "GL_ARB_fragment_shader_interlock")
		{
			statement("#ifdef GL_ARB_fragment_shader_interlock");
			statement("#extension GL_ARB_fragment_shader_interlock : enable");
			statement("#define SPIRV_Cross_beginInvocationInterlock() beginInvocationInterlockARB()");
			statement("#define SPIRV_Cross_endInvocationInterlock() endInvocationInterlockARB()");
			statement("#elif defined(GL_INTEL_fragment_shader_ordering)");
			statement("#extension GL_INTEL_fragment_shader_ordering : enable");
			statement("#define SPIRV_Cross_beginInvocationInterlock() beginFragmentShaderOrderingINTEL()");
			statement("#define SPIRV_Cross_endInvocationInterlock()");
			statement("#endif");
		}
		else
			statement("#extension ", ext, " : require");
	}

	if (!options.vulkan_semantics)
	{
		using Supp = ShaderSubgroupSupportHelper;
		auto result = shader_subgroup_supporter.resolve();

		for (uint32_t feature_index = 0; feature_index < Supp::FeatureCount; feature_index++)
		{
			auto feature = static_cast<Supp::Feature>(feature_index);
			if (!shader_subgroup_supporter.is_feature_requested(feature))
				continue;

			auto exts = Supp::get_candidates_for_feature(feature, result);
			if (exts.empty())
				continue;

			statement("");

			for (auto &ext : exts)
			{
				const char *name = Supp::get_extension_name(ext);
				const char *extra_predicate = Supp::get_extra_required_extension_predicate(ext);
				auto extra_names = Supp::get_extra_required_extension_names(ext);
				statement(&ext != &exts.front() ? "#elif" : "#if", " defined(", name, ")",
				          (*extra_predicate != '\0' ? " && " : ""), extra_predicate);
				for (const auto &e : extra_names)
					statement("#extension ", e, " : enable");
				statement("#extension ", name, " : require");
			}

			if (!Supp::can_feature_be_implemented_without_extensions(feature))
			{
				statement("#else");
				statement("#error No extensions available to emulate requested subgroup feature.");
			}

			statement("#endif");
		}
	}

	for (auto &header : header_lines)
		statement(header);

	SmallVector<string> inputs;
	SmallVector<string> outputs;

	switch (execution.model)
	{
	case ExecutionModelVertex:
		if (options.ovr_multiview_view_count)
			inputs.push_back(join("num_views = ", options.ovr_multiview_view_count));
		break;
	case ExecutionModelGeometry:
		if ((execution.flags.get(ExecutionModeInvocations)) && execution.invocations != 1)
			inputs.push_back(join("invocations = ", execution.invocations));
		if (execution.flags.get(ExecutionModeInputPoints))
			inputs.push_back("points");
		if (execution.flags.get(ExecutionModeInputLines))
			inputs.push_back("lines");
		if (execution.flags.get(ExecutionModeInputLinesAdjacency))
			inputs.push_back("lines_adjacency");
		if (execution.flags.get(ExecutionModeTriangles))
			inputs.push_back("triangles");
		if (execution.flags.get(ExecutionModeInputTrianglesAdjacency))
			inputs.push_back("triangles_adjacency");

		if (!execution.geometry_passthrough)
		{
			// For passthrough, these are implies and cannot be declared in shader.
			outputs.push_back(join("max_vertices = ", execution.output_vertices));
			if (execution.flags.get(ExecutionModeOutputTriangleStrip))
				outputs.push_back("triangle_strip");
			if (execution.flags.get(ExecutionModeOutputPoints))
				outputs.push_back("points");
			if (execution.flags.get(ExecutionModeOutputLineStrip))
				outputs.push_back("line_strip");
		}
		break;

	case ExecutionModelTessellationControl:
		if (execution.flags.get(ExecutionModeOutputVertices))
			outputs.push_back(join("vertices = ", execution.output_vertices));
		break;

	case ExecutionModelTessellationEvaluation:
		if (execution.flags.get(ExecutionModeQuads))
			inputs.push_back("quads");
		if (execution.flags.get(ExecutionModeTriangles))
			inputs.push_back("triangles");
		if (execution.flags.get(ExecutionModeIsolines))
			inputs.push_back("isolines");
		if (execution.flags.get(ExecutionModePointMode))
			inputs.push_back("point_mode");

		if (!execution.flags.get(ExecutionModeIsolines))
		{
			if (execution.flags.get(ExecutionModeVertexOrderCw))
				inputs.push_back("cw");
			if (execution.flags.get(ExecutionModeVertexOrderCcw))
				inputs.push_back("ccw");
		}

		if (execution.flags.get(ExecutionModeSpacingFractionalEven))
			inputs.push_back("fractional_even_spacing");
		if (execution.flags.get(ExecutionModeSpacingFractionalOdd))
			inputs.push_back("fractional_odd_spacing");
		if (execution.flags.get(ExecutionModeSpacingEqual))
			inputs.push_back("equal_spacing");
		break;

	case ExecutionModelGLCompute:
	case ExecutionModelTaskEXT:
	case ExecutionModelMeshEXT:
	{
		if (execution.workgroup_size.constant != 0 || execution.flags.get(ExecutionModeLocalSizeId))
		{
			SpecializationConstant wg_x, wg_y, wg_z;
			get_work_group_size_specialization_constants(wg_x, wg_y, wg_z);

			// If there are any spec constants on legacy GLSL, defer declaration, we need to set up macro
			// declarations before we can emit the work group size.
			if (options.vulkan_semantics ||
			    ((wg_x.id == ConstantID(0)) && (wg_y.id == ConstantID(0)) && (wg_z.id == ConstantID(0))))
				build_workgroup_size(inputs, wg_x, wg_y, wg_z);
		}
		else
		{
			inputs.push_back(join("local_size_x = ", execution.workgroup_size.x));
			inputs.push_back(join("local_size_y = ", execution.workgroup_size.y));
			inputs.push_back(join("local_size_z = ", execution.workgroup_size.z));
		}

		if (execution.model == ExecutionModelMeshEXT)
		{
			outputs.push_back(join("max_vertices = ", execution.output_vertices));
			outputs.push_back(join("max_primitives = ", execution.output_primitives));
			if (execution.flags.get(ExecutionModeOutputTrianglesEXT))
				outputs.push_back("triangles");
			else if (execution.flags.get(ExecutionModeOutputLinesEXT))
				outputs.push_back("lines");
			else if (execution.flags.get(ExecutionModeOutputPoints))
				outputs.push_back("points");
		}
		break;
	}

	case ExecutionModelFragment:
		if (options.es)
		{
			switch (options.fragment.default_float_precision)
			{
			case Options::Lowp:
				statement("precision lowp float;");
				break;

			case Options::Mediump:
				statement("precision mediump float;");
				break;

			case Options::Highp:
				statement("precision highp float;");
				break;

			default:
				break;
			}

			switch (options.fragment.default_int_precision)
			{
			case Options::Lowp:
				statement("precision lowp int;");
				break;

			case Options::Mediump:
				statement("precision mediump int;");
				break;

			case Options::Highp:
				statement("precision highp int;");
				break;

			default:
				break;
			}
		}

		if (execution.flags.get(ExecutionModeEarlyFragmentTests))
			inputs.push_back("early_fragment_tests");
		if (execution.flags.get(ExecutionModePostDepthCoverage))
			inputs.push_back("post_depth_coverage");

		if (interlock_used)
			statement("#if defined(GL_ARB_fragment_shader_interlock)");

		if (execution.flags.get(ExecutionModePixelInterlockOrderedEXT))
			statement("layout(pixel_interlock_ordered) in;");
		else if (execution.flags.get(ExecutionModePixelInterlockUnorderedEXT))
			statement("layout(pixel_interlock_unordered) in;");
		else if (execution.flags.get(ExecutionModeSampleInterlockOrderedEXT))
			statement("layout(sample_interlock_ordered) in;");
		else if (execution.flags.get(ExecutionModeSampleInterlockUnorderedEXT))
			statement("layout(sample_interlock_unordered) in;");

		if (interlock_used)
		{
			statement("#elif !defined(GL_INTEL_fragment_shader_ordering)");
			statement("#error Fragment Shader Interlock/Ordering extension missing!");
			statement("#endif");
		}

		if (!options.es && execution.flags.get(ExecutionModeDepthGreater))
			statement("layout(depth_greater) out float gl_FragDepth;");
		else if (!options.es && execution.flags.get(ExecutionModeDepthLess))
			statement("layout(depth_less) out float gl_FragDepth;");

		break;

	default:
		break;
	}

	for (auto &cap : ir.declared_capabilities)
		if (cap == CapabilityRayTraversalPrimitiveCullingKHR)
			statement("layout(primitive_culling);");

	if (!inputs.empty())
		statement("layout(", merge(inputs), ") in;");
	if (!outputs.empty())
		statement("layout(", merge(outputs), ") out;");

	statement("");
}

bool CompilerGLSL::type_is_empty(const SPIRType &type)
{
	return type.basetype == SPIRType::Struct && type.member_types.empty();
}

void CompilerGLSL::emit_struct(SPIRType &type)
{
	// Struct types can be stamped out multiple times
	// with just different offsets, matrix layouts, etc ...
	// Type-punning with these types is legal, which complicates things
	// when we are storing struct and array types in an SSBO for example.
	// If the type master is packed however, we can no longer assume that the struct declaration will be redundant.
	if (type.type_alias != TypeID(0) &&
	    !has_extended_decoration(type.type_alias, SPIRVCrossDecorationBufferBlockRepacked))
		return;

	add_resource_name(type.self);
	auto name = type_to_glsl(type);

	statement(!backend.explicit_struct_type ? "struct " : "", name);
	begin_scope();

	type.member_name_cache.clear();

	uint32_t i = 0;
	bool emitted = false;
	for (auto &member : type.member_types)
	{
		add_member_name(type, i);
		emit_struct_member(type, member, i);
		i++;
		emitted = true;
	}

	// Don't declare empty structs in GLSL, this is not allowed.
	if (type_is_empty(type) && !backend.supports_empty_struct)
	{
		statement("int empty_struct_member;");
		emitted = true;
	}

	if (has_extended_decoration(type.self, SPIRVCrossDecorationPaddingTarget))
		emit_struct_padding_target(type);

	end_scope_decl();

	if (emitted)
		statement("");
}

string CompilerGLSL::to_interpolation_qualifiers(const Bitset &flags)
{
	string res;
	//if (flags & (1ull << DecorationSmooth))
	//    res += "smooth ";
	if (flags.get(DecorationFlat))
		res += "flat ";
	if (flags.get(DecorationNoPerspective))
	{
		if (options.es)
		{
			if (options.version < 300)
				SPIRV_CROSS_THROW("noperspective requires ESSL 300.");
			require_extension_internal("GL_NV_shader_noperspective_interpolation");
		}
		else if (is_legacy_desktop())
			require_extension_internal("GL_EXT_gpu_shader4");
		res += "noperspective ";
	}
	if (flags.get(DecorationCentroid))
		res += "centroid ";
	if (flags.get(DecorationPatch))
		res += "patch ";
	if (flags.get(DecorationSample))
	{
		if (options.es)
		{
			if (options.version < 300)
				SPIRV_CROSS_THROW("sample requires ESSL 300.");
			else if (options.version < 320)
				require_extension_internal("GL_OES_shader_multisample_interpolation");
		}
		res += "sample ";
	}
	if (flags.get(DecorationInvariant) && (options.es || options.version >= 120))
		res += "invariant ";
	if (flags.get(DecorationPerPrimitiveEXT))
	    res += "perprimitiveEXT ";

	if (flags.get(DecorationExplicitInterpAMD))
	{
		require_extension_internal("GL_AMD_shader_explicit_vertex_parameter");
		res += "__explicitInterpAMD ";
	}

	if (flags.get(DecorationPerVertexKHR))
	{
		if (options.es && options.version < 320)
			SPIRV_CROSS_THROW("pervertexEXT requires ESSL 320.");
		else if (!options.es && options.version < 450)
			SPIRV_CROSS_THROW("pervertexEXT requires GLSL 450.");

		if (barycentric_is_nv)
		{
			require_extension_internal("GL_NV_fragment_shader_barycentric");
			res += "pervertexNV ";
		}
		else
		{
			require_extension_internal("GL_EXT_fragment_shader_barycentric");
			res += "pervertexEXT ";
		}
	}

	return res;
}

string CompilerGLSL::layout_for_member(const SPIRType &type, uint32_t index)
{
	if (is_legacy())
		return "";

	bool is_block = has_decoration(type.self, DecorationBlock) || has_decoration(type.self, DecorationBufferBlock);
	if (!is_block)
		return "";

	auto &memb = ir.meta[type.self].members;
	if (index >= memb.size())
		return "";
	auto &dec = memb[index];

	SmallVector<string> attr;

	if (has_member_decoration(type.self, index, DecorationPassthroughNV))
		attr.push_back("passthrough");

	// We can only apply layouts on members in block interfaces.
	// This is a bit problematic because in SPIR-V decorations are applied on the struct types directly.
	// This is not supported on GLSL, so we have to make the assumption that if a struct within our buffer block struct
	// has a decoration, it was originally caused by a top-level layout() qualifier in GLSL.
	//
	// We would like to go from (SPIR-V style):
	//
	// struct Foo { layout(row_major) mat4 matrix; };
	// buffer UBO { Foo foo; };
	//
	// to
	//
	// struct Foo { mat4 matrix; }; // GLSL doesn't support any layout shenanigans in raw struct declarations.
	// buffer UBO { layout(row_major) Foo foo; }; // Apply the layout on top-level.
	auto flags = combined_decoration_for_member(type, index);

	if (flags.get(DecorationRowMajor))
		attr.push_back("row_major");
	// We don't emit any global layouts, so column_major is default.
	//if (flags & (1ull << DecorationColMajor))
	//    attr.push_back("column_major");

	if (dec.decoration_flags.get(DecorationLocation) && can_use_io_location(type.storage, true))
		attr.push_back(join("location = ", dec.location));

	// Can only declare component if we can declare location.
	if (dec.decoration_flags.get(DecorationComponent) && can_use_io_location(type.storage, true))
	{
		if (!options.es)
		{
			if (options.version < 440 && options.version >= 140)
				require_extension_internal("GL_ARB_enhanced_layouts");
			else if (options.version < 140)
				SPIRV_CROSS_THROW("Component decoration is not supported in targets below GLSL 1.40.");
			attr.push_back(join("component = ", dec.component));
		}
		else
			SPIRV_CROSS_THROW("Component decoration is not supported in ES targets.");
	}

	// SPIRVCrossDecorationPacked is set by layout_for_variable earlier to mark that we need to emit offset qualifiers.
	// This is only done selectively in GLSL as needed.
	if (has_extended_decoration(type.self, SPIRVCrossDecorationExplicitOffset) &&
	    dec.decoration_flags.get(DecorationOffset))
		attr.push_back(join("offset = ", dec.offset));
	else if (type.storage == StorageClassOutput && dec.decoration_flags.get(DecorationOffset))
		attr.push_back(join("xfb_offset = ", dec.offset));

	if (attr.empty())
		return "";

	string res = "layout(";
	res += merge(attr);
	res += ") ";
	return res;
}

const char *CompilerGLSL::format_to_glsl(spv::ImageFormat format)
{
	if (options.es && is_desktop_only_format(format))
		SPIRV_CROSS_THROW("Attempting to use image format not supported in ES profile.");

	switch (format)
	{
	case ImageFormatRgba32f:
		return "rgba32f";
	case ImageFormatRgba16f:
		return "rgba16f";
	case ImageFormatR32f:
		return "r32f";
	case ImageFormatRgba8:
		return "rgba8";
	case ImageFormatRgba8Snorm:
		return "rgba8_snorm";
	case ImageFormatRg32f:
		return "rg32f";
	case ImageFormatRg16f:
		return "rg16f";
	case ImageFormatRgba32i:
		return "rgba32i";
	case ImageFormatRgba16i:
		return "rgba16i";
	case ImageFormatR32i:
		return "r32i";
	case ImageFormatRgba8i:
		return "rgba8i";
	case ImageFormatRg32i:
		return "rg32i";
	case ImageFormatRg16i:
		return "rg16i";
	case ImageFormatRgba32ui:
		return "rgba32ui";
	case ImageFormatRgba16ui:
		return "rgba16ui";
	case ImageFormatR32ui:
		return "r32ui";
	case ImageFormatRgba8ui:
		return "rgba8ui";
	case ImageFormatRg32ui:
		return "rg32ui";
	case ImageFormatRg16ui:
		return "rg16ui";
	case ImageFormatR11fG11fB10f:
		return "r11f_g11f_b10f";
	case ImageFormatR16f:
		return "r16f";
	case ImageFormatRgb10A2:
		return "rgb10_a2";
	case ImageFormatR8:
		return "r8";
	case ImageFormatRg8:
		return "rg8";
	case ImageFormatR16:
		return "r16";
	case ImageFormatRg16:
		return "rg16";
	case ImageFormatRgba16:
		return "rgba16";
	case ImageFormatR16Snorm:
		return "r16_snorm";
	case ImageFormatRg16Snorm:
		return "rg16_snorm";
	case ImageFormatRgba16Snorm:
		return "rgba16_snorm";
	case ImageFormatR8Snorm:
		return "r8_snorm";
	case ImageFormatRg8Snorm:
		return "rg8_snorm";
	case ImageFormatR8ui:
		return "r8ui";
	case ImageFormatRg8ui:
		return "rg8ui";
	case ImageFormatR16ui:
		return "r16ui";
	case ImageFormatRgb10a2ui:
		return "rgb10_a2ui";
	case ImageFormatR8i:
		return "r8i";
	case ImageFormatRg8i:
		return "rg8i";
	case ImageFormatR16i:
		return "r16i";
	default:
	case ImageFormatUnknown:
		return nullptr;
	}
}

uint32_t CompilerGLSL::type_to_packed_base_size(const SPIRType &type, BufferPackingStandard)
{
	switch (type.basetype)
	{
	case SPIRType::Double:
	case SPIRType::Int64:
	case SPIRType::UInt64:
		return 8;
	case SPIRType::Float:
	case SPIRType::Int:
	case SPIRType::UInt:
		return 4;
	case SPIRType::Half:
	case SPIRType::Short:
	case SPIRType::UShort:
		return 2;
	case SPIRType::SByte:
	case SPIRType::UByte:
		return 1;

	default:
		SPIRV_CROSS_THROW("Unrecognized type in type_to_packed_base_size.");
	}
}

uint32_t CompilerGLSL::type_to_packed_alignment(const SPIRType &type, const Bitset &flags,
                                                BufferPackingStandard packing)
{
	// If using PhysicalStorageBufferEXT storage class, this is a pointer,
	// and is 64-bit.
	if (type_is_top_level_physical_pointer(type))
	{
		if (!type.pointer)
			SPIRV_CROSS_THROW("Types in PhysicalStorageBufferEXT must be pointers.");

		if (ir.addressing_model == AddressingModelPhysicalStorageBuffer64EXT)
		{
			if (packing_is_vec4_padded(packing) && type_is_array_of_pointers(type))
				return 16;
			else
				return 8;
		}
		else
			SPIRV_CROSS_THROW("AddressingModelPhysicalStorageBuffer64EXT must be used for PhysicalStorageBufferEXT.");
	}
	else if (type_is_top_level_array(type))
	{
		uint32_t minimum_alignment = 1;
		if (packing_is_vec4_padded(packing))
			minimum_alignment = 16;

		auto *tmp = &get<SPIRType>(type.parent_type);
		while (!tmp->array.empty())
			tmp = &get<SPIRType>(tmp->parent_type);

		// Get the alignment of the base type, then maybe round up.
		return max(minimum_alignment, type_to_packed_alignment(*tmp, flags, packing));
	}

	if (type.basetype == SPIRType::Struct)
	{
		// Rule 9. Structs alignments are maximum alignment of its members.
		uint32_t alignment = 1;
		for (uint32_t i = 0; i < type.member_types.size(); i++)
		{
			auto member_flags = ir.meta[type.self].members[i].decoration_flags;
			alignment =
			    max(alignment, type_to_packed_alignment(get<SPIRType>(type.member_types[i]), member_flags, packing));
		}

		// In std140, struct alignment is rounded up to 16.
		if (packing_is_vec4_padded(packing))
			alignment = max<uint32_t>(alignment, 16u);

		return alignment;
	}
	else
	{
		const uint32_t base_alignment = type_to_packed_base_size(type, packing);

		// Alignment requirement for scalar block layout is always the alignment for the most basic component.
		if (packing_is_scalar(packing))
			return base_alignment;

		// Vectors are *not* aligned in HLSL, but there's an extra rule where vectors cannot straddle
		// a vec4, this is handled outside since that part knows our current offset.
		if (type.columns == 1 && packing_is_hlsl(packing))
			return base_alignment;

		// From 7.6.2.2 in GL 4.5 core spec.
		// Rule 1
		if (type.vecsize == 1 && type.columns == 1)
			return base_alignment;

		// Rule 2
		if ((type.vecsize == 2 || type.vecsize == 4) && type.columns == 1)
			return type.vecsize * base_alignment;

		// Rule 3
		if (type.vecsize == 3 && type.columns == 1)
			return 4 * base_alignment;

		// Rule 4 implied. Alignment does not change in std430.

		// Rule 5. Column-major matrices are stored as arrays of
		// vectors.
		if (flags.get(DecorationColMajor) && type.columns > 1)
		{
			if (packing_is_vec4_padded(packing))
				return 4 * base_alignment;
			else if (type.vecsize == 3)
				return 4 * base_alignment;
			else
				return type.vecsize * base_alignment;
		}

		// Rule 6 implied.

		// Rule 7.
		if (flags.get(DecorationRowMajor) && type.vecsize > 1)
		{
			if (packing_is_vec4_padded(packing))
				return 4 * base_alignment;
			else if (type.columns == 3)
				return 4 * base_alignment;
			else
				return type.columns * base_alignment;
		}

		// Rule 8 implied.
	}

	SPIRV_CROSS_THROW("Did not find suitable rule for type. Bogus decorations?");
}

uint32_t CompilerGLSL::type_to_packed_array_stride(const SPIRType &type, const Bitset &flags,
                                                   BufferPackingStandard packing)
{
	// Array stride is equal to aligned size of the underlying type.
	uint32_t parent = type.parent_type;
	assert(parent);

	auto &tmp = get<SPIRType>(parent);

	uint32_t size = type_to_packed_size(tmp, flags, packing);
	uint32_t alignment = type_to_packed_alignment(type, flags, packing);
	return (size + alignment - 1) & ~(alignment - 1);
}

uint32_t CompilerGLSL::type_to_packed_size(const SPIRType &type, const Bitset &flags, BufferPackingStandard packing)
{
	// If using PhysicalStorageBufferEXT storage class, this is a pointer,
	// and is 64-bit.
	if (type_is_top_level_physical_pointer(type))
	{
		if (!type.pointer)
			SPIRV_CROSS_THROW("Types in PhysicalStorageBufferEXT must be pointers.");

		if (ir.addressing_model == AddressingModelPhysicalStorageBuffer64EXT)
			return 8;
		else
			SPIRV_CROSS_THROW("AddressingModelPhysicalStorageBuffer64EXT must be used for PhysicalStorageBufferEXT.");
	}
	else if (type_is_top_level_array(type))
	{
		uint32_t packed_size = to_array_size_literal(type) * type_to_packed_array_stride(type, flags, packing);

		// For arrays of vectors and matrices in HLSL, the last element has a size which depends on its vector size,
		// so that it is possible to pack other vectors into the last element.
		if (packing_is_hlsl(packing) && type.basetype != SPIRType::Struct)
			packed_size -= (4 - type.vecsize) * (type.width / 8);

		return packed_size;
	}

	uint32_t size = 0;

	if (type.basetype == SPIRType::Struct)
	{
		uint32_t pad_alignment = 1;

		for (uint32_t i = 0; i < type.member_types.size(); i++)
		{
			auto member_flags = ir.meta[type.self].members[i].decoration_flags;
			auto &member_type = get<SPIRType>(type.member_types[i]);

			uint32_t packed_alignment = type_to_packed_alignment(member_type, member_flags, packing);
			uint32_t alignment = max(packed_alignment, pad_alignment);

			// The next member following a struct member is aligned to the base alignment of the struct that came before.
			// GL 4.5 spec, 7.6.2.2.
			if (member_type.basetype == SPIRType::Struct)
				pad_alignment = packed_alignment;
			else
				pad_alignment = 1;

			size = (size + alignment - 1) & ~(alignment - 1);
			size += type_to_packed_size(member_type, member_flags, packing);
		}
	}
	else
	{
		const uint32_t base_alignment = type_to_packed_base_size(type, packing);

		if (packing_is_scalar(packing))
		{
			size = type.vecsize * type.columns * base_alignment;
		}
		else
		{
			if (type.columns == 1)
				size = type.vecsize * base_alignment;

			if (flags.get(DecorationColMajor) && type.columns > 1)
			{
				if (packing_is_vec4_padded(packing))
					size = type.columns * 4 * base_alignment;
				else if (type.vecsize == 3)
					size = type.columns * 4 * base_alignment;
				else
					size = type.columns * type.vecsize * base_alignment;
			}

			if (flags.get(DecorationRowMajor) && type.vecsize > 1)
			{
				if (packing_is_vec4_padded(packing))
					size = type.vecsize * 4 * base_alignment;
				else if (type.columns == 3)
					size = type.vecsize * 4 * base_alignment;
				else
					size = type.vecsize * type.columns * base_alignment;
			}

			// For matrices in HLSL, the last element has a size which depends on its vector size,
			// so that it is possible to pack other vectors into the last element.
			if (packing_is_hlsl(packing) && type.columns > 1)
				size -= (4 - type.vecsize) * (type.width / 8);
		}
	}

	return size;
}

bool CompilerGLSL::buffer_is_packing_standard(const SPIRType &type, BufferPackingStandard packing,
                                              uint32_t *failed_validation_index, uint32_t start_offset,
                                              uint32_t end_offset)
{
	// This is very tricky and error prone, but try to be exhaustive and correct here.
	// SPIR-V doesn't directly say if we're using std430 or std140.
	// SPIR-V communicates this using Offset and ArrayStride decorations (which is what really matters),
	// so we have to try to infer whether or not the original GLSL source was std140 or std430 based on this information.
	// We do not have to consider shared or packed since these layouts are not allowed in Vulkan SPIR-V (they are useless anyways, and custom offsets would do the same thing).
	//
	// It is almost certain that we're using std430, but it gets tricky with arrays in particular.
	// We will assume std430, but infer std140 if we can prove the struct is not compliant with std430.
	//
	// The only two differences between std140 and std430 are related to padding alignment/array stride
	// in arrays and structs. In std140 they take minimum vec4 alignment.
	// std430 only removes the vec4 requirement.

	uint32_t offset = 0;
	uint32_t pad_alignment = 1;

	bool is_top_level_block =
	    has_decoration(type.self, DecorationBlock) || has_decoration(type.self, DecorationBufferBlock);

	for (uint32_t i = 0; i < type.member_types.size(); i++)
	{
		auto &memb_type = get<SPIRType>(type.member_types[i]);
		auto member_flags = ir.meta[type.self].members[i].decoration_flags;

		// Verify alignment rules.
		uint32_t packed_alignment = type_to_packed_alignment(memb_type, member_flags, packing);

		// This is a rather dirty workaround to deal with some cases of OpSpecConstantOp used as array size, e.g:
		// layout(constant_id = 0) const int s = 10;
		// const int S = s + 5; // SpecConstantOp
		// buffer Foo { int data[S]; }; // <-- Very hard for us to deduce a fixed value here,
		// we would need full implementation of compile-time constant folding. :(
		// If we are the last member of a struct, there might be cases where the actual size of that member is irrelevant
		// for our analysis (e.g. unsized arrays).
		// This lets us simply ignore that there are spec constant op sized arrays in our buffers.
		// Querying size of this member will fail, so just don't call it unless we have to.
		//
		// This is likely "best effort" we can support without going into unacceptably complicated workarounds.
		bool member_can_be_unsized =
		    is_top_level_block && size_t(i + 1) == type.member_types.size() && !memb_type.array.empty();

		uint32_t packed_size = 0;
		if (!member_can_be_unsized || packing_is_hlsl(packing))
			packed_size = type_to_packed_size(memb_type, member_flags, packing);

		// We only need to care about this if we have non-array types which can straddle the vec4 boundary.
		uint32_t actual_offset = type_struct_member_offset(type, i);

		if (packing_is_hlsl(packing))
		{
			// If a member straddles across a vec4 boundary, alignment is actually vec4.
			uint32_t begin_word = actual_offset / 16;
			uint32_t end_word = (actual_offset + packed_size - 1) / 16;
			if (begin_word != end_word)
				packed_alignment = max<uint32_t>(packed_alignment, 16u);
		}

		// Field is not in the specified range anymore and we can ignore any further fields.
		if (actual_offset >= end_offset)
			break;

		uint32_t alignment = max(packed_alignment, pad_alignment);
		offset = (offset + alignment - 1) & ~(alignment - 1);

		// The next member following a struct member is aligned to the base alignment of the struct that came before.
		// GL 4.5 spec, 7.6.2.2.
		if (memb_type.basetype == SPIRType::Struct && !memb_type.pointer)
			pad_alignment = packed_alignment;
		else
			pad_alignment = 1;

		// Only care about packing if we are in the given range
		if (actual_offset >= start_offset)
		{
			// We only care about offsets in std140, std430, etc ...
			// For EnhancedLayout variants, we have the flexibility to choose our own offsets.
			if (!packing_has_flexible_offset(packing))
			{
				if (actual_offset != offset) // This cannot be the packing we're looking for.
				{
					if (failed_validation_index)
						*failed_validation_index = i;
					return false;
				}
			}
			else if ((actual_offset & (alignment - 1)) != 0)
			{
				// We still need to verify that alignment rules are observed, even if we have explicit offset.
				if (failed_validation_index)
					*failed_validation_index = i;
				return false;
			}

			// Verify array stride rules.
			if (type_is_top_level_array(memb_type) &&
			    type_to_packed_array_stride(memb_type, member_flags, packing) !=
			    type_struct_member_array_stride(type, i))
			{
				if (failed_validation_index)
					*failed_validation_index = i;
				return false;
			}

			// Verify that sub-structs also follow packing rules.
			// We cannot use enhanced layouts on substructs, so they better be up to spec.
			auto substruct_packing = packing_to_substruct_packing(packing);

			if (!memb_type.pointer && !memb_type.member_types.empty() &&
			    !buffer_is_packing_standard(memb_type, substruct_packing))
			{
				if (failed_validation_index)
					*failed_validation_index = i;
				return false;
			}
		}

		// Bump size.
		offset = actual_offset + packed_size;
	}

	return true;
}

bool CompilerGLSL::can_use_io_location(StorageClass storage, bool block)
{
	// Location specifiers are must have in SPIR-V, but they aren't really supported in earlier versions of GLSL.
	// Be very explicit here about how to solve the issue.
	if ((get_execution_model() != ExecutionModelVertex && storage == StorageClassInput) ||
	    (get_execution_model() != ExecutionModelFragment && storage == StorageClassOutput))
	{
		uint32_t minimum_desktop_version = block ? 440 : 410;
		// ARB_enhanced_layouts vs ARB_separate_shader_objects ...

		if (!options.es && options.version < minimum_desktop_version && !options.separate_shader_objects)
			return false;
		else if (options.es && options.version < 310)
			return false;
	}

	if ((get_execution_model() == ExecutionModelVertex && storage == StorageClassInput) ||
	    (get_execution_model() == ExecutionModelFragment && storage == StorageClassOutput))
	{
		if (options.es && options.version < 300)
			return false;
		else if (!options.es && options.version < 330)
			return false;
	}

	if (storage == StorageClassUniform || storage == StorageClassUniformConstant || storage == StorageClassPushConstant)
	{
		if (options.es && options.version < 310)
			return false;
		else if (!options.es && options.version < 430)
			return false;
	}

	return true;
}

string CompilerGLSL::layout_for_variable(const SPIRVariable &var)
{
	// FIXME: Come up with a better solution for when to disable layouts.
	// Having layouts depend on extensions as well as which types
	// of layouts are used. For now, the simple solution is to just disable
	// layouts for legacy versions.
	if (is_legacy())
		return "";

	if (subpass_input_is_framebuffer_fetch(var.self))
		return "";

	SmallVector<string> attr;

	auto &type = get<SPIRType>(var.basetype);
	auto &flags = get_decoration_bitset(var.self);
	auto &typeflags = get_decoration_bitset(type.self);

	if (flags.get(DecorationPassthroughNV))
		attr.push_back("passthrough");

	if (options.vulkan_semantics && var.storage == StorageClassPushConstant)
		attr.push_back("push_constant");
	else if (var.storage == StorageClassShaderRecordBufferKHR)
		attr.push_back(ray_tracing_is_khr ? "shaderRecordEXT" : "shaderRecordNV");

	if (flags.get(DecorationRowMajor))
		attr.push_back("row_major");
	if (flags.get(DecorationColMajor))
		attr.push_back("column_major");

	if (options.vulkan_semantics)
	{
		if (flags.get(DecorationInputAttachmentIndex))
			attr.push_back(join("input_attachment_index = ", get_decoration(var.self, DecorationInputAttachmentIndex)));
	}

	bool is_block = has_decoration(type.self, DecorationBlock);
	if (flags.get(DecorationLocation) && can_use_io_location(var.storage, is_block))
	{
		Bitset combined_decoration;
		for (uint32_t i = 0; i < ir.meta[type.self].members.size(); i++)
			combined_decoration.merge_or(combined_decoration_for_member(type, i));

		// If our members have location decorations, we don't need to
		// emit location decorations at the top as well (looks weird).
		if (!combined_decoration.get(DecorationLocation))
			attr.push_back(join("location = ", get_decoration(var.self, DecorationLocation)));
	}

	if (get_execution_model() == ExecutionModelFragment && var.storage == StorageClassOutput &&
	    location_is_non_coherent_framebuffer_fetch(get_decoration(var.self, DecorationLocation)))
	{
		attr.push_back("noncoherent");
	}

	// Transform feedback
	bool uses_enhanced_layouts = false;
	if (is_block && var.storage == StorageClassOutput)
	{
		// For blocks, there is a restriction where xfb_stride/xfb_buffer must only be declared on the block itself,
		// since all members must match the same xfb_buffer. The only thing we will declare for members of the block
		// is the xfb_offset.
		uint32_t member_count = uint32_t(type.member_types.size());
		bool have_xfb_buffer_stride = false;
		bool have_any_xfb_offset = false;
		bool have_geom_stream = false;
		uint32_t xfb_stride = 0, xfb_buffer = 0, geom_stream = 0;

		if (flags.get(DecorationXfbBuffer) && flags.get(DecorationXfbStride))
		{
			have_xfb_buffer_stride = true;
			xfb_buffer = get_decoration(var.self, DecorationXfbBuffer);
			xfb_stride = get_decoration(var.self, DecorationXfbStride);
		}

		if (flags.get(DecorationStream))
		{
			have_geom_stream = true;
			geom_stream = get_decoration(var.self, DecorationStream);
		}

		// Verify that none of the members violate our assumption.
		for (uint32_t i = 0; i < member_count; i++)
		{
			if (has_member_decoration(type.self, i, DecorationStream))
			{
				uint32_t member_geom_stream = get_member_decoration(type.self, i, DecorationStream);
				if (have_geom_stream && member_geom_stream != geom_stream)
					SPIRV_CROSS_THROW("IO block member Stream mismatch.");
				have_geom_stream = true;
				geom_stream = member_geom_stream;
			}

			// Only members with an Offset decoration participate in XFB.
			if (!has_member_decoration(type.self, i, DecorationOffset))
				continue;
			have_any_xfb_offset = true;

			if (has_member_decoration(type.self, i, DecorationXfbBuffer))
			{
				uint32_t buffer_index = get_member_decoration(type.self, i, DecorationXfbBuffer);
				if (have_xfb_buffer_stride && buffer_index != xfb_buffer)
					SPIRV_CROSS_THROW("IO block member XfbBuffer mismatch.");
				have_xfb_buffer_stride = true;
				xfb_buffer = buffer_index;
			}

			if (has_member_decoration(type.self, i, DecorationXfbStride))
			{
				uint32_t stride = get_member_decoration(type.self, i, DecorationXfbStride);
				if (have_xfb_buffer_stride && stride != xfb_stride)
					SPIRV_CROSS_THROW("IO block member XfbStride mismatch.");
				have_xfb_buffer_stride = true;
				xfb_stride = stride;
			}
		}

		if (have_xfb_buffer_stride && have_any_xfb_offset)
		{
			attr.push_back(join("xfb_buffer = ", xfb_buffer));
			attr.push_back(join("xfb_stride = ", xfb_stride));
			uses_enhanced_layouts = true;
		}

		if (have_geom_stream)
		{
			if (get_execution_model() != ExecutionModelGeometry)
				SPIRV_CROSS_THROW("Geometry streams can only be used in geometry shaders.");
			if (options.es)
				SPIRV_CROSS_THROW("Multiple geometry streams not supported in ESSL.");
			if (options.version < 400)
				require_extension_internal("GL_ARB_transform_feedback3");
			attr.push_back(join("stream = ", get_decoration(var.self, DecorationStream)));
		}
	}
	else if (var.storage == StorageClassOutput)
	{
		if (flags.get(DecorationXfbBuffer) && flags.get(DecorationXfbStride) && flags.get(DecorationOffset))
		{
			// XFB for standalone variables, we can emit all decorations.
			attr.push_back(join("xfb_buffer = ", get_decoration(var.self, DecorationXfbBuffer)));
			attr.push_back(join("xfb_stride = ", get_decoration(var.self, DecorationXfbStride)));
			attr.push_back(join("xfb_offset = ", get_decoration(var.self, DecorationOffset)));
			uses_enhanced_layouts = true;
		}

		if (flags.get(DecorationStream))
		{
			if (get_execution_model() != ExecutionModelGeometry)
				SPIRV_CROSS_THROW("Geometry streams can only be used in geometry shaders.");
			if (options.es)
				SPIRV_CROSS_THROW("Multiple geometry streams not supported in ESSL.");
			if (options.version < 400)
				require_extension_internal("GL_ARB_transform_feedback3");
			attr.push_back(join("stream = ", get_decoration(var.self, DecorationStream)));
		}
	}

	// Can only declare Component if we can declare location.
	if (flags.get(DecorationComponent) && can_use_io_location(var.storage, is_block))
	{
		uses_enhanced_layouts = true;
		attr.push_back(join("component = ", get_decoration(var.self, DecorationComponent)));
	}

	if (uses_enhanced_layouts)
	{
		if (!options.es)
		{
			if (options.version < 440 && options.version >= 140)
				require_extension_internal("GL_ARB_enhanced_layouts");
			else if (options.version < 140)
				SPIRV_CROSS_THROW("GL_ARB_enhanced_layouts is not supported in targets below GLSL 1.40.");
			if (!options.es && options.version < 440)
				require_extension_internal("GL_ARB_enhanced_layouts");
		}
		else if (options.es)
			SPIRV_CROSS_THROW("GL_ARB_enhanced_layouts is not supported in ESSL.");
	}

	if (flags.get(DecorationIndex))
		attr.push_back(join("index = ", get_decoration(var.self, DecorationIndex)));

	// Do not emit set = decoration in regular GLSL output, but
	// we need to preserve it in Vulkan GLSL mode.
	if (var.storage != StorageClassPushConstant && var.storage != StorageClassShaderRecordBufferKHR)
	{
		if (flags.get(DecorationDescriptorSet) && options.vulkan_semantics)
			attr.push_back(join("set = ", get_decoration(var.self, DecorationDescriptorSet)));
	}

	bool push_constant_block = options.vulkan_semantics && var.storage == StorageClassPushConstant;
	bool ssbo_block = var.storage == StorageClassStorageBuffer || var.storage == StorageClassShaderRecordBufferKHR ||
	                  (var.storage == StorageClassUniform && typeflags.get(DecorationBufferBlock));
	bool emulated_ubo = var.storage == StorageClassPushConstant && options.emit_push_constant_as_uniform_buffer;
	bool ubo_block = var.storage == StorageClassUniform && typeflags.get(DecorationBlock);

	// GL 3.0/GLSL 1.30 is not considered legacy, but it doesn't have UBOs ...
	bool can_use_buffer_blocks = (options.es && options.version >= 300) || (!options.es && options.version >= 140);

	// pretend no UBOs when options say so
	if (ubo_block && options.emit_uniform_buffer_as_plain_uniforms)
		can_use_buffer_blocks = false;

	bool can_use_binding;
	if (options.es)
		can_use_binding = options.version >= 310;
	else
		can_use_binding = options.enable_420pack_extension || (options.version >= 420);

	// Make sure we don't emit binding layout for a classic uniform on GLSL 1.30.
	if (!can_use_buffer_blocks && var.storage == StorageClassUniform)
		can_use_binding = false;

	if (var.storage == StorageClassShaderRecordBufferKHR)
		can_use_binding = false;

	if (can_use_binding && flags.get(DecorationBinding))
		attr.push_back(join("binding = ", get_decoration(var.self, DecorationBinding)));

	if (var.storage != StorageClassOutput && flags.get(DecorationOffset))
		attr.push_back(join("offset = ", get_decoration(var.self, DecorationOffset)));

	// Instead of adding explicit offsets for every element here, just assume we're using std140 or std430.
	// If SPIR-V does not comply with either layout, we cannot really work around it.
	if (can_use_buffer_blocks && (ubo_block || emulated_ubo))
	{
		attr.push_back(buffer_to_packing_standard(type, false));
	}
	else if (can_use_buffer_blocks && (push_constant_block || ssbo_block))
	{
		attr.push_back(buffer_to_packing_standard(type, true));
	}

	// For images, the type itself adds a layout qualifer.
	// Only emit the format for storage images.
	if (type.basetype == SPIRType::Image && type.image.sampled == 2)
	{
		const char *fmt = format_to_glsl(type.image.format);
		if (fmt)
			attr.push_back(fmt);
	}

	if (attr.empty())
		return "";

	string res = "layout(";
	res += merge(attr);
	res += ") ";
	return res;
}

string CompilerGLSL::buffer_to_packing_standard(const SPIRType &type, bool support_std430_without_scalar_layout)
{
	if (support_std430_without_scalar_layout && buffer_is_packing_standard(type, BufferPackingStd430))
		return "std430";
	else if (buffer_is_packing_standard(type, BufferPackingStd140))
		return "std140";
	else if (options.vulkan_semantics && buffer_is_packing_standard(type, BufferPackingScalar))
	{
		require_extension_internal("GL_EXT_scalar_block_layout");
		return "scalar";
	}
	else if (support_std430_without_scalar_layout &&
	         buffer_is_packing_standard(type, BufferPackingStd430EnhancedLayout))
	{
		if (options.es && !options.vulkan_semantics)
			SPIRV_CROSS_THROW("Push constant block cannot be expressed as neither std430 nor std140. ES-targets do "
			                  "not support GL_ARB_enhanced_layouts.");
		if (!options.es && !options.vulkan_semantics && options.version < 440)
			require_extension_internal("GL_ARB_enhanced_layouts");

		set_extended_decoration(type.self, SPIRVCrossDecorationExplicitOffset);
		return "std430";
	}
	else if (buffer_is_packing_standard(type, BufferPackingStd140EnhancedLayout))
	{
		// Fallback time. We might be able to use the ARB_enhanced_layouts to deal with this difference,
		// however, we can only use layout(offset) on the block itself, not any substructs, so the substructs better be the appropriate layout.
		// Enhanced layouts seem to always work in Vulkan GLSL, so no need for extensions there.
		if (options.es && !options.vulkan_semantics)
			SPIRV_CROSS_THROW("Push constant block cannot be expressed as neither std430 nor std140. ES-targets do "
			                  "not support GL_ARB_enhanced_layouts.");
		if (!options.es && !options.vulkan_semantics && options.version < 440)
			require_extension_internal("GL_ARB_enhanced_layouts");

		set_extended_decoration(type.self, SPIRVCrossDecorationExplicitOffset);
		return "std140";
	}
	else if (options.vulkan_semantics && buffer_is_packing_standard(type, BufferPackingScalarEnhancedLayout))
	{
		set_extended_decoration(type.self, SPIRVCrossDecorationExplicitOffset);
		require_extension_internal("GL_EXT_scalar_block_layout");
		return "scalar";
	}
	else if (!support_std430_without_scalar_layout && options.vulkan_semantics &&
	         buffer_is_packing_standard(type, BufferPackingStd430))
	{
		// UBOs can support std430 with GL_EXT_scalar_block_layout.
		require_extension_internal("GL_EXT_scalar_block_layout");
		return "std430";
	}
	else if (!support_std430_without_scalar_layout && options.vulkan_semantics &&
	         buffer_is_packing_standard(type, BufferPackingStd430EnhancedLayout))
	{
		// UBOs can support std430 with GL_EXT_scalar_block_layout.
		set_extended_decoration(type.self, SPIRVCrossDecorationExplicitOffset);
		require_extension_internal("GL_EXT_scalar_block_layout");
		return "std430";
	}
	else
	{
		SPIRV_CROSS_THROW("Buffer block cannot be expressed as any of std430, std140, scalar, even with enhanced "
		                  "layouts. You can try flattening this block to support a more flexible layout.");
	}
}

void CompilerGLSL::emit_push_constant_block(const SPIRVariable &var)
{
	if (flattened_buffer_blocks.count(var.self))
		emit_buffer_block_flattened(var);
	else if (options.vulkan_semantics)
		emit_push_constant_block_vulkan(var);
	else if (options.emit_push_constant_as_uniform_buffer)
		emit_buffer_block_native(var);
	else
		emit_push_constant_block_glsl(var);
}

void CompilerGLSL::emit_push_constant_block_vulkan(const SPIRVariable &var)
{
	emit_buffer_block(var);
}

void CompilerGLSL::emit_push_constant_block_glsl(const SPIRVariable &var)
{
	// OpenGL has no concept of push constant blocks, implement it as a uniform struct.
	auto &type = get<SPIRType>(var.basetype);

	unset_decoration(var.self, DecorationBinding);
	unset_decoration(var.self, DecorationDescriptorSet);

#if 0
    if (flags & ((1ull << DecorationBinding) | (1ull << DecorationDescriptorSet)))
        SPIRV_CROSS_THROW("Push constant blocks cannot be compiled to GLSL with Binding or Set syntax. "
                            "Remap to location with reflection API first or disable these decorations.");
#endif

	// We're emitting the push constant block as a regular struct, so disable the block qualifier temporarily.
	// Otherwise, we will end up emitting layout() qualifiers on naked structs which is not allowed.
	bool block_flag = has_decoration(type.self, DecorationBlock);
	unset_decoration(type.self, DecorationBlock);

	emit_struct(type);

	if (block_flag)
		set_decoration(type.self, DecorationBlock);

	emit_uniform(var);
	statement("");
}

void CompilerGLSL::emit_buffer_block(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	bool ubo_block = var.storage == StorageClassUniform && has_decoration(type.self, DecorationBlock);

	if (flattened_buffer_blocks.count(var.self))
		emit_buffer_block_flattened(var);
	else if (is_legacy() || (!options.es && options.version == 130) ||
	         (ubo_block && options.emit_uniform_buffer_as_plain_uniforms))
		emit_buffer_block_legacy(var);
	else
		emit_buffer_block_native(var);
}

void CompilerGLSL::emit_buffer_block_legacy(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	bool ssbo = var.storage == StorageClassStorageBuffer ||
	            ir.meta[type.self].decoration.decoration_flags.get(DecorationBufferBlock);
	if (ssbo)
		SPIRV_CROSS_THROW("SSBOs not supported in legacy targets.");

	// We're emitting the push constant block as a regular struct, so disable the block qualifier temporarily.
	// Otherwise, we will end up emitting layout() qualifiers on naked structs which is not allowed.
	auto &block_flags = ir.meta[type.self].decoration.decoration_flags;
	bool block_flag = block_flags.get(DecorationBlock);
	block_flags.clear(DecorationBlock);
	emit_struct(type);
	if (block_flag)
		block_flags.set(DecorationBlock);
	emit_uniform(var);
	statement("");
}

void CompilerGLSL::emit_buffer_reference_block(uint32_t type_id, bool forward_declaration)
{
	auto &type = get<SPIRType>(type_id);
	string buffer_name;

	if (forward_declaration)
	{
		// Block names should never alias, but from HLSL input they kind of can because block types are reused for UAVs ...
		// Allow aliased name since we might be declaring the block twice. Once with buffer reference (forward declared) and one proper declaration.
		// The names must match up.
		buffer_name = to_name(type.self, false);

		// Shaders never use the block by interface name, so we don't
		// have to track this other than updating name caches.
		// If we have a collision for any reason, just fallback immediately.
		if (ir.meta[type.self].decoration.alias.empty() ||
		    block_ssbo_names.find(buffer_name) != end(block_ssbo_names) ||
		    resource_names.find(buffer_name) != end(resource_names))
		{
			buffer_name = join("_", type.self);
		}

		// Make sure we get something unique for both global name scope and block name scope.
		// See GLSL 4.5 spec: section 4.3.9 for details.
		add_variable(block_ssbo_names, resource_names, buffer_name);

		// If for some reason buffer_name is an illegal name, make a final fallback to a workaround name.
		// This cannot conflict with anything else, so we're safe now.
		// We cannot reuse this fallback name in neither global scope (blocked by block_names) nor block name scope.
		if (buffer_name.empty())
			buffer_name = join("_", type.self);

		block_names.insert(buffer_name);
		block_ssbo_names.insert(buffer_name);

		// Ensure we emit the correct name when emitting non-forward pointer type.
		ir.meta[type.self].decoration.alias = buffer_name;
	}
	else if (type.basetype != SPIRType::Struct)
		buffer_name = type_to_glsl(type);
	else
		buffer_name = to_name(type.self, false);

	if (!forward_declaration)
	{
		auto itr = physical_storage_type_to_alignment.find(type_id);
		uint32_t alignment = 0;
		if (itr != physical_storage_type_to_alignment.end())
			alignment = itr->second.alignment;

		if (type.basetype == SPIRType::Struct)
		{
			SmallVector<std::string> attributes;
			attributes.push_back("buffer_reference");
			if (alignment)
				attributes.push_back(join("buffer_reference_align = ", alignment));
			attributes.push_back(buffer_to_packing_standard(type, true));

			auto flags = ir.get_buffer_block_type_flags(type);
			string decorations;
			if (flags.get(DecorationRestrict))
				decorations += " restrict";
			if (flags.get(DecorationCoherent))
				decorations += " coherent";
			if (flags.get(DecorationNonReadable))
				decorations += " writeonly";
			if (flags.get(DecorationNonWritable))
				decorations += " readonly";

			statement("layout(", merge(attributes), ")", decorations, " buffer ", buffer_name);
		}
		else if (alignment)
			statement("layout(buffer_reference, buffer_reference_align = ", alignment, ") buffer ", buffer_name);
		else
			statement("layout(buffer_reference) buffer ", buffer_name);

		begin_scope();

		if (type.basetype == SPIRType::Struct)
		{
			type.member_name_cache.clear();

			uint32_t i = 0;
			for (auto &member : type.member_types)
			{
				add_member_name(type, i);
				emit_struct_member(type, member, i);
				i++;
			}
		}
		else
		{
			auto &pointee_type = get_pointee_type(type);
			statement(type_to_glsl(pointee_type), " value", type_to_array_glsl(pointee_type), ";");
		}

		end_scope_decl();
		statement("");
	}
	else
	{
		statement("layout(buffer_reference) buffer ", buffer_name, ";");
	}
}

void CompilerGLSL::emit_buffer_block_native(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);

	Bitset flags = ir.get_buffer_block_flags(var);
	bool ssbo = var.storage == StorageClassStorageBuffer || var.storage == StorageClassShaderRecordBufferKHR ||
	            ir.meta[type.self].decoration.decoration_flags.get(DecorationBufferBlock);
	bool is_restrict = ssbo && flags.get(DecorationRestrict);
	bool is_writeonly = ssbo && flags.get(DecorationNonReadable);
	bool is_readonly = ssbo && flags.get(DecorationNonWritable);
	bool is_coherent = ssbo && flags.get(DecorationCoherent);

	// Block names should never alias, but from HLSL input they kind of can because block types are reused for UAVs ...
	auto buffer_name = to_name(type.self, false);

	auto &block_namespace = ssbo ? block_ssbo_names : block_ubo_names;

	// Shaders never use the block by interface name, so we don't
	// have to track this other than updating name caches.
	// If we have a collision for any reason, just fallback immediately.
	if (ir.meta[type.self].decoration.alias.empty() || block_namespace.find(buffer_name) != end(block_namespace) ||
	    resource_names.find(buffer_name) != end(resource_names))
	{
		buffer_name = get_block_fallback_name(var.self);
	}

	// Make sure we get something unique for both global name scope and block name scope.
	// See GLSL 4.5 spec: section 4.3.9 for details.
	add_variable(block_namespace, resource_names, buffer_name);

	// If for some reason buffer_name is an illegal name, make a final fallback to a workaround name.
	// This cannot conflict with anything else, so we're safe now.
	// We cannot reuse this fallback name in neither global scope (blocked by block_names) nor block name scope.
	if (buffer_name.empty())
		buffer_name = join("_", get<SPIRType>(var.basetype).self, "_", var.self);

	block_names.insert(buffer_name);
	block_namespace.insert(buffer_name);

	// Save for post-reflection later.
	declared_block_names[var.self] = buffer_name;

	statement(layout_for_variable(var), is_coherent ? "coherent " : "", is_restrict ? "restrict " : "",
	          is_writeonly ? "writeonly " : "", is_readonly ? "readonly " : "", ssbo ? "buffer " : "uniform ",
	          buffer_name);

	begin_scope();

	type.member_name_cache.clear();

	uint32_t i = 0;
	for (auto &member : type.member_types)
	{
		add_member_name(type, i);
		emit_struct_member(type, member, i);
		i++;
	}

	// var.self can be used as a backup name for the block name,
	// so we need to make sure we don't disturb the name here on a recompile.
	// It will need to be reset if we have to recompile.
	preserve_alias_on_reset(var.self);
	add_resource_name(var.self);
	end_scope_decl(to_name(var.self) + type_to_array_glsl(type));
	statement("");
}

void CompilerGLSL::emit_buffer_block_flattened(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);

	// Block names should never alias.
	auto buffer_name = to_name(type.self, false);
	size_t buffer_size = (get_declared_struct_size(type) + 15) / 16;

	SPIRType::BaseType basic_type;
	if (get_common_basic_type(type, basic_type))
	{
		SPIRType tmp;
		tmp.basetype = basic_type;
		tmp.vecsize = 4;
		if (basic_type != SPIRType::Float && basic_type != SPIRType::Int && basic_type != SPIRType::UInt)
			SPIRV_CROSS_THROW("Basic types in a flattened UBO must be float, int or uint.");

		auto flags = ir.get_buffer_block_flags(var);
		statement("uniform ", flags_to_qualifiers_glsl(tmp, flags), type_to_glsl(tmp), " ", buffer_name, "[",
		          buffer_size, "];");
	}
	else
		SPIRV_CROSS_THROW("All basic types in a flattened block must be the same.");
}

const char *CompilerGLSL::to_storage_qualifiers_glsl(const SPIRVariable &var)
{
	auto &execution = get_entry_point();

	if (subpass_input_is_framebuffer_fetch(var.self))
		return "";

	if (var.storage == StorageClassInput || var.storage == StorageClassOutput)
	{
		if (is_legacy() && execution.model == ExecutionModelVertex)
			return var.storage == StorageClassInput ? "attribute " : "varying ";
		else if (is_legacy() && execution.model == ExecutionModelFragment)
			return "varying "; // Fragment outputs are renamed so they never hit this case.
		else if (execution.model == ExecutionModelFragment && var.storage == StorageClassOutput)
		{
			uint32_t loc = get_decoration(var.self, DecorationLocation);
			bool is_inout = location_is_framebuffer_fetch(loc);
			if (is_inout)
				return "inout ";
			else
				return "out ";
		}
		else
			return var.storage == StorageClassInput ? "in " : "out ";
	}
	else if (var.storage == StorageClassUniformConstant || var.storage == StorageClassUniform ||
	         var.storage == StorageClassPushConstant)
	{
		return "uniform ";
	}
	else if (var.storage == StorageClassRayPayloadKHR)
	{
		return ray_tracing_is_khr ? "rayPayloadEXT " : "rayPayloadNV ";
	}
	else if (var.storage == StorageClassIncomingRayPayloadKHR)
	{
		return ray_tracing_is_khr ? "rayPayloadInEXT " : "rayPayloadInNV ";
	}
	else if (var.storage == StorageClassHitAttributeKHR)
	{
		return ray_tracing_is_khr ? "hitAttributeEXT " : "hitAttributeNV ";
	}
	else if (var.storage == StorageClassCallableDataKHR)
	{
		return ray_tracing_is_khr ? "callableDataEXT " : "callableDataNV ";
	}
	else if (var.storage == StorageClassIncomingCallableDataKHR)
	{
		return ray_tracing_is_khr ? "callableDataInEXT " : "callableDataInNV ";
	}

	return "";
}

void CompilerGLSL::emit_flattened_io_block_member(const std::string &basename, const SPIRType &type, const char *qual,
                                                  const SmallVector<uint32_t> &indices)
{
	uint32_t member_type_id = type.self;
	const SPIRType *member_type = &type;
	const SPIRType *parent_type = nullptr;
	auto flattened_name = basename;
	for (auto &index : indices)
	{
		flattened_name += "_";
		flattened_name += to_member_name(*member_type, index);
		parent_type = member_type;
		member_type_id = member_type->member_types[index];
		member_type = &get<SPIRType>(member_type_id);
	}

	assert(member_type->basetype != SPIRType::Struct);

	// We're overriding struct member names, so ensure we do so on the primary type.
	if (parent_type->type_alias)
		parent_type = &get<SPIRType>(parent_type->type_alias);

	// Sanitize underscores because joining the two identifiers might create more than 1 underscore in a row,
	// which is not allowed.
	ParsedIR::sanitize_underscores(flattened_name);

	uint32_t last_index = indices.back();

	// Pass in the varying qualifier here so it will appear in the correct declaration order.
	// Replace member name while emitting it so it encodes both struct name and member name.
	auto backup_name = get_member_name(parent_type->self, last_index);
	auto member_name = to_member_name(*parent_type, last_index);
	set_member_name(parent_type->self, last_index, flattened_name);
	emit_struct_member(*parent_type, member_type_id, last_index, qual);
	// Restore member name.
	set_member_name(parent_type->self, last_index, member_name);
}

void CompilerGLSL::emit_flattened_io_block_struct(const std::string &basename, const SPIRType &type, const char *qual,
                                                  const SmallVector<uint32_t> &indices)
{
	auto sub_indices = indices;
	sub_indices.push_back(0);

	const SPIRType *member_type = &type;
	for (auto &index : indices)
		member_type = &get<SPIRType>(member_type->member_types[index]);

	assert(member_type->basetype == SPIRType::Struct);

	if (!member_type->array.empty())
		SPIRV_CROSS_THROW("Cannot flatten array of structs in I/O blocks.");

	for (uint32_t i = 0; i < uint32_t(member_type->member_types.size()); i++)
	{
		sub_indices.back() = i;
		if (get<SPIRType>(member_type->member_types[i]).basetype == SPIRType::Struct)
			emit_flattened_io_block_struct(basename, type, qual, sub_indices);
		else
			emit_flattened_io_block_member(basename, type, qual, sub_indices);
	}
}

void CompilerGLSL::emit_flattened_io_block(const SPIRVariable &var, const char *qual)
{
	auto &var_type = get<SPIRType>(var.basetype);
	if (!var_type.array.empty())
		SPIRV_CROSS_THROW("Array of varying structs cannot be flattened to legacy-compatible varyings.");

	// Emit flattened types based on the type alias. Normally, we are never supposed to emit
	// struct declarations for aliased types.
	auto &type = var_type.type_alias ? get<SPIRType>(var_type.type_alias) : var_type;

	auto old_flags = ir.meta[type.self].decoration.decoration_flags;
	// Emit the members as if they are part of a block to get all qualifiers.
	ir.meta[type.self].decoration.decoration_flags.set(DecorationBlock);

	type.member_name_cache.clear();

	SmallVector<uint32_t> member_indices;
	member_indices.push_back(0);
	auto basename = to_name(var.self);

	uint32_t i = 0;
	for (auto &member : type.member_types)
	{
		add_member_name(type, i);
		auto &membertype = get<SPIRType>(member);

		member_indices.back() = i;
		if (membertype.basetype == SPIRType::Struct)
			emit_flattened_io_block_struct(basename, type, qual, member_indices);
		else
			emit_flattened_io_block_member(basename, type, qual, member_indices);
		i++;
	}

	ir.meta[type.self].decoration.decoration_flags = old_flags;

	// Treat this variable as fully flattened from now on.
	flattened_structs[var.self] = true;
}

void CompilerGLSL::emit_interface_block(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);

	if (var.storage == StorageClassInput && type.basetype == SPIRType::Double &&
	    !options.es && options.version < 410)
	{
		require_extension_internal("GL_ARB_vertex_attrib_64bit");
	}

	// Either make it plain in/out or in/out blocks depending on what shader is doing ...
	bool block = ir.meta[type.self].decoration.decoration_flags.get(DecorationBlock);
	const char *qual = to_storage_qualifiers_glsl(var);

	if (block)
	{
		// ESSL earlier than 310 and GLSL earlier than 150 did not support
		// I/O variables which are struct types.
		// To support this, flatten the struct into separate varyings instead.
		if (options.force_flattened_io_blocks || (options.es && options.version < 310) ||
		    (!options.es && options.version < 150))
		{
			// I/O blocks on ES require version 310 with Android Extension Pack extensions, or core version 320.
			// On desktop, I/O blocks were introduced with geometry shaders in GL 3.2 (GLSL 150).
			emit_flattened_io_block(var, qual);
		}
		else
		{
			if (options.es && options.version < 320)
			{
				// Geometry and tessellation extensions imply this extension.
				if (!has_extension("GL_EXT_geometry_shader") && !has_extension("GL_EXT_tessellation_shader"))
					require_extension_internal("GL_EXT_shader_io_blocks");
			}

			// Workaround to make sure we can emit "patch in/out" correctly.
			fixup_io_block_patch_primitive_qualifiers(var);

			// Block names should never alias.
			auto block_name = to_name(type.self, false);

			// The namespace for I/O blocks is separate from other variables in GLSL.
			auto &block_namespace = type.storage == StorageClassInput ? block_input_names : block_output_names;

			// Shaders never use the block by interface name, so we don't
			// have to track this other than updating name caches.
			if (block_name.empty() || block_namespace.find(block_name) != end(block_namespace))
				block_name = get_fallback_name(type.self);
			else
				block_namespace.insert(block_name);

			// If for some reason buffer_name is an illegal name, make a final fallback to a workaround name.
			// This cannot conflict with anything else, so we're safe now.
			if (block_name.empty())
				block_name = join("_", get<SPIRType>(var.basetype).self, "_", var.self);

			// Instance names cannot alias block names.
			resource_names.insert(block_name);

			const char *block_qualifier;
			if (has_decoration(var.self, DecorationPatch))
				block_qualifier = "patch ";
			else if (has_decoration(var.self, DecorationPerPrimitiveEXT))
				block_qualifier = "perprimitiveEXT ";
			else
				block_qualifier = "";

			statement(layout_for_variable(var), block_qualifier, qual, block_name);
			begin_scope();

			type.member_name_cache.clear();

			uint32_t i = 0;
			for (auto &member : type.member_types)
			{
				add_member_name(type, i);
				emit_struct_member(type, member, i);
				i++;
			}

			add_resource_name(var.self);
			end_scope_decl(join(to_name(var.self), type_to_array_glsl(type)));
			statement("");
		}
	}
	else
	{
		// ESSL earlier than 310 and GLSL earlier than 150 did not support
		// I/O variables which are struct types.
		// To support this, flatten the struct into separate varyings instead.
		if (type.basetype == SPIRType::Struct &&
		    (options.force_flattened_io_blocks || (options.es && options.version < 310) ||
		     (!options.es && options.version < 150)))
		{
			emit_flattened_io_block(var, qual);
		}
		else
		{
			add_resource_name(var.self);

			// Legacy GLSL did not support int attributes, we automatically
			// declare them as float and cast them on load/store
			SPIRType newtype = type;
			if (is_legacy() && var.storage == StorageClassInput && type.basetype == SPIRType::Int)
				newtype.basetype = SPIRType::Float;

			// Tessellation control and evaluation shaders must have either
			// gl_MaxPatchVertices or unsized arrays for input arrays.
			// Opt for unsized as it's the more "correct" variant to use.
			if (type.storage == StorageClassInput && !type.array.empty() &&
			    !has_decoration(var.self, DecorationPatch) &&
			    (get_entry_point().model == ExecutionModelTessellationControl ||
			     get_entry_point().model == ExecutionModelTessellationEvaluation))
			{
				newtype.array.back() = 0;
				newtype.array_size_literal.back() = true;
			}

			statement(layout_for_variable(var), to_qualifiers_glsl(var.self),
			          variable_decl(newtype, to_name(var.self), var.self), ";");
		}
	}
}

void CompilerGLSL::emit_uniform(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	if (type.basetype == SPIRType::Image && type.image.sampled == 2 && type.image.dim != DimSubpassData)
	{
		if (!options.es && options.version < 420)
			require_extension_internal("GL_ARB_shader_image_load_store");
		else if (options.es && options.version < 310)
			SPIRV_CROSS_THROW("At least ESSL 3.10 required for shader image load store.");
	}

	add_resource_name(var.self);
	statement(layout_for_variable(var), variable_decl(var), ";");
}

string CompilerGLSL::constant_value_macro_name(uint32_t id)
{
	return join("SPIRV_CROSS_CONSTANT_ID_", id);
}

void CompilerGLSL::emit_specialization_constant_op(const SPIRConstantOp &constant)
{
	auto &type = get<SPIRType>(constant.basetype);
	add_resource_name(constant.self);
	auto name = to_name(constant.self);
	statement("const ", variable_decl(type, name), " = ", constant_op_expression(constant), ";");
}

int CompilerGLSL::get_constant_mapping_to_workgroup_component(const SPIRConstant &c) const
{
	auto &entry_point = get_entry_point();
	int index = -1;

	// Need to redirect specialization constants which are used as WorkGroupSize to the builtin,
	// since the spec constant declarations are never explicitly declared.
	if (entry_point.workgroup_size.constant == 0 && entry_point.flags.get(ExecutionModeLocalSizeId))
	{
		if (c.self == entry_point.workgroup_size.id_x)
			index = 0;
		else if (c.self == entry_point.workgroup_size.id_y)
			index = 1;
		else if (c.self == entry_point.workgroup_size.id_z)
			index = 2;
	}

	return index;
}

void CompilerGLSL::emit_constant(const SPIRConstant &constant)
{
	auto &type = get<SPIRType>(constant.constant_type);

	SpecializationConstant wg_x, wg_y, wg_z;
	ID workgroup_size_id = get_work_group_size_specialization_constants(wg_x, wg_y, wg_z);

	// This specialization constant is implicitly declared by emitting layout() in;
	if (constant.self == workgroup_size_id)
		return;

	// These specialization constants are implicitly declared by emitting layout() in;
	// In legacy GLSL, we will still need to emit macros for these, so a layout() in; declaration
	// later can use macro overrides for work group size.
	bool is_workgroup_size_constant = ConstantID(constant.self) == wg_x.id || ConstantID(constant.self) == wg_y.id ||
	                                  ConstantID(constant.self) == wg_z.id;

	if (options.vulkan_semantics && is_workgroup_size_constant)
	{
		// Vulkan GLSL does not need to declare workgroup spec constants explicitly, it is handled in layout().
		return;
	}
	else if (!options.vulkan_semantics && is_workgroup_size_constant &&
	         !has_decoration(constant.self, DecorationSpecId))
	{
		// Only bother declaring a workgroup size if it is actually a specialization constant, because we need macros.
		return;
	}

	add_resource_name(constant.self);
	auto name = to_name(constant.self);

	// Only scalars have constant IDs.
	if (has_decoration(constant.self, DecorationSpecId))
	{
		if (options.vulkan_semantics)
		{
			statement("layout(constant_id = ", get_decoration(constant.self, DecorationSpecId), ") const ",
			          variable_decl(type, name), " = ", constant_expression(constant), ";");
		}
		else
		{
			const string &macro_name = constant.specialization_constant_macro_name;
			statement("#ifndef ", macro_name);
			statement("#define ", macro_name, " ", constant_expression(constant));
			statement("#endif");

			// For workgroup size constants, only emit the macros.
			if (!is_workgroup_size_constant)
				statement("const ", variable_decl(type, name), " = ", macro_name, ";");
		}
	}
	else
	{
		statement("const ", variable_decl(type, name), " = ", constant_expression(constant), ";");
	}
}

void CompilerGLSL::emit_entry_point_declarations()
{
}

void CompilerGLSL::replace_illegal_names(const unordered_set<string> &keywords)
{
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, const SPIRVariable &var) {
		if (is_hidden_variable(var))
			return;

		auto *meta = ir.find_meta(var.self);
		if (!meta)
			return;

		auto &m = meta->decoration;
		if (keywords.find(m.alias) != end(keywords))
			m.alias = join("_", m.alias);
	});

	ir.for_each_typed_id<SPIRFunction>([&](uint32_t, const SPIRFunction &func) {
		auto *meta = ir.find_meta(func.self);
		if (!meta)
			return;

		auto &m = meta->decoration;
		if (keywords.find(m.alias) != end(keywords))
			m.alias = join("_", m.alias);
	});

	ir.for_each_typed_id<SPIRType>([&](uint32_t, const SPIRType &type) {
		auto *meta = ir.find_meta(type.self);
		if (!meta)
			return;

		auto &m = meta->decoration;
		if (keywords.find(m.alias) != end(keywords))
			m.alias = join("_", m.alias);

		for (auto &memb : meta->members)
			if (keywords.find(memb.alias) != end(keywords))
				memb.alias = join("_", memb.alias);
	});
}

void CompilerGLSL::replace_illegal_names()
{
	// clang-format off
	static const unordered_set<string> keywords = {
		"abs", "acos", "acosh", "all", "any", "asin", "asinh", "atan", "atanh",
		"atomicAdd", "atomicCompSwap", "atomicCounter", "atomicCounterDecrement", "atomicCounterIncrement",
		"atomicExchange", "atomicMax", "atomicMin", "atomicOr", "atomicXor",
		"bitCount", "bitfieldExtract", "bitfieldInsert", "bitfieldReverse",
		"ceil", "cos", "cosh", "cross", "degrees",
		"dFdx", "dFdxCoarse", "dFdxFine",
		"dFdy", "dFdyCoarse", "dFdyFine",
		"distance", "dot", "EmitStreamVertex", "EmitVertex", "EndPrimitive", "EndStreamPrimitive", "equal", "exp", "exp2",
		"faceforward", "findLSB", "findMSB", "float16BitsToInt16", "float16BitsToUint16", "floatBitsToInt", "floatBitsToUint", "floor", "fma", "fract",
		"frexp", "fwidth", "fwidthCoarse", "fwidthFine",
		"greaterThan", "greaterThanEqual", "groupMemoryBarrier",
		"imageAtomicAdd", "imageAtomicAnd", "imageAtomicCompSwap", "imageAtomicExchange", "imageAtomicMax", "imageAtomicMin", "imageAtomicOr", "imageAtomicXor",
		"imageLoad", "imageSamples", "imageSize", "imageStore", "imulExtended", "int16BitsToFloat16", "intBitsToFloat", "interpolateAtOffset", "interpolateAtCentroid", "interpolateAtSample",
		"inverse", "inversesqrt", "isinf", "isnan", "ldexp", "length", "lessThan", "lessThanEqual", "log", "log2",
		"matrixCompMult", "max", "memoryBarrier", "memoryBarrierAtomicCounter", "memoryBarrierBuffer", "memoryBarrierImage", "memoryBarrierShared",
		"min", "mix", "mod", "modf", "noise", "noise1", "noise2", "noise3", "noise4", "normalize", "not", "notEqual",
		"outerProduct", "packDouble2x32", "packHalf2x16", "packInt2x16", "packInt4x16", "packSnorm2x16", "packSnorm4x8",
		"packUint2x16", "packUint4x16", "packUnorm2x16", "packUnorm4x8", "pow",
		"radians", "reflect", "refract", "round", "roundEven", "sign", "sin", "sinh", "smoothstep", "sqrt", "step",
		"tan", "tanh", "texelFetch", "texelFetchOffset", "texture", "textureGather", "textureGatherOffset", "textureGatherOffsets",
		"textureGrad", "textureGradOffset", "textureLod", "textureLodOffset", "textureOffset", "textureProj", "textureProjGrad",
		"textureProjGradOffset", "textureProjLod", "textureProjLodOffset", "textureProjOffset", "textureQueryLevels", "textureQueryLod", "textureSamples", "textureSize",
		"transpose", "trunc", "uaddCarry", "uint16BitsToFloat16", "uintBitsToFloat", "umulExtended", "unpackDouble2x32", "unpackHalf2x16", "unpackInt2x16", "unpackInt4x16",
		"unpackSnorm2x16", "unpackSnorm4x8", "unpackUint2x16", "unpackUint4x16", "unpackUnorm2x16", "unpackUnorm4x8", "usubBorrow",

		"active", "asm", "atomic_uint", "attribute", "bool", "break", "buffer",
		"bvec2", "bvec3", "bvec4", "case", "cast", "centroid", "class", "coherent", "common", "const", "continue", "default", "discard",
		"dmat2", "dmat2x2", "dmat2x3", "dmat2x4", "dmat3", "dmat3x2", "dmat3x3", "dmat3x4", "dmat4", "dmat4x2", "dmat4x3", "dmat4x4",
		"do", "double", "dvec2", "dvec3", "dvec4", "else", "enum", "extern", "external", "false", "filter", "fixed", "flat", "float",
		"for", "fvec2", "fvec3", "fvec4", "goto", "half", "highp", "hvec2", "hvec3", "hvec4", "if", "iimage1D", "iimage1DArray",
		"iimage2D", "iimage2DArray", "iimage2DMS", "iimage2DMSArray", "iimage2DRect", "iimage3D", "iimageBuffer", "iimageCube",
		"iimageCubeArray", "image1D", "image1DArray", "image2D", "image2DArray", "image2DMS", "image2DMSArray", "image2DRect",
		"image3D", "imageBuffer", "imageCube", "imageCubeArray", "in", "inline", "inout", "input", "int", "interface", "invariant",
		"isampler1D", "isampler1DArray", "isampler2D", "isampler2DArray", "isampler2DMS", "isampler2DMSArray", "isampler2DRect",
		"isampler3D", "isamplerBuffer", "isamplerCube", "isamplerCubeArray", "ivec2", "ivec3", "ivec4", "layout", "long", "lowp",
		"mat2", "mat2x2", "mat2x3", "mat2x4", "mat3", "mat3x2", "mat3x3", "mat3x4", "mat4", "mat4x2", "mat4x3", "mat4x4", "mediump",
		"namespace", "noinline", "noperspective", "out", "output", "packed", "partition", "patch", "precise", "precision", "public", "readonly",
		"resource", "restrict", "return", "sample", "sampler1D", "sampler1DArray", "sampler1DArrayShadow",
		"sampler1DShadow", "sampler2D", "sampler2DArray", "sampler2DArrayShadow", "sampler2DMS", "sampler2DMSArray",
		"sampler2DRect", "sampler2DRectShadow", "sampler2DShadow", "sampler3D", "sampler3DRect", "samplerBuffer",
		"samplerCube", "samplerCubeArray", "samplerCubeArrayShadow", "samplerCubeShadow", "shared", "short", "sizeof", "smooth", "static",
		"struct", "subroutine", "superp", "switch", "template", "this", "true", "typedef", "uimage1D", "uimage1DArray", "uimage2D",
		"uimage2DArray", "uimage2DMS", "uimage2DMSArray", "uimage2DRect", "uimage3D", "uimageBuffer", "uimageCube",
		"uimageCubeArray", "uint", "uniform", "union", "unsigned", "usampler1D", "usampler1DArray", "usampler2D", "usampler2DArray",
		"usampler2DMS", "usampler2DMSArray", "usampler2DRect", "usampler3D", "usamplerBuffer", "usamplerCube",
		"usamplerCubeArray", "using", "uvec2", "uvec3", "uvec4", "varying", "vec2", "vec3", "vec4", "void", "volatile",
		"while", "writeonly",
	};
	// clang-format on

	replace_illegal_names(keywords);
}

void CompilerGLSL::replace_fragment_output(SPIRVariable &var)
{
	auto &m = ir.meta[var.self].decoration;
	uint32_t location = 0;
	if (m.decoration_flags.get(DecorationLocation))
		location = m.location;

	// If our variable is arrayed, we must not emit the array part of this as the SPIR-V will
	// do the access chain part of this for us.
	auto &type = get<SPIRType>(var.basetype);

	if (type.array.empty())
	{
		// Redirect the write to a specific render target in legacy GLSL.
		m.alias = join("gl_FragData[", location, "]");

		if (is_legacy_es() && location != 0)
			require_extension_internal("GL_EXT_draw_buffers");
	}
	else if (type.array.size() == 1)
	{
		// If location is non-zero, we probably have to add an offset.
		// This gets really tricky since we'd have to inject an offset in the access chain.
		// FIXME: This seems like an extremely odd-ball case, so it's probably fine to leave it like this for now.
		m.alias = "gl_FragData";
		if (location != 0)
			SPIRV_CROSS_THROW("Arrayed output variable used, but location is not 0. "
			                  "This is unimplemented in SPIRV-Cross.");

		if (is_legacy_es())
			require_extension_internal("GL_EXT_draw_buffers");
	}
	else
		SPIRV_CROSS_THROW("Array-of-array output variable used. This cannot be implemented in legacy GLSL.");

	var.compat_builtin = true; // We don't want to declare this variable, but use the name as-is.
}

void CompilerGLSL::replace_fragment_outputs()
{
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);

		if (!is_builtin_variable(var) && !var.remapped_variable && type.pointer && var.storage == StorageClassOutput)
			replace_fragment_output(var);
	});
}

string CompilerGLSL::remap_swizzle(const SPIRType &out_type, uint32_t input_components, const string &expr)
{
	if (out_type.vecsize == input_components)
		return expr;
	else if (input_components == 1 && !backend.can_swizzle_scalar)
		return join(type_to_glsl(out_type), "(", expr, ")");
	else
	{
		// FIXME: This will not work with packed expressions.
		auto e = enclose_expression(expr) + ".";
		// Just clamp the swizzle index if we have more outputs than inputs.
		for (uint32_t c = 0; c < out_type.vecsize; c++)
			e += index_to_swizzle(min(c, input_components - 1));
		if (backend.swizzle_is_function && out_type.vecsize > 1)
			e += "()";

		remove_duplicate_swizzle(e);
		return e;
	}
}

void CompilerGLSL::emit_pls()
{
	auto &execution = get_entry_point();
	if (execution.model != ExecutionModelFragment)
		SPIRV_CROSS_THROW("Pixel local storage only supported in fragment shaders.");

	if (!options.es)
		SPIRV_CROSS_THROW("Pixel local storage only supported in OpenGL ES.");

	if (options.version < 300)
		SPIRV_CROSS_THROW("Pixel local storage only supported in ESSL 3.0 and above.");

	if (!pls_inputs.empty())
	{
		statement("__pixel_local_inEXT _PLSIn");
		begin_scope();
		for (auto &input : pls_inputs)
			statement(pls_decl(input), ";");
		end_scope_decl();
		statement("");
	}

	if (!pls_outputs.empty())
	{
		statement("__pixel_local_outEXT _PLSOut");
		begin_scope();
		for (auto &output : pls_outputs)
			statement(pls_decl(output), ";");
		end_scope_decl();
		statement("");
	}
}

void CompilerGLSL::fixup_image_load_store_access()
{
	if (!options.enable_storage_image_qualifier_deduction)
		return;

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t var, const SPIRVariable &) {
		auto &vartype = expression_type(var);
		if (vartype.basetype == SPIRType::Image && vartype.image.sampled == 2)
		{
			// Very old glslangValidator and HLSL compilers do not emit required qualifiers here.
			// Solve this by making the image access as restricted as possible and loosen up if we need to.
			// If any no-read/no-write flags are actually set, assume that the compiler knows what it's doing.

			if (!has_decoration(var, DecorationNonWritable) && !has_decoration(var, DecorationNonReadable))
			{
				set_decoration(var, DecorationNonWritable);
				set_decoration(var, DecorationNonReadable);
			}
		}
	});
}

static bool is_block_builtin(BuiltIn builtin)
{
	return builtin == BuiltInPosition || builtin == BuiltInPointSize || builtin == BuiltInClipDistance ||
	       builtin == BuiltInCullDistance;
}

bool CompilerGLSL::should_force_emit_builtin_block(StorageClass storage)
{
	// If the builtin block uses XFB, we need to force explicit redeclaration of the builtin block.

	if (storage != StorageClassOutput)
		return false;
	bool should_force = false;

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		if (should_force)
			return;

		auto &type = this->get<SPIRType>(var.basetype);
		bool block = has_decoration(type.self, DecorationBlock);
		if (var.storage == storage && block && is_builtin_variable(var))
		{
			uint32_t member_count = uint32_t(type.member_types.size());
			for (uint32_t i = 0; i < member_count; i++)
			{
				if (has_member_decoration(type.self, i, DecorationBuiltIn) &&
				    is_block_builtin(BuiltIn(get_member_decoration(type.self, i, DecorationBuiltIn))) &&
				    has_member_decoration(type.self, i, DecorationOffset))
				{
					should_force = true;
				}
			}
		}
		else if (var.storage == storage && !block && is_builtin_variable(var))
		{
			if (is_block_builtin(BuiltIn(get_decoration(type.self, DecorationBuiltIn))) &&
			    has_decoration(var.self, DecorationOffset))
			{
				should_force = true;
			}
		}
	});

	// If we're declaring clip/cull planes with control points we need to force block declaration.
	if ((get_execution_model() == ExecutionModelTessellationControl ||
	     get_execution_model() == ExecutionModelMeshEXT) &&
	    (clip_distance_count || cull_distance_count))
	{
		should_force = true;
	}

	return should_force;
}

void CompilerGLSL::fixup_implicit_builtin_block_names(ExecutionModel model)
{
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);
		bool block = has_decoration(type.self, DecorationBlock);
		if ((var.storage == StorageClassOutput || var.storage == StorageClassInput) && block &&
		    is_builtin_variable(var))
		{
			if (model != ExecutionModelMeshEXT)
			{
				// Make sure the array has a supported name in the code.
				if (var.storage == StorageClassOutput)
					set_name(var.self, "gl_out");
				else if (var.storage == StorageClassInput)
					set_name(var.self, "gl_in");
			}
			else
			{
				auto flags = get_buffer_block_flags(var.self);
				if (flags.get(DecorationPerPrimitiveEXT))
				{
					set_name(var.self, "gl_MeshPrimitivesEXT");
					set_name(type.self, "gl_MeshPerPrimitiveEXT");
				}
				else
				{
					set_name(var.self, "gl_MeshVerticesEXT");
					set_name(type.self, "gl_MeshPerVertexEXT");
				}
			}
		}

		if (model == ExecutionModelMeshEXT && var.storage == StorageClassOutput && !block)
		{
			auto *m = ir.find_meta(var.self);
			if (m && m->decoration.builtin)
			{
				auto builtin_type = m->decoration.builtin_type;
				if (builtin_type == BuiltInPrimitivePointIndicesEXT)
					set_name(var.self, "gl_PrimitivePointIndicesEXT");
				else if (builtin_type == BuiltInPrimitiveLineIndicesEXT)
					set_name(var.self, "gl_PrimitiveLineIndicesEXT");
				else if (builtin_type == BuiltInPrimitiveTriangleIndicesEXT)
					set_name(var.self, "gl_PrimitiveTriangleIndicesEXT");
			}
		}
	});
}

void CompilerGLSL::emit_declared_builtin_block(StorageClass storage, ExecutionModel model)
{
	Bitset emitted_builtins;
	Bitset global_builtins;
	const SPIRVariable *block_var = nullptr;
	bool emitted_block = false;

	// Need to use declared size in the type.
	// These variables might have been declared, but not statically used, so we haven't deduced their size yet.
	uint32_t cull_distance_size = 0;
	uint32_t clip_distance_size = 0;

	bool have_xfb_buffer_stride = false;
	bool have_geom_stream = false;
	bool have_any_xfb_offset = false;
	uint32_t xfb_stride = 0, xfb_buffer = 0, geom_stream = 0;
	std::unordered_map<uint32_t, uint32_t> builtin_xfb_offsets;

	const auto builtin_is_per_vertex_set = [](BuiltIn builtin) -> bool {
		return builtin == BuiltInPosition || builtin == BuiltInPointSize ||
			builtin == BuiltInClipDistance || builtin == BuiltInCullDistance;
	};

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);
		bool block = has_decoration(type.self, DecorationBlock);
		Bitset builtins;

		if (var.storage == storage && block && is_builtin_variable(var))
		{
			uint32_t index = 0;
			for (auto &m : ir.meta[type.self].members)
			{
				if (m.builtin && builtin_is_per_vertex_set(m.builtin_type))
				{
					builtins.set(m.builtin_type);
					if (m.builtin_type == BuiltInCullDistance)
						cull_distance_size = to_array_size_literal(this->get<SPIRType>(type.member_types[index]));
					else if (m.builtin_type == BuiltInClipDistance)
						clip_distance_size = to_array_size_literal(this->get<SPIRType>(type.member_types[index]));

					if (is_block_builtin(m.builtin_type) && m.decoration_flags.get(DecorationOffset))
					{
						have_any_xfb_offset = true;
						builtin_xfb_offsets[m.builtin_type] = m.offset;
					}

					if (is_block_builtin(m.builtin_type) && m.decoration_flags.get(DecorationStream))
					{
						uint32_t stream = m.stream;
						if (have_geom_stream && geom_stream != stream)
							SPIRV_CROSS_THROW("IO block member Stream mismatch.");
						have_geom_stream = true;
						geom_stream = stream;
					}
				}
				index++;
			}

			if (storage == StorageClassOutput && has_decoration(var.self, DecorationXfbBuffer) &&
			    has_decoration(var.self, DecorationXfbStride))
			{
				uint32_t buffer_index = get_decoration(var.self, DecorationXfbBuffer);
				uint32_t stride = get_decoration(var.self, DecorationXfbStride);
				if (have_xfb_buffer_stride && buffer_index != xfb_buffer)
					SPIRV_CROSS_THROW("IO block member XfbBuffer mismatch.");
				if (have_xfb_buffer_stride && stride != xfb_stride)
					SPIRV_CROSS_THROW("IO block member XfbBuffer mismatch.");
				have_xfb_buffer_stride = true;
				xfb_buffer = buffer_index;
				xfb_stride = stride;
			}

			if (storage == StorageClassOutput && has_decoration(var.self, DecorationStream))
			{
				uint32_t stream = get_decoration(var.self, DecorationStream);
				if (have_geom_stream && geom_stream != stream)
					SPIRV_CROSS_THROW("IO block member Stream mismatch.");
				have_geom_stream = true;
				geom_stream = stream;
			}
		}
		else if (var.storage == storage && !block && is_builtin_variable(var))
		{
			// While we're at it, collect all declared global builtins (HLSL mostly ...).
			auto &m = ir.meta[var.self].decoration;
			if (m.builtin && builtin_is_per_vertex_set(m.builtin_type))
			{
				global_builtins.set(m.builtin_type);
				if (m.builtin_type == BuiltInCullDistance)
					cull_distance_size = to_array_size_literal(type);
				else if (m.builtin_type == BuiltInClipDistance)
					clip_distance_size = to_array_size_literal(type);

				if (is_block_builtin(m.builtin_type) && m.decoration_flags.get(DecorationXfbStride) &&
				    m.decoration_flags.get(DecorationXfbBuffer) && m.decoration_flags.get(DecorationOffset))
				{
					have_any_xfb_offset = true;
					builtin_xfb_offsets[m.builtin_type] = m.offset;
					uint32_t buffer_index = m.xfb_buffer;
					uint32_t stride = m.xfb_stride;
					if (have_xfb_buffer_stride && buffer_index != xfb_buffer)
						SPIRV_CROSS_THROW("IO block member XfbBuffer mismatch.");
					if (have_xfb_buffer_stride && stride != xfb_stride)
						SPIRV_CROSS_THROW("IO block member XfbBuffer mismatch.");
					have_xfb_buffer_stride = true;
					xfb_buffer = buffer_index;
					xfb_stride = stride;
				}

				if (is_block_builtin(m.builtin_type) && m.decoration_flags.get(DecorationStream))
				{
					uint32_t stream = get_decoration(var.self, DecorationStream);
					if (have_geom_stream && geom_stream != stream)
						SPIRV_CROSS_THROW("IO block member Stream mismatch.");
					have_geom_stream = true;
					geom_stream = stream;
				}
			}
		}

		if (builtins.empty())
			return;

		if (emitted_block)
			SPIRV_CROSS_THROW("Cannot use more than one builtin I/O block.");

		emitted_builtins = builtins;
		emitted_block = true;
		block_var = &var;
	});

	global_builtins =
	    Bitset(global_builtins.get_lower() & ((1ull << BuiltInPosition) | (1ull << BuiltInPointSize) |
	                                          (1ull << BuiltInClipDistance) | (1ull << BuiltInCullDistance)));

	// Try to collect all other declared builtins.
	if (!emitted_block)
		emitted_builtins = global_builtins;

	// Can't declare an empty interface block.
	if (emitted_builtins.empty())
		return;

	if (storage == StorageClassOutput)
	{
		SmallVector<string> attr;
		if (have_xfb_buffer_stride && have_any_xfb_offset)
		{
			if (!options.es)
			{
				if (options.version < 440 && options.version >= 140)
					require_extension_internal("GL_ARB_enhanced_layouts");
				else if (options.version < 140)
					SPIRV_CROSS_THROW("Component decoration is not supported in targets below GLSL 1.40.");
				if (!options.es && options.version < 440)
					require_extension_internal("GL_ARB_enhanced_layouts");
			}
			else if (options.es)
				SPIRV_CROSS_THROW("Need GL_ARB_enhanced_layouts for xfb_stride or xfb_buffer.");
			attr.push_back(join("xfb_buffer = ", xfb_buffer, ", xfb_stride = ", xfb_stride));
		}

		if (have_geom_stream)
		{
			if (get_execution_model() != ExecutionModelGeometry)
				SPIRV_CROSS_THROW("Geometry streams can only be used in geometry shaders.");
			if (options.es)
				SPIRV_CROSS_THROW("Multiple geometry streams not supported in ESSL.");
			if (options.version < 400)
				require_extension_internal("GL_ARB_transform_feedback3");
			attr.push_back(join("stream = ", geom_stream));
		}

		if (model == ExecutionModelMeshEXT)
			statement("out gl_MeshPerVertexEXT");
		else if (!attr.empty())
			statement("layout(", merge(attr), ") out gl_PerVertex");
		else
			statement("out gl_PerVertex");
	}
	else
	{
		// If we have passthrough, there is no way PerVertex cannot be passthrough.
		if (get_entry_point().geometry_passthrough)
			statement("layout(passthrough) in gl_PerVertex");
		else
			statement("in gl_PerVertex");
	}

	begin_scope();
	if (emitted_builtins.get(BuiltInPosition))
	{
		auto itr = builtin_xfb_offsets.find(BuiltInPosition);
		if (itr != end(builtin_xfb_offsets))
			statement("layout(xfb_offset = ", itr->second, ") vec4 gl_Position;");
		else
			statement("vec4 gl_Position;");
	}

	if (emitted_builtins.get(BuiltInPointSize))
	{
		auto itr = builtin_xfb_offsets.find(BuiltInPointSize);
		if (itr != end(builtin_xfb_offsets))
			statement("layout(xfb_offset = ", itr->second, ") float gl_PointSize;");
		else
			statement("float gl_PointSize;");
	}

	if (emitted_builtins.get(BuiltInClipDistance))
	{
		auto itr = builtin_xfb_offsets.find(BuiltInClipDistance);
		if (itr != end(builtin_xfb_offsets))
			statement("layout(xfb_offset = ", itr->second, ") float gl_ClipDistance[", clip_distance_size, "];");
		else
			statement("float gl_ClipDistance[", clip_distance_size, "];");
	}

	if (emitted_builtins.get(BuiltInCullDistance))
	{
		auto itr = builtin_xfb_offsets.find(BuiltInCullDistance);
		if (itr != end(builtin_xfb_offsets))
			statement("layout(xfb_offset = ", itr->second, ") float gl_CullDistance[", cull_distance_size, "];");
		else
			statement("float gl_CullDistance[", cull_distance_size, "];");
	}

	bool builtin_array = model == ExecutionModelTessellationControl ||
	                     (model == ExecutionModelMeshEXT && storage == StorageClassOutput) ||
	                     (model == ExecutionModelGeometry && storage == StorageClassInput) ||
	                     (model == ExecutionModelTessellationEvaluation && storage == StorageClassInput);

	if (builtin_array)
	{
		const char *instance_name;
		if (model == ExecutionModelMeshEXT)
			instance_name = "gl_MeshVerticesEXT"; // Per primitive is never synthesized.
		else
			instance_name = storage == StorageClassInput ? "gl_in" : "gl_out";

		if (model == ExecutionModelTessellationControl && storage == StorageClassOutput)
			end_scope_decl(join(instance_name, "[", get_entry_point().output_vertices, "]"));
		else
			end_scope_decl(join(instance_name, "[]"));
	}
	else
		end_scope_decl();
	statement("");
}

bool CompilerGLSL::variable_is_lut(const SPIRVariable &var) const
{
	bool statically_assigned = var.statically_assigned && var.static_expression != ID(0) && var.remapped_variable;

	if (statically_assigned)
	{
		auto *constant = maybe_get<SPIRConstant>(var.static_expression);
		if (constant && constant->is_used_as_lut)
			return true;
	}

	return false;
}

void CompilerGLSL::emit_resources()
{
	auto &execution = get_entry_point();

	replace_illegal_names();

	// Legacy GL uses gl_FragData[], redeclare all fragment outputs
	// with builtins.
	if (execution.model == ExecutionModelFragment && is_legacy())
		replace_fragment_outputs();

	// Emit PLS blocks if we have such variables.
	if (!pls_inputs.empty() || !pls_outputs.empty())
		emit_pls();

	switch (execution.model)
	{
	case ExecutionModelGeometry:
	case ExecutionModelTessellationControl:
	case ExecutionModelTessellationEvaluation:
	case ExecutionModelMeshEXT:
		fixup_implicit_builtin_block_names(execution.model);
		break;

	default:
		break;
	}

	// Emit custom gl_PerVertex for SSO compatibility.
	if (options.separate_shader_objects && !options.es && execution.model != ExecutionModelFragment)
	{
		switch (execution.model)
		{
		case ExecutionModelGeometry:
		case ExecutionModelTessellationControl:
		case ExecutionModelTessellationEvaluation:
			emit_declared_builtin_block(StorageClassInput, execution.model);
			emit_declared_builtin_block(StorageClassOutput, execution.model);
			break;

		case ExecutionModelVertex:
		case ExecutionModelMeshEXT:
			emit_declared_builtin_block(StorageClassOutput, execution.model);
			break;

		default:
			break;
		}
	}
	else if (should_force_emit_builtin_block(StorageClassOutput))
	{
		emit_declared_builtin_block(StorageClassOutput, execution.model);
	}
	else if (execution.geometry_passthrough)
	{
		// Need to declare gl_in with Passthrough.
		// If we're doing passthrough, we cannot emit an output block, so the output block test above will never pass.
		emit_declared_builtin_block(StorageClassInput, execution.model);
	}
	else
	{
		// Need to redeclare clip/cull distance with explicit size to use them.
		// SPIR-V mandates these builtins have a size declared.
		const char *storage = execution.model == ExecutionModelFragment ? "in" : "out";
		if (clip_distance_count != 0)
			statement(storage, " float gl_ClipDistance[", clip_distance_count, "];");
		if (cull_distance_count != 0)
			statement(storage, " float gl_CullDistance[", cull_distance_count, "];");
		if (clip_distance_count != 0 || cull_distance_count != 0)
			statement("");
	}

	if (position_invariant && (options.es || options.version >= 120))
	{
		statement("invariant gl_Position;");
		statement("");
	}

	bool emitted = false;

	// If emitted Vulkan GLSL,
	// emit specialization constants as actual floats,
	// spec op expressions will redirect to the constant name.
	//
	{
		auto loop_lock = ir.create_loop_hard_lock();
		for (auto &id_ : ir.ids_for_constant_undef_or_type)
		{
			auto &id = ir.ids[id_];

			if (id.get_type() == TypeConstant)
			{
				auto &c = id.get<SPIRConstant>();

				bool needs_declaration = c.specialization || c.is_used_as_lut;

				if (needs_declaration)
				{
					if (!options.vulkan_semantics && c.specialization)
					{
						c.specialization_constant_macro_name =
						    constant_value_macro_name(get_decoration(c.self, DecorationSpecId));
					}
					emit_constant(c);
					emitted = true;
				}
			}
			else if (id.get_type() == TypeConstantOp)
			{
				emit_specialization_constant_op(id.get<SPIRConstantOp>());
				emitted = true;
			}
			else if (id.get_type() == TypeType)
			{
				auto *type = &id.get<SPIRType>();

				bool is_natural_struct = type->basetype == SPIRType::Struct && type->array.empty() && !type->pointer &&
				                         (!has_decoration(type->self, DecorationBlock) &&
				                          !has_decoration(type->self, DecorationBufferBlock));

				// Special case, ray payload and hit attribute blocks are not really blocks, just regular structs.
				if (type->basetype == SPIRType::Struct && type->pointer &&
				    has_decoration(type->self, DecorationBlock) &&
				    (type->storage == StorageClassRayPayloadKHR || type->storage == StorageClassIncomingRayPayloadKHR ||
				     type->storage == StorageClassHitAttributeKHR))
				{
					type = &get<SPIRType>(type->parent_type);
					is_natural_struct = true;
				}

				if (is_natural_struct)
				{
					if (emitted)
						statement("");
					emitted = false;

					emit_struct(*type);
				}
			}
			else if (id.get_type() == TypeUndef)
			{
				auto &undef = id.get<SPIRUndef>();
				auto &type = this->get<SPIRType>(undef.basetype);
				// OpUndef can be void for some reason ...
				if (type.basetype == SPIRType::Void)
					return;

				string initializer;
				if (options.force_zero_initialized_variables && type_can_zero_initialize(type))
					initializer = join(" = ", to_zero_initialized_expression(undef.basetype));

				// FIXME: If used in a constant, we must declare it as one.
				statement(variable_decl(type, to_name(undef.self), undef.self), initializer, ";");
				emitted = true;
			}
		}
	}

	if (emitted)
		statement("");

	// If we needed to declare work group size late, check here.
	// If the work group size depends on a specialization constant, we need to declare the layout() block
	// after constants (and their macros) have been declared.
	if (execution.model == ExecutionModelGLCompute && !options.vulkan_semantics &&
	    (execution.workgroup_size.constant != 0 || execution.flags.get(ExecutionModeLocalSizeId)))
	{
		SpecializationConstant wg_x, wg_y, wg_z;
		get_work_group_size_specialization_constants(wg_x, wg_y, wg_z);

		if ((wg_x.id != ConstantID(0)) || (wg_y.id != ConstantID(0)) || (wg_z.id != ConstantID(0)))
		{
			SmallVector<string> inputs;
			build_workgroup_size(inputs, wg_x, wg_y, wg_z);
			statement("layout(", merge(inputs), ") in;");
			statement("");
		}
	}

	emitted = false;

	if (ir.addressing_model == AddressingModelPhysicalStorageBuffer64EXT)
	{
		for (auto type : physical_storage_non_block_pointer_types)
		{
			emit_buffer_reference_block(type, false);
		}

		// Output buffer reference blocks.
		// Do this in two stages, one with forward declaration,
		// and one without. Buffer reference blocks can reference themselves
		// to support things like linked lists.
		ir.for_each_typed_id<SPIRType>([&](uint32_t self, SPIRType &type) {
			if (type.basetype == SPIRType::Struct && type.pointer &&
			    type.pointer_depth == 1 && !type_is_array_of_pointers(type) &&
			    type.storage == StorageClassPhysicalStorageBufferEXT)
			{
				emit_buffer_reference_block(self, true);
			}
		});

		ir.for_each_typed_id<SPIRType>([&](uint32_t self, SPIRType &type) {
			if (type.basetype == SPIRType::Struct &&
			    type.pointer && type.pointer_depth == 1 && !type_is_array_of_pointers(type) &&
			    type.storage == StorageClassPhysicalStorageBufferEXT)
			{
				emit_buffer_reference_block(self, false);
			}
		});
	}

	// Output UBOs and SSBOs
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);

		bool is_block_storage = type.storage == StorageClassStorageBuffer || type.storage == StorageClassUniform ||
		                        type.storage == StorageClassShaderRecordBufferKHR;
		bool has_block_flags = ir.meta[type.self].decoration.decoration_flags.get(DecorationBlock) ||
		                       ir.meta[type.self].decoration.decoration_flags.get(DecorationBufferBlock);

		if (var.storage != StorageClassFunction && type.pointer && is_block_storage && !is_hidden_variable(var) &&
		    has_block_flags)
		{
			emit_buffer_block(var);
		}
	});

	// Output push constant blocks
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);
		if (var.storage != StorageClassFunction && type.pointer && type.storage == StorageClassPushConstant &&
		    !is_hidden_variable(var))
		{
			emit_push_constant_block(var);
		}
	});

	bool skip_separate_image_sampler = !combined_image_samplers.empty() || !options.vulkan_semantics;

	// Output Uniform Constants (values, samplers, images, etc).
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);

		// If we're remapping separate samplers and images, only emit the combined samplers.
		if (skip_separate_image_sampler)
		{
			// Sampler buffers are always used without a sampler, and they will also work in regular GL.
			bool sampler_buffer = type.basetype == SPIRType::Image && type.image.dim == DimBuffer;
			bool separate_image = type.basetype == SPIRType::Image && type.image.sampled == 1;
			bool separate_sampler = type.basetype == SPIRType::Sampler;
			if (!sampler_buffer && (separate_image || separate_sampler))
				return;
		}

		if (var.storage != StorageClassFunction && type.pointer &&
		    (type.storage == StorageClassUniformConstant || type.storage == StorageClassAtomicCounter ||
		     type.storage == StorageClassRayPayloadKHR || type.storage == StorageClassIncomingRayPayloadKHR ||
		     type.storage == StorageClassCallableDataKHR || type.storage == StorageClassIncomingCallableDataKHR ||
		     type.storage == StorageClassHitAttributeKHR) &&
		    !is_hidden_variable(var))
		{
			emit_uniform(var);
			emitted = true;
		}
	});

	if (emitted)
		statement("");
	emitted = false;

	bool emitted_base_instance = false;

	// Output in/out interfaces.
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);

		bool is_hidden = is_hidden_variable(var);

		// Unused output I/O variables might still be required to implement framebuffer fetch.
		if (var.storage == StorageClassOutput && !is_legacy() &&
		    location_is_framebuffer_fetch(get_decoration(var.self, DecorationLocation)) != 0)
		{
			is_hidden = false;
		}

		if (var.storage != StorageClassFunction && type.pointer &&
		    (var.storage == StorageClassInput || var.storage == StorageClassOutput) &&
		    interface_variable_exists_in_entry_point(var.self) && !is_hidden)
		{
			if (options.es && get_execution_model() == ExecutionModelVertex && var.storage == StorageClassInput &&
			    type.array.size() == 1)
			{
				SPIRV_CROSS_THROW("OpenGL ES doesn't support array input variables in vertex shader.");
			}
			emit_interface_block(var);
			emitted = true;
		}
		else if (is_builtin_variable(var))
		{
			auto builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
			// For gl_InstanceIndex emulation on GLES, the API user needs to
			// supply this uniform.

			// The draw parameter extension is soft-enabled on GL with some fallbacks.
			if (!options.vulkan_semantics)
			{
				if (!emitted_base_instance &&
				    ((options.vertex.support_nonzero_base_instance && builtin == BuiltInInstanceIndex) ||
				     (builtin == BuiltInBaseInstance)))
				{
					statement("#ifdef GL_ARB_shader_draw_parameters");
					statement("#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB");
					statement("#else");
					// A crude, but simple workaround which should be good enough for non-indirect draws.
					statement("uniform int SPIRV_Cross_BaseInstance;");
					statement("#endif");
					emitted = true;
					emitted_base_instance = true;
				}
				else if (builtin == BuiltInBaseVertex)
				{
					statement("#ifdef GL_ARB_shader_draw_parameters");
					statement("#define SPIRV_Cross_BaseVertex gl_BaseVertexARB");
					statement("#else");
					// A crude, but simple workaround which should be good enough for non-indirect draws.
					statement("uniform int SPIRV_Cross_BaseVertex;");
					statement("#endif");
				}
				else if (builtin == BuiltInDrawIndex)
				{
					statement("#ifndef GL_ARB_shader_draw_parameters");
					// Cannot really be worked around.
					statement("#error GL_ARB_shader_draw_parameters is not supported.");
					statement("#endif");
				}
			}
		}
	});

	// Global variables.
	for (auto global : global_variables)
	{
		auto &var = get<SPIRVariable>(global);
		if (is_hidden_variable(var, true))
			continue;

		if (var.storage != StorageClassOutput)
		{
			if (!variable_is_lut(var))
			{
				add_resource_name(var.self);

				string initializer;
				if (options.force_zero_initialized_variables && var.storage == StorageClassPrivate &&
				    !var.initializer && !var.static_expression && type_can_zero_initialize(get_variable_data_type(var)))
				{
					initializer = join(" = ", to_zero_initialized_expression(get_variable_data_type_id(var)));
				}

				statement(variable_decl(var), initializer, ";");
				emitted = true;
			}
		}
		else if (var.initializer && maybe_get<SPIRConstant>(var.initializer) != nullptr)
		{
			emit_output_variable_initializer(var);
		}
	}

	if (emitted)
		statement("");
}

void CompilerGLSL::emit_output_variable_initializer(const SPIRVariable &var)
{
	// If a StorageClassOutput variable has an initializer, we need to initialize it in main().
	auto &entry_func = this->get<SPIRFunction>(ir.default_entry_point);
	auto &type = get<SPIRType>(var.basetype);
	bool is_patch = has_decoration(var.self, DecorationPatch);
	bool is_block = has_decoration(type.self, DecorationBlock);
	bool is_control_point = get_execution_model() == ExecutionModelTessellationControl && !is_patch;

	if (is_block)
	{
		uint32_t member_count = uint32_t(type.member_types.size());
		bool type_is_array = type.array.size() == 1;
		uint32_t array_size = 1;
		if (type_is_array)
			array_size = to_array_size_literal(type);
		uint32_t iteration_count = is_control_point ? 1 : array_size;

		// If the initializer is a block, we must initialize each block member one at a time.
		for (uint32_t i = 0; i < member_count; i++)
		{
			// These outputs might not have been properly declared, so don't initialize them in that case.
			if (has_member_decoration(type.self, i, DecorationBuiltIn))
			{
				if (get_member_decoration(type.self, i, DecorationBuiltIn) == BuiltInCullDistance &&
				    !cull_distance_count)
					continue;

				if (get_member_decoration(type.self, i, DecorationBuiltIn) == BuiltInClipDistance &&
				    !clip_distance_count)
					continue;
			}

			// We need to build a per-member array first, essentially transposing from AoS to SoA.
			// This code path hits when we have an array of blocks.
			string lut_name;
			if (type_is_array)
			{
				lut_name = join("_", var.self, "_", i, "_init");
				uint32_t member_type_id = get<SPIRType>(var.basetype).member_types[i];
				auto &member_type = get<SPIRType>(member_type_id);
				auto array_type = member_type;
				array_type.parent_type = member_type_id;
				array_type.array.push_back(array_size);
				array_type.array_size_literal.push_back(true);

				SmallVector<string> exprs;
				exprs.reserve(array_size);
				auto &c = get<SPIRConstant>(var.initializer);
				for (uint32_t j = 0; j < array_size; j++)
					exprs.push_back(to_expression(get<SPIRConstant>(c.subconstants[j]).subconstants[i]));
				statement("const ", type_to_glsl(array_type), " ", lut_name, type_to_array_glsl(array_type), " = ",
				          type_to_glsl_constructor(array_type), "(", merge(exprs, ", "), ");");
			}

			for (uint32_t j = 0; j < iteration_count; j++)
			{
				entry_func.fixup_hooks_in.push_back([=, &var]() {
					AccessChainMeta meta;
					auto &c = this->get<SPIRConstant>(var.initializer);

					uint32_t invocation_id = 0;
					uint32_t member_index_id = 0;
					if (is_control_point)
					{
						uint32_t ids = ir.increase_bound_by(3);
						SPIRType uint_type;
						uint_type.basetype = SPIRType::UInt;
						uint_type.width = 32;
						set<SPIRType>(ids, uint_type);
						set<SPIRExpression>(ids + 1, builtin_to_glsl(BuiltInInvocationId, StorageClassInput), ids, true);
						set<SPIRConstant>(ids + 2, ids, i, false);
						invocation_id = ids + 1;
						member_index_id = ids + 2;
					}

					if (is_patch)
					{
						statement("if (gl_InvocationID == 0)");
						begin_scope();
					}

					if (type_is_array && !is_control_point)
					{
						uint32_t indices[2] = { j, i };
						auto chain = access_chain_internal(var.self, indices, 2, ACCESS_CHAIN_INDEX_IS_LITERAL_BIT, &meta);
						statement(chain, " = ", lut_name, "[", j, "];");
					}
					else if (is_control_point)
					{
						uint32_t indices[2] = { invocation_id, member_index_id };
						auto chain = access_chain_internal(var.self, indices, 2, 0, &meta);
						statement(chain, " = ", lut_name, "[", builtin_to_glsl(BuiltInInvocationId, StorageClassInput), "];");
					}
					else
					{
						auto chain =
								access_chain_internal(var.self, &i, 1, ACCESS_CHAIN_INDEX_IS_LITERAL_BIT, &meta);
						statement(chain, " = ", to_expression(c.subconstants[i]), ";");
					}

					if (is_patch)
						end_scope();
				});
			}
		}
	}
	else if (is_control_point)
	{
		auto lut_name = join("_", var.self, "_init");
		statement("const ", type_to_glsl(type), " ", lut_name, type_to_array_glsl(type),
		          " = ", to_expression(var.initializer), ";");
		entry_func.fixup_hooks_in.push_back([&, lut_name]() {
			statement(to_expression(var.self), "[gl_InvocationID] = ", lut_name, "[gl_InvocationID];");
		});
	}
	else if (has_decoration(var.self, DecorationBuiltIn) &&
	         BuiltIn(get_decoration(var.self, DecorationBuiltIn)) == BuiltInSampleMask)
	{
		// We cannot copy the array since gl_SampleMask is unsized in GLSL. Unroll time! <_<
		entry_func.fixup_hooks_in.push_back([&] {
			auto &c = this->get<SPIRConstant>(var.initializer);
			uint32_t num_constants = uint32_t(c.subconstants.size());
			for (uint32_t i = 0; i < num_constants; i++)
			{
				// Don't use to_expression on constant since it might be uint, just fish out the raw int.
				statement(to_expression(var.self), "[", i, "] = ",
				          convert_to_string(this->get<SPIRConstant>(c.subconstants[i]).scalar_i32()), ";");
			}
		});
	}
	else
	{
		auto lut_name = join("_", var.self, "_init");
		statement("const ", type_to_glsl(type), " ", lut_name,
		          type_to_array_glsl(type), " = ", to_expression(var.initializer), ";");
		entry_func.fixup_hooks_in.push_back([&, lut_name, is_patch]() {
			if (is_patch)
			{
				statement("if (gl_InvocationID == 0)");
				begin_scope();
			}
			statement(to_expression(var.self), " = ", lut_name, ";");
			if (is_patch)
				end_scope();
		});
	}
}

void CompilerGLSL::emit_subgroup_arithmetic_workaround(const std::string &func, Op op, GroupOperation group_op)
{
	std::string result;
	switch (group_op)
	{
	case GroupOperationReduce:
		result = "reduction";
		break;

	case GroupOperationExclusiveScan:
		result = "excl_scan";
		break;

	case GroupOperationInclusiveScan:
		result = "incl_scan";
		break;

	default:
		SPIRV_CROSS_THROW("Unsupported workaround for arithmetic group operation");
	}

	struct TypeInfo
	{
		std::string type;
		std::string identity;
	};

	std::vector<TypeInfo> type_infos;
	switch (op)
	{
	case OpGroupNonUniformIAdd:
	{
		type_infos.emplace_back(TypeInfo{ "uint", "0u" });
		type_infos.emplace_back(TypeInfo{ "uvec2", "uvec2(0u)" });
		type_infos.emplace_back(TypeInfo{ "uvec3", "uvec3(0u)" });
		type_infos.emplace_back(TypeInfo{ "uvec4", "uvec4(0u)" });
		type_infos.emplace_back(TypeInfo{ "int", "0" });
		type_infos.emplace_back(TypeInfo{ "ivec2", "ivec2(0)" });
		type_infos.emplace_back(TypeInfo{ "ivec3", "ivec3(0)" });
		type_infos.emplace_back(TypeInfo{ "ivec4", "ivec4(0)" });
		break;
	}

	case OpGroupNonUniformFAdd:
	{
		type_infos.emplace_back(TypeInfo{ "float", "0.0f" });
		type_infos.emplace_back(TypeInfo{ "vec2", "vec2(0.0f)" });
		type_infos.emplace_back(TypeInfo{ "vec3", "vec3(0.0f)" });
		type_infos.emplace_back(TypeInfo{ "vec4", "vec4(0.0f)" });
		// ARB_gpu_shader_fp64 is required in GL4.0 which in turn is required by NV_thread_shuffle
		type_infos.emplace_back(TypeInfo{ "double", "0.0LF" });
		type_infos.emplace_back(TypeInfo{ "dvec2", "dvec2(0.0LF)" });
		type_infos.emplace_back(TypeInfo{ "dvec3", "dvec3(0.0LF)" });
		type_infos.emplace_back(TypeInfo{ "dvec4", "dvec4(0.0LF)" });
		break;
	}

	case OpGroupNonUniformIMul:
	{
		type_infos.emplace_back(TypeInfo{ "uint", "1u" });
		type_infos.emplace_back(TypeInfo{ "uvec2", "uvec2(1u)" });
		type_infos.emplace_back(TypeInfo{ "uvec3", "uvec3(1u)" });
		type_infos.emplace_back(TypeInfo{ "uvec4", "uvec4(1u)" });
		type_infos.emplace_back(TypeInfo{ "int", "1" });
		type_infos.emplace_back(TypeInfo{ "ivec2", "ivec2(1)" });
		type_infos.emplace_back(TypeInfo{ "ivec3", "ivec3(1)" });
		type_infos.emplace_back(TypeInfo{ "ivec4", "ivec4(1)" });
		break;
	}

	case OpGroupNonUniformFMul:
	{
		type_infos.emplace_back(TypeInfo{ "float", "1.0f" });
		type_infos.emplace_back(TypeInfo{ "vec2", "vec2(1.0f)" });
		type_infos.emplace_back(TypeInfo{ "vec3", "vec3(1.0f)" });
		type_infos.emplace_back(TypeInfo{ "vec4", "vec4(1.0f)" });
		type_infos.emplace_back(TypeInfo{ "double", "0.0LF" });
		type_infos.emplace_back(TypeInfo{ "dvec2", "dvec2(1.0LF)" });
		type_infos.emplace_back(TypeInfo{ "dvec3", "dvec3(1.0LF)" });
		type_infos.emplace_back(TypeInfo{ "dvec4", "dvec4(1.0LF)" });
		break;
	}

	default:
		SPIRV_CROSS_THROW("Unsupported workaround for arithmetic group operation");
	}

	const bool op_is_addition = op == OpGroupNonUniformIAdd || op == OpGroupNonUniformFAdd;
	const bool op_is_multiplication = op == OpGroupNonUniformIMul || op == OpGroupNonUniformFMul;
	std::string op_symbol;
	if (op_is_addition)
	{
		op_symbol = "+=";
	}
	else if (op_is_multiplication)
	{
		op_symbol = "*=";
	}

	for (const TypeInfo &t : type_infos)
	{
		statement(t.type, " ", func, "(", t.type, " v)");
		begin_scope();
		statement(t.type, " ", result, " = ", t.identity, ";");
		statement("uvec4 active_threads = subgroupBallot(true);");
		statement("if (subgroupBallotBitCount(active_threads) == gl_SubgroupSize)");
		begin_scope();
		statement("uint total = gl_SubgroupSize / 2u;");
		statement(result, " = v;");
		statement("for (uint i = 1u; i <= total; i <<= 1u)");
		begin_scope();
		statement("bool valid;");
		if (group_op == GroupOperationReduce)
		{
			statement(t.type, " s = shuffleXorNV(", result, ", i, gl_SubgroupSize, valid);");
		}
		else if (group_op == GroupOperationExclusiveScan || group_op == GroupOperationInclusiveScan)
		{
			statement(t.type, " s = shuffleUpNV(", result, ", i, gl_SubgroupSize, valid);");
		}
		if (op_is_addition || op_is_multiplication)
		{
			statement(result, " ", op_symbol, " valid ? s : ", t.identity, ";");
		}
		end_scope();
		if (group_op == GroupOperationExclusiveScan)
		{
			statement(result, " = shuffleUpNV(", result, ", 1u, gl_SubgroupSize);");
			statement("if (subgroupElect())");
			begin_scope();
			statement(result, " = ", t.identity, ";");
			end_scope();
		}
		end_scope();
		statement("else");
		begin_scope();
		if (group_op == GroupOperationExclusiveScan)
		{
			statement("uint total = subgroupBallotBitCount(gl_SubgroupLtMask);");
		}
		else if (group_op == GroupOperationInclusiveScan)
		{
			statement("uint total = subgroupBallotBitCount(gl_SubgroupLeMask);");
		}
		statement("for (uint i = 0u; i < gl_SubgroupSize; ++i)");
		begin_scope();
		statement("bool valid = subgroupBallotBitExtract(active_threads, i);");
		statement(t.type, " s = shuffleNV(v, i, gl_SubgroupSize);");
		if (group_op == GroupOperationExclusiveScan || group_op == GroupOperationInclusiveScan)
		{
			statement("valid = valid && (i < total);");
		}
		if (op_is_addition || op_is_multiplication)
		{
			statement(result, " ", op_symbol, " valid ? s : ", t.identity, ";");
		}
		end_scope();
		end_scope();
		statement("return ", result, ";");
		end_scope();
	}
}

void CompilerGLSL::emit_extension_workarounds(spv::ExecutionModel model)
{
	static const char *workaround_types[] = { "int",   "ivec2", "ivec3", "ivec4", "uint",   "uvec2", "uvec3", "uvec4",
		                                      "float", "vec2",  "vec3",  "vec4",  "double", "dvec2", "dvec3", "dvec4" };

	if (!options.vulkan_semantics)
	{
		using Supp = ShaderSubgroupSupportHelper;
		auto result = shader_subgroup_supporter.resolve();

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupMask))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupMask, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("#define gl_SubgroupEqMask uvec4(gl_ThreadEqMaskNV, 0u, 0u, 0u)");
					statement("#define gl_SubgroupGeMask uvec4(gl_ThreadGeMaskNV, 0u, 0u, 0u)");
					statement("#define gl_SubgroupGtMask uvec4(gl_ThreadGtMaskNV, 0u, 0u, 0u)");
					statement("#define gl_SubgroupLeMask uvec4(gl_ThreadLeMaskNV, 0u, 0u, 0u)");
					statement("#define gl_SubgroupLtMask uvec4(gl_ThreadLtMaskNV, 0u, 0u, 0u)");
					break;
				case Supp::ARB_shader_ballot:
					statement("#define gl_SubgroupEqMask uvec4(unpackUint2x32(gl_SubGroupEqMaskARB), 0u, 0u)");
					statement("#define gl_SubgroupGeMask uvec4(unpackUint2x32(gl_SubGroupGeMaskARB), 0u, 0u)");
					statement("#define gl_SubgroupGtMask uvec4(unpackUint2x32(gl_SubGroupGtMaskARB), 0u, 0u)");
					statement("#define gl_SubgroupLeMask uvec4(unpackUint2x32(gl_SubGroupLeMaskARB), 0u, 0u)");
					statement("#define gl_SubgroupLtMask uvec4(unpackUint2x32(gl_SubGroupLtMaskARB), 0u, 0u)");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupSize))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupSize, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("#define gl_SubgroupSize gl_WarpSizeNV");
					break;
				case Supp::ARB_shader_ballot:
					statement("#define gl_SubgroupSize gl_SubGroupSizeARB");
					break;
				case Supp::AMD_gcn_shader:
					statement("#define gl_SubgroupSize uint(gl_SIMDGroupSizeAMD)");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupInvocationID))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupInvocationID, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("#define gl_SubgroupInvocationID gl_ThreadInWarpNV");
					break;
				case Supp::ARB_shader_ballot:
					statement("#define gl_SubgroupInvocationID gl_SubGroupInvocationARB");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupID))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupID, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("#define gl_SubgroupID gl_WarpIDNV");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::NumSubgroups))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::NumSubgroups, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("#define gl_NumSubgroups gl_WarpsPerSMNV");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupBroadcast_First))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupBroadcast_First, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_shuffle:
					for (const char *t : workaround_types)
					{
						statement(t, " subgroupBroadcastFirst(", t,
						          " value) { return shuffleNV(value, findLSB(ballotThreadNV(true)), gl_WarpSizeNV); }");
					}
					for (const char *t : workaround_types)
					{
						statement(t, " subgroupBroadcast(", t,
						          " value, uint id) { return shuffleNV(value, id, gl_WarpSizeNV); }");
					}
					break;
				case Supp::ARB_shader_ballot:
					for (const char *t : workaround_types)
					{
						statement(t, " subgroupBroadcastFirst(", t,
						          " value) { return readFirstInvocationARB(value); }");
					}
					for (const char *t : workaround_types)
					{
						statement(t, " subgroupBroadcast(", t,
						          " value, uint id) { return readInvocationARB(value, id); }");
					}
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupBallotFindLSB_MSB))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupBallotFindLSB_MSB, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("uint subgroupBallotFindLSB(uvec4 value) { return findLSB(value.x); }");
					statement("uint subgroupBallotFindMSB(uvec4 value) { return findMSB(value.x); }");
					break;
				default:
					break;
				}
			}
			statement("#else");
			statement("uint subgroupBallotFindLSB(uvec4 value)");
			begin_scope();
			statement("int firstLive = findLSB(value.x);");
			statement("return uint(firstLive != -1 ? firstLive : (findLSB(value.y) + 32));");
			end_scope();
			statement("uint subgroupBallotFindMSB(uvec4 value)");
			begin_scope();
			statement("int firstLive = findMSB(value.y);");
			statement("return uint(firstLive != -1 ? (firstLive + 32) : findMSB(value.x));");
			end_scope();
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupAll_Any_AllEqualBool))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupAll_Any_AllEqualBool, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_gpu_shader_5:
					statement("bool subgroupAll(bool value) { return allThreadsNV(value); }");
					statement("bool subgroupAny(bool value) { return anyThreadNV(value); }");
					statement("bool subgroupAllEqual(bool value) { return allThreadsEqualNV(value); }");
					break;
				case Supp::ARB_shader_group_vote:
					statement("bool subgroupAll(bool v) { return allInvocationsARB(v); }");
					statement("bool subgroupAny(bool v) { return anyInvocationARB(v); }");
					statement("bool subgroupAllEqual(bool v) { return allInvocationsEqualARB(v); }");
					break;
				case Supp::AMD_gcn_shader:
					statement("bool subgroupAll(bool value) { return ballotAMD(value) == ballotAMD(true); }");
					statement("bool subgroupAny(bool value) { return ballotAMD(value) != 0ull; }");
					statement("bool subgroupAllEqual(bool value) { uint64_t b = ballotAMD(value); return b == 0ull || "
					          "b == ballotAMD(true); }");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupAllEqualT))
		{
			statement("#ifndef GL_KHR_shader_subgroup_vote");
			statement(
			    "#define _SPIRV_CROSS_SUBGROUP_ALL_EQUAL_WORKAROUND(type) bool subgroupAllEqual(type value) { return "
			    "subgroupAllEqual(subgroupBroadcastFirst(value) == value); }");
			for (const char *t : workaround_types)
				statement("_SPIRV_CROSS_SUBGROUP_ALL_EQUAL_WORKAROUND(", t, ")");
			statement("#undef _SPIRV_CROSS_SUBGROUP_ALL_EQUAL_WORKAROUND");
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupBallot))
		{
			auto exts = Supp::get_candidates_for_feature(Supp::SubgroupBallot, result);

			for (auto &e : exts)
			{
				const char *name = Supp::get_extension_name(e);
				statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

				switch (e)
				{
				case Supp::NV_shader_thread_group:
					statement("uvec4 subgroupBallot(bool v) { return uvec4(ballotThreadNV(v), 0u, 0u, 0u); }");
					break;
				case Supp::ARB_shader_ballot:
					statement("uvec4 subgroupBallot(bool v) { return uvec4(unpackUint2x32(ballotARB(v)), 0u, 0u); }");
					break;
				default:
					break;
				}
			}
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupElect))
		{
			statement("#ifndef GL_KHR_shader_subgroup_basic");
			statement("bool subgroupElect()");
			begin_scope();
			statement("uvec4 activeMask = subgroupBallot(true);");
			statement("uint firstLive = subgroupBallotFindLSB(activeMask);");
			statement("return gl_SubgroupInvocationID == firstLive;");
			end_scope();
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupBarrier))
		{
			// Extensions we're using in place of GL_KHR_shader_subgroup_basic state
			// that subgroup execute in lockstep so this barrier is implicit.
			// However the GL 4.6 spec also states that `barrier` implies a shared memory barrier,
			// and a specific test of optimizing scans by leveraging lock-step invocation execution,
			// has shown that a `memoryBarrierShared` is needed in place of a `subgroupBarrier`.
			// https://github.com/buildaworldnet/IrrlichtBAW/commit/d8536857991b89a30a6b65d29441e51b64c2c7ad#diff-9f898d27be1ea6fc79b03d9b361e299334c1a347b6e4dc344ee66110c6aa596aR19
			statement("#ifndef GL_KHR_shader_subgroup_basic");
			statement("void subgroupBarrier() { memoryBarrierShared(); }");
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupMemBarrier))
		{
			if (model == spv::ExecutionModelGLCompute)
			{
				statement("#ifndef GL_KHR_shader_subgroup_basic");
				statement("void subgroupMemoryBarrier() { groupMemoryBarrier(); }");
				statement("void subgroupMemoryBarrierBuffer() { groupMemoryBarrier(); }");
				statement("void subgroupMemoryBarrierShared() { memoryBarrierShared(); }");
				statement("void subgroupMemoryBarrierImage() { groupMemoryBarrier(); }");
				statement("#endif");
			}
			else
			{
				statement("#ifndef GL_KHR_shader_subgroup_basic");
				statement("void subgroupMemoryBarrier() { memoryBarrier(); }");
				statement("void subgroupMemoryBarrierBuffer() { memoryBarrierBuffer(); }");
				statement("void subgroupMemoryBarrierImage() { memoryBarrierImage(); }");
				statement("#endif");
			}
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupInverseBallot_InclBitCount_ExclBitCout))
		{
			statement("#ifndef GL_KHR_shader_subgroup_ballot");
			statement("bool subgroupInverseBallot(uvec4 value)");
			begin_scope();
			statement("return any(notEqual(value.xy & gl_SubgroupEqMask.xy, uvec2(0u)));");
			end_scope();

			statement("uint subgroupBallotInclusiveBitCount(uvec4 value)");
			begin_scope();
			statement("uvec2 v = value.xy & gl_SubgroupLeMask.xy;");
			statement("ivec2 c = bitCount(v);");
			statement_no_indent("#ifdef GL_NV_shader_thread_group");
			statement("return uint(c.x);");
			statement_no_indent("#else");
			statement("return uint(c.x + c.y);");
			statement_no_indent("#endif");
			end_scope();

			statement("uint subgroupBallotExclusiveBitCount(uvec4 value)");
			begin_scope();
			statement("uvec2 v = value.xy & gl_SubgroupLtMask.xy;");
			statement("ivec2 c = bitCount(v);");
			statement_no_indent("#ifdef GL_NV_shader_thread_group");
			statement("return uint(c.x);");
			statement_no_indent("#else");
			statement("return uint(c.x + c.y);");
			statement_no_indent("#endif");
			end_scope();
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupBallotBitCount))
		{
			statement("#ifndef GL_KHR_shader_subgroup_ballot");
			statement("uint subgroupBallotBitCount(uvec4 value)");
			begin_scope();
			statement("ivec2 c = bitCount(value.xy);");
			statement_no_indent("#ifdef GL_NV_shader_thread_group");
			statement("return uint(c.x);");
			statement_no_indent("#else");
			statement("return uint(c.x + c.y);");
			statement_no_indent("#endif");
			end_scope();
			statement("#endif");
			statement("");
		}

		if (shader_subgroup_supporter.is_feature_requested(Supp::SubgroupBallotBitExtract))
		{
			statement("#ifndef GL_KHR_shader_subgroup_ballot");
			statement("bool subgroupBallotBitExtract(uvec4 value, uint index)");
			begin_scope();
			statement_no_indent("#ifdef GL_NV_shader_thread_group");
			statement("uint shifted = value.x >> index;");
			statement_no_indent("#else");
			statement("uint shifted = value[index >> 5u] >> (index & 0x1fu);");
			statement_no_indent("#endif");
			statement("return (shifted & 1u) != 0u;");
			end_scope();
			statement("#endif");
			statement("");
		}

		auto arithmetic_feature_helper =
		    [&](Supp::Feature feat, std::string func_name, spv::Op op, spv::GroupOperation group_op)
		{
			if (shader_subgroup_supporter.is_feature_requested(feat))
			{
				auto exts = Supp::get_candidates_for_feature(feat, result);
				for (auto &e : exts)
				{
					const char *name = Supp::get_extension_name(e);
					statement(&e == &exts.front() ? "#if" : "#elif", " defined(", name, ")");

					switch (e)
					{
					case Supp::NV_shader_thread_shuffle:
						emit_subgroup_arithmetic_workaround(func_name, op, group_op);
						break;
					default:
						break;
					}
				}
				statement("#endif");
				statement("");
			}
		};

		arithmetic_feature_helper(Supp::SubgroupArithmeticIAddReduce, "subgroupAdd", OpGroupNonUniformIAdd,
		                          GroupOperationReduce);
		arithmetic_feature_helper(Supp::SubgroupArithmeticIAddExclusiveScan, "subgroupExclusiveAdd",
		                          OpGroupNonUniformIAdd, GroupOperationExclusiveScan);
		arithmetic_feature_helper(Supp::SubgroupArithmeticIAddInclusiveScan, "subgroupInclusiveAdd",
		                          OpGroupNonUniformIAdd, GroupOperationInclusiveScan);
		arithmetic_feature_helper(Supp::SubgroupArithmeticFAddReduce, "subgroupAdd", OpGroupNonUniformFAdd,
		                          GroupOperationReduce);
		arithmetic_feature_helper(Supp::SubgroupArithmeticFAddExclusiveScan, "subgroupExclusiveAdd",
		                          OpGroupNonUniformFAdd, GroupOperationExclusiveScan);
		arithmetic_feature_helper(Supp::SubgroupArithmeticFAddInclusiveScan, "subgroupInclusiveAdd",
		                          OpGroupNonUniformFAdd, GroupOperationInclusiveScan);

		arithmetic_feature_helper(Supp::SubgroupArithmeticIMulReduce, "subgroupMul", OpGroupNonUniformIMul,
		                          GroupOperationReduce);
		arithmetic_feature_helper(Supp::SubgroupArithmeticIMulExclusiveScan, "subgroupExclusiveMul",
		                          OpGroupNonUniformIMul, GroupOperationExclusiveScan);
		arithmetic_feature_helper(Supp::SubgroupArithmeticIMulInclusiveScan, "subgroupInclusiveMul",
		                          OpGroupNonUniformIMul, GroupOperationInclusiveScan);
		arithmetic_feature_helper(Supp::SubgroupArithmeticFMulReduce, "subgroupMul", OpGroupNonUniformFMul,
		                          GroupOperationReduce);
		arithmetic_feature_helper(Supp::SubgroupArithmeticFMulExclusiveScan, "subgroupExclusiveMul",
		                          OpGroupNonUniformFMul, GroupOperationExclusiveScan);
		arithmetic_feature_helper(Supp::SubgroupArithmeticFMulInclusiveScan, "subgroupInclusiveMul",
		                          OpGroupNonUniformFMul, GroupOperationInclusiveScan);
	}

	if (!workaround_ubo_load_overload_types.empty())
	{
		for (auto &type_id : workaround_ubo_load_overload_types)
		{
			auto &type = get<SPIRType>(type_id);

			if (options.es && is_matrix(type))
			{
				// Need both variants.
				// GLSL cannot overload on precision, so need to dispatch appropriately.
				statement("highp ", type_to_glsl(type), " spvWorkaroundRowMajor(highp ", type_to_glsl(type), " wrap) { return wrap; }");
				statement("mediump ", type_to_glsl(type), " spvWorkaroundRowMajorMP(mediump ", type_to_glsl(type), " wrap) { return wrap; }");
			}
			else
			{
				statement(type_to_glsl(type), " spvWorkaroundRowMajor(", type_to_glsl(type), " wrap) { return wrap; }");
			}
		}
		statement("");
	}
}

void CompilerGLSL::emit_polyfills(uint32_t polyfills, bool relaxed)
{
	const char *qual = "";
	const char *suffix = (options.es && relaxed) ? "MP" : "";
	if (options.es)
		qual = relaxed ? "mediump " : "highp ";

	if (polyfills & PolyfillTranspose2x2)
	{
		statement(qual, "mat2 spvTranspose", suffix, "(", qual, "mat2 m)");
		begin_scope();
		statement("return mat2(m[0][0], m[1][0], m[0][1], m[1][1]);");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillTranspose3x3)
	{
		statement(qual, "mat3 spvTranspose", suffix, "(", qual, "mat3 m)");
		begin_scope();
		statement("return mat3(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2]);");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillTranspose4x4)
	{
		statement(qual, "mat4 spvTranspose", suffix, "(", qual, "mat4 m)");
		begin_scope();
		statement("return mat4(m[0][0], m[1][0], m[2][0], m[3][0], m[0][1], m[1][1], m[2][1], m[3][1], m[0][2], "
		          "m[1][2], m[2][2], m[3][2], m[0][3], m[1][3], m[2][3], m[3][3]);");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillDeterminant2x2)
	{
		statement(qual, "float spvDeterminant", suffix, "(", qual, "mat2 m)");
		begin_scope();
		statement("return m[0][0] * m[1][1] - m[0][1] * m[1][0];");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillDeterminant3x3)
	{
		statement(qual, "float spvDeterminant", suffix, "(", qual, "mat3 m)");
		begin_scope();
		statement("return dot(m[0], vec3(m[1][1] * m[2][2] - m[1][2] * m[2][1], "
		                                "m[1][2] * m[2][0] - m[1][0] * m[2][2], "
		                                "m[1][0] * m[2][1] - m[1][1] * m[2][0]));");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillDeterminant4x4)
	{
		statement(qual, "float spvDeterminant", suffix, "(", qual, "mat4 m)");
		begin_scope();
		statement("return dot(m[0], vec4("
		          "m[2][1] * m[3][2] * m[1][3] - m[3][1] * m[2][2] * m[1][3] + m[3][1] * m[1][2] * m[2][3] - m[1][1] * m[3][2] * m[2][3] - m[2][1] * m[1][2] * m[3][3] + m[1][1] * m[2][2] * m[3][3], "
		          "m[3][0] * m[2][2] * m[1][3] - m[2][0] * m[3][2] * m[1][3] - m[3][0] * m[1][2] * m[2][3] + m[1][0] * m[3][2] * m[2][3] + m[2][0] * m[1][2] * m[3][3] - m[1][0] * m[2][2] * m[3][3], "
		          "m[2][0] * m[3][1] * m[1][3] - m[3][0] * m[2][1] * m[1][3] + m[3][0] * m[1][1] * m[2][3] - m[1][0] * m[3][1] * m[2][3] - m[2][0] * m[1][1] * m[3][3] + m[1][0] * m[2][1] * m[3][3], "
		          "m[3][0] * m[2][1] * m[1][2] - m[2][0] * m[3][1] * m[1][2] - m[3][0] * m[1][1] * m[2][2] + m[1][0] * m[3][1] * m[2][2] + m[2][0] * m[1][1] * m[3][2] - m[1][0] * m[2][1] * m[3][2]));");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillMatrixInverse2x2)
	{
		statement(qual, "mat2 spvInverse", suffix, "(", qual, "mat2 m)");
		begin_scope();
		statement("return mat2(m[1][1], -m[0][1], -m[1][0], m[0][0]) "
		          "* (1.0 / (m[0][0] * m[1][1] - m[1][0] * m[0][1]));");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillMatrixInverse3x3)
	{
		statement(qual, "mat3 spvInverse", suffix, "(", qual, "mat3 m)");
		begin_scope();
		statement(qual, "vec3 t = vec3(m[1][1] * m[2][2] - m[1][2] * m[2][1], m[1][2] * m[2][0] - m[1][0] * m[2][2], m[1][0] * m[2][1] - m[1][1] * m[2][0]);");
		statement("return mat3(t[0], "
		                      "m[0][2] * m[2][1] - m[0][1] * m[2][2], "
		                      "m[0][1] * m[1][2] - m[0][2] * m[1][1], "
		                      "t[1], "
		                      "m[0][0] * m[2][2] - m[0][2] * m[2][0], "
		                      "m[0][2] * m[1][0] - m[0][0] * m[1][2], "
		                      "t[2], "
		                      "m[0][1] * m[2][0] - m[0][0] * m[2][1], "
		                      "m[0][0] * m[1][1] - m[0][1] * m[1][0]) "
		                      "* (1.0 / dot(m[0], t));");
		end_scope();
		statement("");
	}

	if (polyfills & PolyfillMatrixInverse4x4)
	{
		statement(qual, "mat4 spvInverse", suffix, "(", qual, "mat4 m)");
		begin_scope();
		statement(qual, "vec4 t = vec4("
		          "m[2][1] * m[3][2] * m[1][3] - m[3][1] * m[2][2] * m[1][3] + m[3][1] * m[1][2] * m[2][3] - m[1][1] * m[3][2] * m[2][3] - m[2][1] * m[1][2] * m[3][3] + m[1][1] * m[2][2] * m[3][3], "
		          "m[3][0] * m[2][2] * m[1][3] - m[2][0] * m[3][2] * m[1][3] - m[3][0] * m[1][2] * m[2][3] + m[1][0] * m[3][2] * m[2][3] + m[2][0] * m[1][2] * m[3][3] - m[1][0] * m[2][2] * m[3][3], "
		          "m[2][0] * m[3][1] * m[1][3] - m[3][0] * m[2][1] * m[1][3] + m[3][0] * m[1][1] * m[2][3] - m[1][0] * m[3][1] * m[2][3] - m[2][0] * m[1][1] * m[3][3] + m[1][0] * m[2][1] * m[3][3], "
		          "m[3][0] * m[2][1] * m[1][2] - m[2][0] * m[3][1] * m[1][2] - m[3][0] * m[1][1] * m[2][2] + m[1][0] * m[3][1] * m[2][2] + m[2][0] * m[1][1] * m[3][2] - m[1][0] * m[2][1] * m[3][2]);");
		statement("return mat4("
		          "t[0], "
		          "m[3][1] * m[2][2] * m[0][3] - m[2][1] * m[3][2] * m[0][3] - m[3][1] * m[0][2] * m[2][3] + m[0][1] * m[3][2] * m[2][3] + m[2][1] * m[0][2] * m[3][3] - m[0][1] * m[2][2] * m[3][3], "
		          "m[1][1] * m[3][2] * m[0][3] - m[3][1] * m[1][2] * m[0][3] + m[3][1] * m[0][2] * m[1][3] - m[0][1] * m[3][2] * m[1][3] - m[1][1] * m[0][2] * m[3][3] + m[0][1] * m[1][2] * m[3][3], "
		          "m[2][1] * m[1][2] * m[0][3] - m[1][1] * m[2][2] * m[0][3] - m[2][1] * m[0][2] * m[1][3] + m[0][1] * m[2][2] * m[1][3] + m[1][1] * m[0][2] * m[2][3] - m[0][1] * m[1][2] * m[2][3], "
		          "t[1], "
		          "m[2][0] * m[3][2] * m[0][3] - m[3][0] * m[2][2] * m[0][3] + m[3][0] * m[0][2] * m[2][3] - m[0][0] * m[3][2] * m[2][3] - m[2][0] * m[0][2] * m[3][3] + m[0][0] * m[2][2] * m[3][3], "
		          "m[3][0] * m[1][2] * m[0][3] - m[1][0] * m[3][2] * m[0][3] - m[3][0] * m[0][2] * m[1][3] + m[0][0] * m[3][2] * m[1][3] + m[1][0] * m[0][2] * m[3][3] - m[0][0] * m[1][2] * m[3][3], "
		          "m[1][0] * m[2][2] * m[0][3] - m[2][0] * m[1][2] * m[0][3] + m[2][0] * m[0][2] * m[1][3] - m[0][0] * m[2][2] * m[1][3] - m[1][0] * m[0][2] * m[2][3] + m[0][0] * m[1][2] * m[2][3], "
		          "t[2], "
		          "m[3][0] * m[2][1] * m[0][3] - m[2][0] * m[3][1] * m[0][3] - m[3][0] * m[0][1] * m[2][3] + m[0][0] * m[3][1] * m[2][3] + m[2][0] * m[0][1] * m[3][3] - m[0][0] * m[2][1] * m[3][3], "
		          "m[1][0] * m[3][1] * m[0][3] - m[3][0] * m[1][1] * m[0][3] + m[3][0] * m[0][1] * m[1][3] - m[0][0] * m[3][1] * m[1][3] - m[1][0] * m[0][1] * m[3][3] + m[0][0] * m[1][1] * m[3][3], "
		          "m[2][0] * m[1][1] * m[0][3] - m[1][0] * m[2][1] * m[0][3] - m[2][0] * m[0][1] * m[1][3] + m[0][0] * m[2][1] * m[1][3] + m[1][0] * m[0][1] * m[2][3] - m[0][0] * m[1][1] * m[2][3], "
		          "t[3], "
		          "m[2][0] * m[3][1] * m[0][2] - m[3][0] * m[2][1] * m[0][2] + m[3][0] * m[0][1] * m[2][2] - m[0][0] * m[3][1] * m[2][2] - m[2][0] * m[0][1] * m[3][2] + m[0][0] * m[2][1] * m[3][2], "
		          "m[3][0] * m[1][1] * m[0][2] - m[1][0] * m[3][1] * m[0][2] - m[3][0] * m[0][1] * m[1][2] + m[0][0] * m[3][1] * m[1][2] + m[1][0] * m[0][1] * m[3][2] - m[0][0] * m[1][1] * m[3][2], "
		          "m[1][0] * m[2][1] * m[0][2] - m[2][0] * m[1][1] * m[0][2] + m[2][0] * m[0][1] * m[1][2] - m[0][0] * m[2][1] * m[1][2] - m[1][0] * m[0][1] * m[2][2] + m[0][0] * m[1][1] * m[2][2]) "
		          "* (1.0 / dot(m[0], t));");
		end_scope();
		statement("");
	}
}

// Returns a string representation of the ID, usable as a function arg.
// Default is to simply return the expression representation fo the arg ID.
// Subclasses may override to modify the return value.
string CompilerGLSL::to_func_call_arg(const SPIRFunction::Parameter &, uint32_t id)
{
	// Make sure that we use the name of the original variable, and not the parameter alias.
	uint32_t name_id = id;
	auto *var = maybe_get<SPIRVariable>(id);
	if (var && var->basevariable)
		name_id = var->basevariable;
	return to_expression(name_id);
}

void CompilerGLSL::force_temporary_and_recompile(uint32_t id)
{
	auto res = forced_temporaries.insert(id);

	// Forcing new temporaries guarantees forward progress.
	if (res.second)
		force_recompile_guarantee_forward_progress();
	else
		force_recompile();
}

uint32_t CompilerGLSL::consume_temporary_in_precision_context(uint32_t type_id, uint32_t id, Options::Precision precision)
{
	// Constants do not have innate precision.
	auto handle_type = ir.ids[id].get_type();
	if (handle_type == TypeConstant || handle_type == TypeConstantOp || handle_type == TypeUndef)
		return id;

	// Ignore anything that isn't 32-bit values.
	auto &type = get<SPIRType>(type_id);
	if (type.pointer)
		return id;
	if (type.basetype != SPIRType::Float && type.basetype != SPIRType::UInt && type.basetype != SPIRType::Int)
		return id;

	if (precision == Options::DontCare)
	{
		// If precision is consumed as don't care (operations only consisting of constants),
		// we need to bind the expression to a temporary,
		// otherwise we have no way of controlling the precision later.
		auto itr = forced_temporaries.insert(id);
		if (itr.second)
			force_recompile_guarantee_forward_progress();
		return id;
	}

	auto current_precision = has_decoration(id, DecorationRelaxedPrecision) ? Options::Mediump : Options::Highp;
	if (current_precision == precision)
		return id;

	auto itr = temporary_to_mirror_precision_alias.find(id);
	if (itr == temporary_to_mirror_precision_alias.end())
	{
		uint32_t alias_id = ir.increase_bound_by(1);
		auto &m = ir.meta[alias_id];
		if (auto *input_m = ir.find_meta(id))
			m = *input_m;

		const char *prefix;
		if (precision == Options::Mediump)
		{
			set_decoration(alias_id, DecorationRelaxedPrecision);
			prefix = "mp_copy_";
		}
		else
		{
			unset_decoration(alias_id, DecorationRelaxedPrecision);
			prefix = "hp_copy_";
		}

		auto alias_name = join(prefix, to_name(id));
		ParsedIR::sanitize_underscores(alias_name);
		set_name(alias_id, alias_name);

		emit_op(type_id, alias_id, to_expression(id), true);
		temporary_to_mirror_precision_alias[id] = alias_id;
		forced_temporaries.insert(id);
		forced_temporaries.insert(alias_id);
		force_recompile_guarantee_forward_progress();
		id = alias_id;
	}
	else
	{
		id = itr->second;
	}

	return id;
}

void CompilerGLSL::handle_invalid_expression(uint32_t id)
{
	// We tried to read an invalidated expression.
	// This means we need another pass at compilation, but next time,
	// force temporary variables so that they cannot be invalidated.
	force_temporary_and_recompile(id);

	// If the invalid expression happened as a result of a CompositeInsert
	// overwrite, we must block this from happening next iteration.
	if (composite_insert_overwritten.count(id))
		block_composite_insert_overwrite.insert(id);
}

// Converts the format of the current expression from packed to unpacked,
// by wrapping the expression in a constructor of the appropriate type.
// GLSL does not support packed formats, so simply return the expression.
// Subclasses that do will override.
string CompilerGLSL::unpack_expression_type(string expr_str, const SPIRType &, uint32_t, bool, bool)
{
	return expr_str;
}

// Sometimes we proactively enclosed an expression where it turns out we might have not needed it after all.
void CompilerGLSL::strip_enclosed_expression(string &expr)
{
	if (expr.size() < 2 || expr.front() != '(' || expr.back() != ')')
		return;

	// Have to make sure that our first and last parens actually enclose everything inside it.
	uint32_t paren_count = 0;
	for (auto &c : expr)
	{
		if (c == '(')
			paren_count++;
		else if (c == ')')
		{
			paren_count--;

			// If we hit 0 and this is not the final char, our first and final parens actually don't
			// enclose the expression, and we cannot strip, e.g.: (a + b) * (c + d).
			if (paren_count == 0 && &c != &expr.back())
				return;
		}
	}
	expr.erase(expr.size() - 1, 1);
	expr.erase(begin(expr));
}

bool CompilerGLSL::needs_enclose_expression(const std::string &expr)
{
	bool need_parens = false;

	// If the expression starts with a unary we need to enclose to deal with cases where we have back-to-back
	// unary expressions.
	if (!expr.empty())
	{
		auto c = expr.front();
		if (c == '-' || c == '+' || c == '!' || c == '~' || c == '&' || c == '*')
			need_parens = true;
	}

	if (!need_parens)
	{
		uint32_t paren_count = 0;
		for (auto c : expr)
		{
			if (c == '(' || c == '[')
				paren_count++;
			else if (c == ')' || c == ']')
			{
				assert(paren_count);
				paren_count--;
			}
			else if (c == ' ' && paren_count == 0)
			{
				need_parens = true;
				break;
			}
		}
		assert(paren_count == 0);
	}

	return need_parens;
}

string CompilerGLSL::enclose_expression(const string &expr)
{
	// If this expression contains any spaces which are not enclosed by parentheses,
	// we need to enclose it so we can treat the whole string as an expression.
	// This happens when two expressions have been part of a binary op earlier.
	if (needs_enclose_expression(expr))
		return join('(', expr, ')');
	else
		return expr;
}

string CompilerGLSL::dereference_expression(const SPIRType &expr_type, const std::string &expr)
{
	// If this expression starts with an address-of operator ('&'), then
	// just return the part after the operator.
	// TODO: Strip parens if unnecessary?
	if (expr.front() == '&')
		return expr.substr(1);
	else if (backend.native_pointers)
		return join('*', expr);
	else if (expr_type.storage == StorageClassPhysicalStorageBufferEXT && expr_type.basetype != SPIRType::Struct &&
	         expr_type.pointer_depth == 1)
	{
		return join(enclose_expression(expr), ".value");
	}
	else
		return expr;
}

string CompilerGLSL::address_of_expression(const std::string &expr)
{
	if (expr.size() > 3 && expr[0] == '(' && expr[1] == '*' && expr.back() == ')')
	{
		// If we have an expression which looks like (*foo), taking the address of it is the same as stripping
		// the first two and last characters. We might have to enclose the expression.
		// This doesn't work for cases like (*foo + 10),
		// but this is an r-value expression which we cannot take the address of anyways.
		return enclose_expression(expr.substr(2, expr.size() - 3));
	}
	else if (expr.front() == '*')
	{
		// If this expression starts with a dereference operator ('*'), then
		// just return the part after the operator.
		return expr.substr(1);
	}
	else
		return join('&', enclose_expression(expr));
}

// Just like to_expression except that we enclose the expression inside parentheses if needed.
string CompilerGLSL::to_enclosed_expression(uint32_t id, bool register_expression_read)
{
	return enclose_expression(to_expression(id, register_expression_read));
}

// Used explicitly when we want to read a row-major expression, but without any transpose shenanigans.
// need_transpose must be forced to false.
string CompilerGLSL::to_unpacked_row_major_matrix_expression(uint32_t id)
{
	return unpack_expression_type(to_expression(id), expression_type(id),
	                              get_extended_decoration(id, SPIRVCrossDecorationPhysicalTypeID),
	                              has_extended_decoration(id, SPIRVCrossDecorationPhysicalTypePacked), true);
}

string CompilerGLSL::to_unpacked_expression(uint32_t id, bool register_expression_read)
{
	// If we need to transpose, it will also take care of unpacking rules.
	auto *e = maybe_get<SPIRExpression>(id);
	bool need_transpose = e && e->need_transpose;
	bool is_remapped = has_extended_decoration(id, SPIRVCrossDecorationPhysicalTypeID);
	bool is_packed = has_extended_decoration(id, SPIRVCrossDecorationPhysicalTypePacked);

	if (!need_transpose && (is_remapped || is_packed))
	{
		return unpack_expression_type(to_expression(id, register_expression_read),
		                              get_pointee_type(expression_type_id(id)),
		                              get_extended_decoration(id, SPIRVCrossDecorationPhysicalTypeID),
		                              has_extended_decoration(id, SPIRVCrossDecorationPhysicalTypePacked), false);
	}
	else
		return to_expression(id, register_expression_read);
}

string CompilerGLSL::to_enclosed_unpacked_expression(uint32_t id, bool register_expression_read)
{
	return enclose_expression(to_unpacked_expression(id, register_expression_read));
}

string CompilerGLSL::to_dereferenced_expression(uint32_t id, bool register_expression_read)
{
	auto &type = expression_type(id);
	if (type.pointer && should_dereference(id))
		return dereference_expression(type, to_enclosed_expression(id, register_expression_read));
	else
		return to_expression(id, register_expression_read);
}

string CompilerGLSL::to_pointer_expression(uint32_t id, bool register_expression_read)
{
	auto &type = expression_type(id);
	if (type.pointer && expression_is_lvalue(id) && !should_dereference(id))
		return address_of_expression(to_enclosed_expression(id, register_expression_read));
	else
		return to_unpacked_expression(id, register_expression_read);
}

string CompilerGLSL::to_enclosed_pointer_expression(uint32_t id, bool register_expression_read)
{
	auto &type = expression_type(id);
	if (type.pointer && expression_is_lvalue(id) && !should_dereference(id))
		return address_of_expression(to_enclosed_expression(id, register_expression_read));
	else
		return to_enclosed_unpacked_expression(id, register_expression_read);
}

string CompilerGLSL::to_extract_component_expression(uint32_t id, uint32_t index)
{
	auto expr = to_enclosed_expression(id);
	if (has_extended_decoration(id, SPIRVCrossDecorationPhysicalTypePacked))
		return join(expr, "[", index, "]");
	else
		return join(expr, ".", index_to_swizzle(index));
}

string CompilerGLSL::to_extract_constant_composite_expression(uint32_t result_type, const SPIRConstant &c,
                                                              const uint32_t *chain, uint32_t length)
{
	// It is kinda silly if application actually enter this path since they know the constant up front.
	// It is useful here to extract the plain constant directly.
	SPIRConstant tmp;
	tmp.constant_type = result_type;
	auto &composite_type = get<SPIRType>(c.constant_type);
	assert(composite_type.basetype != SPIRType::Struct && composite_type.array.empty());
	assert(!c.specialization);

	if (is_matrix(composite_type))
	{
		if (length == 2)
		{
			tmp.m.c[0].vecsize = 1;
			tmp.m.columns = 1;
			tmp.m.c[0].r[0] = c.m.c[chain[0]].r[chain[1]];
		}
		else
		{
			assert(length == 1);
			tmp.m.c[0].vecsize = composite_type.vecsize;
			tmp.m.columns = 1;
			tmp.m.c[0] = c.m.c[chain[0]];
		}
	}
	else
	{
		assert(length == 1);
		tmp.m.c[0].vecsize = 1;
		tmp.m.columns = 1;
		tmp.m.c[0].r[0] = c.m.c[0].r[chain[0]];
	}

	return constant_expression(tmp);
}

string CompilerGLSL::to_rerolled_array_expression(const SPIRType &parent_type,
                                                  const string &base_expr, const SPIRType &type)
{
	bool remapped_boolean = parent_type.basetype == SPIRType::Struct &&
	                        type.basetype == SPIRType::Boolean &&
	                        backend.boolean_in_struct_remapped_type != SPIRType::Boolean;

	SPIRType tmp_type;
	if (remapped_boolean)
	{
		tmp_type = get<SPIRType>(type.parent_type);
		tmp_type.basetype = backend.boolean_in_struct_remapped_type;
	}
	else if (type.basetype == SPIRType::Boolean && backend.boolean_in_struct_remapped_type != SPIRType::Boolean)
	{
		// It's possible that we have an r-value expression that was OpLoaded from a struct.
		// We have to reroll this and explicitly cast the input to bool, because the r-value is short.
		tmp_type = get<SPIRType>(type.parent_type);
		remapped_boolean = true;
	}

	uint32_t size = to_array_size_literal(type);
	auto &parent = get<SPIRType>(type.parent_type);
	string expr = "{ ";

	for (uint32_t i = 0; i < size; i++)
	{
		auto subexpr = join(base_expr, "[", convert_to_string(i), "]");
		if (!type_is_top_level_array(parent))
		{
			if (remapped_boolean)
				subexpr = join(type_to_glsl(tmp_type), "(", subexpr, ")");
			expr += subexpr;
		}
		else
			expr += to_rerolled_array_expression(parent_type, subexpr, parent);

		if (i + 1 < size)
			expr += ", ";
	}

	expr += " }";
	return expr;
}

string CompilerGLSL::to_composite_constructor_expression(const SPIRType &parent_type, uint32_t id, bool block_like_type)
{
	auto &type = expression_type(id);

	bool reroll_array = false;
	bool remapped_boolean = parent_type.basetype == SPIRType::Struct &&
	                        type.basetype == SPIRType::Boolean &&
	                        backend.boolean_in_struct_remapped_type != SPIRType::Boolean;

	if (type_is_top_level_array(type))
	{
		reroll_array = !backend.array_is_value_type ||
		               (block_like_type && !backend.array_is_value_type_in_buffer_blocks);

		if (remapped_boolean)
		{
			// Forced to reroll if we have to change bool[] to short[].
			reroll_array = true;
		}
	}

	if (reroll_array)
	{
		// For this case, we need to "re-roll" an array initializer from a temporary.
		// We cannot simply pass the array directly, since it decays to a pointer and it cannot
		// participate in a struct initializer. E.g.
		// float arr[2] = { 1.0, 2.0 };
		// Foo foo = { arr }; must be transformed to
		// Foo foo = { { arr[0], arr[1] } };
		// The array sizes cannot be deduced from specialization constants since we cannot use any loops.

		// We're only triggering one read of the array expression, but this is fine since arrays have to be declared
		// as temporaries anyways.
		return to_rerolled_array_expression(parent_type, to_enclosed_expression(id), type);
	}
	else
	{
		auto expr = to_unpacked_expression(id);
		if (remapped_boolean)
		{
			auto tmp_type = type;
			tmp_type.basetype = backend.boolean_in_struct_remapped_type;
			expr = join(type_to_glsl(tmp_type), "(", expr, ")");
		}

		return expr;
	}
}

string CompilerGLSL::to_non_uniform_aware_expression(uint32_t id)
{
	string expr = to_expression(id);

	if (has_decoration(id, DecorationNonUniform))
		convert_non_uniform_expression(expr, id);

	return expr;
}

string CompilerGLSL::to_expression(uint32_t id, bool register_expression_read)
{
	auto itr = invalid_expressions.find(id);
	if (itr != end(invalid_expressions))
		handle_invalid_expression(id);

	if (ir.ids[id].get_type() == TypeExpression)
	{
		// We might have a more complex chain of dependencies.
		// A possible scenario is that we
		//
		// %1 = OpLoad
		// %2 = OpDoSomething %1 %1. here %2 will have a dependency on %1.
		// %3 = OpDoSomethingAgain %2 %2. Here %3 will lose the link to %1 since we don't propagate the dependencies like that.
		// OpStore %1 %foo // Here we can invalidate %1, and hence all expressions which depend on %1. Only %2 will know since it's part of invalid_expressions.
		// %4 = OpDoSomethingAnotherTime %3 %3 // If we forward all expressions we will see %1 expression after store, not before.
		//
		// However, we can propagate up a list of depended expressions when we used %2, so we can check if %2 is invalid when reading %3 after the store,
		// and see that we should not forward reads of the original variable.
		auto &expr = get<SPIRExpression>(id);
		for (uint32_t dep : expr.expression_dependencies)
			if (invalid_expressions.find(dep) != end(invalid_expressions))
				handle_invalid_expression(dep);
	}

	if (register_expression_read)
		track_expression_read(id);

	switch (ir.ids[id].get_type())
	{
	case TypeExpression:
	{
		auto &e = get<SPIRExpression>(id);
		if (e.base_expression)
			return to_enclosed_expression(e.base_expression) + e.expression;
		else if (e.need_transpose)
		{
			// This should not be reached for access chains, since we always deal explicitly with transpose state
			// when consuming an access chain expression.
			uint32_t physical_type_id = get_extended_decoration(id, SPIRVCrossDecorationPhysicalTypeID);
			bool is_packed = has_extended_decoration(id, SPIRVCrossDecorationPhysicalTypePacked);
			bool relaxed = has_decoration(id, DecorationRelaxedPrecision);
			return convert_row_major_matrix(e.expression, get<SPIRType>(e.expression_type), physical_type_id,
			                                is_packed, relaxed);
		}
		else if (flattened_structs.count(id))
		{
			return load_flattened_struct(e.expression, get<SPIRType>(e.expression_type));
		}
		else
		{
			if (is_forcing_recompilation())
			{
				// During first compilation phase, certain expression patterns can trigger exponential growth of memory.
				// Avoid this by returning dummy expressions during this phase.
				// Do not use empty expressions here, because those are sentinels for other cases.
				return "_";
			}
			else
				return e.expression;
		}
	}

	case TypeConstant:
	{
		auto &c = get<SPIRConstant>(id);
		auto &type = get<SPIRType>(c.constant_type);

		// WorkGroupSize may be a constant.
		if (has_decoration(c.self, DecorationBuiltIn))
			return builtin_to_glsl(BuiltIn(get_decoration(c.self, DecorationBuiltIn)), StorageClassGeneric);
		else if (c.specialization)
		{
			if (backend.workgroup_size_is_hidden)
			{
				int wg_index = get_constant_mapping_to_workgroup_component(c);
				if (wg_index >= 0)
				{
					auto wg_size = join(builtin_to_glsl(BuiltInWorkgroupSize, StorageClassInput), vector_swizzle(1, wg_index));
					if (type.basetype != SPIRType::UInt)
						wg_size = bitcast_expression(type, SPIRType::UInt, wg_size);
					return wg_size;
				}
			}

			if (expression_is_forwarded(id))
				return constant_expression(c);

			return to_name(id);
		}
		else if (c.is_used_as_lut)
			return to_name(id);
		else if (type.basetype == SPIRType::Struct && !backend.can_declare_struct_inline)
			return to_name(id);
		else if (!type.array.empty() && !backend.can_declare_arrays_inline)
			return to_name(id);
		else
			return constant_expression(c);
	}

	case TypeConstantOp:
		return to_name(id);

	case TypeVariable:
	{
		auto &var = get<SPIRVariable>(id);
		// If we try to use a loop variable before the loop header, we have to redirect it to the static expression,
		// the variable has not been declared yet.
		if (var.statically_assigned || (var.loop_variable && !var.loop_variable_enable))
		{
			// We might try to load from a loop variable before it has been initialized.
			// Prefer static expression and fallback to initializer.
			if (var.static_expression)
				return to_expression(var.static_expression);
			else if (var.initializer)
				return to_expression(var.initializer);
			else
			{
				// We cannot declare the variable yet, so have to fake it.
				uint32_t undef_id = ir.increase_bound_by(1);
				return emit_uninitialized_temporary_expression(get_variable_data_type_id(var), undef_id).expression;
			}
		}
		else if (var.deferred_declaration)
		{
			var.deferred_declaration = false;
			return variable_decl(var);
		}
		else if (flattened_structs.count(id))
		{
			return load_flattened_struct(to_name(id), get<SPIRType>(var.basetype));
		}
		else
		{
			auto &dec = ir.meta[var.self].decoration;
			if (dec.builtin)
				return builtin_to_glsl(dec.builtin_type, var.storage);
			else
				return to_name(id);
		}
	}

	case TypeCombinedImageSampler:
		// This type should never be taken the expression of directly.
		// The intention is that texture sampling functions will extract the image and samplers
		// separately and take their expressions as needed.
		// GLSL does not use this type because OpSampledImage immediately creates a combined image sampler
		// expression ala sampler2D(texture, sampler).
		SPIRV_CROSS_THROW("Combined image samplers have no default expression representation.");

	case TypeAccessChain:
		// We cannot express this type. They only have meaning in other OpAccessChains, OpStore or OpLoad.
		SPIRV_CROSS_THROW("Access chains have no default expression representation.");

	default:
		return to_name(id);
	}
}

SmallVector<ConstantID> CompilerGLSL::get_composite_constant_ids(ConstantID const_id)
{
	if (auto *constant = maybe_get<SPIRConstant>(const_id))
	{
		const auto &type = get<SPIRType>(constant->constant_type);
		if (is_array(type) || type.basetype == SPIRType::Struct)
			return constant->subconstants;
		if (is_matrix(type))
			return SmallVector<ConstantID>(constant->m.id);
		if (is_vector(type))
			return SmallVector<ConstantID>(constant->m.c[0].id);
		SPIRV_CROSS_THROW("Unexpected scalar constant!");
	}
	if (!const_composite_insert_ids.count(const_id))
		SPIRV_CROSS_THROW("Unimplemented for this OpSpecConstantOp!");
	return const_composite_insert_ids[const_id];
}

void CompilerGLSL::fill_composite_constant(SPIRConstant &constant, TypeID type_id,
                                           const SmallVector<ConstantID> &initializers)
{
	auto &type = get<SPIRType>(type_id);
	constant.specialization = true;
	if (is_array(type) || type.basetype == SPIRType::Struct)
	{
		constant.subconstants = initializers;
	}
	else if (is_matrix(type))
	{
		constant.m.columns = type.columns;
		for (uint32_t i = 0; i < type.columns; ++i)
		{
			constant.m.id[i] = initializers[i];
			constant.m.c[i].vecsize = type.vecsize;
		}
	}
	else if (is_vector(type))
	{
		constant.m.c[0].vecsize = type.vecsize;
		for (uint32_t i = 0; i < type.vecsize; ++i)
			constant.m.c[0].id[i] = initializers[i];
	}
	else
		SPIRV_CROSS_THROW("Unexpected scalar in SpecConstantOp CompositeInsert!");
}

void CompilerGLSL::set_composite_constant(ConstantID const_id, TypeID type_id,
                                          const SmallVector<ConstantID> &initializers)
{
	if (maybe_get<SPIRConstantOp>(const_id))
	{
		const_composite_insert_ids[const_id] = initializers;
		return;
	}

	auto &constant = set<SPIRConstant>(const_id, type_id);
	fill_composite_constant(constant, type_id, initializers);
	forwarded_temporaries.insert(const_id);
}

TypeID CompilerGLSL::get_composite_member_type(TypeID type_id, uint32_t member_idx)
{
	auto &type = get<SPIRType>(type_id);
	if (is_array(type))
		return type.parent_type;
	if (type.basetype == SPIRType::Struct)
		return type.member_types[member_idx];
	if (is_matrix(type))
		return type.parent_type;
	if (is_vector(type))
		return type.parent_type;
	SPIRV_CROSS_THROW("Shouldn't reach lower than vector handling OpSpecConstantOp CompositeInsert!");
}

string CompilerGLSL::constant_op_expression(const SPIRConstantOp &cop)
{
	auto &type = get<SPIRType>(cop.basetype);
	bool binary = false;
	bool unary = false;
	string op;

	if (is_legacy() && is_unsigned_opcode(cop.opcode))
		SPIRV_CROSS_THROW("Unsigned integers are not supported on legacy targets.");

	// TODO: Find a clean way to reuse emit_instruction.
	switch (cop.opcode)
	{
	case OpSConvert:
	case OpUConvert:
	case OpFConvert:
		op = type_to_glsl_constructor(type);
		break;

#define GLSL_BOP(opname, x) \
	case Op##opname:        \
		binary = true;      \
		op = x;             \
		break

#define GLSL_UOP(opname, x) \
	case Op##opname:        \
		unary = true;       \
		op = x;             \
		break

		GLSL_UOP(SNegate, "-");
		GLSL_UOP(Not, "~");
		GLSL_BOP(IAdd, "+");
		GLSL_BOP(ISub, "-");
		GLSL_BOP(IMul, "*");
		GLSL_BOP(SDiv, "/");
		GLSL_BOP(UDiv, "/");
		GLSL_BOP(UMod, "%");
		GLSL_BOP(SMod, "%");
		GLSL_BOP(ShiftRightLogical, ">>");
		GLSL_BOP(ShiftRightArithmetic, ">>");
		GLSL_BOP(ShiftLeftLogical, "<<");
		GLSL_BOP(BitwiseOr, "|");
		GLSL_BOP(BitwiseXor, "^");
		GLSL_BOP(BitwiseAnd, "&");
		GLSL_BOP(LogicalOr, "||");
		GLSL_BOP(LogicalAnd, "&&");
		GLSL_UOP(LogicalNot, "!");
		GLSL_BOP(LogicalEqual, "==");
		GLSL_BOP(LogicalNotEqual, "!=");
		GLSL_BOP(IEqual, "==");
		GLSL_BOP(INotEqual, "!=");
		GLSL_BOP(ULessThan, "<");
		GLSL_BOP(SLessThan, "<");
		GLSL_BOP(ULessThanEqual, "<=");
		GLSL_BOP(SLessThanEqual, "<=");
		GLSL_BOP(UGreaterThan, ">");
		GLSL_BOP(SGreaterThan, ">");
		GLSL_BOP(UGreaterThanEqual, ">=");
		GLSL_BOP(SGreaterThanEqual, ">=");

	case OpSRem:
	{
		uint32_t op0 = cop.arguments[0];
		uint32_t op1 = cop.arguments[1];
		return join(to_enclosed_expression(op0), " - ", to_enclosed_expression(op1), " * ", "(",
		                 to_enclosed_expression(op0), " / ", to_enclosed_expression(op1), ")");
	}

	case OpSelect:
	{
		if (cop.arguments.size() < 3)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		// This one is pretty annoying. It's triggered from
		// uint(bool), int(bool) from spec constants.
		// In order to preserve its compile-time constness in Vulkan GLSL,
		// we need to reduce the OpSelect expression back to this simplified model.
		// If we cannot, fail.
		if (to_trivial_mix_op(type, op, cop.arguments[2], cop.arguments[1], cop.arguments[0]))
		{
			// Implement as a simple cast down below.
		}
		else
		{
			// Implement a ternary and pray the compiler understands it :)
			return to_ternary_expression(type, cop.arguments[0], cop.arguments[1], cop.arguments[2]);
		}
		break;
	}

	case OpVectorShuffle:
	{
		string expr = type_to_glsl_constructor(type);
		expr += "(";

		uint32_t left_components = expression_type(cop.arguments[0]).vecsize;
		string left_arg = to_enclosed_expression(cop.arguments[0]);
		string right_arg = to_enclosed_expression(cop.arguments[1]);

		for (uint32_t i = 2; i < uint32_t(cop.arguments.size()); i++)
		{
			uint32_t index = cop.arguments[i];
			if (index == 0xFFFFFFFF)
			{
				SPIRConstant c;
				c.constant_type = type.parent_type;
				assert(type.parent_type != ID(0));
				expr += constant_expression(c);
			}
			else if (index >= left_components)
			{
				expr += right_arg + "." + "xyzw"[index - left_components];
			}
			else
			{
				expr += left_arg + "." + "xyzw"[index];
			}

			if (i + 1 < uint32_t(cop.arguments.size()))
				expr += ", ";
		}

		expr += ")";
		return expr;
	}

	case OpCompositeExtract:
	{
		auto expr = access_chain_internal(cop.arguments[0], &cop.arguments[1], uint32_t(cop.arguments.size() - 1),
		                                  ACCESS_CHAIN_INDEX_IS_LITERAL_BIT, nullptr);
		return expr;
	}

	case OpCompositeInsert:
	{
		SmallVector<ConstantID> new_init = get_composite_constant_ids(cop.arguments[1]);
		uint32_t idx;
		uint32_t target_id = cop.self;
		uint32_t target_type_id = cop.basetype;
		// We have to drill down to the part we want to modify, and create new
		// constants for each containing part.
		for (idx = 2; idx < cop.arguments.size() - 1; ++idx)
		{
			uint32_t new_const = ir.increase_bound_by(1);
			uint32_t old_const = new_init[cop.arguments[idx]];
			new_init[cop.arguments[idx]] = new_const;
			set_composite_constant(target_id, target_type_id, new_init);
			new_init = get_composite_constant_ids(old_const);
			target_id = new_const;
			target_type_id = get_composite_member_type(target_type_id, cop.arguments[idx]);
		}
		// Now replace the initializer with the one from this instruction.
		new_init[cop.arguments[idx]] = cop.arguments[0];
		set_composite_constant(target_id, target_type_id, new_init);
		SPIRConstant tmp_const(cop.basetype);
		fill_composite_constant(tmp_const, cop.basetype, const_composite_insert_ids[cop.self]);
		return constant_expression(tmp_const);
	}

	default:
		// Some opcodes are unimplemented here, these are currently not possible to test from glslang.
		SPIRV_CROSS_THROW("Unimplemented spec constant op.");
	}

	uint32_t bit_width = 0;
	if (unary || binary || cop.opcode == OpSConvert || cop.opcode == OpUConvert)
		bit_width = expression_type(cop.arguments[0]).width;

	SPIRType::BaseType input_type;
	bool skip_cast_if_equal_type = opcode_is_sign_invariant(cop.opcode);

	switch (cop.opcode)
	{
	case OpIEqual:
	case OpINotEqual:
		input_type = to_signed_basetype(bit_width);
		break;

	case OpSLessThan:
	case OpSLessThanEqual:
	case OpSGreaterThan:
	case OpSGreaterThanEqual:
	case OpSMod:
	case OpSDiv:
	case OpShiftRightArithmetic:
	case OpSConvert:
	case OpSNegate:
		input_type = to_signed_basetype(bit_width);
		break;

	case OpULessThan:
	case OpULessThanEqual:
	case OpUGreaterThan:
	case OpUGreaterThanEqual:
	case OpUMod:
	case OpUDiv:
	case OpShiftRightLogical:
	case OpUConvert:
		input_type = to_unsigned_basetype(bit_width);
		break;

	default:
		input_type = type.basetype;
		break;
	}

#undef GLSL_BOP
#undef GLSL_UOP
	if (binary)
	{
		if (cop.arguments.size() < 2)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		string cast_op0;
		string cast_op1;
		auto expected_type = binary_op_bitcast_helper(cast_op0, cast_op1, input_type, cop.arguments[0],
		                                              cop.arguments[1], skip_cast_if_equal_type);

		if (type.basetype != input_type && type.basetype != SPIRType::Boolean)
		{
			expected_type.basetype = input_type;
			auto expr = bitcast_glsl_op(type, expected_type);
			expr += '(';
			expr += join(cast_op0, " ", op, " ", cast_op1);
			expr += ')';
			return expr;
		}
		else
			return join("(", cast_op0, " ", op, " ", cast_op1, ")");
	}
	else if (unary)
	{
		if (cop.arguments.size() < 1)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		// Auto-bitcast to result type as needed.
		// Works around various casting scenarios in glslang as there is no OpBitcast for specialization constants.
		return join("(", op, bitcast_glsl(type, cop.arguments[0]), ")");
	}
	else if (cop.opcode == OpSConvert || cop.opcode == OpUConvert)
	{
		if (cop.arguments.size() < 1)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		auto &arg_type = expression_type(cop.arguments[0]);
		if (arg_type.width < type.width && input_type != arg_type.basetype)
		{
			auto expected = arg_type;
			expected.basetype = input_type;
			return join(op, "(", bitcast_glsl(expected, cop.arguments[0]), ")");
		}
		else
			return join(op, "(", to_expression(cop.arguments[0]), ")");
	}
	else
	{
		if (cop.arguments.size() < 1)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");
		return join(op, "(", to_expression(cop.arguments[0]), ")");
	}
}

string CompilerGLSL::constant_expression(const SPIRConstant &c,
                                         bool inside_block_like_struct_scope,
                                         bool inside_struct_scope)
{
	auto &type = get<SPIRType>(c.constant_type);

	if (type_is_top_level_pointer(type))
	{
		return backend.null_pointer_literal;
	}
	else if (!c.subconstants.empty())
	{
		// Handles Arrays and structures.
		string res;

		// Only consider the decay if we are inside a struct scope where we are emitting a member with Offset decoration.
		// Outside a block-like struct declaration, we can always bind to a constant array with templated type.
		// Should look at ArrayStride here as well, but it's possible to declare a constant struct
		// with Offset = 0, using no ArrayStride on the enclosed array type.
		// A particular CTS test hits this scenario.
		bool array_type_decays = inside_block_like_struct_scope &&
		                         type_is_top_level_array(type) &&
		                         !backend.array_is_value_type_in_buffer_blocks;

		// Allow Metal to use the array<T> template to make arrays a value type
		bool needs_trailing_tracket = false;
		if (backend.use_initializer_list && backend.use_typed_initializer_list && type.basetype == SPIRType::Struct &&
		    !type_is_top_level_array(type))
		{
			res = type_to_glsl_constructor(type) + "{ ";
		}
		else if (backend.use_initializer_list && backend.use_typed_initializer_list && backend.array_is_value_type &&
		         type_is_top_level_array(type) && !array_type_decays)
		{
			const auto *p_type = &type;
			SPIRType tmp_type;

			if (inside_struct_scope &&
			    backend.boolean_in_struct_remapped_type != SPIRType::Boolean &&
			    type.basetype == SPIRType::Boolean)
			{
				tmp_type = type;
				tmp_type.basetype = backend.boolean_in_struct_remapped_type;
				p_type = &tmp_type;
			}

			res = type_to_glsl_constructor(*p_type) + "({ ";
			needs_trailing_tracket = true;
		}
		else if (backend.use_initializer_list)
		{
			res = "{ ";
		}
		else
		{
			res = type_to_glsl_constructor(type) + "(";
		}

		uint32_t subconstant_index = 0;
		for (auto &elem : c.subconstants)
		{
			if (auto *op = maybe_get<SPIRConstantOp>(elem))
			{
				res += constant_op_expression(*op);
			}
			else if (maybe_get<SPIRUndef>(elem) != nullptr)
			{
				res += to_name(elem);
			}
			else
			{
				auto &subc = get<SPIRConstant>(elem);
				if (subc.specialization && !expression_is_forwarded(elem))
					res += to_name(elem);
				else
				{
					if (!type_is_top_level_array(type) && type.basetype == SPIRType::Struct)
					{
						// When we get down to emitting struct members, override the block-like information.
						// For constants, we can freely mix and match block-like state.
						inside_block_like_struct_scope =
						    has_member_decoration(type.self, subconstant_index, DecorationOffset);
					}

					if (type.basetype == SPIRType::Struct)
						inside_struct_scope = true;

					res += constant_expression(subc, inside_block_like_struct_scope, inside_struct_scope);
				}
			}

			if (&elem != &c.subconstants.back())
				res += ", ";

			subconstant_index++;
		}

		res += backend.use_initializer_list ? " }" : ")";
		if (needs_trailing_tracket)
			res += ")";

		return res;
	}
	else if (type.basetype == SPIRType::Struct && type.member_types.size() == 0)
	{
		// Metal tessellation likes empty structs which are then constant expressions.
		if (backend.supports_empty_struct)
			return "{ }";
		else if (backend.use_typed_initializer_list)
			return join(type_to_glsl(type), "{ 0 }");
		else if (backend.use_initializer_list)
			return "{ 0 }";
		else
			return join(type_to_glsl(type), "(0)");
	}
	else if (c.columns() == 1)
	{
		auto res = constant_expression_vector(c, 0);

		if (inside_struct_scope &&
		    backend.boolean_in_struct_remapped_type != SPIRType::Boolean &&
		    type.basetype == SPIRType::Boolean)
		{
			SPIRType tmp_type = type;
			tmp_type.basetype = backend.boolean_in_struct_remapped_type;
			res = join(type_to_glsl(tmp_type), "(", res, ")");
		}

		return res;
	}
	else
	{
		string res = type_to_glsl(type) + "(";
		for (uint32_t col = 0; col < c.columns(); col++)
		{
			if (c.specialization_constant_id(col) != 0)
				res += to_name(c.specialization_constant_id(col));
			else
				res += constant_expression_vector(c, col);

			if (col + 1 < c.columns())
				res += ", ";
		}
		res += ")";

		if (inside_struct_scope &&
		    backend.boolean_in_struct_remapped_type != SPIRType::Boolean &&
		    type.basetype == SPIRType::Boolean)
		{
			SPIRType tmp_type = type;
			tmp_type.basetype = backend.boolean_in_struct_remapped_type;
			res = join(type_to_glsl(tmp_type), "(", res, ")");
		}

		return res;
	}
}

#ifdef _MSC_VER
// snprintf does not exist or is buggy on older MSVC versions, some of them
// being used by MinGW. Use sprintf instead and disable corresponding warning.
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

string CompilerGLSL::convert_half_to_string(const SPIRConstant &c, uint32_t col, uint32_t row)
{
	string res;
	float float_value = c.scalar_f16(col, row);

	// There is no literal "hf" in GL_NV_gpu_shader5, so to avoid lots
	// of complicated workarounds, just value-cast to the half type always.
	if (std::isnan(float_value) || std::isinf(float_value))
	{
		SPIRType type;
		type.basetype = SPIRType::Half;
		type.vecsize = 1;
		type.columns = 1;

		if (float_value == numeric_limits<float>::infinity())
			res = join(type_to_glsl(type), "(1.0 / 0.0)");
		else if (float_value == -numeric_limits<float>::infinity())
			res = join(type_to_glsl(type), "(-1.0 / 0.0)");
		else if (std::isnan(float_value))
			res = join(type_to_glsl(type), "(0.0 / 0.0)");
		else
			SPIRV_CROSS_THROW("Cannot represent non-finite floating point constant.");
	}
	else
	{
		SPIRType type;
		type.basetype = SPIRType::Half;
		type.vecsize = 1;
		type.columns = 1;
		res = join(type_to_glsl(type), "(", convert_to_string(float_value, current_locale_radix_character), ")");
	}

	return res;
}

string CompilerGLSL::convert_float_to_string(const SPIRConstant &c, uint32_t col, uint32_t row)
{
	string res;
	float float_value = c.scalar_f32(col, row);

	if (std::isnan(float_value) || std::isinf(float_value))
	{
		// Use special representation.
		if (!is_legacy())
		{
			SPIRType out_type;
			SPIRType in_type;
			out_type.basetype = SPIRType::Float;
			in_type.basetype = SPIRType::UInt;
			out_type.vecsize = 1;
			in_type.vecsize = 1;
			out_type.width = 32;
			in_type.width = 32;

			char print_buffer[32];
#ifdef _WIN32
			sprintf(print_buffer, "0x%xu", c.scalar(col, row));
#else
			snprintf(print_buffer, sizeof(print_buffer), "0x%xu", c.scalar(col, row));
#endif

			const char *comment = "inf";
			if (float_value == -numeric_limits<float>::infinity())
				comment = "-inf";
			else if (std::isnan(float_value))
				comment = "nan";
			res = join(bitcast_glsl_op(out_type, in_type), "(", print_buffer, " /* ", comment, " */)");
		}
		else
		{
			if (float_value == numeric_limits<float>::infinity())
			{
				if (backend.float_literal_suffix)
					res = "(1.0f / 0.0f)";
				else
					res = "(1.0 / 0.0)";
			}
			else if (float_value == -numeric_limits<float>::infinity())
			{
				if (backend.float_literal_suffix)
					res = "(-1.0f / 0.0f)";
				else
					res = "(-1.0 / 0.0)";
			}
			else if (std::isnan(float_value))
			{
				if (backend.float_literal_suffix)
					res = "(0.0f / 0.0f)";
				else
					res = "(0.0 / 0.0)";
			}
			else
				SPIRV_CROSS_THROW("Cannot represent non-finite floating point constant.");
		}
	}
	else
	{
		res = convert_to_string(float_value, current_locale_radix_character);
		if (backend.float_literal_suffix)
			res += "f";
	}

	return res;
}

std::string CompilerGLSL::convert_double_to_string(const SPIRConstant &c, uint32_t col, uint32_t row)
{
	string res;
	double double_value = c.scalar_f64(col, row);

	if (std::isnan(double_value) || std::isinf(double_value))
	{
		// Use special representation.
		if (!is_legacy())
		{
			SPIRType out_type;
			SPIRType in_type;
			out_type.basetype = SPIRType::Double;
			in_type.basetype = SPIRType::UInt64;
			out_type.vecsize = 1;
			in_type.vecsize = 1;
			out_type.width = 64;
			in_type.width = 64;

			uint64_t u64_value = c.scalar_u64(col, row);

			if (options.es && options.version < 310) // GL_NV_gpu_shader5 fallback requires 310.
				SPIRV_CROSS_THROW("64-bit integers not supported in ES profile before version 310.");
			require_extension_internal("GL_ARB_gpu_shader_int64");

			char print_buffer[64];
#ifdef _WIN32
			sprintf(print_buffer, "0x%llx%s", static_cast<unsigned long long>(u64_value),
			        backend.long_long_literal_suffix ? "ull" : "ul");
#else
			snprintf(print_buffer, sizeof(print_buffer), "0x%llx%s", static_cast<unsigned long long>(u64_value),
			         backend.long_long_literal_suffix ? "ull" : "ul");
#endif

			const char *comment = "inf";
			if (double_value == -numeric_limits<double>::infinity())
				comment = "-inf";
			else if (std::isnan(double_value))
				comment = "nan";
			res = join(bitcast_glsl_op(out_type, in_type), "(", print_buffer, " /* ", comment, " */)");
		}
		else
		{
			if (options.es)
				SPIRV_CROSS_THROW("FP64 not supported in ES profile.");
			if (options.version < 400)
				require_extension_internal("GL_ARB_gpu_shader_fp64");

			if (double_value == numeric_limits<double>::infinity())
			{
				if (backend.double_literal_suffix)
					res = "(1.0lf / 0.0lf)";
				else
					res = "(1.0 / 0.0)";
			}
			else if (double_value == -numeric_limits<double>::infinity())
			{
				if (backend.double_literal_suffix)
					res = "(-1.0lf / 0.0lf)";
				else
					res = "(-1.0 / 0.0)";
			}
			else if (std::isnan(double_value))
			{
				if (backend.double_literal_suffix)
					res = "(0.0lf / 0.0lf)";
				else
					res = "(0.0 / 0.0)";
			}
			else
				SPIRV_CROSS_THROW("Cannot represent non-finite floating point constant.");
		}
	}
	else
	{
		res = convert_to_string(double_value, current_locale_radix_character);
		if (backend.double_literal_suffix)
			res += "lf";
	}

	return res;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

string CompilerGLSL::constant_expression_vector(const SPIRConstant &c, uint32_t vector)
{
	auto type = get<SPIRType>(c.constant_type);
	type.columns = 1;

	auto scalar_type = type;
	scalar_type.vecsize = 1;

	string res;
	bool splat = backend.use_constructor_splatting && c.vector_size() > 1;
	bool swizzle_splat = backend.can_swizzle_scalar && c.vector_size() > 1;

	if (!type_is_floating_point(type))
	{
		// Cannot swizzle literal integers as a special case.
		swizzle_splat = false;
	}

	if (splat || swizzle_splat)
	{
		// Cannot use constant splatting if we have specialization constants somewhere in the vector.
		for (uint32_t i = 0; i < c.vector_size(); i++)
		{
			if (c.specialization_constant_id(vector, i) != 0)
			{
				splat = false;
				swizzle_splat = false;
				break;
			}
		}
	}

	if (splat || swizzle_splat)
	{
		if (type.width == 64)
		{
			uint64_t ident = c.scalar_u64(vector, 0);
			for (uint32_t i = 1; i < c.vector_size(); i++)
			{
				if (ident != c.scalar_u64(vector, i))
				{
					splat = false;
					swizzle_splat = false;
					break;
				}
			}
		}
		else
		{
			uint32_t ident = c.scalar(vector, 0);
			for (uint32_t i = 1; i < c.vector_size(); i++)
			{
				if (ident != c.scalar(vector, i))
				{
					splat = false;
					swizzle_splat = false;
				}
			}
		}
	}

	if (c.vector_size() > 1 && !swizzle_splat)
		res += type_to_glsl(type) + "(";

	switch (type.basetype)
	{
	case SPIRType::Half:
		if (splat || swizzle_splat)
		{
			res += convert_half_to_string(c, vector, 0);
			if (swizzle_splat)
				res = remap_swizzle(get<SPIRType>(c.constant_type), 1, res);
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
					res += convert_half_to_string(c, vector, i);

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Float:
		if (splat || swizzle_splat)
		{
			res += convert_float_to_string(c, vector, 0);
			if (swizzle_splat)
				res = remap_swizzle(get<SPIRType>(c.constant_type), 1, res);
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
					res += convert_float_to_string(c, vector, i);

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Double:
		if (splat || swizzle_splat)
		{
			res += convert_double_to_string(c, vector, 0);
			if (swizzle_splat)
				res = remap_swizzle(get<SPIRType>(c.constant_type), 1, res);
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
					res += convert_double_to_string(c, vector, i);

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Int64:
	{
		auto tmp = type;
		tmp.vecsize = 1;
		tmp.columns = 1;
		auto int64_type = type_to_glsl(tmp);

		if (splat)
		{
			res += convert_to_string(c.scalar_i64(vector, 0), int64_type, backend.long_long_literal_suffix);
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
					res += convert_to_string(c.scalar_i64(vector, i), int64_type, backend.long_long_literal_suffix);

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;
	}

	case SPIRType::UInt64:
		if (splat)
		{
			res += convert_to_string(c.scalar_u64(vector, 0));
			if (backend.long_long_literal_suffix)
				res += "ull";
			else
				res += "ul";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
				{
					res += convert_to_string(c.scalar_u64(vector, i));
					if (backend.long_long_literal_suffix)
						res += "ull";
					else
						res += "ul";
				}

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::UInt:
		if (splat)
		{
			res += convert_to_string(c.scalar(vector, 0));
			if (is_legacy())
			{
				// Fake unsigned constant literals with signed ones if possible.
				// Things like array sizes, etc, tend to be unsigned even though they could just as easily be signed.
				if (c.scalar_i32(vector, 0) < 0)
					SPIRV_CROSS_THROW("Tried to convert uint literal into int, but this made the literal negative.");
			}
			else if (backend.uint32_t_literal_suffix)
				res += "u";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
				{
					res += convert_to_string(c.scalar(vector, i));
					if (is_legacy())
					{
						// Fake unsigned constant literals with signed ones if possible.
						// Things like array sizes, etc, tend to be unsigned even though they could just as easily be signed.
						if (c.scalar_i32(vector, i) < 0)
							SPIRV_CROSS_THROW("Tried to convert uint literal into int, but this made "
							                  "the literal negative.");
					}
					else if (backend.uint32_t_literal_suffix)
						res += "u";
				}

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Int:
		if (splat)
			res += convert_to_string(c.scalar_i32(vector, 0));
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
					res += convert_to_string(c.scalar_i32(vector, i));
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::UShort:
		if (splat)
		{
			res += convert_to_string(c.scalar(vector, 0));
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
				{
					if (*backend.uint16_t_literal_suffix)
					{
						res += convert_to_string(c.scalar_u16(vector, i));
						res += backend.uint16_t_literal_suffix;
					}
					else
					{
						// If backend doesn't have a literal suffix, we need to value cast.
						res += type_to_glsl(scalar_type);
						res += "(";
						res += convert_to_string(c.scalar_u16(vector, i));
						res += ")";
					}
				}

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Short:
		if (splat)
		{
			res += convert_to_string(c.scalar_i16(vector, 0));
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
				{
					if (*backend.int16_t_literal_suffix)
					{
						res += convert_to_string(c.scalar_i16(vector, i));
						res += backend.int16_t_literal_suffix;
					}
					else
					{
						// If backend doesn't have a literal suffix, we need to value cast.
						res += type_to_glsl(scalar_type);
						res += "(";
						res += convert_to_string(c.scalar_i16(vector, i));
						res += ")";
					}
				}

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::UByte:
		if (splat)
		{
			res += convert_to_string(c.scalar_u8(vector, 0));
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
				{
					res += type_to_glsl(scalar_type);
					res += "(";
					res += convert_to_string(c.scalar_u8(vector, i));
					res += ")";
				}

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::SByte:
		if (splat)
		{
			res += convert_to_string(c.scalar_i8(vector, 0));
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
				{
					res += type_to_glsl(scalar_type);
					res += "(";
					res += convert_to_string(c.scalar_i8(vector, i));
					res += ")";
				}

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Boolean:
		if (splat)
			res += c.scalar(vector, 0) ? "true" : "false";
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				if (c.vector_size() > 1 && c.specialization_constant_id(vector, i) != 0)
					res += to_expression(c.specialization_constant_id(vector, i));
				else
					res += c.scalar(vector, i) ? "true" : "false";

				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	default:
		SPIRV_CROSS_THROW("Invalid constant expression basetype.");
	}

	if (c.vector_size() > 1 && !swizzle_splat)
		res += ")";

	return res;
}

SPIRExpression &CompilerGLSL::emit_uninitialized_temporary_expression(uint32_t type, uint32_t id)
{
	forced_temporaries.insert(id);
	emit_uninitialized_temporary(type, id);
	return set<SPIRExpression>(id, to_name(id), type, true);
}

void CompilerGLSL::emit_uninitialized_temporary(uint32_t result_type, uint32_t result_id)
{
	// If we're declaring temporaries inside continue blocks,
	// we must declare the temporary in the loop header so that the continue block can avoid declaring new variables.
	if (!block_temporary_hoisting && current_continue_block && !hoisted_temporaries.count(result_id))
	{
		auto &header = get<SPIRBlock>(current_continue_block->loop_dominator);
		if (find_if(begin(header.declare_temporary), end(header.declare_temporary),
		            [result_type, result_id](const pair<uint32_t, uint32_t> &tmp) {
			            return tmp.first == result_type && tmp.second == result_id;
		            }) == end(header.declare_temporary))
		{
			header.declare_temporary.emplace_back(result_type, result_id);
			hoisted_temporaries.insert(result_id);
			force_recompile();
		}
	}
	else if (hoisted_temporaries.count(result_id) == 0)
	{
		auto &type = get<SPIRType>(result_type);
		auto &flags = get_decoration_bitset(result_id);

		// The result_id has not been made into an expression yet, so use flags interface.
		add_local_variable_name(result_id);

		string initializer;
		if (options.force_zero_initialized_variables && type_can_zero_initialize(type))
			initializer = join(" = ", to_zero_initialized_expression(result_type));

		statement(flags_to_qualifiers_glsl(type, flags), variable_decl(type, to_name(result_id)), initializer, ";");
	}
}

string CompilerGLSL::declare_temporary(uint32_t result_type, uint32_t result_id)
{
	auto &type = get<SPIRType>(result_type);

	// If we're declaring temporaries inside continue blocks,
	// we must declare the temporary in the loop header so that the continue block can avoid declaring new variables.
	if (!block_temporary_hoisting && current_continue_block && !hoisted_temporaries.count(result_id))
	{
		auto &header = get<SPIRBlock>(current_continue_block->loop_dominator);
		if (find_if(begin(header.declare_temporary), end(header.declare_temporary),
		            [result_type, result_id](const pair<uint32_t, uint32_t> &tmp) {
			            return tmp.first == result_type && tmp.second == result_id;
		            }) == end(header.declare_temporary))
		{
			header.declare_temporary.emplace_back(result_type, result_id);
			hoisted_temporaries.insert(result_id);
			force_recompile_guarantee_forward_progress();
		}

		return join(to_name(result_id), " = ");
	}
	else if (hoisted_temporaries.count(result_id))
	{
		// The temporary has already been declared earlier, so just "declare" the temporary by writing to it.
		return join(to_name(result_id), " = ");
	}
	else
	{
		// The result_id has not been made into an expression yet, so use flags interface.
		add_local_variable_name(result_id);
		auto &flags = get_decoration_bitset(result_id);
		return join(flags_to_qualifiers_glsl(type, flags), variable_decl(type, to_name(result_id)), " = ");
	}
}

bool CompilerGLSL::expression_is_forwarded(uint32_t id) const
{
	return forwarded_temporaries.count(id) != 0;
}

bool CompilerGLSL::expression_suppresses_usage_tracking(uint32_t id) const
{
	return suppressed_usage_tracking.count(id) != 0;
}

bool CompilerGLSL::expression_read_implies_multiple_reads(uint32_t id) const
{
	auto *expr = maybe_get<SPIRExpression>(id);
	if (!expr)
		return false;

	// If we're emitting code at a deeper loop level than when we emitted the expression,
	// we're probably reading the same expression over and over.
	return current_loop_level > expr->emitted_loop_level;
}

SPIRExpression &CompilerGLSL::emit_op(uint32_t result_type, uint32_t result_id, const string &rhs, bool forwarding,
                                      bool suppress_usage_tracking)
{
	if (forwarding && (forced_temporaries.find(result_id) == end(forced_temporaries)))
	{
		// Just forward it without temporary.
		// If the forward is trivial, we do not force flushing to temporary for this expression.
		forwarded_temporaries.insert(result_id);
		if (suppress_usage_tracking)
			suppressed_usage_tracking.insert(result_id);

		return set<SPIRExpression>(result_id, rhs, result_type, true);
	}
	else
	{
		// If expression isn't immutable, bind it to a temporary and make the new temporary immutable (they always are).
		statement(declare_temporary(result_type, result_id), rhs, ";");
		return set<SPIRExpression>(result_id, to_name(result_id), result_type, true);
	}
}

void CompilerGLSL::emit_unary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op)
{
	bool forward = should_forward(op0);
	emit_op(result_type, result_id, join(op, to_enclosed_unpacked_expression(op0)), forward);
	inherit_expression_dependencies(result_id, op0);
}

void CompilerGLSL::emit_unary_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op)
{
	auto &type = get<SPIRType>(result_type);
	bool forward = should_forward(op0);
	emit_op(result_type, result_id, join(type_to_glsl(type), "(", op, to_enclosed_unpacked_expression(op0), ")"), forward);
	inherit_expression_dependencies(result_id, op0);
}

void CompilerGLSL::emit_mesh_tasks(SPIRBlock &block)
{
	statement("EmitMeshTasksEXT(",
	          to_unpacked_expression(block.mesh.groups[0]), ", ",
	          to_unpacked_expression(block.mesh.groups[1]), ", ",
	          to_unpacked_expression(block.mesh.groups[2]), ");");
}

void CompilerGLSL::emit_binary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, const char *op)
{
	// Various FP arithmetic opcodes such as add, sub, mul will hit this.
	bool force_temporary_precise = backend.support_precise_qualifier &&
	                               has_decoration(result_id, DecorationNoContraction) &&
	                               type_is_floating_point(get<SPIRType>(result_type));
	bool forward = should_forward(op0) && should_forward(op1) && !force_temporary_precise;

	emit_op(result_type, result_id,
	        join(to_enclosed_unpacked_expression(op0), " ", op, " ", to_enclosed_unpacked_expression(op1)), forward);

	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
}

void CompilerGLSL::emit_unrolled_unary_op(uint32_t result_type, uint32_t result_id, uint32_t operand, const char *op)
{
	auto &type = get<SPIRType>(result_type);
	auto expr = type_to_glsl_constructor(type);
	expr += '(';
	for (uint32_t i = 0; i < type.vecsize; i++)
	{
		// Make sure to call to_expression multiple times to ensure
		// that these expressions are properly flushed to temporaries if needed.
		expr += op;
		expr += to_extract_component_expression(operand, i);

		if (i + 1 < type.vecsize)
			expr += ", ";
	}
	expr += ')';
	emit_op(result_type, result_id, expr, should_forward(operand));

	inherit_expression_dependencies(result_id, operand);
}

void CompilerGLSL::emit_unrolled_binary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                           const char *op, bool negate, SPIRType::BaseType expected_type)
{
	auto &type0 = expression_type(op0);
	auto &type1 = expression_type(op1);

	SPIRType target_type0 = type0;
	SPIRType target_type1 = type1;
	target_type0.basetype = expected_type;
	target_type1.basetype = expected_type;
	target_type0.vecsize = 1;
	target_type1.vecsize = 1;

	auto &type = get<SPIRType>(result_type);
	auto expr = type_to_glsl_constructor(type);
	expr += '(';
	for (uint32_t i = 0; i < type.vecsize; i++)
	{
		// Make sure to call to_expression multiple times to ensure
		// that these expressions are properly flushed to temporaries if needed.
		if (negate)
			expr += "!(";

		if (expected_type != SPIRType::Unknown && type0.basetype != expected_type)
			expr += bitcast_expression(target_type0, type0.basetype, to_extract_component_expression(op0, i));
		else
			expr += to_extract_component_expression(op0, i);

		expr += ' ';
		expr += op;
		expr += ' ';

		if (expected_type != SPIRType::Unknown && type1.basetype != expected_type)
			expr += bitcast_expression(target_type1, type1.basetype, to_extract_component_expression(op1, i));
		else
			expr += to_extract_component_expression(op1, i);

		if (negate)
			expr += ")";

		if (i + 1 < type.vecsize)
			expr += ", ";
	}
	expr += ')';
	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1));

	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
}

SPIRType CompilerGLSL::binary_op_bitcast_helper(string &cast_op0, string &cast_op1, SPIRType::BaseType &input_type,
                                                uint32_t op0, uint32_t op1, bool skip_cast_if_equal_type)
{
	auto &type0 = expression_type(op0);
	auto &type1 = expression_type(op1);

	// We have to bitcast if our inputs are of different type, or if our types are not equal to expected inputs.
	// For some functions like OpIEqual and INotEqual, we don't care if inputs are of different types than expected
	// since equality test is exactly the same.
	bool cast = (type0.basetype != type1.basetype) || (!skip_cast_if_equal_type && type0.basetype != input_type);

	// Create a fake type so we can bitcast to it.
	// We only deal with regular arithmetic types here like int, uints and so on.
	SPIRType expected_type;
	expected_type.basetype = input_type;
	expected_type.vecsize = type0.vecsize;
	expected_type.columns = type0.columns;
	expected_type.width = type0.width;

	if (cast)
	{
		cast_op0 = bitcast_glsl(expected_type, op0);
		cast_op1 = bitcast_glsl(expected_type, op1);
	}
	else
	{
		// If we don't cast, our actual input type is that of the first (or second) argument.
		cast_op0 = to_enclosed_unpacked_expression(op0);
		cast_op1 = to_enclosed_unpacked_expression(op1);
		input_type = type0.basetype;
	}

	return expected_type;
}

bool CompilerGLSL::emit_complex_bitcast(uint32_t result_type, uint32_t id, uint32_t op0)
{
	// Some bitcasts may require complex casting sequences, and are implemented here.
	// Otherwise a simply unary function will do with bitcast_glsl_op.

	auto &output_type = get<SPIRType>(result_type);
	auto &input_type = expression_type(op0);
	string expr;

	if (output_type.basetype == SPIRType::Half && input_type.basetype == SPIRType::Float && input_type.vecsize == 1)
		expr = join("unpackFloat2x16(floatBitsToUint(", to_unpacked_expression(op0), "))");
	else if (output_type.basetype == SPIRType::Float && input_type.basetype == SPIRType::Half &&
	         input_type.vecsize == 2)
		expr = join("uintBitsToFloat(packFloat2x16(", to_unpacked_expression(op0), "))");
	else
		return false;

	emit_op(result_type, id, expr, should_forward(op0));
	return true;
}

void CompilerGLSL::emit_binary_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                       const char *op, SPIRType::BaseType input_type,
                                       bool skip_cast_if_equal_type,
                                       bool implicit_integer_promotion)
{
	string cast_op0, cast_op1;
	auto expected_type = binary_op_bitcast_helper(cast_op0, cast_op1, input_type, op0, op1, skip_cast_if_equal_type);
	auto &out_type = get<SPIRType>(result_type);

	// We might have casted away from the result type, so bitcast again.
	// For example, arithmetic right shift with uint inputs.
	// Special case boolean outputs since relational opcodes output booleans instead of int/uint.
	auto bitop = join(cast_op0, " ", op, " ", cast_op1);
	string expr;

	if (implicit_integer_promotion)
	{
		// Simple value cast.
		expr = join(type_to_glsl(out_type), '(', bitop, ')');
	}
	else if (out_type.basetype != input_type && out_type.basetype != SPIRType::Boolean)
	{
		expected_type.basetype = input_type;
		expr = join(bitcast_glsl_op(out_type, expected_type), '(', bitop, ')');
	}
	else
	{
		expr = std::move(bitop);
	}

	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1));
	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
}

void CompilerGLSL::emit_unary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op)
{
	bool forward = should_forward(op0);
	emit_op(result_type, result_id, join(op, "(", to_unpacked_expression(op0), ")"), forward);
	inherit_expression_dependencies(result_id, op0);
}

void CompilerGLSL::emit_binary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                       const char *op)
{
	// Opaque types (e.g. OpTypeSampledImage) must always be forwarded in GLSL
	const auto &type = get_type(result_type);
	bool must_forward = type_is_opaque_value(type);
	bool forward = must_forward || (should_forward(op0) && should_forward(op1));
	emit_op(result_type, result_id, join(op, "(", to_unpacked_expression(op0), ", ", to_unpacked_expression(op1), ")"),
	        forward);
	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
}

void CompilerGLSL::emit_atomic_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                       const char *op)
{
	auto &type = get<SPIRType>(result_type);
	if (type_is_floating_point(type))
	{
		if (!options.vulkan_semantics)
			SPIRV_CROSS_THROW("Floating point atomics requires Vulkan semantics.");
		if (options.es)
			SPIRV_CROSS_THROW("Floating point atomics requires desktop GLSL.");
		require_extension_internal("GL_EXT_shader_atomic_float");
	}

	forced_temporaries.insert(result_id);
	emit_op(result_type, result_id,
	        join(op, "(", to_non_uniform_aware_expression(op0), ", ",
	             to_unpacked_expression(op1), ")"), false);
	flush_all_atomic_capable_variables();
}

void CompilerGLSL::emit_atomic_func_op(uint32_t result_type, uint32_t result_id,
                                       uint32_t op0, uint32_t op1, uint32_t op2,
                                       const char *op)
{
	forced_temporaries.insert(result_id);
	emit_op(result_type, result_id,
	        join(op, "(", to_non_uniform_aware_expression(op0), ", ",
	             to_unpacked_expression(op1), ", ", to_unpacked_expression(op2), ")"), false);
	flush_all_atomic_capable_variables();
}

void CompilerGLSL::emit_unary_func_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op,
                                           SPIRType::BaseType input_type, SPIRType::BaseType expected_result_type)
{
	auto &out_type = get<SPIRType>(result_type);
	auto &expr_type = expression_type(op0);
	auto expected_type = out_type;

	// Bit-widths might be different in unary cases because we use it for SConvert/UConvert and friends.
	expected_type.basetype = input_type;
	expected_type.width = expr_type.width;

	string cast_op;
	if (expr_type.basetype != input_type)
	{
		if (expr_type.basetype == SPIRType::Boolean)
			cast_op = join(type_to_glsl(expected_type), "(", to_unpacked_expression(op0), ")");
		else
			cast_op = bitcast_glsl(expected_type, op0);
	}
	else
		cast_op = to_unpacked_expression(op0);

	string expr;
	if (out_type.basetype != expected_result_type)
	{
		expected_type.basetype = expected_result_type;
		expected_type.width = out_type.width;
		if (out_type.basetype == SPIRType::Boolean)
			expr = type_to_glsl(out_type);
		else
			expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(op, "(", cast_op, ")");
		expr += ')';
	}
	else
	{
		expr += join(op, "(", cast_op, ")");
	}

	emit_op(result_type, result_id, expr, should_forward(op0));
	inherit_expression_dependencies(result_id, op0);
}

// Very special case. Handling bitfieldExtract requires us to deal with different bitcasts of different signs
// and different vector sizes all at once. Need a special purpose method here.
void CompilerGLSL::emit_trinary_func_op_bitextract(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                                   uint32_t op2, const char *op,
                                                   SPIRType::BaseType expected_result_type,
                                                   SPIRType::BaseType input_type0, SPIRType::BaseType input_type1,
                                                   SPIRType::BaseType input_type2)
{
	auto &out_type = get<SPIRType>(result_type);
	auto expected_type = out_type;
	expected_type.basetype = input_type0;

	string cast_op0 =
	    expression_type(op0).basetype != input_type0 ? bitcast_glsl(expected_type, op0) : to_unpacked_expression(op0);

	auto op1_expr = to_unpacked_expression(op1);
	auto op2_expr = to_unpacked_expression(op2);

	// Use value casts here instead. Input must be exactly int or uint, but SPIR-V might be 16-bit.
	expected_type.basetype = input_type1;
	expected_type.vecsize = 1;
	string cast_op1 = expression_type(op1).basetype != input_type1 ?
	                      join(type_to_glsl_constructor(expected_type), "(", op1_expr, ")") :
	                      op1_expr;

	expected_type.basetype = input_type2;
	expected_type.vecsize = 1;
	string cast_op2 = expression_type(op2).basetype != input_type2 ?
	                      join(type_to_glsl_constructor(expected_type), "(", op2_expr, ")") :
	                      op2_expr;

	string expr;
	if (out_type.basetype != expected_result_type)
	{
		expected_type.vecsize = out_type.vecsize;
		expected_type.basetype = expected_result_type;
		expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(op, "(", cast_op0, ", ", cast_op1, ", ", cast_op2, ")");
		expr += ')';
	}
	else
	{
		expr += join(op, "(", cast_op0, ", ", cast_op1, ", ", cast_op2, ")");
	}

	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1) && should_forward(op2));
	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
	inherit_expression_dependencies(result_id, op2);
}

void CompilerGLSL::emit_trinary_func_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                             uint32_t op2, const char *op, SPIRType::BaseType input_type)
{
	auto &out_type = get<SPIRType>(result_type);
	auto expected_type = out_type;
	expected_type.basetype = input_type;
	string cast_op0 =
	    expression_type(op0).basetype != input_type ? bitcast_glsl(expected_type, op0) : to_unpacked_expression(op0);
	string cast_op1 =
	    expression_type(op1).basetype != input_type ? bitcast_glsl(expected_type, op1) : to_unpacked_expression(op1);
	string cast_op2 =
	    expression_type(op2).basetype != input_type ? bitcast_glsl(expected_type, op2) : to_unpacked_expression(op2);

	string expr;
	if (out_type.basetype != input_type)
	{
		expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(op, "(", cast_op0, ", ", cast_op1, ", ", cast_op2, ")");
		expr += ')';
	}
	else
	{
		expr += join(op, "(", cast_op0, ", ", cast_op1, ", ", cast_op2, ")");
	}

	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1) && should_forward(op2));
	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
	inherit_expression_dependencies(result_id, op2);
}

void CompilerGLSL::emit_binary_func_op_cast_clustered(uint32_t result_type, uint32_t result_id, uint32_t op0,
                                                      uint32_t op1, const char *op, SPIRType::BaseType input_type)
{
	// Special purpose method for implementing clustered subgroup opcodes.
	// Main difference is that op1 does not participate in any casting, it needs to be a literal.
	auto &out_type = get<SPIRType>(result_type);
	auto expected_type = out_type;
	expected_type.basetype = input_type;
	string cast_op0 =
	    expression_type(op0).basetype != input_type ? bitcast_glsl(expected_type, op0) : to_unpacked_expression(op0);

	string expr;
	if (out_type.basetype != input_type)
	{
		expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(op, "(", cast_op0, ", ", to_expression(op1), ")");
		expr += ')';
	}
	else
	{
		expr += join(op, "(", cast_op0, ", ", to_expression(op1), ")");
	}

	emit_op(result_type, result_id, expr, should_forward(op0));
	inherit_expression_dependencies(result_id, op0);
}

void CompilerGLSL::emit_binary_func_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                            const char *op, SPIRType::BaseType input_type, bool skip_cast_if_equal_type)
{
	string cast_op0, cast_op1;
	auto expected_type = binary_op_bitcast_helper(cast_op0, cast_op1, input_type, op0, op1, skip_cast_if_equal_type);
	auto &out_type = get<SPIRType>(result_type);

	// Special case boolean outputs since relational opcodes output booleans instead of int/uint.
	string expr;
	if (out_type.basetype != input_type && out_type.basetype != SPIRType::Boolean)
	{
		expected_type.basetype = input_type;
		expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(op, "(", cast_op0, ", ", cast_op1, ")");
		expr += ')';
	}
	else
	{
		expr += join(op, "(", cast_op0, ", ", cast_op1, ")");
	}

	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1));
	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
}

void CompilerGLSL::emit_trinary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                        uint32_t op2, const char *op)
{
	bool forward = should_forward(op0) && should_forward(op1) && should_forward(op2);
	emit_op(result_type, result_id,
	        join(op, "(", to_unpacked_expression(op0), ", ", to_unpacked_expression(op1), ", ",
	             to_unpacked_expression(op2), ")"),
	        forward);

	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
	inherit_expression_dependencies(result_id, op2);
}

void CompilerGLSL::emit_quaternary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                           uint32_t op2, uint32_t op3, const char *op)
{
	bool forward = should_forward(op0) && should_forward(op1) && should_forward(op2) && should_forward(op3);
	emit_op(result_type, result_id,
	        join(op, "(", to_unpacked_expression(op0), ", ", to_unpacked_expression(op1), ", ",
	             to_unpacked_expression(op2), ", ", to_unpacked_expression(op3), ")"),
	        forward);

	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
	inherit_expression_dependencies(result_id, op2);
	inherit_expression_dependencies(result_id, op3);
}

void CompilerGLSL::emit_bitfield_insert_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                           uint32_t op2, uint32_t op3, const char *op,
                                           SPIRType::BaseType offset_count_type)
{
	// Only need to cast offset/count arguments. Types of base/insert must be same as result type,
	// and bitfieldInsert is sign invariant.
	bool forward = should_forward(op0) && should_forward(op1) && should_forward(op2) && should_forward(op3);

	auto op0_expr = to_unpacked_expression(op0);
	auto op1_expr = to_unpacked_expression(op1);
	auto op2_expr = to_unpacked_expression(op2);
	auto op3_expr = to_unpacked_expression(op3);

	SPIRType target_type;
	target_type.vecsize = 1;
	target_type.basetype = offset_count_type;

	if (expression_type(op2).basetype != offset_count_type)
	{
		// Value-cast here. Input might be 16-bit. GLSL requires int.
		op2_expr = join(type_to_glsl_constructor(target_type), "(", op2_expr, ")");
	}

	if (expression_type(op3).basetype != offset_count_type)
	{
		// Value-cast here. Input might be 16-bit. GLSL requires int.
		op3_expr = join(type_to_glsl_constructor(target_type), "(", op3_expr, ")");
	}

	emit_op(result_type, result_id, join(op, "(", op0_expr, ", ", op1_expr, ", ", op2_expr, ", ", op3_expr, ")"),
	        forward);

	inherit_expression_dependencies(result_id, op0);
	inherit_expression_dependencies(result_id, op1);
	inherit_expression_dependencies(result_id, op2);
	inherit_expression_dependencies(result_id, op3);
}

string CompilerGLSL::legacy_tex_op(const std::string &op, const SPIRType &imgtype, uint32_t tex)
{
	const char *type;
	switch (imgtype.image.dim)
	{
	case spv::Dim1D:
		// Force 2D path for ES.
		if (options.es)
			type = (imgtype.image.arrayed && !options.es) ? "2DArray" : "2D";
		else
			type = (imgtype.image.arrayed && !options.es) ? "1DArray" : "1D";
		break;
	case spv::Dim2D:
		type = (imgtype.image.arrayed && !options.es) ? "2DArray" : "2D";
		break;
	case spv::Dim3D:
		type = "3D";
		break;
	case spv::DimCube:
		type = "Cube";
		break;
	case spv::DimRect:
		type = "2DRect";
		break;
	case spv::DimBuffer:
		type = "Buffer";
		break;
	case spv::DimSubpassData:
		type = "2D";
		break;
	default:
		type = "";
		break;
	}

	// In legacy GLSL, an extension is required for textureLod in the fragment
	// shader or textureGrad anywhere.
	bool legacy_lod_ext = false;
	auto &execution = get_entry_point();
	if (op == "textureGrad" || op == "textureProjGrad" ||
	    ((op == "textureLod" || op == "textureProjLod") && execution.model != ExecutionModelVertex))
	{
		if (is_legacy_es())
		{
			legacy_lod_ext = true;
			require_extension_internal("GL_EXT_shader_texture_lod");
		}
		else if (is_legacy_desktop())
			require_extension_internal("GL_ARB_shader_texture_lod");
	}

	if (op == "textureLodOffset" || op == "textureProjLodOffset")
	{
		if (is_legacy_es())
			SPIRV_CROSS_THROW(join(op, " not allowed in legacy ES"));

		require_extension_internal("GL_EXT_gpu_shader4");
	}

	// GLES has very limited support for shadow samplers.
	// Basically shadow2D and shadow2DProj work through EXT_shadow_samplers,
	// everything else can just throw
	bool is_comparison = is_depth_image(imgtype, tex);
	if (is_comparison && is_legacy_es())
	{
		if (op == "texture" || op == "textureProj")
			require_extension_internal("GL_EXT_shadow_samplers");
		else
			SPIRV_CROSS_THROW(join(op, " not allowed on depth samplers in legacy ES"));

		if (imgtype.image.dim == spv::DimCube)
			return "shadowCubeNV";
	}

	if (op == "textureSize")
	{
		if (is_legacy_es())
			SPIRV_CROSS_THROW("textureSize not supported in legacy ES");
		if (is_comparison)
			SPIRV_CROSS_THROW("textureSize not supported on shadow sampler in legacy GLSL");
		require_extension_internal("GL_EXT_gpu_shader4");
	}

	if (op == "texelFetch" && is_legacy_es())
		SPIRV_CROSS_THROW("texelFetch not supported in legacy ES");

	bool is_es_and_depth = is_legacy_es() && is_comparison;
	std::string type_prefix = is_comparison ? "shadow" : "texture";

	if (op == "texture")
		return is_es_and_depth ? join(type_prefix, type, "EXT") : join(type_prefix, type);
	else if (op == "textureLod")
		return join(type_prefix, type, legacy_lod_ext ? "LodEXT" : "Lod");
	else if (op == "textureProj")
		return join(type_prefix, type, is_es_and_depth ? "ProjEXT" : "Proj");
	else if (op == "textureGrad")
		return join(type_prefix, type, is_legacy_es() ? "GradEXT" : is_legacy_desktop() ? "GradARB" : "Grad");
	else if (op == "textureProjLod")
		return join(type_prefix, type, legacy_lod_ext ? "ProjLodEXT" : "ProjLod");
	else if (op == "textureLodOffset")
		return join(type_prefix, type, "LodOffset");
	else if (op == "textureProjGrad")
		return join(type_prefix, type,
		            is_legacy_es() ? "ProjGradEXT" : is_legacy_desktop() ? "ProjGradARB" : "ProjGrad");
	else if (op == "textureProjLodOffset")
		return join(type_prefix, type, "ProjLodOffset");
	else if (op == "textureSize")
		return join("textureSize", type);
	else if (op == "texelFetch")
		return join("texelFetch", type);
	else
	{
		SPIRV_CROSS_THROW(join("Unsupported legacy texture op: ", op));
	}
}

bool CompilerGLSL::to_trivial_mix_op(const SPIRType &type, string &op, uint32_t left, uint32_t right, uint32_t lerp)
{
	auto *cleft = maybe_get<SPIRConstant>(left);
	auto *cright = maybe_get<SPIRConstant>(right);
	auto &lerptype = expression_type(lerp);

	// If our targets aren't constants, we cannot use construction.
	if (!cleft || !cright)
		return false;

	// If our targets are spec constants, we cannot use construction.
	if (cleft->specialization || cright->specialization)
		return false;

	auto &value_type = get<SPIRType>(cleft->constant_type);

	if (lerptype.basetype != SPIRType::Boolean)
		return false;
	if (value_type.basetype == SPIRType::Struct || is_array(value_type))
		return false;
	if (!backend.use_constructor_splatting && value_type.vecsize != lerptype.vecsize)
		return false;

	// Only valid way in SPIR-V 1.4 to use matrices in select is a scalar select.
	// matrix(scalar) constructor fills in diagnonals, so gets messy very quickly.
	// Just avoid this case.
	if (value_type.columns > 1)
		return false;

	// If our bool selects between 0 and 1, we can cast from bool instead, making our trivial constructor.
	bool ret = true;
	for (uint32_t row = 0; ret && row < value_type.vecsize; row++)
	{
		switch (type.basetype)
		{
		case SPIRType::Short:
		case SPIRType::UShort:
			ret = cleft->scalar_u16(0, row) == 0 && cright->scalar_u16(0, row) == 1;
			break;

		case SPIRType::Int:
		case SPIRType::UInt:
			ret = cleft->scalar(0, row) == 0 && cright->scalar(0, row) == 1;
			break;

		case SPIRType::Half:
			ret = cleft->scalar_f16(0, row) == 0.0f && cright->scalar_f16(0, row) == 1.0f;
			break;

		case SPIRType::Float:
			ret = cleft->scalar_f32(0, row) == 0.0f && cright->scalar_f32(0, row) == 1.0f;
			break;

		case SPIRType::Double:
			ret = cleft->scalar_f64(0, row) == 0.0 && cright->scalar_f64(0, row) == 1.0;
			break;

		case SPIRType::Int64:
		case SPIRType::UInt64:
			ret = cleft->scalar_u64(0, row) == 0 && cright->scalar_u64(0, row) == 1;
			break;

		default:
			ret = false;
			break;
		}
	}

	if (ret)
		op = type_to_glsl_constructor(type);
	return ret;
}

string CompilerGLSL::to_ternary_expression(const SPIRType &restype, uint32_t select, uint32_t true_value,
                                           uint32_t false_value)
{
	string expr;
	auto &lerptype = expression_type(select);

	if (lerptype.vecsize == 1)
		expr = join(to_enclosed_expression(select), " ? ", to_enclosed_pointer_expression(true_value), " : ",
		            to_enclosed_pointer_expression(false_value));
	else
	{
		auto swiz = [this](uint32_t expression, uint32_t i) { return to_extract_component_expression(expression, i); };

		expr = type_to_glsl_constructor(restype);
		expr += "(";
		for (uint32_t i = 0; i < restype.vecsize; i++)
		{
			expr += swiz(select, i);
			expr += " ? ";
			expr += swiz(true_value, i);
			expr += " : ";
			expr += swiz(false_value, i);
			if (i + 1 < restype.vecsize)
				expr += ", ";
		}
		expr += ")";
	}

	return expr;
}

void CompilerGLSL::emit_mix_op(uint32_t result_type, uint32_t id, uint32_t left, uint32_t right, uint32_t lerp)
{
	auto &lerptype = expression_type(lerp);
	auto &restype = get<SPIRType>(result_type);

	// If this results in a variable pointer, assume it may be written through.
	if (restype.pointer)
	{
		register_write(left);
		register_write(right);
	}

	string mix_op;
	bool has_boolean_mix = *backend.boolean_mix_function &&
	                       ((options.es && options.version >= 310) || (!options.es && options.version >= 450));
	bool trivial_mix = to_trivial_mix_op(restype, mix_op, left, right, lerp);

	// Cannot use boolean mix when the lerp argument is just one boolean,
	// fall back to regular trinary statements.
	if (lerptype.vecsize == 1)
		has_boolean_mix = false;

	// If we can reduce the mix to a simple cast, do so.
	// This helps for cases like int(bool), uint(bool) which is implemented with
	// OpSelect bool 1 0.
	if (trivial_mix)
	{
		emit_unary_func_op(result_type, id, lerp, mix_op.c_str());
	}
	else if (!has_boolean_mix && lerptype.basetype == SPIRType::Boolean)
	{
		// Boolean mix not supported on desktop without extension.
		// Was added in OpenGL 4.5 with ES 3.1 compat.
		//
		// Could use GL_EXT_shader_integer_mix on desktop at least,
		// but Apple doesn't support it. :(
		// Just implement it as ternary expressions.
		auto expr = to_ternary_expression(get<SPIRType>(result_type), lerp, right, left);
		emit_op(result_type, id, expr, should_forward(left) && should_forward(right) && should_forward(lerp));
		inherit_expression_dependencies(id, left);
		inherit_expression_dependencies(id, right);
		inherit_expression_dependencies(id, lerp);
	}
	else if (lerptype.basetype == SPIRType::Boolean)
		emit_trinary_func_op(result_type, id, left, right, lerp, backend.boolean_mix_function);
	else
		emit_trinary_func_op(result_type, id, left, right, lerp, "mix");
}

string CompilerGLSL::to_combined_image_sampler(VariableID image_id, VariableID samp_id)
{
	// Keep track of the array indices we have used to load the image.
	// We'll need to use the same array index into the combined image sampler array.
	auto image_expr = to_non_uniform_aware_expression(image_id);
	string array_expr;
	auto array_index = image_expr.find_first_of('[');
	if (array_index != string::npos)
		array_expr = image_expr.substr(array_index, string::npos);

	auto &args = current_function->arguments;

	// For GLSL and ESSL targets, we must enumerate all possible combinations for sampler2D(texture2D, sampler) and redirect
	// all possible combinations into new sampler2D uniforms.
	auto *image = maybe_get_backing_variable(image_id);
	auto *samp = maybe_get_backing_variable(samp_id);
	if (image)
		image_id = image->self;
	if (samp)
		samp_id = samp->self;

	auto image_itr = find_if(begin(args), end(args),
	                         [image_id](const SPIRFunction::Parameter &param) { return image_id == param.id; });

	auto sampler_itr = find_if(begin(args), end(args),
	                           [samp_id](const SPIRFunction::Parameter &param) { return samp_id == param.id; });

	if (image_itr != end(args) || sampler_itr != end(args))
	{
		// If any parameter originates from a parameter, we will find it in our argument list.
		bool global_image = image_itr == end(args);
		bool global_sampler = sampler_itr == end(args);
		VariableID iid = global_image ? image_id : VariableID(uint32_t(image_itr - begin(args)));
		VariableID sid = global_sampler ? samp_id : VariableID(uint32_t(sampler_itr - begin(args)));

		auto &combined = current_function->combined_parameters;
		auto itr = find_if(begin(combined), end(combined), [=](const SPIRFunction::CombinedImageSamplerParameter &p) {
			return p.global_image == global_image && p.global_sampler == global_sampler && p.image_id == iid &&
			       p.sampler_id == sid;
		});

		if (itr != end(combined))
			return to_expression(itr->id) + array_expr;
		else
		{
			SPIRV_CROSS_THROW("Cannot find mapping for combined sampler parameter, was "
			                  "build_combined_image_samplers() used "
			                  "before compile() was called?");
		}
	}
	else
	{
		// For global sampler2D, look directly at the global remapping table.
		auto &mapping = combined_image_samplers;
		auto itr = find_if(begin(mapping), end(mapping), [image_id, samp_id](const CombinedImageSampler &combined) {
			return combined.image_id == image_id && combined.sampler_id == samp_id;
		});

		if (itr != end(combined_image_samplers))
			return to_expression(itr->combined_id) + array_expr;
		else
		{
			SPIRV_CROSS_THROW("Cannot find mapping for combined sampler, was build_combined_image_samplers() used "
			                  "before compile() was called?");
		}
	}
}

bool CompilerGLSL::is_supported_subgroup_op_in_opengl(spv::Op op, const uint32_t *ops)
{
	switch (op)
	{
	case OpGroupNonUniformElect:
	case OpGroupNonUniformBallot:
	case OpGroupNonUniformBallotFindLSB:
	case OpGroupNonUniformBallotFindMSB:
	case OpGroupNonUniformBroadcast:
	case OpGroupNonUniformBroadcastFirst:
	case OpGroupNonUniformAll:
	case OpGroupNonUniformAny:
	case OpGroupNonUniformAllEqual:
	case OpControlBarrier:
	case OpMemoryBarrier:
	case OpGroupNonUniformBallotBitCount:
	case OpGroupNonUniformBallotBitExtract:
	case OpGroupNonUniformInverseBallot:
		return true;
	case OpGroupNonUniformIAdd:
	case OpGroupNonUniformFAdd:
	case OpGroupNonUniformIMul:
	case OpGroupNonUniformFMul:
	{
		const GroupOperation operation = static_cast<GroupOperation>(ops[3]);
		if (operation == GroupOperationReduce || operation == GroupOperationInclusiveScan ||
		    operation == GroupOperationExclusiveScan)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	default:
		return false;
	}
}

void CompilerGLSL::emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id)
{
	if (options.vulkan_semantics && combined_image_samplers.empty())
	{
		emit_binary_func_op(result_type, result_id, image_id, samp_id,
		                    type_to_glsl(get<SPIRType>(result_type), result_id).c_str());
	}
	else
	{
		// Make sure to suppress usage tracking. It is illegal to create temporaries of opaque types.
		emit_op(result_type, result_id, to_combined_image_sampler(image_id, samp_id), true, true);
	}

	// Make sure to suppress usage tracking and any expression invalidation.
	// It is illegal to create temporaries of opaque types.
	forwarded_temporaries.erase(result_id);
}

static inline bool image_opcode_is_sample_no_dref(Op op)
{
	switch (op)
	{
	case OpImageSampleExplicitLod:
	case OpImageSampleImplicitLod:
	case OpImageSampleProjExplicitLod:
	case OpImageSampleProjImplicitLod:
	case OpImageFetch:
	case OpImageRead:
	case OpImageSparseSampleExplicitLod:
	case OpImageSparseSampleImplicitLod:
	case OpImageSparseSampleProjExplicitLod:
	case OpImageSparseSampleProjImplicitLod:
	case OpImageSparseFetch:
	case OpImageSparseRead:
		return true;

	default:
		return false;
	}
}

void CompilerGLSL::emit_sparse_feedback_temporaries(uint32_t result_type_id, uint32_t id, uint32_t &feedback_id,
                                                    uint32_t &texel_id)
{
	// Need to allocate two temporaries.
	if (options.es)
		SPIRV_CROSS_THROW("Sparse texture feedback is not supported on ESSL.");
	require_extension_internal("GL_ARB_sparse_texture2");

	auto &temps = extra_sub_expressions[id];
	if (temps == 0)
		temps = ir.increase_bound_by(2);

	feedback_id = temps + 0;
	texel_id = temps + 1;

	auto &return_type = get<SPIRType>(result_type_id);
	if (return_type.basetype != SPIRType::Struct || return_type.member_types.size() != 2)
		SPIRV_CROSS_THROW("Invalid return type for sparse feedback.");
	emit_uninitialized_temporary(return_type.member_types[0], feedback_id);
	emit_uninitialized_temporary(return_type.member_types[1], texel_id);
}

uint32_t CompilerGLSL::get_sparse_feedback_texel_id(uint32_t id) const
{
	auto itr = extra_sub_expressions.find(id);
	if (itr == extra_sub_expressions.end())
		return 0;
	else
		return itr->second + 1;
}

void CompilerGLSL::emit_texture_op(const Instruction &i, bool sparse)
{
	auto *ops = stream(i);
	auto op = static_cast<Op>(i.op);

	SmallVector<uint32_t> inherited_expressions;

	uint32_t result_type_id = ops[0];
	uint32_t id = ops[1];
	auto &return_type = get<SPIRType>(result_type_id);

	uint32_t sparse_code_id = 0;
	uint32_t sparse_texel_id = 0;
	if (sparse)
		emit_sparse_feedback_temporaries(result_type_id, id, sparse_code_id, sparse_texel_id);

	bool forward = false;
	string expr = to_texture_op(i, sparse, &forward, inherited_expressions);

	if (sparse)
	{
		statement(to_expression(sparse_code_id), " = ", expr, ";");
		expr = join(type_to_glsl(return_type), "(", to_expression(sparse_code_id), ", ", to_expression(sparse_texel_id),
		            ")");
		forward = true;
		inherited_expressions.clear();
	}

	emit_op(result_type_id, id, expr, forward);
	for (auto &inherit : inherited_expressions)
		inherit_expression_dependencies(id, inherit);

	// Do not register sparse ops as control dependent as they are always lowered to a temporary.
	switch (op)
	{
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleImplicitLod:
	case OpImageSampleProjImplicitLod:
	case OpImageSampleProjDrefImplicitLod:
		register_control_dependent_expression(id);
		break;

	default:
		break;
	}
}

std::string CompilerGLSL::to_texture_op(const Instruction &i, bool sparse, bool *forward,
                                        SmallVector<uint32_t> &inherited_expressions)
{
	auto *ops = stream(i);
	auto op = static_cast<Op>(i.op);
	uint32_t length = i.length;

	uint32_t result_type_id = ops[0];
	VariableID img = ops[2];
	uint32_t coord = ops[3];
	uint32_t dref = 0;
	uint32_t comp = 0;
	bool gather = false;
	bool proj = false;
	bool fetch = false;
	bool nonuniform_expression = false;
	const uint32_t *opt = nullptr;

	auto &result_type = get<SPIRType>(result_type_id);

	inherited_expressions.push_back(coord);
	if (has_decoration(img, DecorationNonUniform) && !maybe_get_backing_variable(img))
		nonuniform_expression = true;

	switch (op)
	{
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleDrefExplicitLod:
	case OpImageSparseSampleDrefImplicitLod:
	case OpImageSparseSampleDrefExplicitLod:
		dref = ops[4];
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageSampleProjDrefImplicitLod:
	case OpImageSampleProjDrefExplicitLod:
	case OpImageSparseSampleProjDrefImplicitLod:
	case OpImageSparseSampleProjDrefExplicitLod:
		dref = ops[4];
		opt = &ops[5];
		length -= 5;
		proj = true;
		break;

	case OpImageDrefGather:
	case OpImageSparseDrefGather:
		dref = ops[4];
		opt = &ops[5];
		length -= 5;
		gather = true;
		if (options.es && options.version < 310)
			SPIRV_CROSS_THROW("textureGather requires ESSL 310.");
		else if (!options.es && options.version < 400)
			SPIRV_CROSS_THROW("textureGather with depth compare requires GLSL 400.");
		break;

	case OpImageGather:
	case OpImageSparseGather:
		comp = ops[4];
		opt = &ops[5];
		length -= 5;
		gather = true;
		if (options.es && options.version < 310)
			SPIRV_CROSS_THROW("textureGather requires ESSL 310.");
		else if (!options.es && options.version < 400)
		{
			if (!expression_is_constant_null(comp))
				SPIRV_CROSS_THROW("textureGather with component requires GLSL 400.");
			require_extension_internal("GL_ARB_texture_gather");
		}
		break;

	case OpImageFetch:
	case OpImageSparseFetch:
	case OpImageRead: // Reads == fetches in Metal (other langs will not get here)
		opt = &ops[4];
		length -= 4;
		fetch = true;
		break;

	case OpImageSampleProjImplicitLod:
	case OpImageSampleProjExplicitLod:
	case OpImageSparseSampleProjImplicitLod:
	case OpImageSparseSampleProjExplicitLod:
		opt = &ops[4];
		length -= 4;
		proj = true;
		break;

	default:
		opt = &ops[4];
		length -= 4;
		break;
	}

	// Bypass pointers because we need the real image struct
	auto &type = expression_type(img);
	auto &imgtype = get<SPIRType>(type.self);

	uint32_t coord_components = 0;
	switch (imgtype.image.dim)
	{
	case spv::Dim1D:
		coord_components = 1;
		break;
	case spv::Dim2D:
		coord_components = 2;
		break;
	case spv::Dim3D:
		coord_components = 3;
		break;
	case spv::DimCube:
		coord_components = 3;
		break;
	case spv::DimBuffer:
		coord_components = 1;
		break;
	default:
		coord_components = 2;
		break;
	}

	if (dref)
		inherited_expressions.push_back(dref);

	if (proj)
		coord_components++;
	if (imgtype.image.arrayed)
		coord_components++;

	uint32_t bias = 0;
	uint32_t lod = 0;
	uint32_t grad_x = 0;
	uint32_t grad_y = 0;
	uint32_t coffset = 0;
	uint32_t offset = 0;
	uint32_t coffsets = 0;
	uint32_t sample = 0;
	uint32_t minlod = 0;
	uint32_t flags = 0;

	if (length)
	{
		flags = *opt++;
		length--;
	}

	auto test = [&](uint32_t &v, uint32_t flag) {
		if (length && (flags & flag))
		{
			v = *opt++;
			inherited_expressions.push_back(v);
			length--;
		}
	};

	test(bias, ImageOperandsBiasMask);
	test(lod, ImageOperandsLodMask);
	test(grad_x, ImageOperandsGradMask);
	test(grad_y, ImageOperandsGradMask);
	test(coffset, ImageOperandsConstOffsetMask);
	test(offset, ImageOperandsOffsetMask);
	test(coffsets, ImageOperandsConstOffsetsMask);
	test(sample, ImageOperandsSampleMask);
	test(minlod, ImageOperandsMinLodMask);

	TextureFunctionBaseArguments base_arg                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        