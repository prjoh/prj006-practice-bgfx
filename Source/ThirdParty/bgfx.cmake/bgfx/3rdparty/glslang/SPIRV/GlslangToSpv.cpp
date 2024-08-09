//
// Copyright (C) 2014-2016 LunarG, Inc.
// Copyright (C) 2015-2020 Google, Inc.
// Copyright (C) 2017 ARM Limited.
// Modifications Copyright (C) 2020 Advanced Micro Devices, Inc. All rights reserved.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

//
// Visit the nodes in the glslang intermediate tree representation to
// translate them to SPIR-V.
//

#include "spirv.hpp"
#include "GlslangToSpv.h"
#include "SpvBuilder.h"
namespace spv {
    #include "GLSL.std.450.h"
    #include "GLSL.ext.KHR.h"
    #include "GLSL.ext.EXT.h"
    #include "GLSL.ext.AMD.h"
    #include "GLSL.ext.NV.h"
    #include "GLSL.ext.ARM.h"
    #include "NonSemanticDebugPrintf.h"
}

// Glslang includes
#include "../glslang/MachineIndependent/localintermediate.h"
#include "../glslang/MachineIndependent/SymbolTable.h"
#include "../glslang/Include/Common.h"

// Build-time generated includes
#include "glslang/build_info.h"

#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace {

namespace {
class SpecConstantOpModeGuard {
public:
    SpecConstantOpModeGuard(spv::Builder* builder)
        : builder_(builder) {
        previous_flag_ = builder->isInSpecConstCodeGenMode();
    }
    ~SpecConstantOpModeGuard() {
        previous_flag_ ? builder_->setToSpecConstCodeGenMode()
                       : builder_->setToNormalCodeGenMode();
    }
    void turnOnSpecConstantOpMode() {
        builder_->setToSpecConstCodeGenMode();
    }

private:
    spv::Builder* builder_;
    bool previous_flag_;
};

struct OpDecorations {
    public:
        OpDecorations(spv::Decoration precision, spv::Decoration noContraction, spv::Decoration nonUniform) :
            precision(precision)
            ,
            noContraction(noContraction),
            nonUniform(nonUniform)
        { }

    spv::Decoration precision;

        void addNoContraction(spv::Builder& builder, spv::Id t) { builder.addDecoration(t, noContraction); }
        void addNonUniform(spv::Builder& builder, spv::Id t)  { builder.addDecoration(t, nonUniform); }
    protected:
        spv::Decoration noContraction;
        spv::Decoration nonUniform;
};

} // namespace

//
// The main holder of information for translating glslang to SPIR-V.
//
// Derives from the AST walking base class.
//
class TGlslangToSpvTraverser : public glslang::TIntermTraverser {
public:
    TGlslangToSpvTraverser(unsigned int spvVersion, const glslang::TIntermediate*, spv::SpvBuildLogger* logger,
        glslang::SpvOptions& options);
    virtual ~TGlslangToSpvTraverser() { }

    bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate*);
    bool visitBinary(glslang::TVisit, glslang::TIntermBinary*);
    void visitConstantUnion(glslang::TIntermConstantUnion*);
    bool visitSelection(glslang::TVisit, glslang::TIntermSelection*);
    bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch*);
    void visitSymbol(glslang::TIntermSymbol* symbol);
    bool visitUnary(glslang::TVisit, glslang::TIntermUnary*);
    bool visitLoop(glslang::TVisit, glslang::TIntermLoop*);
    bool visitBranch(glslang::TVisit visit, glslang::TIntermBranch*);

    void finishSpv();
    void dumpSpv(std::vector<unsigned int>& out);

protected:
    TGlslangToSpvTraverser(TGlslangToSpvTraverser&);
    TGlslangToSpvTraverser& operator=(TGlslangToSpvTraverser&);

    spv::Decoration TranslateInterpolationDecoration(const glslang::TQualifier& qualifier);
    spv::Decoration TranslateAuxiliaryStorageDecoration(const glslang::TQualifier& qualifier);
    spv::Decoration TranslateNonUniformDecoration(const glslang::TQualifier& qualifier);
    spv::Decoration TranslateNonUniformDecoration(const spv::Builder::AccessChain::CoherentFlags& coherentFlags);
    spv::Builder::AccessChain::CoherentFlags TranslateCoherent(const glslang::TType& type);
    spv::MemoryAccessMask TranslateMemoryAccess(const spv::Builder::AccessChain::CoherentFlags &coherentFlags);
    spv::ImageOperandsMask TranslateImageOperands(const spv::Builder::AccessChain::CoherentFlags &coherentFlags);
    spv::Scope TranslateMemoryScope(const spv::Builder::AccessChain::CoherentFlags &coherentFlags);
    spv::BuiltIn TranslateBuiltInDecoration(glslang::TBuiltInVariable, bool memberDeclaration);
    spv::ImageFormat TranslateImageFormat(const glslang::TType& type);
    spv::SelectionControlMask TranslateSelectionControl(const glslang::TIntermSelection&) const;
    spv::SelectionControlMask TranslateSwitchControl(const glslang::TIntermSwitch&) const;
    spv::LoopControlMask TranslateLoopControl(const glslang::TIntermLoop&, std::vector<unsigned int>& operands) const;
    spv::StorageClass TranslateStorageClass(const glslang::TType&);
    void TranslateLiterals(const glslang::TVector<const glslang::TIntermConstantUnion*>&, std::vector<unsigned>&) const;
    void addIndirectionIndexCapabilities(const glslang::TType& baseType, const glslang::TType& indexType);
    spv::Id createSpvVariable(const glslang::TIntermSymbol*, spv::Id forcedType);
    spv::Id getSampledType(const glslang::TSampler&);
    spv::Id getInvertedSwizzleType(const glslang::TIntermTyped&);
    spv::Id createInvertedSwizzle(spv::Decoration precision, const glslang::TIntermTyped&, spv::Id parentResult);
    void convertSwizzle(const glslang::TIntermAggregate&, std::vector<unsigned>& swizzle);
    spv::Id convertGlslangToSpvType(const glslang::TType& type, bool forwardReferenceOnly = false);
    spv::Id convertGlslangToSpvType(const glslang::TType& type, glslang::TLayoutPacking, const glslang::TQualifier&,
        bool lastBufferBlockMember, bool forwardReferenceOnly = false);
    bool filterMember(const glslang::TType& member);
    spv::Id convertGlslangStructToSpvType(const glslang::TType&, const glslang::TTypeList* glslangStruct,
                                          glslang::TLayoutPacking, const glslang::TQualifier&);
    void decorateStructType(const glslang::TType&, const glslang::TTypeList* glslangStruct, glslang::TLayoutPacking,
                            const glslang::TQualifier&, spv::Id, const std::vector<spv::Id>& spvMembers);
    spv::Id makeArraySizeId(const glslang::TArraySizes&, int dim, bool allowZero = false);
    spv::Id accessChainLoad(const glslang::TType& type);
    void    accessChainStore(const glslang::TType& type, spv::Id rvalue);
    void multiTypeStore(const glslang::TType&, spv::Id rValue);
    spv::Id convertLoadedBoolInUniformToUint(const glslang::TType& type, spv::Id nominalTypeId, spv::Id loadedId);
    glslang::TLayoutPacking getExplicitLayout(const glslang::TType& type) const;
    int getArrayStride(const glslang::TType& arrayType, glslang::TLayoutPacking, glslang::TLayoutMatrix);
    int getMatrixStride(const glslang::TType& matrixType, glslang::TLayoutPacking, glslang::TLayoutMatrix);
    void updateMemberOffset(const glslang::TType& structType, const glslang::TType& memberType, int& currentOffset,
                            int& nextOffset, glslang::TLayoutPacking, glslang::TLayoutMatrix);
    void declareUseOfStructMember(const glslang::TTypeList& members, int glslangMember);

    bool isShaderEntryPoint(const glslang::TIntermAggregate* node);
    bool writableParam(glslang::TStorageQualifier) const;
    bool originalParam(glslang::TStorageQualifier, const glslang::TType&, bool implicitThisParam);
    void makeFunctions(const glslang::TIntermSequence&);
    void makeGlobalInitializers(const glslang::TIntermSequence&);
    void collectRayTracingLinkerObjects();
    void visitFunctions(const glslang::TIntermSequence&);
    void handleFunctionEntry(const glslang::TIntermAggregate* node);
    void translateArguments(const glslang::TIntermAggregate& node, std::vector<spv::Id>& arguments,
        spv::Builder::AccessChain::CoherentFlags &lvalueCoherentFlags);
    void translateArguments(glslang::TIntermUnary& node, std::vector<spv::Id>& arguments);
    spv::Id createImageTextureFunctionCall(glslang::TIntermOperator* node);
    spv::Id handleUserFunctionCall(const glslang::TIntermAggregate*);

    spv::Id createBinaryOperation(glslang::TOperator op, OpDecorations&, spv::Id typeId, spv::Id left, spv::Id right,
                                  glslang::TBasicType typeProxy, bool reduceComparison = true);
    spv::Id createBinaryMatrixOperation(spv::Op, OpDecorations&, spv::Id typeId, spv::Id left, spv::Id right);
    spv::Id createUnaryOperation(glslang::TOperator op, OpDecorations&, spv::Id typeId, spv::Id operand,
                                 glslang::TBasicType typeProxy,
                                 const spv::Builder::AccessChain::CoherentFlags &lvalueCoherentFlags);
    spv::Id createUnaryMatrixOperation(spv::Op op, OpDecorations&, spv::Id typeId, spv::Id operand,
                                       glslang::TBasicType typeProxy);
    spv::Id createConversion(glslang::TOperator op, OpDecorations&, spv::Id destTypeId, spv::Id operand,
                             glslang::TBasicType typeProxy);
    spv::Id createIntWidthConversion(glslang::TOperator op, spv::Id operand, int vectorSize, spv::Id destType);
    spv::Id makeSmearedConstant(spv::Id constant, int vectorSize);
    spv::Id createAtomicOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId,
        std::vector<spv::Id>& operands, glslang::TBasicType typeProxy,
        const spv::Builder::AccessChain::CoherentFlags &lvalueCoherentFlags);
    spv::Id createInvocationsOperation(glslang::TOperator op, spv::Id typeId, std::vector<spv::Id>& operands,
        glslang::TBasicType typeProxy);
    spv::Id CreateInvocationsVectorOperation(spv::Op op, spv::GroupOperation groupOperation,
        spv::Id typeId, std::vector<spv::Id>& operands);
    spv::Id createSubgroupOperation(glslang::TOperator op, spv::Id typeId, std::vector<spv::Id>& operands,
        glslang::TBasicType typeProxy);
    spv::Id createMiscOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId,
        std::vector<spv::Id>& operands, glslang::TBasicType typeProxy);
    spv::Id createNoArgOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId);
    spv::Id getSymbolId(const glslang::TIntermSymbol* node);
    void addMeshNVDecoration(spv::Id id, int member, const glslang::TQualifier & qualifier);
    spv::Id createSpvConstant(const glslang::TIntermTyped&);
    spv::Id createSpvConstantFromConstUnionArray(const glslang::TType& type, const glslang::TConstUnionArray&,
        int& nextConst, bool specConstant);
    bool isTrivialLeaf(const glslang::TIntermTyped* node);
    bool isTrivial(const glslang::TIntermTyped* node);
    spv::Id createShortCircuit(glslang::TOperator, glslang::TIntermTyped& left, glslang::TIntermTyped& right);
    spv::Id getExtBuiltins(const char* name);
    std::pair<spv::Id, spv::Id> getForcedType(glslang::TBuiltInVariable builtIn, const glslang::TType&);
    spv::Id translateForcedType(spv::Id object);
    spv::Id createCompositeConstruct(spv::Id typeId, std::vector<spv::Id> constituents);

    glslang::SpvOptions& options;
    spv::Function* shaderEntry;
    spv::Function* currentFunction;
    spv::Instruction* entryPoint;
    int sequenceDepth;

    spv::SpvBuildLogger* logger;

    // There is a 1:1 mapping between a spv builder and a module; this is thread safe
    spv::Builder builder;
    bool inEntryPoint;
    bool entryPointTerminated;
    bool linkageOnly;                  // true when visiting the set of objects in the AST present only for
                                       // establishing interface, whether or not they were statically used
    std::set<spv::Id> iOSet;           // all input/output variables from either static use or declaration of interface
    const glslang::TIntermediate* glslangIntermediate;
    bool nanMinMaxClamp;               // true if use NMin/NMax/NClamp instead of FMin/FMax/FClamp
    spv::Id stdBuiltins;
    spv::Id nonSemanticDebugPrintf;
    std::unordered_map<std::string, spv::Id> extBuiltinMap;

    std::unordered_map<long long, spv::Id> symbolValues;
    std::unordered_map<uint32_t, spv::Id> builtInVariableIds;
    std::unordered_set<long long> rValueParameters;  // set of formal function parameters passed as rValues,
                                               // rather than a pointer
    std::unordered_map<std::string, spv::Function*> functionMap;
    std::unordered_map<const glslang::TTypeList*, spv::Id> structMap[glslang::ElpCount][glslang::ElmCount];
    // for mapping glslang block indices to spv indices (e.g., due to hidden members):
    std::unordered_map<long long, std::vector<int>> memberRemapper;
    // for mapping glslang symbol struct to symbol Id
    std::unordered_map<const glslang::TTypeList*, long long> glslangTypeToIdMap;
    std::stack<bool> breakForLoop;  // false means break for switch
    std::unordered_map<std::string, const glslang::TIntermSymbol*> counterOriginator;
    // Map pointee types for EbtReference to their forward pointers
    std::map<const glslang::TType *, spv::Id> forwardPointers;
    // Type forcing, for when SPIR-V wants a different type than the AST,
    // requiring local translation to and from SPIR-V type on every access.
    // Maps <builtin-variable-id -> AST-required-type-id>
    std::unordered_map<spv::Id, spv::Id> forceType;
    // Used by Task shader while generating opearnds for OpEmitMeshTasksEXT
    spv::Id taskPayloadID;
    // Used later for generating OpTraceKHR/OpExecuteCallableKHR/OpHitObjectRecordHit*/OpHitObjectGetShaderBindingTableData
    std::unordered_map<unsigned int, glslang::TIntermSymbol *> locationToSymbol[4];
};

//
// Helper functions for translating glslang representations to SPIR-V enumerants.
//

// Translate glslang profile to SPIR-V source language.
spv::SourceLanguage TranslateSourceLanguage(glslang::EShSource source, EProfile profile)
{
    switch (source) {
    case glslang::EShSourceGlsl:
        switch (profile) {
        case ENoProfile:
        case ECoreProfile:
        case ECompatibilityProfile:
            return spv::SourceLanguageGLSL;
        case EEsProfile:
            return spv::SourceLanguageESSL;
        default:
            return spv::SourceLanguageUnknown;
        }
    case glslang::EShSourceHlsl:
        return spv::SourceLanguageHLSL;
    default:
        return spv::SourceLanguageUnknown;
    }
}

// Translate glslang language (stage) to SPIR-V execution model.
spv::ExecutionModel TranslateExecutionModel(EShLanguage stage, bool isMeshShaderEXT = false)
{
    switch (stage) {
    case EShLangVertex:           return spv::ExecutionModelVertex;
    case EShLangFragment:         return spv::ExecutionModelFragment;
    case EShLangCompute:          return spv::ExecutionModelGLCompute;
    case EShLangTessControl:      return spv::ExecutionModelTessellationControl;
    case EShLangTessEvaluation:   return spv::ExecutionModelTessellationEvaluation;
    case EShLangGeometry:         return spv::ExecutionModelGeometry;
    case EShLangRayGen:           return spv::ExecutionModelRayGenerationKHR;
    case EShLangIntersect:        return spv::ExecutionModelIntersectionKHR;
    case EShLangAnyHit:           return spv::ExecutionModelAnyHitKHR;
    case EShLangClosestHit:       return spv::ExecutionModelClosestHitKHR;
    case EShLangMiss:             return spv::ExecutionModelMissKHR;
    case EShLangCallable:         return spv::ExecutionModelCallableKHR;
    case EShLangTask:             return (isMeshShaderEXT)? spv::ExecutionModelTaskEXT : spv::ExecutionModelTaskNV;
    case EShLangMesh:             return (isMeshShaderEXT)? spv::ExecutionModelMeshEXT: spv::ExecutionModelMeshNV;
    default:
        assert(0);
        return spv::ExecutionModelFragment;
    }
}

// Translate glslang sampler type to SPIR-V dimensionality.
spv::Dim TranslateDimensionality(const glslang::TSampler& sampler)
{
    switch (sampler.dim) {
    case glslang::Esd1D:      return spv::Dim1D;
    case glslang::Esd2D:      return spv::Dim2D;
    case glslang::Esd3D:      return spv::Dim3D;
    case glslang::EsdCube:    return spv::DimCube;
    case glslang::EsdRect:    return spv::DimRect;
    case glslang::EsdBuffer:  return spv::DimBuffer;
    case glslang::EsdSubpass: return spv::DimSubpassData;
    case glslang::EsdAttachmentEXT: return spv::DimTileImageDataEXT;
    default:
        assert(0);
        return spv::Dim2D;
    }
}

// Translate glslang precision to SPIR-V precision decorations.
spv::Decoration TranslatePrecisionDecoration(glslang::TPrecisionQualifier glslangPrecision)
{
    switch (glslangPrecision) {
    case glslang::EpqLow:    return spv::DecorationRelaxedPrecision;
    case glslang::EpqMedium: return spv::DecorationRelaxedPrecision;
    default:
        return spv::NoPrecision;
    }
}

// Translate glslang type to SPIR-V precision decorations.
spv::Decoration TranslatePrecisionDecoration(const glslang::TType& type)
{
    return TranslatePrecisionDecoration(type.getQualifier().precision);
}

// Translate glslang type to SPIR-V block decorations.
spv::Decoration TranslateBlockDecoration(const glslang::TStorageQualifier storage, bool useStorageBuffer)
{
    switch (storage) {
    case glslang::EvqUniform:      return spv::DecorationBlock;
    case glslang::EvqBuffer:       return useStorageBuffer ? spv::DecorationBlock : spv::DecorationBufferBlock;
    case glslang::EvqVaryingIn:    return spv::DecorationBlock;
    case glslang::EvqVaryingOut:   return spv::DecorationBlock;
    case glslang::EvqShared:       return spv::DecorationBlock;
    case glslang::EvqPayload:      return spv::DecorationBlock;
    case glslang::EvqPayloadIn:    return spv::DecorationBlock;
    case glslang::EvqHitAttr:      return spv::DecorationBlock;
    case glslang::EvqCallableData:   return spv::DecorationBlock;
    case glslang::EvqCallableDataIn: return spv::DecorationBlock;
    case glslang::EvqHitObjectAttrNV: return spv::DecorationBlock;
    default:
        assert(0);
        break;
    }

    return spv::DecorationMax;
}

// Translate glslang type to SPIR-V memory decorations.
void TranslateMemoryDecoration(const glslang::TQualifier& qualifier, std::vector<spv::Decoration>& memory,
    bool useVulkanMemoryModel)
{
    if (!useVulkanMemoryModel) {
        if (qualifier.isCoherent())
            memory.push_back(spv::DecorationCoherent);
        if (qualifier.isVolatile()) {
            memory.push_back(spv::DecorationVolatile);
            memory.push_back(spv::DecorationCoherent);
        }
    }
    if (qualifier.isRestrict())
        memory.push_back(spv::DecorationRestrict);
    if (qualifier.isReadOnly())
        memory.push_back(spv::DecorationNonWritable);
    if (qualifier.isWriteOnly())
       memory.push_back(spv::DecorationNonReadable);
}

// Translate glslang type to SPIR-V layout decorations.
spv::Decoration TranslateLayoutDecoration(const glslang::TType& type, glslang::TLayoutMatrix matrixLayout)
{
    if (type.isMatrix()) {
        switch (matrixLayout) {
        case glslang::ElmRowMajor:
            return spv::DecorationRowMajor;
        case glslang::ElmColumnMajor:
            return spv::DecorationColMajor;
        default:
            // opaque layouts don't need a majorness
            return spv::DecorationMax;
        }
    } else {
        switch (type.getBasicType()) {
        default:
            return spv::DecorationMax;
            break;
        case glslang::EbtBlock:
            switch (type.getQualifier().storage) {
            case glslang::EvqShared:
            case glslang::EvqUniform:
            case glslang::EvqBuffer:
                switch (type.getQualifier().layoutPacking) {
                case glslang::ElpShared:  return spv::DecorationGLSLShared;
                case glslang::ElpPacked:  return spv::DecorationGLSLPacked;
                default:
                    return spv::DecorationMax;
                }
            case glslang::EvqVaryingIn:
            case glslang::EvqVaryingOut:
                if (type.getQualifier().isTaskMemory()) {
                    switch (type.getQualifier().layoutPacking) {
                    case glslang::ElpShared:  return spv::DecorationGLSLShared;
                    case glslang::ElpPacked:  return spv::DecorationGLSLPacked;
                    default: break;
                    }
                } else {
                    assert(type.getQualifier().layoutPacking == glslang::ElpNone);
                }
                return spv::DecorationMax;
            case glslang::EvqPayload:
            case glslang::EvqPayloadIn:
            case glslang::EvqHitAttr:
            case glslang::EvqCallableData:
            case glslang::EvqCallableDataIn:
            case glslang::EvqHitObjectAttrNV:
                return spv::DecorationMax;
            default:
                assert(0);
                return spv::DecorationMax;
            }
        }
    }
}

// Translate glslang type to SPIR-V interpolation decorations.
// Returns spv::DecorationMax when no decoration
// should be applied.
spv::Decoration TGlslangToSpvTraverser::TranslateInterpolationDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.smooth)
        // Smooth decoration doesn't exist in SPIR-V 1.0
        return spv::DecorationMax;
    else if (qualifier.isNonPerspective())
        return spv::DecorationNoPerspective;
    else if (qualifier.flat)
        return spv::DecorationFlat;
    else if (qualifier.isExplicitInterpolation()) {
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::DecorationExplicitInterpAMD;
    }
    else
        return spv::DecorationMax;
}

// Translate glslang type to SPIR-V auxiliary storage decorations.
// Returns spv::DecorationMax when no decoration
// should be applied.
spv::Decoration TGlslangToSpvTraverser::TranslateAuxiliaryStorageDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.centroid)
        return spv::DecorationCentroid;
    else if (qualifier.patch)
        return spv::DecorationPatch;
    else if (qualifier.sample) {
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::DecorationSample;
    }

    return spv::DecorationMax;
}

// If glslang type is invariant, return SPIR-V invariant decoration.
spv::Decoration TranslateInvariantDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.invariant)
        return spv::DecorationInvariant;
    else
        return spv::DecorationMax;
}

// If glslang type is noContraction, return SPIR-V NoContraction decoration.
spv::Decoration TranslateNoContractionDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.isNoContraction())
        return spv::DecorationNoContraction;
    else
        return spv::DecorationMax;
}

// If glslang type is nonUniform, return SPIR-V NonUniform decoration.
spv::Decoration TGlslangToSpvTraverser::TranslateNonUniformDecoration(const glslang::TQualifier& qualifier)
{
    if (qualifier.isNonUniform()) {
        builder.addIncorporatedExtension("SPV_EXT_descriptor_indexing", spv::Spv_1_5);
        builder.addCapability(spv::CapabilityShaderNonUniformEXT);
        return spv::DecorationNonUniformEXT;
    } else
        return spv::DecorationMax;
}

// If lvalue flags contains nonUniform, return SPIR-V NonUniform decoration.
spv::Decoration TGlslangToSpvTraverser::TranslateNonUniformDecoration(
    const spv::Builder::AccessChain::CoherentFlags& coherentFlags)
{
    if (coherentFlags.isNonUniform()) {
        builder.addIncorporatedExtension("SPV_EXT_descriptor_indexing", spv::Spv_1_5);
        builder.addCapability(spv::CapabilityShaderNonUniformEXT);
        return spv::DecorationNonUniformEXT;
    } else
        return spv::DecorationMax;
}

spv::MemoryAccessMask TGlslangToSpvTraverser::TranslateMemoryAccess(
    const spv::Builder::AccessChain::CoherentFlags &coherentFlags)
{
    spv::MemoryAccessMask mask = spv::MemoryAccessMaskNone;

    if (!glslangIntermediate->usingVulkanMemoryModel() || coherentFlags.isImage)
        return mask;

    if (coherentFlags.isVolatile() || coherentFlags.anyCoherent()) {
        mask = mask | spv::MemoryAccessMakePointerAvailableKHRMask |
                      spv::MemoryAccessMakePointerVisibleKHRMask;
    }

    if (coherentFlags.nonprivate) {
        mask = mask | spv::MemoryAccessNonPrivatePointerKHRMask;
    }
    if (coherentFlags.volatil) {
        mask = mask | spv::MemoryAccessVolatileMask;
    }
    if (mask != spv::MemoryAccessMaskNone) {
        builder.addCapability(spv::CapabilityVulkanMemoryModelKHR);
    }

    return mask;
}

spv::ImageOperandsMask TGlslangToSpvTraverser::TranslateImageOperands(
    const spv::Builder::AccessChain::CoherentFlags &coherentFlags)
{
    spv::ImageOperandsMask mask = spv::ImageOperandsMaskNone;

    if (!glslangIntermediate->usingVulkanMemoryModel())
        return mask;

    if (coherentFlags.volatil ||
        coherentFlags.anyCoherent()) {
        mask = mask | spv::ImageOperandsMakeTexelAvailableKHRMask |
                      spv::ImageOperandsMakeTexelVisibleKHRMask;
    }
    if (coherentFlags.nonprivate) {
        mask = mask | spv::ImageOperandsNonPrivateTexelKHRMask;
    }
    if (coherentFlags.volatil) {
        mask = mask | spv::ImageOperandsVolatileTexelKHRMask;
    }
    if (mask != spv::ImageOperandsMaskNone) {
        builder.addCapability(spv::CapabilityVulkanMemoryModelKHR);
    }

    return mask;
}

spv::Builder::AccessChain::CoherentFlags TGlslangToSpvTraverser::TranslateCoherent(const glslang::TType& type)
{
    spv::Builder::AccessChain::CoherentFlags flags = {};
    flags.coherent = type.getQualifier().coherent;
    flags.devicecoherent = type.getQualifier().devicecoherent;
    flags.queuefamilycoherent = type.getQualifier().queuefamilycoherent;
    // shared variables are implicitly workgroupcoherent in GLSL.
    flags.workgroupcoherent = type.getQualifier().workgroupcoherent ||
                              type.getQualifier().storage == glslang::EvqShared;
    flags.subgroupcoherent = type.getQualifier().subgroupcoherent;
    flags.shadercallcoherent = type.getQualifier().shadercallcoherent;
    flags.volatil = type.getQualifier().volatil;
    // *coherent variables are implicitly nonprivate in GLSL
    flags.nonprivate = type.getQualifier().nonprivate ||
                       flags.anyCoherent() ||
                       flags.volatil;
    flags.isImage = type.getBasicType() == glslang::EbtSampler;
    flags.nonUniform = type.getQualifier().nonUniform;
    return flags;
}

spv::Scope TGlslangToSpvTraverser::TranslateMemoryScope(
    const spv::Builder::AccessChain::CoherentFlags &coherentFlags)
{
    spv::Scope scope = spv::ScopeMax;

    if (coherentFlags.volatil || coherentFlags.coherent) {
        // coherent defaults to Device scope in the old model, QueueFamilyKHR scope in the new model
        scope = glslangIntermediate->usingVulkanMemoryModel() ? spv::ScopeQueueFamilyKHR : spv::ScopeDevice;
    } else if (coherentFlags.devicecoherent) {
        scope = spv::ScopeDevice;
    } else if (coherentFlags.queuefamilycoherent) {
        scope = spv::ScopeQueueFamilyKHR;
    } else if (coherentFlags.workgroupcoherent) {
        scope = spv::ScopeWorkgroup;
    } else if (coherentFlags.subgroupcoherent) {
        scope = spv::ScopeSubgroup;
    } else if (coherentFlags.shadercallcoherent) {
        scope = spv::ScopeShaderCallKHR;
    }
    if (glslangIntermediate->usingVulkanMemoryModel() && scope == spv::ScopeDevice) {
        builder.addCapability(spv::CapabilityVulkanMemoryModelDeviceScopeKHR);
    }

    return scope;
}

// Translate a glslang built-in variable to a SPIR-V built in decoration.  Also generate
// associated capabilities when required.  For some built-in variables, a capability
// is generated only when using the variable in an executable instruction, but not when
// just declaring a struct member variable with it.  This is true for PointSize,
// ClipDistance, and CullDistance.
spv::BuiltIn TGlslangToSpvTraverser::TranslateBuiltInDecoration(glslang::TBuiltInVariable builtIn,
    bool memberDeclaration)
{
    switch (builtIn) {
    case glslang::EbvPointSize:
        // Defer adding the capability until the built-in is actually used.
        if (! memberDeclaration) {
            switch (glslangIntermediate->getStage()) {
            case EShLangGeometry:
                builder.addCapability(spv::CapabilityGeometryPointSize);
                break;
            case EShLangTessControl:
            case EShLangTessEvaluation:
                builder.addCapability(spv::CapabilityTessellationPointSize);
                break;
            default:
                break;
            }
        }
        return spv::BuiltInPointSize;

    case glslang::EbvPosition:             return spv::BuiltInPosition;
    case glslang::EbvVertexId:             return spv::BuiltInVertexId;
    case glslang::EbvInstanceId:           return spv::BuiltInInstanceId;
    case glslang::EbvVertexIndex:          return spv::BuiltInVertexIndex;
    case glslang::EbvInstanceIndex:        return spv::BuiltInInstanceIndex;

    case glslang::EbvFragCoord:            return spv::BuiltInFragCoord;
    case glslang::EbvPointCoord:           return spv::BuiltInPointCoord;
    case glslang::EbvFace:                 return spv::BuiltInFrontFacing;
    case glslang::EbvFragDepth:            return spv::BuiltInFragDepth;

    case glslang::EbvNumWorkGroups:        return spv::BuiltInNumWorkgroups;
    case glslang::EbvWorkGroupSize:        return spv::BuiltInWorkgroupSize;
    case glslang::EbvWorkGroupId:          return spv::BuiltInWorkgroupId;
    case glslang::EbvLocalInvocationId:    return spv::BuiltInLocalInvocationId;
    case glslang::EbvLocalInvocationIndex: return spv::BuiltInLocalInvocationIndex;
    case glslang::EbvGlobalInvocationId:   return spv::BuiltInGlobalInvocationId;

    // These *Distance capabilities logically belong here, but if the member is declared and
    // then never used, consumers of SPIR-V prefer the capability not be declared.
    // They are now generated when used, rather than here when declared.
    // Potentially, the specification should be more clear what the minimum
    // use needed is to trigger the capability.
    //
    case glslang::EbvClipDistance:
        if (!memberDeclaration)
            builder.addCapability(spv::CapabilityClipDistance);
        return spv::BuiltInClipDistance;

    case glslang::EbvCullDistance:
        if (!memberDeclaration)
            builder.addCapability(spv::CapabilityCullDistance);
        return spv::BuiltInCullDistance;

    case glslang::EbvViewportIndex:
        if (glslangIntermediate->getStage() == EShLangGeometry ||
            glslangIntermediate->getStage() == EShLangFragment) {
            builder.addCapability(spv::CapabilityMultiViewport);
        }
        if (glslangIntermediate->getStage() == EShLangVertex ||
            glslangIntermediate->getStage() == EShLangTessControl ||
            glslangIntermediate->getStage() == EShLangTessEvaluation) {

            if (builder.getSpvVersion() < spv::Spv_1_5) {
                builder.addIncorporatedExtension(spv::E_SPV_EXT_shader_viewport_index_layer, spv::Spv_1_5);
                builder.addCapability(spv::CapabilityShaderViewportIndexLayerEXT);
            }
            else
                builder.addCapability(spv::CapabilityShaderViewportIndex);
        }
        return spv::BuiltInViewportIndex;

    case glslang::EbvSampleId:
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::BuiltInSampleId;

    case glslang::EbvSamplePosition:
        builder.addCapability(spv::CapabilitySampleRateShading);
        return spv::BuiltInSamplePosition;

    case glslang::EbvSampleMask:
        return spv::BuiltInSampleMask;

    case glslang::EbvLayer:
        if (glslangIntermediate->getStage() == EShLangMesh) {
            return spv::BuiltInLayer;
        }
        if (glslangIntermediate->getStage() == EShLangGeometry ||
            glslangIntermediate->getStage() == EShLangFragment) {
            builder.addCapability(spv::CapabilityGeometry);
        }
        if (glslangIntermediate->getStage() == EShLangVertex ||
            glslangIntermediate->getStage() == EShLangTessControl ||
            glslangIntermediate->getStage() == EShLangTessEvaluation) {

            if (builder.getSpvVersion() < spv::Spv_1_5) {
                builder.addIncorporatedExtension(spv::E_SPV_EXT_shader_viewport_index_layer, spv::Spv_1_5);
                builder.addCapability(spv::CapabilityShaderViewportIndexLayerEXT);
            } else
                builder.addCapability(spv::CapabilityShaderLayer);
        }
        return spv::BuiltInLayer;

    case glslang::EbvBaseVertex:
        builder.addIncorporatedExtension(spv::E_SPV_KHR_shader_draw_parameters, spv::Spv_1_3);
        builder.addCapability(spv::CapabilityDrawParameters);
        return spv::BuiltInBaseVertex;

    case glslang::EbvBaseInstance:
        builder.addIncorporatedExtension(spv::E_SPV_KHR_shader_draw_parameters, spv::Spv_1_3);
        builder.addCapability(spv::CapabilityDrawParameters);
        return spv::BuiltInBaseInstance;

    case glslang::EbvDrawId:
        builder.addIncorporatedExtension(spv::E_SPV_KHR_shader_draw_parameters, spv::Spv_1_3);
        builder.addCapability(spv::CapabilityDrawParameters);
        return spv::BuiltInDrawIndex;

    case glslang::EbvPrimitiveId:
        if (glslangIntermediate->getStage() == EShLangFragment)
            builder.addCapability(spv::CapabilityGeometry);
        return spv::BuiltInPrimitiveId;

    case glslang::EbvFragStencilRef:
        builder.addExtension(spv::E_SPV_EXT_shader_stencil_export);
        builder.addCapability(spv::CapabilityStencilExportEXT);
        return spv::BuiltInFragStencilRefEXT;

    case glslang::EbvShadingRateKHR:
        builder.addExtension(spv::E_SPV_KHR_fragment_shading_rate);
        builder.addCapability(spv::CapabilityFragmentShadingRateKHR);
        return spv::BuiltInShadingRateKHR;

    case glslang::EbvPrimitiveShadingRateKHR:
        builder.addExtension(spv::E_SPV_KHR_fragment_shading_rate);
        builder.addCapability(spv::CapabilityFragmentShadingRateKHR);
        return spv::BuiltInPrimitiveShadingRateKHR;

    case glslang::EbvInvocationId:         return spv::BuiltInInvocationId;
    case glslang::EbvTessLevelInner:       return spv::BuiltInTessLevelInner;
    case glslang::EbvTessLevelOuter:       return spv::BuiltInTessLevelOuter;
    case glslang::EbvTessCoord:            return spv::BuiltInTessCoord;
    case glslang::EbvPatchVertices:        return spv::BuiltInPatchVertices;
    case glslang::EbvHelperInvocation:     return spv::BuiltInHelperInvocation;

    case glslang::EbvSubGroupSize:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupSize;

    case glslang::EbvSubGroupInvocation:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupLocalInvocationId;

    case glslang::EbvSubGroupEqMask:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupEqMask;

    case glslang::EbvSubGroupGeMask:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupGeMask;

    case glslang::EbvSubGroupGtMask:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupGtMask;

    case glslang::EbvSubGroupLeMask:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupLeMask;

    case glslang::EbvSubGroupLtMask:
        builder.addExtension(spv::E_SPV_KHR_shader_ballot);
        builder.addCapability(spv::CapabilitySubgroupBallotKHR);
        return spv::BuiltInSubgroupLtMask;

    case glslang::EbvNumSubgroups:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        return spv::BuiltInNumSubgroups;

    case glslang::EbvSubgroupID:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        return spv::BuiltInSubgroupId;

    case glslang::EbvSubgroupSize2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        return spv::BuiltInSubgroupSize;

    case glslang::EbvSubgroupInvocation2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        return spv::BuiltInSubgroupLocalInvocationId;

    case glslang::EbvSubgroupEqMask2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        builder.addCapability(spv::CapabilityGroupNonUniformBallot);
        return spv::BuiltInSubgroupEqMask;

    case glslang::EbvSubgroupGeMask2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        builder.addCapability(spv::CapabilityGroupNonUniformBallot);
        return spv::BuiltInSubgroupGeMask;

    case glslang::EbvSubgroupGtMask2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        builder.addCapability(spv::CapabilityGroupNonUniformBallot);
        return spv::BuiltInSubgroupGtMask;

    case glslang::EbvSubgroupLeMask2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        builder.addCapability(spv::CapabilityGroupNonUniformBallot);
        return spv::BuiltInSubgroupLeMask;

    case glslang::EbvSubgroupLtMask2:
        builder.addCapability(spv::CapabilityGroupNonUniform);
        builder.addCapability(spv::CapabilityGroupNonUniformBallot);
        return spv::BuiltInSubgroupLtMask;

    case glslang::EbvBaryCoordNoPersp:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordNoPerspAMD;

    case glslang::EbvBaryCoordNoPerspCentroid:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordNoPerspCentroidAMD;

    case glslang::EbvBaryCoordNoPerspSample:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordNoPerspSampleAMD;

    case glslang::EbvBaryCoordSmooth:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordSmoothAMD;

    case glslang::EbvBaryCoordSmoothCentroid:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordSmoothCentroidAMD;

    case glslang::EbvBaryCoordSmoothSample:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordSmoothSampleAMD;

    case glslang::EbvBaryCoordPullModel:
        builder.addExtension(spv::E_SPV_AMD_shader_explicit_vertex_parameter);
        return spv::BuiltInBaryCoordPullModelAMD;

    case glslang::EbvDeviceIndex:
        builder.addIncorporatedExtension(spv::E_SPV_KHR_device_group, spv::Spv_1_3);
        builder.addCapability(spv::CapabilityDeviceGroup);
        return spv::BuiltInDeviceIndex;

    case glslang::EbvViewIndex:
        builder.addIncorporatedExtension(spv::E_SPV_KHR_multiview, spv::Spv_1_3);
        builder.addCapability(spv::CapabilityMultiView);
        return spv::BuiltInViewIndex;

    case glslang::EbvFragSizeEXT:
        builder.addExtension(spv::E_SPV_EXT_fragment_invocation_density);
        builder.addCapability(spv::CapabilityFragmentDensityEXT);
        return spv::BuiltInFragSizeEXT;

    case glslang::EbvFragInvocationCountEXT:
        builder.addExtension(spv::E_SPV_EXT_fragment_invocation_density);
        builder.addCapability(spv::CapabilityFragmentDensityEXT);
        return spv::BuiltInFragInvocationCountEXT;

    case glslang::EbvViewportMaskNV:
        if (!memberDeclaration) {
            builder.addExtension(spv::E_SPV_NV_viewport_array2);
            builder.addCapability(spv::CapabilityShaderViewportMaskNV);
        }
        return spv::BuiltInViewportMaskNV;
    case glslang::EbvSecondaryPositionNV:
        if (!memberDeclaration) {
            builder.addExtension(spv::E_SPV_NV_stereo_view_rendering);
            builder.addCapability(spv::CapabilityShaderStereoViewNV);
        }
        return spv::BuiltInSecondaryPositionNV;
    case glslang::EbvSecondaryViewportMaskNV:
        if (!memberDeclaration) {
            builder.addExtension(spv::E_SPV_NV_stereo_view_rendering);
            builder.addCapability(spv::CapabilityShaderStereoViewNV);
        }
        return spv::BuiltInSecondaryViewportMaskNV;
    case glslang::EbvPositionPerViewNV:
        if (!memberDeclaration) {
            builder.addExtension(spv::E_SPV_NVX_multiview_per_view_attributes);
            builder.addCapability(spv::CapabilityPerViewAttributesNV);
        }
        return spv::BuiltInPositionPerViewNV;
    case glslang::EbvViewportMaskPerViewNV:
        if (!memberDeclaration) {
            builder.addExtension(spv::E_SPV_NVX_multiview_per_view_attributes);
            builder.addCapability(spv::CapabilityPerViewAttributesNV);
        }
        return spv::BuiltInViewportMaskPerViewNV;
    case glslang::EbvFragFullyCoveredNV:
        builder.addExtension(spv::E_SPV_EXT_fragment_fully_covered);
        builder.addCapability(spv::CapabilityFragmentFullyCoveredEXT);
        return spv::BuiltInFullyCoveredEXT;
    case glslang::EbvFragmentSizeNV:
        builder.addExtension(spv::E_SPV_NV_shading_rate);
        builder.addCapability(spv::CapabilityShadingRateNV);
        return spv::BuiltInFragmentSizeNV;
    case glslang::EbvInvocationsPerPixelNV:
        builder.addExtension(spv::E_SPV_NV_shading_rate);
        builder.addCapability(spv::CapabilityShadingRateNV);
        return spv::BuiltInInvocationsPerPixelNV;

    // ray tracing
    case glslang::EbvLaunchId:
        return spv::BuiltInLaunchIdKHR;
    case glslang::EbvLaunchSize:
        return spv::BuiltInLaunchSizeKHR;
    case glslang::EbvWorldRayOrigin:
        return spv::BuiltInWorldRayOriginKHR;
    case glslang::EbvWorldRayDirection:
        return spv::BuiltInWorldRayDirectionKHR;
    case glslang::EbvObjectRayOrigin:
        return spv::BuiltInObjectRayOriginKHR;
    case glslang::EbvObjectRayDirection:
        return spv::BuiltInObjectRayDirectionKHR;
    case glslang::EbvRayTmin:
        return spv::BuiltInRayTminKHR;
    case glslang::EbvRayTmax:
        return spv::BuiltInRayTmaxKHR;
    case glslang::EbvCullMask:
        return spv::BuiltInCullMaskKHR;
    case glslang::EbvPositionFetch:
        return spv::BuiltInHitTriangleVertexPositionsKHR;
    case glslang::EbvInstanceCustomIndex:
        return spv::BuiltInInstanceCustomIndexKHR;
    case glslang::EbvHitT:
        {
            // this is a GLSL alias of RayTmax
            // in SPV_NV_ray_tracing it has a dedicated builtin
            // but in SPV_KHR_ray_tracing it gets mapped to RayTmax
            auto& extensions = glslangIntermediate->getRequestedExtensions();
            if (extensions.find("GL_NV_ray_tracing") != extensions.end()) {
                return spv::BuiltInHitTNV;
            } else {
                return spv::BuiltInRayTmaxKHR;
            }
        }
    case glslang::EbvHitKind:
        return spv::BuiltInHitKindKHR;
    case glslang::EbvObjectToWorld:
    case glslang::EbvObjectToWorld3x4:
        return spv::BuiltInObjectToWorldKHR;
    case glslang::EbvWorldToObject:
    case glslang::EbvWorldToObject3x4:
        return spv::BuiltInWorldToObjectKHR;
    case glslang::EbvIncomingRayFlags:
        return spv::BuiltInIncomingRayFlagsKHR;
    case glslang::EbvGeometryIndex:
        return spv::BuiltInRayGeometryIndexKHR;
    case glslang::EbvCurrentRayTimeNV:
        builder.addExtension(spv::E_SPV_NV_ray_tracing_motion_blur);
        builder.addCapability(spv::CapabilityRayTracingMotionBlurNV);
        return spv::BuiltInCurrentRayTimeNV;

    // barycentrics
    case glslang::EbvBaryCoordNV:
        builder.addExtension(spv::E_SPV_NV_fragment_shader_barycentric);
        builder.addCapability(spv::CapabilityFragmentBarycentricNV);
        return spv::BuiltInBaryCoordNV;
    case glslang::EbvBaryCoordNoPerspNV:
        builder.addExtension(spv::E_SPV_NV_fragment_shader_barycentric);
        builder.addCapability(spv::CapabilityFragmentBarycentricNV);
        return spv::BuiltInBaryCoordNoPerspNV;

    case glslang::EbvBaryCoordEXT:
        builder.addExtension(spv::E_SPV_KHR_fragment_shader_barycentric);
        builder.addCapability(spv::CapabilityFragmentBarycentricKHR);
        return spv::BuiltInBaryCoordKHR;
    case glslang::EbvBaryCoordNoPerspEXT:
        builder.addExtension(spv::E_SPV_KHR_fragment_shader_barycentric);
        builder.addCapability(spv::CapabilityFragmentBarycentricKHR);
        return spv::BuiltInBaryCoordNoPerspKHR;

    // mesh shaders
    case glslang::EbvTaskCountNV:
        return spv::BuiltInTaskCountNV;
    case glslang::EbvPrimitiveCountNV:
        return spv::BuiltInPrimitiveCountNV;
    case glslang::EbvPrimitiveIndicesNV:
        return spv::BuiltInPrimitiveIndicesNV;
    case glslang::EbvClipDistancePerViewNV:
        return spv::BuiltInClipDistancePerViewNV;
    case glslang::EbvCullDistancePerViewNV:
        return spv::BuiltInCullDistancePerViewNV;
    case glslang::EbvLayerPerViewNV:
        return spv::BuiltInLayerPerViewNV;
    case glslang::EbvMeshViewCountNV:
        return spv::BuiltInMeshViewCountNV;
    case glslang::EbvMeshViewIndicesNV:
        return spv::BuiltInMeshViewIndicesNV;

    // SPV_EXT_mesh_shader
    case glslang::EbvPrimitivePointIndicesEXT:
        return spv::BuiltInPrimitivePointIndicesEXT;
    case glslang::EbvPrimitiveLineIndicesEXT:
        return spv::BuiltInPrimitiveLineIndicesEXT;
    case glslang::EbvPrimitiveTriangleIndicesEXT:
        return spv::BuiltInPrimitiveTriangleIndicesEXT;
    case glslang::EbvCullPrimitiveEXT:
        return spv::BuiltInCullPrimitiveEXT;

    // sm builtins
    case glslang::EbvWarpsPerSM:
        builder.addExtension(spv::E_SPV_NV_shader_sm_builtins);
        builder.addCapability(spv::CapabilityShaderSMBuiltinsNV);
        return spv::BuiltInWarpsPerSMNV;
    case glslang::EbvSMCount:
        builder.addExtension(spv::E_SPV_NV_shader_sm_builtins);
        builder.addCapability(spv::CapabilityShaderSMBuiltinsNV);
        return spv::BuiltInSMCountNV;
    case glslang::EbvWarpID:
        builder.addExtension(spv::E_SPV_NV_shader_sm_builtins);
        builder.addCapability(spv::CapabilityShaderSMBuiltinsNV);
        return spv::BuiltInWarpIDNV;
    case glslang::EbvSMID:
        builder.addExtension(spv::E_SPV_NV_shader_sm_builtins);
        builder.addCapability(spv::CapabilityShaderSMBuiltinsNV);
        return spv::BuiltInSMIDNV;

   // ARM builtins
    case glslang::EbvCoreCountARM:
        builder.addExtension(spv::E_SPV_ARM_core_builtins);
        builder.addCapability(spv::CapabilityCoreBuiltinsARM);
        return spv::BuiltInCoreCountARM;
    case glslang::EbvCoreIDARM:
        builder.addExtension(spv::E_SPV_ARM_core_builtins);
        builder.addCapability(spv::CapabilityCoreBuiltinsARM);
        return spv::BuiltInCoreIDARM;
    case glslang::EbvCoreMaxIDARM:
        builder.addExtension(spv::E_SPV_ARM_core_builtins);
        builder.addCapability(spv::CapabilityCoreBuiltinsARM);
        return spv::BuiltInCoreMaxIDARM;
    case glslang::EbvWarpIDARM:
        builder.addExtension(spv::E_SPV_ARM_core_builtins);
        builder.addCapability(spv::CapabilityCoreBuiltinsARM);
        return spv::BuiltInWarpIDARM;
    case glslang::EbvWarpMaxIDARM:
        builder.addExtension(spv::E_SPV_ARM_core_builtins);
        builder.addCapability(spv::CapabilityCoreBuiltinsARM);
        return spv::BuiltInWarpMaxIDARM;

    default:
        return spv::BuiltInMax;
    }
}

// Translate glslang image layout format to SPIR-V image format.
spv::ImageFormat TGlslangToSpvTraverser::TranslateImageFormat(const glslang::TType& type)
{
    assert(type.getBasicType() == glslang::EbtSampler);

    // Check for capabilities
    switch (type.getQualifier().getFormat()) {
    case glslang::ElfRg32f:
    case glslang::ElfRg16f:
    case glslang::ElfR11fG11fB10f:
    case glslang::ElfR16f:
    case glslang::ElfRgba16:
    case glslang::ElfRgb10A2:
    case glslang::ElfRg16:
    case glslang::ElfRg8:
    case glslang::ElfR16:
    case glslang::ElfR8:
    case glslang::ElfRgba16Snorm:
    case glslang::ElfRg16Snorm:
    case glslang::ElfRg8Snorm:
    case glslang::ElfR16Snorm:
    case glslang::ElfR8Snorm:

    case glslang::ElfRg32i:
    case glslang::ElfRg16i:
    case glslang::ElfRg8i:
    case glslang::ElfR16i:
    case glslang::ElfR8i:

    case glslang::ElfRgb10a2ui:
    case glslang::ElfRg32ui:
    case glslang::ElfRg16ui:
    case glslang::ElfRg8ui:
    case glslang::ElfR16ui:
    case glslang::ElfR8ui:
        builder.addCapability(spv::CapabilityStorageImageExtendedFormats);
        break;

    case glslang::ElfR64ui:
    case glslang::ElfR64i:
        builder.addExtension(spv::E_SPV_EXT_shader_image_int64);
        builder.addCapability(spv::CapabilityInt64ImageEXT);
    default:
        break;
    }

    // do the translation
    switch (type.getQualifier().getFormat()) {
    case glslang::ElfNone:          return spv::ImageFormatUnknown;
    case glslang::ElfRgba32f:       return spv::ImageFormatRgba32f;
    case glslang::ElfRgba16f:       return spv::ImageFormatRgba16f;
    case glslang::ElfR32f:          return spv::ImageFormatR32f;
    case glslang::ElfRgba8:         return spv::ImageFormatRgba8;
    case glslang::ElfRgba8Snorm:    return spv::ImageFormatRgba8Snorm;
    case glslang::ElfRg32f:         return spv::ImageFormatRg32f;
    case glslang::ElfRg16f:         return spv::ImageFormatRg16f;
    case glslang::ElfR11fG11fB10f:  return spv::ImageFormatR11fG11fB10f;
    case glslang::ElfR16f:          return spv::ImageFormatR16f;
    case glslang::ElfRgba16:        return spv::ImageFormatRgba16;
    case glslang::ElfRgb10A2:       return spv::ImageFormatRgb10A2;
    case glslang::ElfRg16:          return spv::ImageFormatRg16;
    case glslang::ElfRg8:           return spv::ImageFormatRg8;
    case glslang::ElfR16:           return spv::ImageFormatR16;
    case glslang::ElfR8:            return spv::ImageFormatR8;
    case glslang::ElfRgba16Snorm:   return spv::ImageFormatRgba16Snorm;
    case glslang::ElfRg16Snorm:     return spv::ImageFormatRg16Snorm;
    case glslang::ElfRg8Snorm:      return spv::ImageFormatRg8Snorm;
    case glslang::ElfR16Snorm:      return spv::ImageFormatR16Snorm;
    case glslang::ElfR8Snorm:       return spv::ImageFormatR8Snorm;
    case glslang::ElfRgba32i:       return spv::ImageFormatRgba32i;
    case glslang::ElfRgba16i:       return spv::ImageFormatRgba16i;
    case glslang::ElfRgba8i:        return spv::ImageFormatRgba8i;
    case glslang::ElfR32i:          return spv::ImageFormatR32i;
    case glslang::ElfRg32i:         return spv::ImageFormatRg32i;
    case glslang::ElfRg16i:         return spv::ImageFormatRg16i;
    case glslang::ElfRg8i:          return spv::ImageFormatRg8i;
    case glslang::ElfR16i:          return spv::ImageFormatR16i;
    case glslang::ElfR8i:           return spv::ImageFormatR8i;
    case glslang::ElfRgba32ui:      return spv::ImageFormatRgba32ui;
    case glslang::ElfRgba16ui:      return spv::ImageFormatRgba16ui;
    case glslang::ElfRgba8ui:       return spv::ImageFormatRgba8ui;
    case glslang::ElfR32ui:         return spv::ImageFormatR32ui;
    case glslang::ElfRg32ui:        return spv::ImageFormatRg32ui;
    case glslang::ElfRg16ui:        return spv::ImageFormatRg16ui;
    case glslang::ElfRgb10a2ui:     return spv::ImageFormatRgb10a2ui;
    case glslang::ElfRg8ui:         return spv::ImageFormatRg8ui;
    case glslang::ElfR16ui:         return spv::ImageFormatR16ui;
    case glslang::ElfR8ui:          return spv::ImageFormatR8ui;
    case glslang::ElfR64ui:         return spv::ImageFormatR64ui;
    case glslang::ElfR64i:          return spv::ImageFormatR64i;
    default:                        return spv::ImageFormatMax;
    }
}

spv::SelectionControlMask TGlslangToSpvTraverser::TranslateSelectionControl(
    const glslang::TIntermSelection& selectionNode) const
{
    if (selectionNode.getFlatten())
        return spv::SelectionControlFlattenMask;
    if (selectionNode.getDontFlatten())
        return spv::SelectionControlDontFlattenMask;
    return spv::SelectionControlMaskNone;
}

spv::SelectionControlMask TGlslangToSpvTraverser::TranslateSwitchControl(const glslang::TIntermSwitch& switchNode)
    const
{
    if (switchNode.getFlatten())
        return spv::SelectionControlFlattenMask;
    if (switchNode.getDontFlatten())
        return spv::SelectionControlDontFlattenMask;
    return spv::SelectionControlMaskNone;
}

// return a non-0 dependency if the dependency argument must be set
spv::LoopControlMask TGlslangToSpvTraverser::TranslateLoopControl(const glslang::TIntermLoop& loopNode,
    std::vector<unsigned int>& operands) const
{
    spv::LoopControlMask control = spv::LoopControlMaskNone;

    if (loopNode.getDontUnroll())
        control = control | spv::LoopControlDontUnrollMask;
    if (loopNode.getUnroll())
        control = control | spv::LoopControlUnrollMask;
    if (unsigned(loopNode.getLoopDependency()) == glslang::TIntermLoop::dependencyInfinite)
        control = control | spv::LoopControlDependencyInfiniteMask;
    else if (loopNode.getLoopDependency() > 0) {
        control = control | spv::LoopControlDependencyLengthMask;
        operands.push_back((unsigned int)loopNode.getLoopDependency());
    }
    if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_4) {
        if (loopNode.getMinIterations() > 0) {
            control = control | spv::LoopControlMinIterationsMask;
            operands.push_back(loopNode.getMinIterations());
        }
        if (loopNode.getMaxIterations() < glslang::TIntermLoop::iterationsInfinite) {
            control = control | spv::LoopControlMaxIterationsMask;
            operands.push_back(loopNode.getMaxIterations());
        }
        if (loopNode.getIterationMultiple() > 1) {
            control = control | spv::LoopControlIterationMultipleMask;
            operands.push_back(loopNode.getIterationMultiple());
        }
        if (loopNode.getPeelCount() > 0) {
            control = control | spv::LoopControlPeelCountMask;
            operands.push_back(loopNode.getPeelCount());
        }
        if (loopNode.getPartialCount() > 0) {
            control = control | spv::LoopControlPartialCountMask;
            operands.push_back(loopNode.getPartialCount());
        }
    }

    return control;
}

// Translate glslang type to SPIR-V storage class.
spv::StorageClass TGlslangToSpvTraverser::TranslateStorageClass(const glslang::TType& type)
{
    if (type.getBasicType() == glslang::EbtRayQuery || type.getBasicType() == glslang::EbtHitObjectNV)
        return spv::StorageClassPrivate;
    if (type.getQualifier().isSpirvByReference()) {
        if (type.getQualifier().isParamInput() || type.getQualifier().isParamOutput())
            return spv::StorageClassFunction;
    }
    if (type.getQualifier().isPipeInput())
        return spv::StorageClassInput;
    if (type.getQualifier().isPipeOutput())
        return spv::StorageClassOutput;
    if (type.getQualifier().storage == glslang::EvqTileImageEXT || type.isAttachmentEXT()) {
        builder.addExtension(spv::E_SPV_EXT_shader_tile_image);
        builder.addCapability(spv::CapabilityTileImageColorReadAccessEXT);
        return spv::StorageClassTileImageEXT;
    }

    if (glslangIntermediate->getSource() != glslang::EShSourceHlsl ||
            type.getQualifier().storage == glslang::EvqUniform) {
        if (type.isAtomic())
            return spv::StorageClassAtomicCounter;
        if (type.containsOpaque() && !glslangIntermediate->getBindlessMode())
            return spv::StorageClassUniformConstant;
    }

    if (type.getQualifier().isUniformOrBuffer() &&
        type.getQualifier().isShaderRecord()) {
        return spv::StorageClassShaderRecordBufferKHR;
    }

    if (glslangIntermediate->usingStorageBuffer() && type.getQualifier().storage == glslang::EvqBuffer) {
        builder.addIncorporatedExtension(spv::E_SPV_KHR_storage_buffer_storage_class, spv::Spv_1_3);
        return spv::StorageClassStorageBuffer;
    }

    if (type.getQualifier().isUniformOrBuffer()) {
        if (type.getQualifier().isPushConstant())
            return spv::StorageClassPushConstant;
        if (type.getBasicType() == glslang::EbtBlock)
            return spv::StorageClassUniform;
        return spv::StorageClassUniformConstant;
    }

    if (type.getQualifier().storage == glslang::EvqShared && type.getBasicType() == glslang::EbtBlock) {
        builder.addExtension(spv::E_SPV_KHR_workgroup_memory_explicit_layout);
        builder.addCapability(spv::CapabilityWorkgroupMemoryExplicitLayoutKHR);
        return spv::StorageClassWorkgroup;
    }

    switch (type.getQualifier().storage) {
    case glslang::EvqGlobal:        return spv::StorageClassPrivate;
    case glslang::EvqConstReadOnly: return spv::StorageClassFunction;
    case glslang::EvqTemporary:     return spv::StorageClassFunction;
    case glslang::EvqShared:           return spv::StorageClassWorkgroup;
    case glslang::EvqPayload:        return spv::StorageClassRayPayloadKHR;
    case glslang::EvqPayloadIn:      return spv::StorageClassIncomingRayPayloadKHR;
    case glslang::EvqHitAttr:        return spv::StorageClassHitAttributeKHR;
    case glslang::EvqCallableData:   return spv::StorageClassCallableDataKHR;
    case glslang::EvqCallableDataIn: return spv::StorageClassIncomingCallableDataKHR;
    case glslang::EvqtaskPayloadSharedEXT : return spv::StorageClassTaskPayloadWorkgroupEXT;
    case glslang::EvqHitObjectAttrNV: return spv::StorageClassHitObjectAttributeNV;
    case glslang::EvqSpirvStorageClass: return static_cast<spv::StorageClass>(type.getQualifier().spirvStorageClass);
    default:
        assert(0);
        break;
    }

    return spv::StorageClassFunction;
}

// Translate glslang constants to SPIR-V literals
void TGlslangToSpvTraverser::TranslateLiterals(const glslang::TVector<const glslang::TIntermConstantUnion*>& constants,
                                               std::vector<unsigned>& literals) const
{
    for (auto constant : constants) {
        if (constant->getBasicType() == glslang::EbtFloat) {
            float floatValue = static_cast<float>(constant->getConstArray()[0].getDConst());
            unsigned literal;
            static_assert(sizeof(literal) == sizeof(floatValue), "sizeof(unsigned) != sizeof(float)");
            memcpy(&literal, &floatValue, sizeof(literal));
            literals.push_back(literal);
        } else if (constant->getBasicType() == glslang::EbtInt) {
            unsigned literal = constant->getConstArray()[0].getIConst();
            literals.push_back(literal);
        } else if (constant->getBasicType() == glslang::EbtUint) {
            unsigned literal = constant->getConstArray()[0].getUConst();
            literals.push_back(literal);
        } else if (constant->getBasicType() == glslang::EbtBool) {
            unsigned literal = constant->getConstArray()[0].getBConst();
            literals.push_back(literal);
        } else if (constant->getBasicType() == glslang::EbtString) {
            auto str = constant->getConstArray()[0].getSConst()->c_str();
            unsigned literal = 0;
            char* literalPtr = reinterpret_cast<char*>(&literal);
            unsigned charCount = 0;
            char ch = 0;
            do {
                ch = *(str++);
                *(literalPtr++) = ch;
                ++charCount;
                if (charCount == 4) {
                    literals.push_back(literal);
                    literalPtr = reinterpret_cast<char*>(&literal);
                    charCount = 0;
                }
            } while (ch != 0);

            // Partial literal is padded with 0
            if (charCount > 0) {
                for (; charCount < 4; ++charCount)
                    *(literalPtr++) = 0;
                literals.push_back(literal);
            }
        } else
            assert(0); // Unexpected type
    }
}

// Add capabilities pertaining to how an array is indexed.
void TGlslangToSpvTraverser::addIndirectionIndexCapabilities(const glslang::TType& baseType,
                                                             const glslang::TType& indexType)
{
    if (indexType.getQualifier().isNonUniform()) {
        // deal with an asserted non-uniform index
        // SPV_EXT_descriptor_indexing already added in TranslateNonUniformDecoration
        if (baseType.getBasicType() == glslang::EbtSampler) {
            if (baseType.getQualifier().hasAttachment())
                builder.addCapability(spv::CapabilityInputAttachmentArrayNonUniformIndexingEXT);
            else if (baseType.isImage() && baseType.getSampler().isBuffer())
                builder.addCapability(spv::CapabilityStorageTexelBufferArrayNonUniformIndexingEXT);
            else if (baseType.isTexture() && baseType.getSampler().isBuffer())
                builder.addCapability(spv::CapabilityUniformTexelBufferArrayNonUniformIndexingEXT);
            else if (baseType.isImage())
                builder.addCapability(spv::CapabilityStorageImageArrayNonUniformIndexingEXT);
            else if (baseType.isTexture())
                builder.addCapability(spv::CapabilitySampledImageArrayNonUniformIndexingEXT);
        } else if (baseType.getBasicType() == glslang::EbtBlock) {
            if (baseType.getQualifier().storage == glslang::EvqBuffer)
                builder.addCapability(spv::CapabilityStorageBufferArrayNonUniformIndexingEXT);
            else if (baseType.getQualifier().storage == glslang::EvqUniform)
                builder.addCapability(spv::CapabilityUniformBufferArrayNonUniformIndexingEXT);
        }
    } else {
        // assume a dynamically uniform index
        if (baseType.getBasicType() == glslang::EbtSampler) {
            if (baseType.getQualifier().hasAttachment()) {
                builder.addIncorporatedExtension("SPV_EXT_descriptor_indexing", spv::Spv_1_5);
                builder.addCapability(spv::CapabilityInputAttachmentArrayDynamicIndexingEXT);
            } else if (baseType.isImage() && baseType.getSampler().isBuffer()) {
                builder.addIncorporatedExtension("SPV_EXT_descriptor_indexing", spv::Spv_1_5);
                builder.addCapability(spv::CapabilityStorageTexelBufferArrayDynamicIndexingEXT);
            } else if (baseType.isTexture() && baseType.getSampler().isBuffer()) {
                builder.addIncorporatedExtension("SPV_EXT_descriptor_indexing", spv::Spv_1_5);
                builder.addCapability(spv::CapabilityUniformTexelBufferArrayDynamicIndexingEXT);
            }
        }
    }
}

// Return whether or not the given type is something that should be tied to a
// descriptor set.
bool IsDescriptorResource(const glslang::TType& type)
{
    // uniform and buffer blocks are included, unless it is a push_constant
    if (type.getBasicType() == glslang::EbtBlock)
        return type.getQualifier().isUniformOrBuffer() &&
        ! type.getQualifier().isShaderRecord() &&
        ! type.getQualifier().isPushConstant();

    // non block...
    // basically samplerXXX/subpass/sampler/texture are all included
    // if they are the global-scope-class, not the function parameter
    // (or local, if they ever exist) class.
    if (type.getBasicType() == glslang::EbtSampler ||
        type.getBasicType() == glslang::EbtAccStruct)
        return type.getQualifier().isUniformOrBuffer();

    // None of the above.
    return false;
}

void InheritQualifiers(glslang::TQualifier& child, const glslang::TQualifier& parent)
{
    if (child.layoutMatrix == glslang::ElmNone)
        child.layoutMatrix = parent.layoutMatrix;

    if (parent.invariant)
        child.invariant = true;
    if (parent.flat)
        child.flat = true;
    if (parent.centroid)
        child.centroid = true;
    if (parent.nopersp)
        child.nopersp = true;
    if (parent.explicitInterp)
        child.explicitInterp = true;
    if (parent.perPrimitiveNV)
        child.perPrimitiveNV = true;
    if (parent.perViewNV)
        child.perViewNV = true;
    if (parent.perTaskNV)
        child.perTaskNV = true;
    if (parent.storage == glslang::EvqtaskPayloadSharedEXT)
        child.storage = glslang::EvqtaskPayloadSharedEXT;
    if (parent.patch)
        child.patch = true;
    if (parent.sample)
        child.sample = true;
    if (parent.coherent)
        child.coherent = true;
    if (parent.devicecoherent)
        child.devicecoherent = true;
    if (parent.queuefamilycoherent)
        child.queuefamilycoherent = true;
    if (parent.workgroupcoherent)
        child.workgroupcoherent = true;
    if (parent.subgroupcoherent)
        child.subgroupcoherent = true;
    if (parent.shadercallcoherent)
        child.shadercallcoherent = true;
    if (parent.nonprivate)
        child.nonprivate = true;
    if (parent.volatil)
        child.volatil = true;
    if (parent.restrict)
        child.restrict = true;
    if (parent.readonly)
        child.readonly = true;
    if (parent.writeonly)
        child.writeonly = true;
    if (parent.nonUniform)
        child.nonUniform = true;
}

bool HasNonLayoutQualifiers(const glslang::TType& type, const glslang::TQualifier& qualifier)
{
    // This should list qualifiers that simultaneous satisfy:
    // - struct members might inherit from a struct declaration
    //     (note that non-block structs don't explicitly inherit,
    //      only implicitly, meaning no decoration involved)
    // - affect decorations on the struct members
    //     (note smooth does not, and expecting something like volatile
    //      to effect the whole object)
    // - are not part of the offset/st430/etc or row/column-major layout
    return qualifier.invariant || (qualifier.hasLocation() && type.getBasicType() == glslang::EbtBlock);
}

//
// Implement the TGlslangToSpvTraverser class.
//

TGlslangToSpvTraverser::TGlslangToSpvTraverser(unsigned int spvVersion,
    const glslang::TIntermediate* glslangIntermediate,
    spv::SpvBuildLogger* buildLogger, glslang::SpvOptions& options) :
        TIntermTraverser(true, false, true),
        options(options),
        shaderEntry(nullptr), currentFunction(nullptr),
        sequenceDepth(0), logger(buildLogger),
        builder(spvVersion, (glslang::GetKhronosToolId() << 16) | glslang::GetSpirvGeneratorVersion(), logger),
        inEntryPoint(false), entryPointTerminated(false), linkageOnly(false),
        glslangIntermediate(glslangIntermediate),
        nanMinMaxClamp(glslangIntermediate->getNanMinMaxClamp()),
        nonSemanticDebugPrintf(0),
        taskPayloadID(0)
{
    bool isMeshShaderExt = (glslangIntermediate->getRequestedExtensions().find(glslang::E_GL_EXT_mesh_shader) !=
                            glslangIntermediate->getRequestedExtensions().end());
    spv::ExecutionModel executionModel = TranslateExecutionModel(glslangIntermediate->getStage(), isMeshShaderExt);

    builder.clearAccessChain();
    builder.setSource(TranslateSourceLanguage(glslangIntermediate->getSource(), glslangIntermediate->getProfile()),
                      glslangIntermediate->getVersion());

    if (options.emitNonSemanticShaderDebugSource)
            this->options.emitNonSemanticShaderDebugInfo = true;
    if (options.emitNonSemanticShaderDebugInfo)
            this->options.generateDebugInfo = true;

    if (this->options.generateDebugInfo) {
        builder.setEmitOpLines();
        builder.setSourceFile(glslangIntermediate->getSourceFile());

        // Set the source shader's text. If for SPV version 1.0, include
        // a preamble in comments stating the OpModuleProcessed instructions.
        // Otherwise, emit those as actual instructions.
        std::string text;
        const std::vector<std::string>& processes = glslangIntermediate->getProcesses();
        for (int p = 0; p < (int)processes.size(); ++p) {
            if (glslangIntermediate->getSpv().spv < glslang::EShTargetSpv_1_1) {
                text.append("// OpModuleProcessed ");
                text.append(processes[p]);
                text.append("\n");
            } else
                builder.addModuleProcessed(processes[p]);
        }
        if (glslangIntermediate->getSpv().spv < glslang::EShTargetSpv_1_1 && (int)processes.size() > 0)
            text.append("#line 1\n");
        text.append(glslangIntermediate->getSourceText());
        builder.setSourceText(text);
        // Pass name and text for all included files
        const std::map<std::string, std::string>& include_txt = glslangIntermediate->getIncludeText();
        for (auto iItr = include_txt.begin(); iItr != include_txt.end(); ++iItr)
            builder.addInclude(iItr->first, iItr->second);
    }

    builder.setEmitNonSemanticShaderDebugInfo(this->options.emitNonSemanticShaderDebugInfo);
    builder.setEmitNonSemanticShaderDebugSource(this->options.emitNonSemanticShaderDebugSource);

    stdBuiltins = builder.import("GLSL.std.450");

    spv::AddressingModel addressingModel = spv::AddressingModelLogical;
    spv::MemoryModel memoryModel = spv::MemoryModelGLSL450;

    if (glslangIntermediate->usingPhysicalStorageBuffer()) {
        addressingModel = spv::AddressingModelPhysicalStorageBuffer64EXT;
        builder.addIncorporatedExtension(spv::E_SPV_KHR_physical_storage_buffer, spv::Spv_1_5);
        builder.addCapability(spv::CapabilityPhysicalStorageBufferAddressesEXT);
    }
    if (glslangIntermediate->usingVulkanMemoryModel()) {
        memoryModel = spv::MemoryModelVulkanKHR;
        builder.addCapability(spv::CapabilityVulkanMemoryModelKHR);
        builder.addIncorporatedExtension(spv::E_SPV_KHR_vulkan_memory_model, spv::Spv_1_5);
    }
    builder.setMemoryModel(addressingModel, memoryModel);

    if (glslangIntermediate->usingVariablePointers()) {
        builder.addCapability(spv::CapabilityVariablePointers);
    }

    shaderEntry = builder.makeEntryPoint(glslangIntermediate->getEntryPointName().c_str());
    entryPoint = builder.addEntryPoint(executionModel, shaderEntry, glslangIntermediate->getEntryPointName().c_str());

    // Add the source extensions
    const auto& sourceExtensions = glslangIntermediate->getRequestedExtensions();
    for (auto it = sourceExtensions.begin(); it != sourceExtensions.end(); ++it)
        builder.addSourceExtension(it->c_str());

    // Add the top-level modes for this shader.

    if (glslangIntermediate->getXfbMode()) {
        builder.addCapability(spv::CapabilityTransformFeedback);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeXfb);
    }

    if (glslangIntermediate->getLayoutPrimitiveCulling()) {
        builder.addCapability(spv::CapabilityRayTraversalPrimitiveCullingKHR);
    }

    if (glslangIntermediate->getSubgroupUniformControlFlow()) {
        builder.addExtension(spv::E_SPV_KHR_subgroup_uniform_control_flow);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeSubgroupUniformControlFlowKHR);
    }

    unsigned int mode;
    switch (glslangIntermediate->getStage()) {
    case EShLangVertex:
        builder.addCapability(spv::CapabilityShader);
        break;

    case EShLangFragment:
        builder.addCapability(spv::CapabilityShader);
        if (glslangIntermediate->getPixelCenterInteger())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModePixelCenterInteger);

        if (glslangIntermediate->getOriginUpperLeft())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOriginUpperLeft);
        else
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOriginLowerLeft);

        if (glslangIntermediate->getEarlyFragmentTests())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeEarlyFragmentTests);

        if (glslangIntermediate->getEarlyAndLateFragmentTestsAMD())
        {
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeEarlyAndLateFragmentTestsAMD);
            builder.addExtension(spv::E_SPV_AMD_shader_early_and_late_fragment_tests);
        }

        if (glslangIntermediate->getPostDepthCoverage()) {
            builder.addCapability(spv::CapabilitySampleMaskPostDepthCoverage);
            builder.addExecutionMode(shaderEntry, spv::ExecutionModePostDepthCoverage);
            builder.addExtension(spv::E_SPV_KHR_post_depth_coverage);
        }

        if (glslangIntermediate->getNonCoherentColorAttachmentReadEXT()) {
            builder.addCapability(spv::CapabilityTileImageColorReadAccessEXT);
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeNonCoherentColorAttachmentReadEXT);
            builder.addExtension(spv::E_SPV_EXT_shader_tile_image);
        }

        if (glslangIntermediate->getNonCoherentDepthAttachmentReadEXT()) {
            builder.addCapability(spv::CapabilityTileImageDepthReadAccessEXT);
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeNonCoherentDepthAttachmentReadEXT);
            builder.addExtension(spv::E_SPV_EXT_shader_tile_image);
        }

        if (glslangIntermediate->getNonCoherentStencilAttachmentReadEXT()) {
            builder.addCapability(spv::CapabilityTileImageStencilReadAccessEXT);
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeNonCoherentStencilAttachmentReadEXT);
            builder.addExtension(spv::E_SPV_EXT_shader_tile_image);
        }

        if (glslangIntermediate->isDepthReplacing())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeDepthReplacing);

        if (glslangIntermediate->isStencilReplacing())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeStencilRefReplacingEXT);

        switch(glslangIntermediate->getDepth()) {
        case glslang::EldGreater:   mode = spv::ExecutionModeDepthGreater;   break;
        case glslang::EldLess:      mode = spv::ExecutionModeDepthLess;      break;
        case glslang::EldUnchanged: mode = spv::ExecutionModeDepthUnchanged; break;
        default:                    mode = spv::ExecutionModeMax;            break;
        }

        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        switch (glslangIntermediate->getStencil()) {
        case glslang::ElsRefUnchangedFrontAMD:  mode = spv::ExecutionModeStencilRefUnchangedFrontAMD; break;
        case glslang::ElsRefGreaterFrontAMD:    mode = spv::ExecutionModeStencilRefGreaterFrontAMD;   break;
        case glslang::ElsRefLessFrontAMD:       mode = spv::ExecutionModeStencilRefLessFrontAMD;      break;
        case glslang::ElsRefUnchangedBackAMD:   mode = spv::ExecutionModeStencilRefUnchangedBackAMD;  break;
        case glslang::ElsRefGreaterBackAMD:     mode = spv::ExecutionModeStencilRefGreaterBackAMD;    break;
        case glslang::ElsRefLessBackAMD:        mode = spv::ExecutionModeStencilRefLessBackAMD;       break;
        default:                       mode = spv::ExecutionModeMax;                         break;
        }

        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
        switch (glslangIntermediate->getInterlockOrdering()) {
        case glslang::EioPixelInterlockOrdered:         mode = spv::ExecutionModePixelInterlockOrderedEXT;
            break;
        case glslang::EioPixelInterlockUnordered:       mode = spv::ExecutionModePixelInterlockUnorderedEXT;
            break;
        case glslang::EioSampleInterlockOrdered:        mode = spv::ExecutionModeSampleInterlockOrderedEXT;
            break;
        case glslang::EioSampleInterlockUnordered:      mode = spv::ExecutionModeSampleInterlockUnorderedEXT;
            break;
        case glslang::EioShadingRateInterlockOrdered:   mode = spv::ExecutionModeShadingRateInterlockOrderedEXT;
            break;
        case glslang::EioShadingRateInterlockUnordered: mode = spv::ExecutionModeShadingRateInterlockUnorderedEXT;
            break;
        default:                                        mode = spv::ExecutionModeMax;
            break;
        }
        if (mode != spv::ExecutionModeMax) {
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
            if (mode == spv::ExecutionModeShadingRateInterlockOrderedEXT ||
                mode == spv::ExecutionModeShadingRateInterlockUnorderedEXT) {
                builder.addCapability(spv::CapabilityFragmentShaderShadingRateInterlockEXT);
            } else if (mode == spv::ExecutionModePixelInterlockOrderedEXT ||
                       mode == spv::ExecutionModePixelInterlockUnorderedEXT) {
                builder.addCapability(spv::CapabilityFragmentShaderPixelInterlockEXT);
            } else {
                builder.addCapability(spv::CapabilityFragmentShaderSampleInterlockEXT);
            }
            builder.addExtension(spv::E_SPV_EXT_fragment_shader_interlock);
        }
    break;

    case EShLangCompute:
        builder.addCapability(spv::CapabilityShader);
        if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_6) {
          std::vector<spv::Id> dimConstId;
          for (int dim = 0; dim < 3; ++dim) {
            bool specConst = (glslangIntermediate->getLocalSizeSpecId(dim) != glslang::TQualifier::layoutNotSet);
            dimConstId.push_back(builder.makeUintConstant(glslangIntermediate->getLocalSize(dim), specConst));
            if (specConst) {
                builder.addDecoration(dimConstId.back(), spv::DecorationSpecId,
                                      glslangIntermediate->getLocalSizeSpecId(dim));
            }
          }
          builder.addExecutionModeId(shaderEntry, spv::ExecutionModeLocalSizeId, dimConstId);
        } else {
          builder.addExecutionMode(shaderEntry, spv::ExecutionModeLocalSize, glslangIntermediate->getLocalSize(0),
                                                                             glslangIntermediate->getLocalSize(1),
                                                                             glslangIntermediate->getLocalSize(2));
        }
        if (glslangIntermediate->getLayoutDerivativeModeNone() == glslang::LayoutDerivativeGroupQuads) {
            builder.addCapability(spv::CapabilityComputeDerivativeGroupQuadsNV);
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeDerivativeGroupQuadsNV);
            builder.addExtension(spv::E_SPV_NV_compute_shader_derivatives);
        } else if (glslangIntermediate->getLayoutDerivativeModeNone() == glslang::LayoutDerivativeGroupLinear) {
            builder.addCapability(spv::CapabilityComputeDerivativeGroupLinearNV);
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeDerivativeGroupLinearNV);
            builder.addExtension(spv::E_SPV_NV_compute_shader_derivatives);
        }
        break;
    case EShLangTessEvaluation:
    case EShLangTessControl:
        builder.addCapability(spv::CapabilityTessellation);

        glslang::TLayoutGeometry primitive;

        if (glslangIntermediate->getStage() == EShLangTessControl) {
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices,
                glslangIntermediate->getVertices());
            primitive = glslangIntermediate->getOutputPrimitive();
        } else {
            primitive = glslangIntermediate->getInputPrimitive();
        }

        switch (primitive) {
        case glslang::ElgTriangles:           mode = spv::ExecutionModeTriangles;     break;
        case glslang::ElgQuads:               mode = spv::ExecutionModeQuads;         break;
        case glslang::ElgIsolines:            mode = spv::ExecutionModeIsolines;      break;
        default:                              mode = spv::ExecutionModeMax;           break;
        }
        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        switch (glslangIntermediate->getVertexSpacing()) {
        case glslang::EvsEqual:            mode = spv::ExecutionModeSpacingEqual;          break;
        case glslang::EvsFractionalEven:   mode = spv::ExecutionModeSpacingFractionalEven; break;
        case glslang::EvsFractionalOdd:    mode = spv::ExecutionModeSpacingFractionalOdd;  break;
        default:                           mode = spv::ExecutionModeMax;                   break;
        }
        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        switch (glslangIntermediate->getVertexOrder()) {
        case glslang::EvoCw:     mode = spv::ExecutionModeVertexOrderCw;  break;
        case glslang::EvoCcw:    mode = spv::ExecutionModeVertexOrderCcw; break;
        default:                 mode = spv::ExecutionModeMax;            break;
        }
        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        if (glslangIntermediate->getPointMode())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModePointMode);
        break;

    case EShLangGeometry:
        builder.addCapability(spv::CapabilityGeometry);
        switch (glslangIntermediate->getInputPrimitive()) {
        case glslang::ElgPoints:             mode = spv::ExecutionModeInputPoints;             break;
        case glslang::ElgLines:              mode = spv::ExecutionModeInputLines;              break;
        case glslang::ElgLinesAdjacency:     mode = spv::ExecutionModeInputLinesAdjacency;     break;
        case glslang::ElgTriangles:          mode = spv::ExecutionModeTriangles;               break;
        case glslang::ElgTrianglesAdjacency: mode = spv::ExecutionModeInputTrianglesAdjacency; break;
        default:                             mode = spv::ExecutionModeMax;                     break;
        }
        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        builder.addExecutionMode(shaderEntry, spv::ExecutionModeInvocations, glslangIntermediate->getInvocations());

        switch (glslangIntermediate->getOutputPrimitive()) {
        case glslang::ElgPoints:        mode = spv::ExecutionModeOutputPoints;                 break;
        case glslang::ElgLineStrip:     mode = spv::ExecutionModeOutputLineStrip;              break;
        case glslang::ElgTriangleStrip: mode = spv::ExecutionModeOutputTriangleStrip;          break;
        default:                        mode = spv::ExecutionModeMax;                          break;
        }
        if (mode != spv::ExecutionModeMax)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices, glslangIntermediate->getVertices());
        break;

    case EShLangRayGen:
    case EShLangIntersect:
    case EShLangAnyHit:
    case EShLangClosestHit:
    case EShLangMiss:
    case EShLangCallable:
    {
        auto& extensions = glslangIntermediate->getRequestedExtensions();
        if (extensions.find("GL_NV_ray_tracing") == extensions.end()) {
            builder.addCapability(spv::CapabilityRayTracingKHR);
            builder.addExtension("SPV_KHR_ray_tracing");
        }
        else {
            builder.addCapability(spv::CapabilityRayTracingNV);
            builder.addExtension("SPV_NV_ray_tracing");
        }
        if (glslangIntermediate->getStage() != EShLangRayGen && glslangIntermediate->getStage() != EShLangCallable) {
            if (extensions.find("GL_EXT_ray_cull_mask") != extensions.end()) {
                builder.addCapability(spv::CapabilityRayCullMaskKHR);
                builder.addExtension("SPV_KHR_ray_cull_mask");
            }
            if (extensions.find("GL_EXT_ray_tracing_position_fetch") != extensions.end()) {
                builder.addCapability(spv::CapabilityRayTracingPositionFetchKHR);
                builder.addExtension("SPV_KHR_ray_tracing_position_fetch");
            }
        }
        break;
    }
    case EShLangTask:
    case EShLangMesh:
        if(isMeshShaderExt) {
            builder.addCapability(spv::CapabilityMeshShadingEXT);
            builder.addExtension(spv::E_SPV_EXT_mesh_shader);
        } else {
            builder.addCapability(spv::CapabilityMeshShadingNV);
            builder.addExtension(spv::E_SPV_NV_mesh_shader);
        }
        if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_6) {
            std::vector<spv::Id> dimConstId;
            for (int dim = 0; dim < 3; ++dim) {
                bool specConst = (glslangIntermediate->getLocalSizeSpecId(dim) != glslang::TQualifier::layoutNotSet);
                dimConstId.push_back(builder.makeUintConstant(glslangIntermediate->getLocalSize(dim), specConst));
                if (specConst) {
                    builder.addDecoration(dimConstId.back(), spv::DecorationSpecId,
                                          glslangIntermediate->getLocalSizeSpecId(dim));
                }
            }
            builder.addExecutionModeId(shaderEntry, spv::ExecutionModeLocalSizeId, dimConstId);
        } else {
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeLocalSize, glslangIntermediate->getLocalSize(0),
                                                                               glslangIntermediate->getLocalSize(1),
                                                                               glslangIntermediate->getLocalSize(2));
        }
        if (glslangIntermediate->getStage() == EShLangMesh) {
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices,
                glslangIntermediate->getVertices());
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputPrimitivesNV,
                glslangIntermediate->getPrimitives());

            switch (glslangIntermediate->getOutputPrimitive()) {
            case glslang::ElgPoints:        mode = spv::ExecutionModeOutputPoints;      break;
            case glslang::ElgLines:         mode = spv::ExecutionModeOutputLinesNV;     break;
            case glslang::ElgTriangles:     mode = spv::ExecutionModeOutputTrianglesNV; break;
            default:                        mode = spv::ExecutionModeMax;               break;
            }
            if (mode != spv::ExecutionModeMax)
                builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
        }
        break;

    default:
        break;
    }

    //
    // Add SPIR-V requirements (GL_EXT_spirv_intrinsics)
    //
    if (glslangIntermediate->hasSpirvRequirement()) {
        const glslang::TSpirvRequirement& spirvRequirement = glslangIntermediate->getSpirvRequirement();

        // Add SPIR-V extension requirement
        for (auto& extension : spirvRequirement.extensions)
            builder.addExtension(extension.c_str());

        // Add SPIR-V capability requirement
        for (auto capability : spirvRequirement.capabilities)
            builder.addCapability(static_cast<spv::Capability>(capability));
    }

    //
    // Add SPIR-V execution mode qualifiers (GL_EXT_spirv_intrinsics)
    //
    if (glslangIntermediate->hasSpirvExecutionMode()) {
        const glslang::TSpirvExecutionMode spirvExecutionMode = glslangIntermediate->getSpirvExecutionMode();

        // Add spirv_execution_mode
        for (auto& mode : spirvExecutionMode.modes) {
            if (!mode.second.empty()) {
                std::vector<unsigned> literals;
                TranslateLiterals(mode.second, literals);
                builder.addExecutionMode(shaderEntry, static_cast<spv::ExecutionMode>(mode.first), literals);
            } else
                builder.addExecutionMode(shaderEntry, static_cast<spv::ExecutionMode>(mode.first));
        }

        // Add spirv_execution_mode_id
        for (auto& modeId : spirvExecutionMode.modeIds) {
            std::vector<spv::Id> operandIds;
            assert(!modeId.second.empty());
            for (auto extraOperand : modeId.second) {
                if (extraOperand->getType().getQualifier().isSpecConstant())
                    operandIds.push_back(getSymbolId(extraOperand->getAsSymbolNode()));
                else
                    operandIds.push_back(createSpvConstant(*extraOperand));
            }
            builder.addExecutionModeId(shaderEntry, static_cast<spv::ExecutionMode>(modeId.first), operandIds);
        }
    }
}

// Finish creating SPV, after the traversal is complete.
void TGlslangToSpvTraverser::finishSpv()
{
    // Finish the entry point function
    if (! entryPointTerminated) {
        builder.setBuildPoint(shaderEntry->getLastBlock());
        builder.leaveFunction();
    }

    // finish off the entry-point SPV instruction by adding the Input/Output <id>
    for (auto it = iOSet.cbegin(); it != iOSet.cend(); ++it)
        entryPoint->addIdOperand(*it);

    // Add capabilities, extensions, remove unneeded decorations, etc.,
    // based on the resulting SPIR-V.
    // Note: WebGPU code generation must have the opportunity to aggressively
    // prune unreachable merge blocks and continue targets.
    builder.postProcess();
}

// Write the SPV into 'out'.
void TGlslangToSpvTraverser::dumpSpv(std::vector<unsigned int>& out)
{
    builder.dump(out);
}

//
// Implement the traversal functions.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  Return false if children were
// already processed.
//

//
// Symbols can turn into
//  - uniform/input reads
//  - output writes
//  - complex lvalue base setups:  foo.bar[3]....  , where we see foo and start up an access chain
//  - something simple that degenerates into the last bullet
//
void TGlslangToSpvTraverser::visitSymbol(glslang::TIntermSymbol* symbol)
{
    // We update the line information even though no code might be generated here
    // This is helpful to yield correct lines for control flow instructions
    builder.setLine(symbol->getLoc().line, symbol->getLoc().getFilename());

    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (symbol->getType().isStruct())
        glslangTypeToIdMap[symbol->getType().getStruct()] = symbol->getId();

    if (symbol->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();
#ifdef ENABLE_HLSL
    // Skip symbol handling if it is string-typed
    if (symbol->getBasicType() == glslang::EbtString)
        return;
#endif

    // getSymbolId() will set up all the IO decorations on the first call.
    // Formal function parameters were mapped during makeFunctions().
    spv::Id id = getSymbolId(symbol);

    if (symbol->getType().getQualifier().isTaskPayload())
        taskPayloadID = id; // cache the taskPayloadID to be used it as operand for OpEmitMeshTasksEXT

    if (builder.isPointer(id)) {
        if (!symbol->getType().getQualifier().isParamInput() &&
            !symbol->getType().getQualifier().isParamOutput()) {
            // Include all "static use" and "linkage only" interface variables on the OpEntryPoint instruction
            // Consider adding to the OpEntryPoint interface list.
            // Only looking at structures if they have at least one member.
            if (!symbol->getType().isStruct() || symbol->getType().getStruct()->size() > 0) {
                spv::StorageClass sc = builder.getStorageClass(id);
                // Before SPIR-V 1.4, we only want to include Input and Output.
                // Starting with SPIR-V 1.4, we want all globals.
                if ((glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_4 && builder.isGlobalStorage(id)) ||
                    (sc == spv::StorageClassInput || sc == spv::StorageClassOutput)) {
                    iOSet.insert(id);
                }
            }
        }

        // If the SPIR-V type is required to be different than the AST type
        // (for ex SubgroupMasks or 3x4 ObjectToWorld/WorldToObject matrices),
        // translate now from the SPIR-V type to the AST type, for the consuming
        // operation.
        // Note this turns it from an l-value to an r-value.
        // Currently, all symbols needing this are inputs; avoid the map lookup when non-input.
        if (symbol->getType().getQualifier().storage == glslang::EvqVaryingIn)
            id = translateForcedType(id);
    }

    // Only process non-linkage-only nodes for generating actual static uses
    if (! linkageOnly || symbol->getQualifier().isSpecConstant()) {
        // Prepare to generate code for the access

        // L-value chains will be computed left to right.  We're on the symbol now,
        // which is the left-most part of the access chain, so now is "clear" time,
        // followed by setting the base.
        builder.clearAccessChain();

        // For now, we consider all user variables as being in memory, so they are pointers,
        // except for
        // A) R-Value arguments to a function, which are an intermediate object.
        //    See comments in handleUserFunctionCall().
        // B) Specialization constants (normal constants don't even come in as a variable),
        //    These are also pure R-values.
        // C) R-Values from type translation, see above call to translateForcedType()
        glslang::TQualifier qualifier = symbol->getQualifier();
        if (qualifier.isSpecConstant() || rValueParameters.find(symbol->getId()) != rValueParameters.end() ||
            !builder.isPointerType(builder.getTypeId(id)))
            builder.setAccessChainRValue(id);
        else
            builder.setAccessChainLValue(id);
    }

#ifdef ENABLE_HLSL
    // Process linkage-only nodes for any special additional interface work.
    if (linkageOnly) {
        if (glslangIntermediate->getHlslFunctionality1()) {
            // Map implicit counter buffers to their originating buffers, which should have been
            // seen by now, given earlier pruning of unused counters, and preservation of order
            // of declaration.
            if (symbol->getType().getQualifier().isUniformOrBuffer()) {
                if (!glslangIntermediate->hasCounterBufferName(symbol->getName())) {
                    // Save possible originating buffers for counter buffers, keyed by
                    // making the potential counter-buffer name.
                    std::string keyName = symbol->getName().c_str();
                    keyName = glslangIntermediate->addCounterBufferName(keyName);
                    counterOriginator[keyName] = symbol;
                } else {
                    // Handle a counter buffer, by finding the saved originating buffer.
                    std::string keyName = symbol->getName().c_str();
                    auto it = counterOriginator.find(keyName);
                    if (it != counterOriginator.end()) {
                        id = getSymbolId(it->second);
                        if (id != spv::NoResult) {
                            spv::Id counterId = getSymbolId(symbol);
                            if (counterId != spv::NoResult) {
                                builder.addExtension("SPV_GOOGLE_hlsl_functionality1");
                                builder.addDecorationId(id, spv::DecorationHlslCounterBufferGOOGLE, counterId);
                            }
                        }
                    }
                }
            }
        }
    }
#endif
}

bool TGlslangToSpvTraverser::visitBinary(glslang::TVisit /* visit */, glslang::TIntermBinary* node)
{
    builder.setLine(node->getLoc().line, node->getLoc().getFilename());
    if (node->getLeft()->getAsSymbolNode() != nullptr && node->getLeft()->getType().isStruct()) {
        glslangTypeToIdMap[node->getLeft()->getType().getStruct()] = node->getLeft()->getAsSymbolNode()->getId();
    }
    if (node->getRight()->getAsSymbolNode() != nullptr && node->getRight()->getType().isStruct()) {
        glslangTypeToIdMap[node->getRight()->getType().getStruct()] = node->getRight()->getAsSymbolNode()->getId();
    }

    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (node->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    // First, handle special cases
    switch (node->getOp()) {
    case glslang::EOpAssign:
    case glslang::EOpAddAssign:
    case glslang::EOpSubAssign:
    case glslang::EOpMulAssign:
    case glslang::EOpVectorTimesMatrixAssign:
    case glslang::EOpVectorTimesScalarAssign:
    case glslang::EOpMatrixTimesScalarAssign:
    case glslang::EOpMatrixTimesMatrixAssign:
    case glslang::EOpDivAssign:
    case glslang::EOpModAssign:
    case glslang::EOpAndAssign:
    case glslang::EOpInclusiveOrAssign:
    case glslang::EOpExclusiveOrAssign:
    case glslang::EOpLeftShiftAssign:
    case glslang::EOpRightShiftAssign:
        // A bin-op assign "a += b" means the same thing as "a = a + b"
        // where a is evaluated before b. For a simple assignment, GLSL
        // says to evaluate the left before the right.  So, always, left
        // node then right node.
        {
            // get the left l-value, save it away
            builder.clearAccessChain();
            node->getLeft()->traverse(this);
            spv::Builder::AccessChain lValue = builder.getAccessChain();

            // evaluate the right
            builder.clearAccessChain();
            node->getRight()->traverse(this);
            spv::Id rValue = accessChainLoad(node->getRight()->getType());

            // reset line number for assignment
            builder.setLine(node->getLoc().line, node->getLoc().getFilename());

            if (node->getOp() != glslang::EOpAssign) {
                // the left is also an r-value
                builder.setAccessChain(lValue);
                spv::Id leftRValue = accessChainLoad(node->getLeft()->getType());

                // do the operation
                spv::Builder::AccessChain::CoherentFlags coherentFlags = TranslateCoherent(node->getLeft()->getType());
                coherentFlags |= TranslateCoherent(node->getRight()->getType());
                OpDecorations decorations = { TranslatePrecisionDecoration(node->getOperationPrecision()),
                                              TranslateNoContractionDecoration(node->getType().getQualifier()),
                                              TranslateNonUniformDecoration(coherentFlags) };
                rValue = createBinaryOperation(node->getOp(), decorations,
                                               convertGlslangToSpvType(node->getType()), leftRValue, rValue,
                                               node->getType().getBasicType());

                // these all need their counterparts in createBinaryOperation()
                assert(rValue != spv::NoResult);
            }

            // store the result
            builder.setAccessChain(lValue);
            multiTypeStore(node->getLeft()->getType(), rValue);

            // assignments are expressions having an rValue after they are evaluated...
            builder.clearAccessChain();
            builder.setAccessChainRValue(rValue);
        }
        return false;
    case glslang::EOpIndexDirect:
    case glslang::EOpIndexDirectStruct:
        {
            // Structure, array, matrix, or vector indirection with statically known index.
            // Get the left part of the access chain.
            node->getLeft()->traverse(this);

            // Add the next element in the chain

            const int glslangIndex = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();
            if (! node->getLeft()->getType().isArray() &&
                node->getLeft()->getType().isVector() &&
                node->getOp() == glslang::EOpIndexDirect) {
                // Swizzle is uniform so propagate uniform into access chain
                spv::Builder::AccessChain::CoherentFlags coherentFlags = TranslateCoherent(node->getLeft()->getType());
                coherentFlags.nonUniform = 0;
                // This is essentially a hard-coded vector swizzle of size 1,
                // so short circuit the access-chain stuff with a swizzle.
                std::vector<unsigned> swizzle;
                swizzle.push_back(glslangIndex);
                int dummySize;
                builder.accessChainPushSwizzle(swizzle, convertGlslangToSpvType(node->getLeft()->getType()),
                                               coherentFlags,
                                               glslangIntermediate->getBaseAlignmentScalar(
                                                   node->getLeft()->getType(), dummySize));
            } else {

                // Load through a block reference is performed with a dot operator that
                // is mapped to EOpIndexDirectStruct. When we get to the actual reference,
                // do a load and reset the access chain.
                if (node->getLeft()->isReference() &&
                    !node->getLeft()->getType().isArray() &&
                    node->getOp() == glslang::EOpIndexDirectStruct)
                {
                    spv::Id left = accessChainLoad(node->getLeft()->getType());
                    builder.clearAccessChain();
                    builder.setAccessChainLValue(left);
                }

                int spvIndex = glslangIndex;
                if (node->getLeft()->getBasicType() == glslang::EbtBlock &&
                    node->getOp() == glslang::EOpIndexDirectStruct)
                {
                    // This may be, e.g., an anonymous block-member selection, which generally need
                    // index remapping due to hidden members in anonymous blocks.
                    long long glslangId = glslangTypeToIdMap[node->getLeft()->getType().getStruct()];
                    if (memberRemapper.find(glslangId) != memberRemapper.end()) {
                        std::vector<int>& remapper = memberRemapper[glslangId];
                        assert(remapper.size() > 0);
                        spvIndex = remapper[glslangIndex];
                    }
                }

                // Struct reference propagates uniform lvalue
                spv::Builder::AccessChain::CoherentFlags coherentFlags =
                        TranslateCoherent(node->getLeft()->getType());
                coherentFlags.nonUniform = 0;

                // normal case for indexing array or structure or block
                builder.accessChainPush(builder.makeIntConstant(spvIndex),
                        coherentFlags,
                        node->getLeft()->getType().getBufferReferenceAlignment());

                // Add capabilities here for accessing PointSize and clip/cull distance.
                // We have deferred generation of associated capabilities until now.
                if (node->getLeft()->getType().isStruct() && ! node->getLeft()->getType().isArray())
                    declareUseOfStructMember(*(node->getLeft()->getType().getStruct()), glslangIndex);
            }
        }
        return false;
    case glslang::EOpIndexIndirect:
        {
            // Array, matrix, or vector indirection with variable index.
            // Will use native SPIR-V access-chain for and array indirection;
            // matrices are arrays of vectors, so will also work for a matrix.
            // Will use the access chain's 'component' for variable index into a vector.

            // This adapter is building access chains left to right.
            // Set up the access chain to the left.
            node->getLeft()->traverse(this);

            // save it so that computing the right side doesn't trash it
            spv::Builder::AccessChain partial = builder.getAccessChain();

            // compute the next index in the chain
            builder.clearAccessChain();
            node->getRight()->traverse(this);
            spv::Id index = accessChainLoad(node->getRight()->getType());

            addIndirectionIndexCapabilities(node->getLeft()->getType(), node->getRight()->getType());

            // restore the saved access chain
            builder.setAccessChain(partial);

            // Only if index is nonUniform should we propagate nonUniform into access chain
            spv::Builder::AccessChain::CoherentFlags index_flags = TranslateCoherent(node->getRight()->getType());
            spv::Builder::AccessChain::CoherentFlags coherent_flags = TranslateCoherent(node->getLeft()->getType());
            coherent_flags.nonUniform = index_flags.nonUniform;

            if (! node->getLeft()->getType().isArray() && node->getLeft()->getType().isVector()) {
                int dummySize;
                builder.accessChainPushComponent(
                    index, convertGlslangToSpvType(node->getLeft()->getType()), coherent_flags,
                                                glslangIntermediate->getBaseAlignmentScalar(node->getLeft()->getType(),
                                                dummySize));
            } else
                builder.accessChainPush(index, coherent_flags,
                                        node->getLeft()->getType().getBufferReferenceAlignment());
        }
        return false;
    case glslang::EOpVectorSwizzle:
        {
            node->getLeft()->traverse(this);
            std::vector<unsigned> swizzle;
            convertSwizzle(*node->getRight()->getAsAggregate(), swizzle);
            int dummySize;
            builder.accessChainPushSwizzle(swizzle, convertGlslangToSpvType(node->getLeft()->getType()),
                                           TranslateCoherent(node->getLeft()->getType()),
                                           glslangIntermediate->getBaseAlignmentScalar(node->getLeft()->getType(),
                                               dummySize));
        }
        return false;
    case glslang::EOpMatrixSwizzle:
        logger->missingFunctionality("matrix swizzle");
        return true;
    case glslang::EOpLogicalOr:
    case glslang::EOpLogicalAnd:
        {

            // These may require short circuiting, but can sometimes be done as straight
            // binary operations.  The right operand must be short circuited if it has
            // side effects, and should probably be if it is complex.
            if (isTrivial(node->getRight()->getAsTyped()))
                break; // handle below as a normal binary operation
            // otherwise, we need to do dynamic short circuiting on the right operand
            spv::Id result = createShortCircuit(node->getOp(), *node->getLeft()->getAsTyped(),
                *node->getRight()->getAsTyped());
            builder.clearAccessChain();
            builder.setAccessChainRValue(result);
        }
        return false;
    default:
        break;
    }

    // Assume generic binary op...

    // get right operand
    builder.clearAccessChain();
    node->getLeft()->traverse(this);
    spv::Id left = accessChainLoad(node->getLeft()->getType());

    // get left operand
    builder.clearAccessChain();
    node->getRight()->traverse(this);
    spv::Id right = accessChainLoad(node->getRight()->getType());

    // get result
    OpDecorations decorations = { TranslatePrecisionDecoration(node->getOperationPrecision()),
                                  TranslateNoContractionDecoration(node->getType().getQualifier()),
                                  TranslateNonUniformDecoration(node->getType().getQualifier()) };
    spv::Id result = createBinaryOperation(node->getOp(), decorations,
                                           convertGlslangToSpvType(node->getType()), left, right,
                                           node->getLeft()->getType().getBasicType());

    builder.clearAccessChain();
    if (! result) {
        logger->missingFunctionality("unknown glslang binary operation");
        return true;  // pick up a child as the place-holder result
    } else {
        builder.setAccessChainRValue(result);
        return false;
    }
}

spv::Id TGlslangToSpvTraverser::convertLoadedBoolInUniformToUint(const glslang::TType& type,
                                                                 spv::Id nominalTypeId,
                                                                 spv::Id loadedId)
{
    if (builder.isScalarType(nominalTypeId)) {
        // Conversion for bool
        spv::Id boolType = builder.makeBoolType();
        if (nominalTypeId != boolType)
            return builder.createBinOp(spv::OpINotEqual, boolType, loadedId, builder.makeUintConstant(0));
    } else if (builder.isVectorType(nominalTypeId)) {
        // Conversion for bvec
        int vecSize = builder.getNumTypeComponents(nominalTypeId);
        spv::Id bvecType = builder.makeVectorType(builder.makeBoolType(), vecSize);
        if (nominalTypeId != bvecType)
            loadedId = builder.createBinOp(spv::OpINotEqual, bvecType, loadedId,
                makeSmearedConstant(builder.makeUintConstant(0), vecSize));
    } else if (builder.isArrayType(nominalTypeId)) {
        // Conversion for bool array
        spv::Id boolArrayTypeId = convertGlslangToSpvType(type);
        if (nominalTypeId != boolArrayTypeId)
        {
            // Use OpCopyLogical from SPIR-V 1.4 if available.
            if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_4)
                return builder.createUnaryOp(spv::OpCopyLogical, boolArrayTypeId, loadedId);

            glslang::TType glslangElementType(type, 0);
            spv::Id elementNominalTypeId = builder.getContainedTypeId(nominalTypeId);
            std::vector<spv::Id> constituents;
            for (int index = 0; index < type.getOuterArraySize(); ++index) {
                // get the element
                spv::Id elementValue = builder.createCompositeExtract(loadedId, elementNominalTypeId, index);

                // recursively convert it
                spv::Id elementConvertedValue = convertLoadedBoolInUniformToUint(glslangElementType, elementNominalTypeId, elementValue);
                constituents.push_back(elementConvertedValue);
            }
            return builder.createCompositeConstruct(boolArrayTypeId, constituents);
        }
    }

    return loadedId;
}

// Figure out what, if any, type changes are needed when accessing a specific built-in.
// Returns <the type SPIR-V requires for declarion, the type to translate to on use>.
// Also see comment for 'forceType', regarding tracking SPIR-V-required types.
std::pair<spv::Id, spv::Id> TGlslangToSpvTraverser::getForcedType(glslang::TBuiltInVariable glslangBuiltIn,
    const glslang::TType& glslangType)
{
    switch(glslangBuiltIn)
    {
        case glslang::EbvSubGroupEqMask:
        case glslang::EbvSubGroupGeMask:
        case glslang::EbvSubGroupGtMask:
        case glslang::EbvSubGroupLeMask:
        case glslang::EbvSubGroupLtMask: {
            // these require changing a 64-bit scaler -> a vector of 32-bit components
            if (glslangType.isVector())
                break;
            spv::Id ivec4_type = builder.makeVectorType(builder.makeUintType(32), 4);
            spv::Id uint64_type = builder.makeUintType(64);
            std::pair<spv::Id, spv::Id> ret(ivec4_type, uint64_type);
            return ret;
        }
        // There are no SPIR-V builtins defined for these and map onto original non-transposed
        // builtins. During visitBinary we insert a transpose
        case glslang::EbvWorldToObject3x4:
        case glslang::EbvObjectToWorld3x4: {
            spv::Id mat43 = builder.makeMatrixType(builder.makeFloatType(32), 4, 3);
            spv::Id mat34 = builder.makeMatrixType(builder.makeFloatType(32), 3, 4);
            std::pair<spv::Id, spv::Id> ret(mat43, mat34);
            return ret;
        }
        default:
            break;
    }

    std::pair<spv::Id, spv::Id> ret(spv::NoType, spv::NoType);
    return ret;
}

// For an object previously identified (see getForcedType() and forceType)
// as needing type translations, do the translation needed for a load, turning
// an L-value into in R-value.
spv::Id TGlslangToSpvTraverser::translateForcedType(spv::Id object)
{
    const auto forceIt = forceType.find(object);
    if (forceIt == forceType.end())
        return object;

    spv::Id desiredTypeId = forceIt->second;
    spv::Id objectTypeId = builder.getTypeId(object);
    assert(builder.isPointerType(objectTypeId));
    objectTypeId = builder.getContainedTypeId(objectTypeId);
    if (builder.isVectorType(objectTypeId) &&
        builder.getScalarTypeWidth(builder.getContainedTypeId(objectTypeId)) == 32) {
        if (builder.getScalarTypeWidth(desiredTypeId) == 64) {
            // handle 32-bit v.xy* -> 64-bit
            builder.clearAccessChain();
            builder.setAccessChainLValue(object);
            object = builder.accessChainLoad(spv::NoPrecision, spv::DecorationMax, spv::DecorationMax, objectTypeId);
            std::vector<spv::Id> components;
            components.push_back(builder.createCompositeExtract(object, builder.getContainedTypeId(objectTypeId), 0));
            components.push_back(builder.createCompositeExtract(object, builder.getContainedTypeId(objectTypeId), 1));

            spv::Id vecType = builder.makeVectorType(builder.getContainedTypeId(objectTypeId), 2);
            return builder.createUnaryOp(spv::OpBitcast, desiredTypeId,
                                         builder.createCompositeConstruct(vecType, components));
        } else {
            logger->missingFunctionality("forcing 32-bit vector type to non 64-bit scalar");
        }
    } else if (builder.isMatrixType(objectTypeId)) {
            // There are no SPIR-V builtins defined for 3x4 variants of ObjectToWorld/WorldToObject
            // and we insert a transpose after loading the original non-transposed builtins
            builder.clearAccessChain();
            builder.setAccessChainLValue(object);
            object = builder.accessChainLoad(spv::NoPrecision, spv::DecorationMax, spv::DecorationMax, objectTypeId);
            return builder.createUnaryOp(spv::OpTranspose, desiredTypeId, object);

    } else  {
        logger->missingFunctionality("forcing non 32-bit vector type");
    }

    return object;
}

bool TGlslangToSpvTraverser::visitUnary(glslang::TVisit /* visit */, glslang::TIntermUnary* node)
{
    builder.setLine(node->getLoc().line, node->getLoc().getFilename());

    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (node->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    spv::Id result = spv::NoResult;

    // try texturing first
    result = createImageTextureFunctionCall(node);
    if (result != spv::NoResult) {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false; // done with this node
    }

    // Non-texturing.

    if (node->getOp() == glslang::EOpArrayLength) {
        // Quite special; won't want to evaluate the operand.

        // Currently, the front-end does not allow .length() on an array until it is sized,
        // except for the last block membeor of an SSBO.
        // TODO: If this changes, link-time sized arrays might show up here, and need their
        // size extracted.

        // Normal .length() would have been constant folded by the front-end.
        // So, this has to be block.lastMember.length().
        // SPV wants "block" and member number as the operands, go get them.

        spv::Id length;
        if (node->getOperand()->getType().isCoopMat()) {
            spv::Id typeId = convertGlslangToSpvType(node->getOperand()->getType());
            assert(builder.isCooperativeMatrixType(typeId));

            if (node->getOperand()->getType().isCoopMatKHR()) {
                length = builder.createCooperativeMatrixLengthKHR(typeId);
            } else {
                spec_constant_op_mode_setter.turnOnSpecConstantOpMode();
                length = builder.createCooperativeMatrixLengthNV(typeId);
            }
        } else {
            glslang::TIntermTyped* block = node->getOperand()->getAsBinaryNode()->getLeft();
            block->traverse(this);
            unsigned int member = node->getOperand()->getAsBinaryNode()->getRight()->getAsConstantUnion()
                ->getConstArray()[0].getUConst();
            length = builder.createArrayLength(builder.accessChainGetLValue(), member);
        }

        // GLSL semantics say the result of .length() is an int, while SPIR-V says
        // signedness must be 0. So, convert from SPIR-V unsigned back to GLSL's
        // AST expectation of a signed result.
        if (glslangIntermediate->getSource() == glslang::EShSourceGlsl) {
            if (builder.isInSpecConstCodeGenMode()) {
                length = builder.createBinOp(spv::OpIAdd, builder.makeIntType(32), length, builder.makeIntConstant(0));
            } else {
                length = builder.createUnaryOp(spv::OpBitcast, builder.makeIntType(32), length);
            }
        }

        builder.clearAccessChain();
        builder.setAccessChainRValue(length);

        return false;
    }

    // Force variable declaration - Debug Mode Only
    if (node->getOp() == glslang::EOpDeclare) {
        builder.clearAccessChain();
        node->getOperand()->traverse(this);
        builder.clearAccessChain();
        return false;
    }

    // Start by evaluating the operand

    // Does it need a swizzle inversion?  If so, evaluation is inverted;
    // operate first on the swizzle base, then apply the swizzle.
    spv::Id invertedType = spv::NoType;
    auto resultType = [&invertedType, &node, this](){ return invertedType != spv::NoType ?
        invertedType : convertGlslangToSpvType(node->getType()); };
    if (node->getOp() == glslang::EOpInterpolateAtCentroid)
        invertedType = getInvertedSwizzleType(*node->getOperand());

    builder.clearAccessChain();
    TIntermNode *operandNode;
    if (invertedType != spv::NoType)
        operandNode = node->getOperand()->getAsBinaryNode()->getLeft();
    else
        operandNode = node->getOperand();

    operandNode->traverse(this);

    spv::Id operand = spv::NoResult;

    spv::Builder::AccessChain::CoherentFlags lvalueCoherentFlags;

    const auto hitObjectOpsWithLvalue = [](glslang::TOperator op) {
        switch(op) {
            case glslang::EOpReorderThreadNV:
            case glslang::EOpHitObjectGetCurrentTimeNV:
            case glslang::EOpHitObjectGetHitKindNV:
            case glslang::EOpHitObjectGetPrimitiveIndexNV:
            case glslang::EOpHitObjectGetGeometryIndexNV:
            case glslang::EOpHitObjectGetInstanceIdNV:
            case glslang::EOpHitObjectGetInstanceCustomIndexNV:
            case glslang::EOpHitObjectGetObjectRayDirectionNV:
            case glslang::EOpHitObjectGetObjectRayOriginNV:
            case glslang::EOpHitObjectGetWorldRayDirectionNV:
            case glslang::EOpHitObjectGetWorldRayOriginNV:
            case glslang::EOpHitObjectGetWorldToObjectNV:
            case glslang::EOpHitObjectGetObjectToWorldNV:
            case glslang::EOpHitObjectGetRayTMaxNV:
            case glslang::EOpHitObjectGetRayTMinNV:
            case glslang::EOpHitObjectIsEmptyNV:
            case glslang::EOpHitObjectIsHitNV:
            case glslang::EOpHitObjectIsMissNV:
            case glslang::EOpHitObjectRecordEmptyNV:
            case glslang::EOpHitObjectGetShaderBindingTableRecordIndexNV:
            case glslang::EOpHitObjectGetShaderRecordBufferHandleNV:
                return true;
            default:
                return false;
        }
    };

    if (node->getOp() == glslang::EOpAtomicCounterIncrement ||
        node->getOp() == glslang::EOpAtomicCounterDecrement ||
        node->getOp() == glslang::EOpAtomicCounter          ||
        (node->getOp() == glslang::EOpInterpolateAtCentroid &&
          glslangIntermediate->getSource() != glslang::EShSourceHlsl)  ||
        node->getOp() == glslang::EOpRayQueryProceed        ||
        node->getOp() == glslang::EOpRayQueryGetRayTMin     ||
        node->getOp() == glslang::EOpRayQueryGetRayFlags    ||
        node->getOp() == glslang::EOpRayQueryGetWorldRayOrigin ||
        node->getOp() == glslang::EOpRayQueryGetWorldRayDirection ||
        node->getOp() == glslang::EOpRayQueryGetIntersectionCandidateAABBOpaque ||
        node->getOp() == glslang::EOpRayQueryTerminate ||
        node->getOp() == glslang::EOpRayQueryConfirmIntersection ||
        (node->getOp() == glslang::EOpSpirvInst && operandNode->getAsTyped()->getQualifier().isSpirvByReference()) ||
        hitObjectOpsWithLvalue(node->getOp())) {
        operand = builder.accessChainGetLValue(); // Special case l-value operands
        lvalueCoherentFlags = builder.getAccessChain().coherentFlags;
        lvalueCoherentFlags |= TranslateCoherent(operandNode->getAsTyped()->getType());
    } else if (operandNode->getAsTyped()->getQualifier().isSpirvLiteral()) {
        // Will be translated to a literal value, make a placeholder here
        operand = spv::NoResult;
    } else {
        operand = accessChainLoad(node->getOperand()->getType());
    }

    OpDecorations decorations = { TranslatePrecisionDecoration(node->getOperationPrecision()),
                                  TranslateNoContractionDecoration(node->getType().getQualifier()),
                                  TranslateNonUniformDecoration(node->getType().getQualifier()) };

    // it could be a conversion
    if (! result)
        result = createConversion(node->getOp(), decorations, resultType(), operand,
            node->getOperand()->getBasicType());

    // if not, then possibly an operation
    if (! result)
        result = createUnaryOperation(node->getOp(), decorations, resultType(), operand,
            node->getOperand()->getBasicType(), lvalueCoherentFlags);

    // it could be attached to a SPIR-V intruction
    if (!result) {
        if (node->getOp() == glslang::EOpSpirvInst) {
            const auto& spirvInst = node->getSpirvInstruction();
            if (spirvInst.set == "") {
                spv::IdImmediate idImmOp = {true, operand};
                if (operandNode->getAsTyped()->getQualifier().isSpirvLiteral()) {
                    // Translate the constant to a literal value
                    std::vector<unsigned> literals;
                    glslang::TVector<const glslang::TIntermConstantUnion*> constants;
                    constants.push_back(operandNode->getAsConstantUnion());
                    TranslateLiterals(constants, literals);
                    idImmOp = {false, literals[0]};
                }

                if (node->getBasicType() == glslang::EbtVoid)
                    builder.createNoResultOp(static_cast<spv::Op>(spirvInst.id), {idImmOp});
                else
                    result = builder.createOp(static_cast<spv::Op>(spirvInst.id), resultType(), {idImmOp});
            } else {
                result = builder.createBuiltinCall(
                    resultType(), spirvInst.set == "GLSL.std.450" ? stdBuiltins : getExtBuiltins(spirvInst.set.c_str()),
                    spirvInst.id, {operand});
            }

            if (node->getBasicType() == glslang::EbtVoid)
                return false; // done with this node
        }
    }

    if (result) {
        if (invertedType) {
            result = createInvertedSwizzle(decorations.precision, *node->getOperand(), result);
            decorations.addNonUniform(builder, result);
        }

        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false; // done with this node
    }

    // it must be a special case, check...
    switch (node->getOp()) {
    case glslang::EOpPostIncrement:
    case glslang::EOpPostDecrement:
    case glslang::EOpPreIncrement:
    case glslang::EOpPreDecrement:
        {
            // we need the integer value "1" or the floating point "1.0" to add/subtract
            spv::Id one = 0;
            if (node->getBasicType() == glslang::EbtFloat)
                one = builder.makeFloatConstant(1.0F);
            else if (node->getBasicType() == glslang::EbtDouble)
                one = builder.makeDoubleConstant(1.0);
            else if (node->getBasicType() == glslang::EbtFloat16)
                one = builder.makeFloat16Constant(1.0F);
            else if (node->getBasicType() == glslang::EbtInt8  || node->getBasicType() == glslang::EbtUint8)
                one = builder.makeInt8Constant(1);
            else if (node->getBasicType() == glslang::EbtInt16 || node->getBasicType() == glslang::EbtUint16)
                one = builder.makeInt16Constant(1);
            else if (node->getBasicType() == glslang::EbtInt64 || node->getBasicType() == glslang::EbtUint64)
                one = builder.makeInt64Constant(1);
            else
                one = builder.makeIntConstant(1);
            glslang::TOperator op;
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPostIncrement)
                op = glslang::EOpAdd;
            else
                op = glslang::EOpSub;

            spv::Id result = createBinaryOperation(op, decorations,
                                                   convertGlslangToSpvType(node->getType()), operand, one,
                                                   node->getType().getBasicType());
            assert(result != spv::NoResult);

            // The result of operation is always stored, but conditionally the
            // consumed result.  The consumed result is always an r-value.
            builder.accessChainStore(result,
                                     TranslateNonUniformDecoration(builder.getAccessChain().coherentFlags));
            builder.clearAccessChain();
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPreDecrement)
                builder.setAccessChainRValue(result);
            else
                builder.setAccessChainRValue(operand);
        }

        return false;

    case glslang::EOpEmitStreamVertex:
        builder.createNoResultOp(spv::OpEmitStreamVertex, operand);
        return false;
    case glslang::EOpEndStreamPrimitive:
        builder.createNoResultOp(spv::OpEndStreamPrimitive, operand);
        return false;
    case glslang::EOpRayQueryTerminate:
        builder.createNoResultOp(spv::OpRayQueryTerminateKHR, operand);
        return false;
    case glslang::EOpRayQueryConfirmIntersection:
        builder.createNoResultOp(spv::OpRayQueryConfirmIntersectionKHR, operand);
        return false;
    case glslang::EOpReorderThreadNV:
        builder.createNoResultOp(spv::OpReorderThreadWithHitObjectNV, operand);
        return false;
    case glslang::EOpHitObjectRecordEmptyNV:
        builder.createNoResultOp(spv::OpHitObjectRecordEmptyNV, operand);
        return false;

    default:
        logger->missingFunctionality("unknown glslang unary");
        return true;  // pick up operand as placeholder result
    }
}

// Construct a composite object, recursively copying members if their types don't match
spv::Id TGlslangToSpvTraverser::createCompositeConstruct(spv::Id resultTypeId, std::vector<spv::Id> constituents)
{
    for (int c = 0; c < (int)constituents.size(); ++c) {
        spv::Id& constituent = constituents[c];
        spv::Id lType = builder.getContainedTypeId(resultTypeId, c);
        spv::Id rType = builder.getTypeId(constituent);
        if (lType != rType) {
            if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_4) {
                constituent = builder.createUnaryOp(spv::OpCopyLogical, lType, constituent);
            } else if (builder.isStructType(rType)) {
                std::vector<spv::Id> rTypeConstituents;
                int numrTypeConstituents = builder.getNumTypeConstituents(rType);
                for (int i = 0; i < numrTypeConstituents; ++i) {
                    rTypeConstituents.push_back(builder.createCompositeExtract(constituent,
                        builder.getContainedTypeId(rType, i), i));
                }
                constituents[c] = createCompositeConstruct(lType, rTypeConstituents);
            } else {
                assert(builder.isArrayType(rType));
                std::vector<spv::Id> rTypeConstituents;
                int numrTypeConstituents = builder.getNumTypeConstituents(rType);

                spv::Id elementRType = builder.getContainedTypeId(rType);
                for (int i = 0; i < numrTypeConstituents; ++i) {
                    rTypeConstituents.push_back(builder.createCompositeExtract(constituent, elementRType, i));
                }
                constituents[c] = createCompositeConstruct(lType, rTypeConstituents);
            }
        }
    }
    return builder.createCompositeConstruct(resultTypeId, constituents);
}

bool TGlslangToSpvTraverser::visitAggregate(glslang::TVisit visit, glslang::TIntermAggregate* node)
{
    SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
    if (node->getType().getQualifier().isSpecConstant())
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();

    spv::Id result = spv::NoResult;
    spv::Id invertedType = spv::NoType;                     // to use to override the natural type of the node
    std::vector<spv::Builder::AccessChain> complexLvalues;  // for holding swizzling l-values too complex for
                                                            // SPIR-V, for an out parameter
    std::vector<spv::Id> temporaryLvalues;                  // temporaries to pass, as proxies for complexLValues

    auto resultType = [&invertedType, &node, this](){ return invertedType != spv::NoType ?
        invertedType :
        convertGlslangToSpvType(node->getType()); };

    // try texturing
    result = createImageTextureFunctionCall(node);
    if (result != spv::NoResult) {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    } else if (node->getOp() == glslang::EOpImageStore ||
        node->getOp() == glslang::EOpImageStoreLod ||
        node->getOp() == glslang::EOpImageAtomicStore) {
        // "imageStore" is a special case, which has no result
        return false;
    }

    glslang::TOperator binOp = glslang::EOpNull;
    bool reduceComparison = true;
    bool isMatrix = false;
    bool noReturnValue = false;
    bool atomic = false;

    spv::Builder::AccessChain::CoherentFlags lvalueCoherentFlags;

    assert(node->getOp());

    spv::Decoration precision = TranslatePrecisionDecoration(node->getOperationPrecision());

    switch (node->getOp()) {
    case glslang::EOpScope:
    case glslang::EOpSequence:
    {
        if (visit == glslang::EvPreVisit) {
            ++sequenceDepth;
            if (sequenceDepth == 1) {
                // If this is the parent node of all the functions, we want to see them
                // early, so all call points have actual SPIR-V functions to reference.
                // In all cases, still let the traverser visit the children for us.
                makeFunctions(node->getAsAggregate()->getSequence());

                // Also, we want all globals initializers to go into the beginning of the entry point, before
                // anything else gets there, so visit out of order, doing them all now.
                makeGlobalInitializers(node->getAsAggregate()->getSequence());

                //Pre process linker objects for ray tracing stages
                if (glslangIntermediate->isRayTracingStage())
                  collectRayTracingLinkerObjects();

                // Initializers are done, don't want to visit again, but functions and link objects need to be processed,
                // so do them manually.
                visitFunctions(node->getAsAggregate()->getSequence());

                return false;
            } else {
                if (node->getOp() == glslang::EOpScope)
                    builder.enterScope(0);
            }
        } else {
            if (sequenceDepth > 1 && node->getOp() == glslang::EOpScope)
                builder.leaveScope();
            --sequenceDepth;
        }

        return true;
    }
    case glslang::EOpLinkerObjects:
    {
        if (visit == glslang::EvPreVisit)
            linkageOnly = true;
        else
            linkageOnly = false;

        return true;
    }
    case glslang::EOpComma:
    {
        // processing from left to right naturally leaves the right-most
        // lying around in the access chain
        glslang::TIntermSequence& glslangOperands = node->getSequence();
        for (int i = 0; i < (int)glslangOperands.size(); ++i)
            glslangOperands[i]->traverse(this);

        return false;
    }
    case glslang::EOpFunction:
        if (visit == glslang::EvPreVisit) {
            if (isShaderEntryPoint(node)) {
                inEntryPoint = true;
                builder.setBuildPoint(shaderEntry->getLastBlock());
                builder.enterFunction(shaderEntry);
                currentFunction = shaderEntry;
            } else {
                handleFunctionEntry(node);
            }
            if (options.generateDebugInfo) {
                const auto& loc = node->getLoc();
                const char* sourceFileName = loc.getFilename();
                spv::Id sourceFileId = sourceFileName ? builder.getStringId(sourceFileName) : builder.getSourceFile();
                currentFunction->setDebugLineInfo(sourceFileId, loc.line, loc.column);
            }
        } else {
            if (inEntryPoint)
                entryPointTerminated = true;
            builder.leaveFunction();
            inEntryPoint = false;
        }

        return true;
    case glslang::EOpParameters:
        // Parameters will have been consumed by EOpFunction processing, but not
        // the body, so we still visited the function node's children, making this
        // child redundant.
        return false;
    case glslang::EOpFunctionCall:
    {
        builder.setLine(node->getLoc().line, node->getLoc().getFilename());
        if (node->isUserDefined())
            result = handleUserFunctionCall(node);
        if (result) {
            builder.clearAccessChain();
            builder.setAccessChainRValue(result);
        } else
            logger->missingFunctionality("missing user function; linker needs to catch that");

        return false;
    }
    case glslang::EOpConstructMat2x2:
    case glslang::EOpConstructMat2x3:
    case glslang::EOpConstructMat2x4:
    case glslang::EOpConstructMat3x2:
    case glslang::EOpConstructMat3x3:
    case glslang::EOpConstructMat3x4:
    case glslang::EOpConstructMat4x2:
    case glslang::EOpConstructMat4x3:
    case glslang::EOpConstructMat4x4:
    case glslang::EOpConstructDMat2x2:
    case glslang::EOpConstructDMat2x3:
    case glslang::EOpConstructDMat2x4:
    case glslang::EOpConstructDMat3x2:
    case glslang::EOpConstructDMat3x3:
    case glslang::EOpConstructDMat3x4:
    case glslang::EOpConstructDMat4x2:
    case glslang::EOpConstructDMat4x3:
    case glslang::EOpConstructDMat4x4:
    case glslang::EOpConstructIMat2x2:
    case glslang::EOpConstructIMat2x3:
    case glslang::EOpConstructIMat2x4:
    case glslang::EOpConstructIMat3x2:
    case glslang::EOpConstructIMat3x3:
    case glslang::EOpConstructIMat3x4:
    case glslang::EOpConstructIMat4x2:
    case glslang::EOpConstructIMat4x3:
    case glslang::EOpConstructIMat4x4:
    case glslang::EOpConstructUMat2x2:
    case glslang::EOpConstructUMat2x3:
    case glslang::EOpConstructUMat2x4:
    case glslang::EOpConstructUMat3x2:
    case glslang::EOpConstructUMat3x3:
    case glslang::EOpConstructUMat3x4:
    case glslang::EOpConstructUMat4x2:
    case glslang::EOpConstructUMat4x3:
    case glslang::EOpConstructUMat4x4:
    case glslang::EOpConstructBMat2x2:
    case glslang::EOpConstructBMat2x3:
    case glslang::EOpConstructBMat2x4:
    case glslang::EOpConstructBMat3x2:
    case glslang::EOpConstructBMat3x3:
    case glslang::EOpConstructBMat3x4:
    case glslang::EOpConstructBMat4x2:
    case glslang::EOpConstructBMat4x3:
    case glslang::EOpConstructBMat4x4:
    case glslang::EOpConstructF16Mat2x2:
    case glslang::EOpConstructF16Mat2x3:
    case glslang::EOpConstructF16Mat2x4:
    case glslang::EOpConstructF16Mat3x2:
    case glslang::EOpConstructF16Mat3x3:
    case glslang::EOpConstructF16Mat3x4:
    case glslang::EOpConstructF16Mat4x2:
    case glslang::EOpConstructF16Mat4x3:
    case glslang::EOpConstructF16Mat4x4:
        isMatrix = true;
        // fall through
    case glslang::EOpConstructFloat:
    case glslang::EOpConstructVec2:
    case glslang::EOpConstructVec3:
    case glslang::EOpConstructVec4:
    case glslang::EOpConstructDouble:
    case glslang::EOpConstructDVec2:
    case glslang::EOpConstructDVec3:
    case glslang::EOpConstructDVec4:
    case glslang::EOpConstructFloat16:
    case glslang::EOpConstructF16Vec2:
    case glslang::EOpConstructF16Vec3:
    case glslang::EOpConstructF16Vec4:
    case glslang::EOpConstructBool:
    case glslang::EOpConstructBVec2:
    case glslang::EOpConstructBVec3:
    case glslang::EOpConstructBVec4:
    case glslang::EOpConstructInt8:
    case glslang::EOpConstructI8Vec2:
    case glslang::EOpConstructI8Vec3:
    case glslang::EOpConstructI8Vec4:
    case glslang::EOpConstructUint8:
    case glslang::EOpConstructU8Vec2:
    case glslang::EOpConstructU8Vec3:
    case glslang::EOpConstructU8Vec4:
    case glslang::EOpConstructInt16:
    case glslang::EOpConstructI16Vec2:
    case glslang::EOpConstructI16Vec3:
    case glslang::EOpConstructI16Vec4:
    case glslang::EOpConstructUint16:
    case glslang::EOpConstructU16Vec2:
    case glslang::EOpConstructU16Vec3:
    case glslang::EOpConstructU16Vec4:
    case glslang::EOpConstructInt:
    case glslang::EOpConstructIVec2:
    case glslang::EOpConstructIVec3:
    case glslang::EOpConstructIVec4:
    case glslang::EOpConstructUint:
    case glslang::EOpConstructUVec2:
    case glslang::EOpConstructUVec3:
    case glslang::EOpConstructUVec4:
    case glslang::EOpConstructInt64:
    case glslang::EOpConstructI64Vec2:
    case glslang::EOpConstructI64Vec3:
    case glslang::EOpConstructI64Vec4:
    case glslang::EOpConstructUint64:
    case glslang::EOpConstructU64Vec2:
    case glslang::EOpConstructU64Vec3:
    case glslang::EOpConstructU64Vec4:
    case glslang::EOpConstructStruct:
    case glslang::EOpConstructTextureSampler:
    case glslang::EOpConstructReference:
    case glslang::EOpConstructCooperativeMatrixNV:
    case glslang::EOpConstructCooperativeMatrixKHR:
    {
        builder.setLine(node->getLoc().line, node->getLoc().getFilename());
        std::vector<spv::Id> arguments;
        translateArguments(*node, arguments, lvalueCoherentFlags);
        spv::Id constructed;
        if (node->getOp() == glslang::EOpConstructTextureSampler) {
            const glslang::TType& texType = node->getSequence()[0]->getAsTyped()->getType();
            if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_6 &&
                texType.getSampler().isBuffer()) {
                // SamplerBuffer is not supported in spirv1.6 so
                // `samplerBuffer(textureBuffer, sampler)` is a no-op
                // and textureBuffer is the result going forward
                constructed = arguments[0];
            } else
                constructed = builder.createOp(spv::OpSampledImage, resultType(), arguments);
        } else if (node->getOp() == glslang::EOpConstructStruct ||
                 node->getOp() == glslang::EOpConstructCooperativeMatrixNV ||
                 node->getOp() == glslang::EOpConstructCooperativeMatrixKHR ||
                 node->getType().isArray()) {
            std::vector<spv::Id> constituents;
            for (int c = 0; c < (int)arguments.size(); ++c)
                constituents.push_back(arguments[c]);
            constructed = createCompositeConstruct(resultType(), constituents);
        } else if (isMatrix)
            constructed = builder.createMatrixConstructor(precision, arguments, resultType());
        else
            constructed = builder.createConstructor(precision, arguments, resultType());

        if (node->getType().getQualifier().isNonUniform()) {
            builder.addDecoration(constructed, spv::DecorationNonUniformEXT);
        }

        builder.clearAccessChain();
        builder.setAccessChainRValue(constructed);

        return false;
    }

    // These six are component-wise compares with component-wise results.
    // Forward on to createBinaryOperation(), requesting a vector result.
    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpVectorEqual:
    case glslang::EOpVectorNotEqual:
    {
        // Map the operation to a binary
        binOp = node->getOp();
        reduceComparison = false;
        switch (node->getOp()) {
        case glslang::EOpVectorEqual:     binOp = glslang::EOpVectorEqual;      break;
        case glslang::EOpVectorNotEqual:  binOp = glslang::EOpVectorNotEqual;   break;
        default:                          binOp = node->getOp();                break;
        }

        break;
    }
    case glslang::EOpMul:
        // component-wise matrix multiply
        binOp = glslang::EOpMul;
        break;
    case glslang::EOpOuterProduct:
        // two vectors multiplied to make a matrix
        binOp = glslang::EOpOuterProduct;
        break;
    case glslang::EOpDot:
    {
        // for scalar dot product, use multiply
        glslang::TIntermSequence& glslangOperands = node->getSequence();
        if (glslangOperands[0]->getAsTyped()->getVectorSize() == 1)
            binOp = glslang::EOpMul;
        break;
    }
    case glslang::EOpMod:
        // when an aggregate, this is the floating-point mod built-in function,
        // which can be emitted by the one in createBinaryOperation()
        binOp = glslang::EOpMod;
        break;

    case glslang::EOpEmitVertex:
    case glslang::EOpEndPrimitive:
    case glslang::EOpBarrier:
    case glslang::EOpMemoryBarrier:
    case glslang::EOpMemoryBarrierAtomicCounter:
    case glslang::EOpMemoryBarrierBuffer:
    case glslang::EOpMemoryBarrierImage:
    case glslang::EOpMemoryBarrierShared:
    case glslang::EOpGroupMemoryBarrier:
    case glslang::EOpDeviceMemoryBarrier:
    case glslang::EOpAllMemoryBarrierWithGroupSync:
    case glslang::EOpDeviceMemoryBarrierWithGroupSync:
    case glslang::EOpWorkgroupMemoryBarrier:
    case glslang::EOpWorkgroupMemoryBarrierWithGroupSync:
    case glslang::EOpSubgroupBarrier:
    case glslang::EOpSubgroupMemoryBarrier:
    case glslang::EOpSubgroupMemoryBarrierBuffer:
    case glslang::EOpSubgroupMemoryBarrierImage:
    case glslang::EOpSubgroupMemoryBarrierShared:
        noReturnValue = true;
        // These all have 0 operands and will naturally finish up in the code below for 0 operands
        break;

    case glslang::EOpAtomicAdd:
    case glslang::EOpAtomicSubtract:
    case glslang::EOpAtomicMin:
    case glslang::EOpAtomicMax:
    case glslang::EOpAtomicAnd:
    case glslang::EOpAtomicOr:
    case glslang::EOpAtomicXor:
    case glslang::EOpAtomicExchange:
    case glslang::EOpAtomicCompSwap:
        atomic = true;
        break;

    case glslang::EOpAtomicStore:
        noReturnValue = true;
        // fallthrough
    case glslang::EOpAtomicLoad:
        atomic = true;
        break;

    case glslang::EOpAtomicCounterAdd:
    case glslang::EOpAtomicCounterSubtract:
    case glslang::EOpAtomicCounterMin:
    case glslang::EOpAtomicCounterMax:
    case glslang::EOpAtomicCounterAnd:
    case glslang::EOpAtomicCounterOr:
    case glslang::EOpAtomicCounterXor:
    case glslang::EOpAtomicCounterExchange:
    case glslang::EOpAtomicCounterCompSwap:
        builder.addExtension("SPV_KHR_shader_atomic_counter_ops");
        builder.addCapability(spv::CapabilityAtomicStorageOps);
        atomic = true;
        break;

    case glslang::EOpAbsDifference:
    case glslang::EOpAddSaturate:
    case glslang::EOpSubSaturate:
    case glslang::EOpAverage:
    case glslang::EOpAverageRounded:
    case glslang::EOpMul32x16:
        builder.addCapability(spv::CapabilityIntegerFunctions2INTEL);
        builder.addExtension("SPV_INTEL_shader_integer_functions2");
        binOp = node->getOp();
        break;

    case glslang::EOpIgnoreIntersectionNV:
    case glslang::EOpTerminateRayNV:
    case glslang::EOpTraceNV:
    case glslang::EOpTraceRayMotionNV:
    case glslang::EOpTraceKHR:
    case glslang::EOpExecuteCallableNV:
    case glslang::EOpExecuteCallableKHR:
    case glslang::EOpWritePackedPrimitiveIndices4x8NV:
    case glslang::EOpEmitMeshTasksEXT:
    case glslang::EOpSetMeshOutputsEXT:
        noReturnValue = true;
        break;
    case glslang::EOpRayQueryInitialize:
    case glslang::EOpRayQueryTerminate:
    case glslang::EOpRayQueryGenerateIntersection:
    case glslang::EOpRayQueryConfirmIntersection:
        builder.addExtension("SPV_KHR_ray_query");
        builder.addCapability(spv::CapabilityRayQueryKHR);
        noReturnValue = true;
        break;
    case glslang::EOpRayQueryProceed:
    case glslang::EOpRayQueryGetIntersectionType:
    case glslang::EOpRayQueryGetRayTMin:
    case glslang::EOpRayQueryGetRayFlags:
    case glslang::EOpRayQueryGetIntersectionT:
    case glslang::EOpRayQueryGetIntersectionInstanceCustomIndex:
    case glslang::EOpRayQueryGetIntersectionInstanceId:
    case glslang::EOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffset:
    case glslang::EOpRayQueryGetIntersectionGeometryIndex:
    case glslang::EOpRayQueryGetIntersectionPrimitiveIndex:
    case glslang::EOpRayQueryGetIntersectionBarycentrics:
    case glslang::EOpRayQueryGetIntersectionFrontFace:
    case glslang::EOpRayQueryGetIntersectionCandidateAABBOpaque:
    case glslang::EOpRayQueryGetIntersectionObjectRayDirection:
    case glslang::EOpRayQueryGetIntersectionObjectRayOrigin:
    case glslang::EOpRayQueryGetWorldRayDirection:
    case glslang::EOpRayQueryGetWorldRayOrigin:
    case glslang::EOpRayQueryGetIntersectionObjectToWorld:
    case glslang::EOpRayQueryGetIntersectionWorldToObject:
        builder.addExtension("SPV_KHR_ray_query");
        builder.addCapability(spv::CapabilityRayQueryKHR);
        break;
    case glslang::EOpCooperativeMatrixLoad:
    case glslang::EOpCooperativeMatrixStore:
    case glslang::EOpCooperativeMatrixLoadNV:
    case glslang::EOpCooperativeMatrixStoreNV:
        noReturnValue = true;
        break;
    case glslang::EOpBeginInvocationInterlock:
    case glslang::EOpEndInvocationInterlock:
        builder.addExtension(spv::E_SPV_EXT_fragment_shader_interlock);
        noReturnValue = true;
        break;

    case glslang::EOpHitObjectTraceRayNV:
    case glslang::EOpHitObjectTraceRayMotionNV:
    case glslang::EOpHitObjectGetAttributesNV:
    case glslang::EOpHitObjectExecuteShaderNV:
    case glslang::EOpHitObjectRecordEmptyNV:
    case glslang::EOpHitObjectRecordMissNV:
    case glslang::EOpHitObjectRecordMissMotionNV:
    case glslang::EOpHitObjectRecordHitNV:
    case glslang::EOpHitObjectRecordHitMotionNV:
    case glslang::EOpHitObjectRecordHitWithIndexNV:
    case glslang::EOpHitObjectRecordHitWithIndexMotionNV:
    case glslang::EOpReorderThreadNV:
        noReturnValue = true;
        //Fallthrough
    case glslang::EOpHitObjectIsEmptyNV:
    case glslang::EOpHitObjectIsMissNV:
    case glslang::EOpHitObjectIsHitNV:
    case glslang::EOpHitObjectGetRayTMinNV:
    case glslang::EOpHitObjectGetRayTMaxNV:
    case glslang::EOpHitObjectGetObjectRayOriginNV:
    case glslang::EOpHitObjectGetObjectRayDirectionNV:
    case glslang::EOpHitObjectGetWorldRayOriginNV:
    case glslang::EOpHitObjectGetWorldRayDirectionNV:
    case glslang::EOpHitObjectGetObjectToWorldNV:
    case glslang::EOpHitObjectGetWorldToObjectNV:
    case glslang::EOpHitObjectGetInstanceCustomIndexNV:
    case glslang::EOpHitObjectGetInstanceIdNV:
    case glslang::EOpHitObjectGetGeometryIndexNV:
    case glslang::EOpHitObjectGetPrimitiveIndexNV:
    case glslang::EOpHitObjectGetHitKindNV:
    case glslang::EOpHitObjectGetCurrentTimeNV:
    case glslang::EOpHitObjectGetShaderBindingTableRecordIndexNV:
    case glslang::EOpHitObjectGetShaderRecordBufferHandleNV:
        builder.addExtension(spv::E_SPV_NV_shader_invocation_reorder);
        builder.addCapability(spv::CapabilityShaderInvocationReorderNV);
        break;
    case glslang::EOpRayQueryGetIntersectionTriangleVertexPositionsEXT:
        builder.addExtension(spv::E_SPV_KHR_ray_tracing_position_fetch);
        builder.addCapability(spv::CapabilityRayQueryPositionFetchKHR);
        noReturnValue = true;
        break;

    case glslang::EOpDebugPrintf:
        noReturnValue = true;
        break;

    default:
        break;
    }

    //
    // See if it maps to a regular operation.
    //
    if (binOp != glslang::EOpNull) {
        glslang::TIntermTyped* left = node->getSequence()[0]->getAsTyped();
        glslang::TIntermTyped* right = node->getSequence()[1]->getAsTyped();
        assert(left && right);

        builder.clearAccessChain();
        left->traverse(this);
        spv::Id leftId = accessChainLoad(left->getType());

        builder.clearAccessChain();
        right->traverse(this);
        spv::Id rightId = accessChainLoad(right->getType());

        builder.setLine(node->getLoc().line, node->getLoc().getFilename());
        OpDecorations decorations = { precision,
                                      TranslateNoContractionDecoration(node->getType().getQualifier()),
                                      TranslateNonUniformDecoration(node->getType().getQualifier()) };
        result = createBinaryOperation(binOp, decorations,
                                       resultType(), leftId, rightId,
                                       left->getType().getBasicType(), reduceComparison);

        // code above should only make binOp that exists in createBinaryOperation
        assert(result != spv::NoResult);
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    }

    //
    // Create the list of operands.
    //
    glslang::TIntermSequence& glslangOperands = node->getSequence();
    std::vector<spv::Id> operands;
    std::vector<spv::IdImmediate> memoryAccessOperands;
    for (int arg = 0; arg < (int)glslangOperands.size(); ++arg) {
        // special case l-value operands; there are just a few
        bool lvalue = false;
        switch (node->getOp()) {
        case glslang::EOpModf:
            if (arg == 1)
                lvalue = true;
            break;



        case glslang::EOpHitObjectRecordHitNV:
        case glslang::EOpHitObjectRecordHitMotionNV:
        case glslang::EOpHitObjectRecordHitWithIndexNV:
        case glslang::EOpHitObjectRecordHitWithIndexMotionNV:
        case glslang::EOpHitObjectTraceRayNV:
        case glslang::EOpHitObjectTraceRayMotionNV:
        case glslang::EOpHitObjectExecuteShaderNV:
        case glslang::EOpHitObjectRecordMissNV:
        case glslang::EOpHitObjectRecordMissMotionNV:
        case glslang::EOpHitObjectGetAttributesNV:
            if (arg == 0)
                lvalue = true;
            break;

        case glslang::EOpRayQueryInitialize:
        case glslang::EOpRayQueryTerminate:
        case glslang::EOpRayQueryConfirmIntersection:
        case glslang::EOpRayQueryProceed:
        case glslang::EOpRayQueryGenerateIntersection:
        case glslang::EOpRayQueryGetIntersectionType:
        case glslang::EOpRayQueryGetIntersectionT:
        case glslang::EOpRayQueryGetIntersectionInstanceCustomIndex:
        case glslang::EOpRayQueryGetIntersectionInstanceId:
        case glslang::EOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffset:
        case glslang::EOpRayQueryGetIntersectionGeometryIndex:
        case glslang::EOpRayQueryGetIntersectionPrimitiveIndex:
        case glslang::EOpRayQueryGetIntersectionBarycentrics:
        case glslang::EOpRayQueryGetIntersectionFrontFace:
        case glslang::EOpRayQueryGetIntersectionObjectRayDirection:
        case glslang::EOpRayQueryGetIntersectionObjectRayOrigin:
        case glslang::EOpRayQueryGetIntersectionObjectToWorld:
        case glslang::EOpRayQueryGetIntersectionWorldToObject:
            if (arg == 0)
                lvalue = true;
            break;

        case glslang::EOpAtomicAdd:
        case glslang::EOpAtomicSubtract:
        case glslang::EOpAtomicMin:
        case glslang::EOpAtomicMax:
        case glslang::EOpAtomicAnd:
        case glslang::EOpAtomicOr:
        case glslang::EOpAtomicXor:
        case glslang::EOpAtomicExchange:
        case glslang::EOpAtomicCompSwap:
            if (arg == 0)
                lvalue = true;
            break;

        case glslang::EOpFrexp:
            if (arg == 1)
                lvalue = true;
            break;
        case glslang::EOpInterpolateAtSample:
        case glslang::EOpInterpolateAtOffset:
        case glslang::EOpInterpolateAtVertex:
            if (arg == 0) {
                // If GLSL, use the address of the interpolant argument.
                // If HLSL, use an internal version of OpInterolates that takes
                // the rvalue of the interpolant. A fixup pass in spirv-opt
                // legalization will remove the OpLoad and convert to an lvalue.
                // Had to do this because legalization will only propagate a
                // builtin into an rvalue.
                lvalue = glslangIntermediate->getSource() != glslang::EShSourceHlsl;

                // Does it need a swizzle inversion?  If so, evaluation is inverted;
                // operate first on the swizzle base, then apply the swizzle.
                // That is, we transform
                //
                //    interpolate(v.zy)  ->  interpolate(v).zy
                //
                if (glslangOperands[0]->getAsOperator() &&
                    glslangOperands[0]->getAsOperator()->getOp() == glslang::EOpVectorSwizzle)
                    invertedType = convertGlslangToSpvType(
                        glslangOperands[0]->getAsBinaryNode()->getLeft()->getType());
            }
            break;
        case glslang::EOpAtomicLoad:
        case glslang::EOpAtomicStore:
        case glslang::EOpAtomicCounterAdd:
        case glslang::EOpAtomicCounterSubtract:
        case glslang::EOpAtomicCounterMin:
        case glslang::EOpAtomicCounterMax:
        case glslang::EOpAtomicCounterAnd:
        case glslang::EOpAtomicCounterOr:
        case glslang::EOpAtomicCounterXor:
        case glslang::EOpAtomicCounterExchange:
        case glslang::EOpAtomicCounterCompSwap:
            if (arg == 0)
                lvalue = true;
            break;
        case glslang::EOpAddCarry:
        case glslang::EOpSubBorrow:
            if (arg == 2)
                lvalue = true;
            break;
        case glslang::EOpUMulExtended:
        case glslang::EOpIMulExtended:
            if (arg >= 2)
                lvalue = true;
            break;
        case glslang::EOpCooperativeMatrixLoad:
        case glslang::EOpCooperativeMatrixLoadNV:
            if (arg == 0 || arg == 1)
                lvalue = true;
            break;
        case glslang::EOpCooperativeMatrixStore:
        case glslang::EOpCooperativeMatrixStoreNV:
            if (arg == 1)
                lvalue = true;
            break;
        case glslang::EOpSpirvInst:
            if (glslangOperands[arg]->getAsTyped()->getQualifier().isSpirvByReference())
                lvalue = true;
            break;
        case glslang::EOpReorderThreadNV:
            //Three variants of reorderThreadNV, two of them use hitObjectNV
            if (arg == 0 && glslangOperands.size() != 2)
                lvalue = true;
            break;
        case glslang::EOpRayQueryGetIntersectionTriangleVertexPositionsEXT:
            if (arg == 0 || arg == 2)
                lvalue = true;
            break;
        default:
            break;
        }
        builder.clearAccessChain();
        if (invertedType != spv::NoType && arg == 0)
            glslangOperands[0]->getAsBinaryNode()->getLeft()->traverse(this);
        else
            glslangOperands[arg]->traverse(this);

        if (node->getOp() == glslang::EOpCooperativeMatrixLoad ||
            node->getOp() == glslang::EOpCooperativeMatrixStore ||
            node->getOp() == glslang::EOpCooperativeMatrixLoadNV ||
            node->getOp() == glslang::EOpCooperativeMatrixStoreNV) {

            if (arg == 1) {
                // fold "element" parameter into the access chain
                spv::Builder::AccessChain save = builder.getAccessChain();
                builder.clearAccessChain();
                glslangOperands[2]->traverse(this);

                spv::Id elementId = accessChainLoad(glslangOperands[2]->getAsTyped()->getType());

                builder.setAccessChain(save);

                // Point to the first element of the array.
                builder.accessChainPush(elementId,
                    TranslateCoherent(glslangOperands[arg]->getAsTyped()->getType()),
                                      glslangOperands[arg]->getAsTyped()->getType().getBufferReferenceAlignment());

                spv::Builder::AccessChain::CoherentFlags coherentFlags = builder.getAccessChain().coherentFlags;
                unsigned int alignment = builder.getAccessChain().alignment;

                int memoryAccess = TranslateMemoryAccess(coherentFlags);
                if (node->getOp() == glslang::EOpCooperativeMatrixLoad ||
                    node->getOp() == glslang::EOpCooperativeMatrixLoadNV)
                    memoryAccess &= ~spv::MemoryAccessMakePointerAvailableKHRMask;
                if (node->getOp() == glslang::EOpCooperativeMatrixStore ||
                    node->getOp() == glslang::EOpCooperativeMatrixStoreNV)
                    memoryAccess &= ~spv::MemoryAccessMakePointerVisibleKHRMask;
                if (builder.getStorageClass(builder.getAccessChain().base) ==
                    spv::StorageClassPhysicalStorageBufferEXT) {
                    memoryAccess = (spv::MemoryAccessMask)(memoryAccess | spv::MemoryAccessAlignedMask);
                }

                memoryAccessOperands.push_back(spv::IdImmediate(false, memoryAccess));

                if (memoryAccess & spv::MemoryAccessAlignedMask) {
                    memoryAccessOperands.push_back(spv::IdImmediate(false, alignment));
                }

                if (memoryAccess &
                    (spv::MemoryAccessMakePointerAvailableKHRMask | spv::MemoryAccessMakePointerVisibleKHRMask)) {
                    memoryAccessOperands.push_back(spv::IdImmediate(true,
                        builder.makeUintConstant(TranslateMemoryScope(coherentFlags))));
                }
            } else if (arg == 2) {
                continue;
            }
        }

        // for l-values, pass the address, for r-values, pass the value
        if (lvalue) {
            if (invertedType == spv::NoType && !builder.isSpvLvalue()) {
                // SPIR-V cannot represent an l-value containing a swizzle that doesn't
                // reduce to a simple access chain.  So, we need a temporary vector to
                // receive the result, and must later swizzle that into the original
                // l-value.
                complexLvalues.push_back(builder.getAccessChain());
                temporaryLvalues.push_back(builder.createVariable(
                    spv::NoPrecision, spv::StorageClassFunction,
                    builder.accessChainGetInferredType(), "swizzleTemp"));
                operands.push_back(temporaryLvalues.back());
            } else {
                operands.push_back(builder.accessChainGetLValue());
            }
            lvalueCoherentFlags = builder.getAccessChain().coherentFlags;
            lvalueCoherentFlags |= TranslateCoherent(glslangOperands[arg]->getAsTyped()->getType());
        } else {
            builder.setLine(node->getLoc().line, node->getLoc().getFilename());
             glslang::TOperator glslangOp = node->getOp();
             if (arg == 1 &&
                (glslangOp == glslang::EOpRayQueryGetIntersectionType ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionT ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionInstanceCustomIndex ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionInstanceId ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffset ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionGeometryIndex ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionPrimitiveIndex ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionBarycentrics ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionFrontFace ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionObjectRayDirection ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionObjectRayOrigin ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionObjectToWorld ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionWorldToObject ||
                 glslangOp == glslang::EOpRayQueryGetIntersectionTriangleVertexPositionsEXT
                    )) {
                bool cond = glslangOperands[arg]->getAsConstantUnion()->getConstArray()[0].getBConst();
                operands.push_back(builder.makeIntConstant(cond ? 1 : 0));
             } else if ((arg == 10 && glslangOp == glslang::EOpTraceKHR) ||
                        (arg == 11 && glslangOp == glslang::EOpTraceRayMotionNV) ||
                        (arg == 1  && glslangOp == glslang::EOpExecuteCallableKHR) ||
                        (arg == 1  && glslangOp == glslang::EOpHitObjectExecuteShaderNV) ||
                        (arg == 11 && glslangOp == glslang::EOpHitObjectTraceRayNV) ||
                        (arg == 12 && glslangOp == glslang::EOpHitObjectTraceRayMotionNV)) {
                 const int set = glslangOp == glslang::EOpExecuteCallableKHR ? 1 : 0;
                 const int location = glslangOperands[arg]->getAsConstantUnion()->getConstArray()[0].getUConst();
                 auto itNode = locationToSymbol[set].find(location);
                 visitSymbol(itNode->second);
                 spv::Id symId = getSymbolId(itNode->second);
                 operands.push_back(symId);
            } else if ((arg == 12 && glslangOp == glslang::EOpHitObjectRecordHitNV) ||
                       (arg == 13 && glslangOp == glslang::EOpHitObjectRecordHitMotionNV) ||
                       (arg == 11 && glslangOp == glslang::EOpHitObjectRecordHitWithIndexNV) ||
                       (arg == 12 && glslangOp == glslang::EOpHitObjectRecordHitWithIndexMotionNV) ||
                       (arg == 1  && glslangOp == glslang::EOpHitObjectGetAttributesNV)) {
                 const int location = glslangOperands[arg]->getAsConstantUnion()->getConstArray()[0].getUConst();
                 const int set = 2;
                 auto itNode = locationToSymbol[set].find(location);
                 visitSymbol(itNode->second);
                 spv::Id symId = getSymbolId(itNode->second);
                 operands.push_back(symId);
             } else if (glslangOperands[arg]->getAsTyped()->getQualifier().isSpirvLiteral()) {
                 // Will be translated to a literal value, make a placeholder here
                 operands.push_back(spv::NoResult);
             } else  {
                operands.push_back(accessChainLoad(glslangOperands[arg]->getAsTyped()->getType()));
             }
        }
    }

    builder.setLine(node->getLoc().line, node->getLoc().getFilename());
    if (node->getOp() == glslang::EOpCooperativeMatrixLoad ||
        node->getOp() == glslang::EOpCooperativeMatrixLoadNV) {
        std::vector<spv::IdImmediate> idImmOps;

        idImmOps.push_back(spv::IdImmediate(true, operands[1])); // buf
        if (node->getOp() == glslang::EOpCooperativeMatrixLoad) {
            idImmOps.push_back(spv::IdImmediate(true, operands[3])); // matrixLayout
            idImmOps.push_back(spv::IdImmediate(true, operands[2])); // stride
        } else {
            idImmOps.push_back(spv::IdImmediate(true, operands[2])); // stride
            idImmOps.push_back(spv::IdImmediate(true, operands[3])); // colMajor
        }
        idImmOps.insert(idImmOps.end(), memoryAccessOperands.begin(), memoryAccessOperands.end());
        // get the pointee type
        spv::Id typeId = builder.getContainedTypeId(builder.getTypeId(operands[0]));
        assert(builder.isCooperativeMatrixType(typeId));
        // do the op
        spv::Id result = node->getOp() == glslang::EOpCooperativeMatrixLoad
                       ? builder.createOp(spv::OpCooperativeMatrixLoadKHR, typeId, idImmOps)
                       : builder.createOp(spv::OpCooperativeMatrixLoadNV, typeId, idImmOps);
        // store the result to the pointer (out param 'm')
        builder.createStore(result, operands[0]);
        result = 0;
    } else if (node->getOp() == glslang::EOpCooperativeMatrixStore ||
               node->getOp() == glslang::EOpCooperativeMatrixStoreNV) {
        std::vector<spv::IdImmediate> idImmOps;

        idImmOps.push_back(spv::IdImmediate(true, operands[1])); // buf
        idImmOps.push_back(spv::IdImmediate(true, operands[0])); // object
        if (node->getOp() == glslang::EOpCooperativeMatrixStore) {
            idImmOps.push_back(spv::IdImmediate(true, operands[3])); // matrixLayout
            idImmOps.push_back(spv::IdImmediate(true, operands[2])); // stride
        } else {
            idImmOps.push_back(spv::IdImmediate(true, operands[2])); // stride
            idImmOps.push_back(spv::IdImmediate(true, operands[3])); // colMajor
        }
        idImmOps.insert(idImmOps.end(), memoryAccessOperands.begin(), memoryAccessOperands.end());

        if (node->getOp() == glslang::EOpCooperativeMatrixStore)
            builder.createNoResultOp(spv::OpCooperativeMatrixStoreKHR, idImmOps);
        else
            builder.createNoResultOp(spv::OpCooperativeMatrixStoreNV, idImmOps);
        result = 0;
    } else if (node->getOp() == glslang::EOpRayQueryGetIntersectionTriangleVertexPositionsEXT) {
        std::vector<spv::IdImmediate> idImmOps;

        idImmOps.push_back(spv::IdImmediate(true, operands[0])); // q
        idImmOps.push_back(spv::IdImmediate(true, operands[1])); // committed

        spv::Id typeId = builder.makeArrayType(builder.makeVectorType(builder.makeFloatType(32), 3),
                                               builder.makeUintConstant(3), 0);
        // do the op
        spv::Id result = builder.createOp(spv::OpRayQueryGetIntersectionTriangleVertexPositionsKHR, typeId, idImmOps);
        // store the result to the pointer (out param 'm')
        builder.createStore(result, operands[2]);
        result = 0;
    } else if (node->getOp() == glslang::EOpCooperativeMatrixMulAdd) {
        uint32_t matrixOperands = 0;

        // If the optional operand is present, initialize matrixOperands to that value.
        if (glslangOperands.size() == 4 && glslangOperands[3]->getAsConstantUnion()) {
            matrixOperands = glslangOperands[3]->getAsConstantUnion()->getConstArray()[0].getIConst();
        }

        // Determine Cooperative Matrix Operands bits from the signedness of the types.
        if (isTypeSignedInt(glslangOperands[0]->getAsTyped()->getBasicType()))
            matrixOperands |= spv::CooperativeMatrixOperandsMatrixASignedComponentsMask;
        if (isTypeSignedInt(glslangOperands[1]->getAsTyped()->getBasicType()))
            matrixOperands |= spv::CooperativeMatrixOperandsMatrixBSignedComponentsMask;
        if (isTypeSignedInt(glslangOperands[2]->getAsTyped()->getBasicType()))
            matrixOperands |= spv::CooperativeMatrixOperandsMatrixCSignedComponentsMask;
        if (isTypeSignedInt(node->getBasicType()))
            matrixOperands |= spv::CooperativeMatrixOperandsMatrixResultSignedComponentsMask;

        std::vector<spv::IdImmediate> idImmOps;
        idImmOps.push_back(spv::IdImmediate(true, operands[0]));
        idImmOps.push_back(spv::IdImmediate(true, operands[1]));
        idImmOps.push_back(spv::IdImmediate(true, operands[2]));
        if (matrixOperands != 0)
            idImmOps.push_back(spv::IdImmediate(false, matrixOperands));

        result = builder.createOp(spv::OpCooperativeMatrixMulAddKHR, resultType(), idImmOps);
    } else if (atomic) {
        // Handle all atomics
        glslang::TBasicType typeProxy = (node->getOp() == glslang::EOpAtomicStore)
            ? node->getSequence()[0]->getAsTyped()->getBasicType() : node->getBasicType();
        result = createAtomicOperation(node->getOp(), precision, resultType(), operands, typeProxy,
            lvalueCoherentFlags);
    } else if (node->getOp() == glslang::EOpSpirvInst) {
        const auto& spirvInst = node->getSpirvInstruction();
        if (spirvInst.set == "") {
            std::vector<spv::IdImmediate> idImmOps;
            for (unsigned int i = 0; i < glslangOperands.size(); ++i) {
                if (glslangOperands[i]->getAsTyped()->getQualifier().isSpirvLiteral()) {
                    // Translate the constant to a literal value
                    std::vector<unsigned> literals;
                    glslang::TVector<const glslang::TIntermConstantUnion*> constants;
                    constants.push_back(glslangOperands[i]->getAsConstantUnion());
                    TranslateLiterals(constants, literals);
                    idImmOps.push_back({false, literals[0]});
                } else
                    idImmOps.push_back({true, operands[i]});
            }

            if (node->getBasicType() == glslang::EbtVoid)
                builder.createNoResultOp(static_cast<spv::Op>(spirvInst.id), idImmOps);
            else
                result = builder.createOp(static_cast<spv::Op>(spirvInst.id), resultType(), idImmOps);
        } else {
            result = builder.createBuiltinCall(
                resultType(), spirvInst.set == "GLSL.std.450" ? stdBuiltins : getExtBuiltins(spirvInst.set.c_str()),
                spirvInst.id, operands);
        }
        noReturnValue = node->getBasicType() == glslang::EbtVoid;
    } else if (node->getOp() == glslang::EOpDebugPrintf) {
        if (!nonSemanticDebugPrintf) {
            nonSemanticDebugPrintf = builder.import("NonSemantic.DebugPrintf");
        }
        result = builder.createBuiltinCall(builder.makeVoidType(), nonSemanticDebugPrintf, spv::NonSemanticDebugPrintfDebugPrintf, operands);
        builder.addExtension(spv::E_SPV_KHR_non_semantic_info);
    } else {
        // Pass through to generic operations.
        switch (glslangOperands.size()) {
        case 0:
            result = createNoArgOperation(node->getOp(), precision, resultType());
            break;
        case 1:
            {
                OpDecorations decorations = { precision,
                                              TranslateNoContractionDecoration(node->getType().getQualifier()),
                                              TranslateNonUniformDecoration(node->getType().getQualifier()) };
                result = createUnaryOperation(
                    node->getOp(), decorations,
                    resultType(), operands.front(),
                    glslangOperands[0]->getAsTyped()->getBasicType(), lvalueCoherentFlags);
            }
            break;
        default:
            result = createMiscOperation(node->getOp(), precision, resultType(), operands, node->getBasicType());
            break;
        }

        if (invertedType != spv::NoResult)
            result = createInvertedSwizzle(precision, *glslangOperands[0]->getAsBinaryNode(), result);

        for (unsigned int i = 0; i < temporaryLvalues.size(); ++i) {
            builder.setAccessChain(complexLvalues[i]);
            builder.accessChainStore(builder.createLoad(temporaryLvalues[i], spv::NoPrecision),
                TranslateNonUniformDecoration(complexLvalues[i].coherentFlags));
        }
    }

    if (noReturnValue)
        return false;

    if (! result) {
        logger->missingFunctionality("unknown glslang aggregate");
        return true;  // pick up a child as a placeholder operand
    } else {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);
        return false;
    }
}

// This path handles both if-then-else and ?:
// The if-then-else has a node type of void, while
// ?: has either a void or a non-void node type
//
// Leaving the result, when not void:
// GLSL only has r-values as the result of a :?, but
// if we have an l-value, that can be more efficient if it will
// become the base of a complex r-value expression, because the
// next layer copies r-values into memory to use the access-chain mechanism
bool TGlslangToSpvTraverser::visitSelection(glslang::TVisit /* visit */, glslang::TIntermSelection* node)
{
    // see if OpSelect can handle it
    const auto isOpSelectable = [&]() {
        if (node->getBasicType() == glslang::EbtVoid)
            return false;
        // OpSelect can do all other types starting with SPV 1.4
        if (glslangIntermediate->getSpv().spv < glslang::EShTargetSpv_1_4) {
            // pre-1.4, only scalars and vectors can be handled
            if ((!node->getType().isScalar() && !node->getType().isVector()))
                return false;
        }
        return true;
    };

    // See if it simple and safe, or required, to execute both sides.
    // Crucially, side effects must be either semantically required or avoided,
    // and there are performance trade-offs.
    // Return true if required or a good idea (and safe) to execute both sides,
    // false otherwise.
    const auto bothSidesPolicy = [&]() -> bool {
        // do we have both sides?
        if (node->getTrueBlock()  == nullptr ||
            node->getFalseBlock() == nullptr)
            return false;

        // required? (unless we write additional code to look for side effects
        // and make performance trade-offs if none are present)
        if (!node->getShortCircuit())
            return true;

        // if not required to execute both, decide based on performance/practicality...

        if (!isOpSelectable())
            return false;

        assert(node->getType() == node->getTrueBlock() ->getAsTyped()->getType() &&
               node->getType() == node->getFalseBlock()->getAsTyped()->getType());

        // return true if a single operand to ? : is okay for OpSelect
        const auto operandOkay = [](glslang::TIntermTyped* node) {
            return node->getAsSymbolNode() || node->getType().getQualifier().isConstant();
        };

        return operandOkay(node->getTrueBlock() ->getAsTyped()) &&
               operandOkay(node->getFalseBlock()->getAsTyped());
    };

    spv::Id result = spv::NoResult; // upcoming result selecting between trueValue and falseValue
    // emit the condition before doing anything with selection
    node->getCondition()->traverse(this);
    spv::Id condition = accessChainLoad(node->getCondition()->getType());

    // Find a way of executing both sides and selecting the right result.
    const auto executeBothSides = [&]() -> void {
        // execute both sides
        spv::Id resultType = convertGlslangToSpvType(node->getType());
        node->getTrueBlock()->traverse(this);
        spv::Id trueValue = accessChainLoad(node->getTrueBlock()->getAsTyped()->getType());
        node->getFalseBlock()->traverse(this);
        spv::Id falseValue = accessChainLoad(node->getFalseBlock()->getAsTyped()->getType());

        builder.setLine(node->getLoc().line, node->getLoc().getFilename());

        // done if void
        if (node->getBasicType() == glslang::EbtVoid)
            return;

        // emit code to select between trueValue and falseValue
        // see if OpSelect can handle the result type, and that the SPIR-V types
        // of the inputs match the result type.
        if (isOpSelectable()) {
            // Emit OpSelect for this selection.

            // smear condition to vector, if necessary (AST is always scalar)
            // Before 1.4, smear like for mix(), starting with 1.4, keep it scalar
            if (glslangIntermediate->getSpv().spv < glslang::EShTargetSpv_1_4 && builder.isVector(trueValue)) {
                condition = builder.smearScalar(spv::NoPrecision, condition,
                                                builder.makeVectorType(builder.makeBoolType(),
                                                                       builder.getNumComponents(trueValue)));
            }

            // If the types do not match, it is because of mismatched decorations on aggregates.
            // Since isOpSelectable only lets us get here for SPIR-V >= 1.4, we can use OpCopyObject
            // to get matching types.
            if (builder.getTypeId(trueValue) != resultType) {
                trueValue = builder.createUnaryOp(spv::OpCopyLogical, resultType, trueValue);
            }
            if (builder.getTypeId(falseValue) != resultType) {
                falseValue = builder.createUnaryOp(spv::OpCopyLogical, resultType, falseValue);
            }

            // OpSelect
            result = builder.createTriOp(spv::OpSelect, resultType, condition, trueValue, falseValue);

            builder.clearAccessChain();
            builder.setAccessChainRValue(result);
        } else {
            // We need control flow to select the result.
            // TODO: Once SPIR-V OpSelect allows arbitrary types, eliminate this path.
            result = builder.createVariable(TranslatePrecisionDecoration(node->getType()),
                spv::StorageClassFunction, resultType);

            // Selection control:
            const spv::SelectionControlMask control = TranslateSelectionControl(*node);

            // make an "if" based on the value created by the condition
            spv::Builder::If ifBuilder(condition, control, builder);

            // emit the "then" statement
            builder.clearAccessChain();
            builder.setAccessChainLValue(result);
            multiTypeStore(node->getType(), trueValue);

            ifBuilder.makeBeginElse();
            // emit the "else" statement
            builder.clearAccessChain();
            builder.setAccessChainLValue(result);
            multiTypeStore(node->getType(), falseValue);

            // finish off the control flow
            ifBuilder.makeEndIf();

            builder.clearAccessChain();
            builder.setAccessChainLValue(result);
        }
    };

    // Execute the one side needed, as per the condition
    const auto executeOneSide = [&]() {
        // Always emit control flow.
        if (node->getBasicType() != glslang::EbtVoid) {
            result = builder.createVariable(TranslatePrecisionDecoration(node->getType()), spv::StorageClassFunction,
                convertGlslangToSpvType(node->getType()));
        }

        // Selection control:
        const spv::SelectionControlMask control = TranslateSelectionControl(*node);

        // make an "if" based on the value created by the condition
        spv::Builder::If ifBuilder(condition, control, builder);

        // emit the "then" statement
        if (node->getTrueBlock() != nullptr) {
            node->getTrueBlock()->traverse(this);
            if (result != spv::NoResult) {
                spv::Id load = accessChainLoad(node->getTrueBlock()->getAsTyped()->getType());

                builder.clearAccessChain();
                builder.setAccessChainLValue(result);
                multiTypeStore(node->getType(), load);
            }
        }

        if (node->getFalseBlock() != nullptr) {
            ifBuilder.makeBeginElse();
            // emit the "else" statement
            node->getFalseBlock()->traverse(this);
            if (result != spv::NoResult) {
                spv::Id load = accessChainLoad(node->getFalseBlock()->getAsTyped()->getType());

                builder.clearAccessChain();
                builder.setAccessChainLValue(result);
                multiTypeStore(node->getType(), load);
            }
        }

        // finish off the control flow
        ifBuilder.makeEndIf();

        if (result != spv::NoResult) {
            builder.clearAccessChain();
            builder.setAccessChainLValue(result);
        }
    };

    // Try for OpSelect (or a requirement to execute both sides)
    if (bothSidesPolicy()) {
        SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
        if (node->getType().getQualifier().isSpecConstant())
            spec_constant_op_mode_setter.turnOnSpecConstantOpMode();
        executeBothSides();
    } else
        executeOneSide();

    return false;
}

bool TGlslangToSpvTraverser::visitSwitch(glslang::TVisit /* visit */, glslang::TIntermSwitch* node)
{
    // emit and get the condition before doing anything with switch
    node->getCondition()->traverse(this);
    spv::Id selector = accessChainLoad(node->getCondition()->getAsTyped()->getType());

    // Selection control:
    const spv::SelectionControlMask control = TranslateSwitchControl(*node);

    // browse the children to sort out code segments
    int defaultSegment = -1;
    std::vector<TIntermNode*> codeSegments;
    glslang::TIntermSequence& sequence = node->getBody()->getSequence();
    std::vector<int> caseValues;
    std::vector<int> valueIndexToSegment(sequence.size());  // note: probably not all are used, it is an overestimate
    for (glslang::TIntermSequence::iterator c = sequence.begin(); c != sequence.end(); ++c) {
        TIntermNode* child = *c;
        if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpDefault)
            defaultSegment = (int)codeSegments.size();
        else if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpCase) {
            valueIndexToSegment[caseValues.size()] = (int)codeSegments.size();
            caseValues.push_back(child->getAsBranchNode()->getExpression()->getAsConstantUnion()
                ->getConstArray()[0].getIConst());
        } else
            codeSegments.push_back(child);
    }

    // handle the case where the last code segment is missing, due to no code
    // statements between the last case and the end of the switch statement
    if ((caseValues.size() && (int)codeSegments.size() == valueIndexToSegment[caseValues.size() - 1]) ||
        (int)codeSegments.size() == defaultSegment)
        codeSegments.push_back(nullptr);

    // make the switch statement
    std::vector<spv::Block*> segmentBlocks; // returned, as the blocks allocated in the call
    builder.makeSwitch(selector, control, (int)codeSegments.size(), caseValues, valueIndexToSegment, defaultSegment,
        segmentBlocks);

    // emit all the code in the segments
    breakForLoop.push(false);
    for (unsigned int s = 0; s < codeSegments.size(); ++s) {
        builder.nextSwitchSegment(segmentBlocks, s);
        if (codeSegments[s])
            codeSegments[s]->traverse(this);
        else
            builder.addSwitchBreak();
    }
    breakForLoop.pop();

    builder.endSwitch(segmentBlocks);

    return false;
}

void TGlslangToSpvTraverser::visitConstantUnion(glslang::TIntermConstantUnion* node)
{
    if (node->getQualifier().isSpirvLiteral())
        return; // Translated to a literal value, skip further processing

    int nextConst = 0;
    spv::Id constant = createSpvConstantFromConstUnionArray(node->getType(), node->getConstArray(), nextConst, false);

    builder.clearAccessChain();
    builder.setAccessChainRValue(constant);
}

bool TGlslangToSpvTraverser::visitLoop(glslang::TVisit /* visit */, glslang::TIntermLoop* node)
{
    auto blocks = builder.makeNewLoop();
    builder.createBranch(&blocks.head);

    // Loop control:
    std::vector<unsigned int> operands;
    const spv::LoopControlMask control = TranslateLoopControl(*node, operands);

    // Spec requires back edges to target header blocks, and every header block
    // must dominate its merge block.  Make a header block first to ensure these
    // conditions are met.  By definition, it will contain OpLoopMerge, followed
    // by a block-ending branch.  But we don't want to put any other body/test
    // instructions in it, since the body/test may have arbitrary instructions,
    // including merges of its own.
    builder.setBuildPoint(&blocks.head);
    builder.setLine(node->getLoc().line, node->getLoc().getFilename());
    builder.createLoopMerge(&blocks.merge, &blocks.continue_target, control, operands);
    if (node->testFirst() && node->getTest()) {
        spv::Block& test = builder.makeNewBlock();
        builder.createBranch(&test);

        builder.setBuildPoint(&test);
        node->getTest()->traverse(this);
        spv::Id condition = accessChainLoad(node->getTest()->getType());
        builder.createConditionalBranch(condition, &blocks.body, &blocks.merge);

        builder.setBuildPoint(&blocks.body);
        breakForLoop.push(true);
        if (node->getBody())
            node->getBody()->traverse(this);
        builder.createBranch(&blocks.continue_target);
        breakForLoop.pop();

        builder.setBuildPoint(&blocks.continue_target);
        if (node->getTerminal())
            node->getTerminal()->traverse(this);
        builder.createBranch(&blocks.head);
    } else {
        builder.setLine(node->getLoc().line, node->getLoc().getFilename());
        builder.createBranch(&blocks.body);

        breakForLoop.push(true);
        builder.setBuildPoint(&blocks.body);
        if (node->getBody())
            node->getBody()->traverse(this);
        builder.createBranch(&blocks.continue_target);
        breakForLoop.pop();

        builder.setBuildPoint(&blocks.continue_target);
        if (node->getTerminal())
            node->getTerminal()->traverse(this);
        if (node->getTest()) {
            node->getTest()->traverse(this);
            spv::Id condition =
                accessChainLoad(node->getTest()->getType());
            builder.createConditionalBranch(condition, &blocks.head, &blocks.merge);
        } else {
            // TODO: unless there was a break/return/discard instruction
            // somewhere in the body, this is an infinite loop, so we should
            // issue a warning.
            builder.createBranch(&blocks.head);
        }
    }
    builder.setBuildPoint(&blocks.merge);
    builder.closeLoop();
    return false;
}

bool TGlslangToSpvTraverser::visitBranch(glslang::TVisit /* visit */, glslang::TIntermBranch* node)
{
    if (node->getExpression())
        node->getExpression()->traverse(this);

    builder.setLine(node->getLoc().line, node->getLoc().getFilename());

    switch (node->getFlowOp()) {
    case glslang::EOpKill:
        if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_6) {
            if (glslangIntermediate->getSource() == glslang::EShSourceHlsl) {
              builder.addCapability(spv::CapabilityDemoteToHelperInvocation);
              builder.createNoResultOp(spv::OpDemoteToHelperInvocationEXT);
            } else {
                builder.makeStatementTerminator(spv::OpTerminateInvocation, "post-terminate-invocation");
            }
        } else {
            builder.makeStatementTerminator(spv::OpKill, "post-discard");
        }
        break;
    case glslang::EOpTerminateInvocation:
        builder.addExtension(spv::E_SPV_KHR_terminate_invocation);
        builder.makeStatementTerminator(spv::OpTerminateInvocation, "post-terminate-invocation");
        break;
    case glslang::EOpBreak:
        if (breakForLoop.top())
            builder.createLoopExit();
        else
            builder.addSwitchBreak();
        break;
    case glslang::EOpContinue:
        builder.createLoopContinue();
        break;
    case glslang::EOpReturn:
        if (node->getExpression() != nullptr) {
            const glslang::TType& glslangReturnType = node->getExpression()->getType();
            spv::Id returnId = accessChainLoad(glslangReturnType);
            if (builder.getTypeId(returnId) != currentFunction->getReturnType() ||
                TranslatePrecisionDecoration(glslangReturnType) != currentFunction->getReturnPrecision()) {
                builder.clearAccessChain();
                spv::Id copyId = builder.createVariable(currentFunction->getReturnPrecision(),
                    spv::StorageClassFunction, currentFunction->getReturnType());
                builder.setAccessChainLValue(copyId);
                multiTypeStore(glslangReturnType, returnId);
                returnId = builder.createLoad(copyId, currentFunction->getReturnPrecision());
            }
            builder.makeReturn(false, returnId);
        } else
            builder.makeReturn(false);

        builder.clearAccessChain();
        break;

    case glslang::EOpDemote:
        builder.createNoResultOp(spv::OpDemoteToHelperInvocationEXT);
        builder.addExtension(spv::E_SPV_EXT_demote_to_helper_invocation);
        builder.addCapability(spv::CapabilityDemoteToHelperInvocationEXT);
        break;
    case glslang::EOpTerminateRayKHR:
        builder.makeStatementTerminator(spv::OpTerminateRayKHR, "post-terminateRayKHR");
        break;
    case glslang::EOpIgnoreIntersectionKHR:
        builder.makeStatementTerminator(spv::OpIgnoreIntersectionKHR, "post-ignoreIntersectionKHR");
        break;

    default:
        assert(0);
        break;
    }

    return false;
}

spv::Id TGlslangToSpvTraverser::createSpvVariable(const glslang::TIntermSymbol* node, spv::Id forcedType)
{
    // First, steer off constants, which are not SPIR-V variables, but
    // can still have a mapping to a SPIR-V Id.
    // This includes specialization constants.
    if (node->getQualifier().isConstant()) {
        spv::Id result = createSpvConstant(*node);
        if (result != spv::NoResult)
            return result;
    }

    // Now, handle actual variables
    spv::StorageClass storageClass = TranslateStorageClass(node->getType());
    spv::Id spvType = forcedType == spv::NoType ? convertGlslangToSpvType(node->getType())
                                                : forcedType;

    const bool contains16BitType = node->getType().contains16BitFloat() ||
                                   node->getType().contains16BitInt();
    if (contains16BitType) {
        switch (storageClass) {
        case spv::StorageClassInput:
        case spv::StorageClassOutput:
            builder.addIncorporatedExtension(spv::E_SPV_KHR_16bit_storage, spv::Spv_1_3);
            builder.addCapability(spv::CapabilityStorageInputOutput16);
            break;
        case spv::StorageClassUniform:
            builder.addIncorporatedExtension(spv::E_SPV_KHR_16bit_storage, spv::Spv_1_3);
            if (node->getType().getQualifier().storage == glslang::EvqBuffer)
                builder.addCapability(spv::CapabilityStorageUniformBufferBlock16);
            else
                builder.addCapability(spv::CapabilityStorageUniform16);
            break;
        case spv::StorageClassPushConstant:
            builder.addIncorporatedExtension(spv::E_SPV_KHR_16bit_storage, spv::Spv_1_3);
            builder.addCapability(spv::CapabilityStoragePushConstant16);
            break;
        case spv::StorageClassStorageBuffer:
        case spv::StorageClassPhysicalStorageBufferEXT:
            builder.addIncorporatedExtension(spv::E_SPV_KHR_16bit_storage, spv::Spv_1_3);
            builder.addCapability(spv::CapabilityStorageUniformBufferBlock16);
            break;
        default:
            if (storageClass == spv::StorageClassWorkgroup &&
                node->getType().getBasicType() == glslang::EbtBlock) {
                builder.addCapability(spv::CapabilityWorkgroupMemoryExplicitLayout16BitAccessKHR);
                break;
            }
            if (node->getType().contains16BitFloat())
                builder.addCapability(spv::CapabilityFloat16);
            if (node->getType().contains16BitInt())
                builder.addCapability(spv::CapabilityInt16);
            break;
        }
    }

    if (node->getType().contains8BitInt()) {
        if (storageClass == spv::StorageClassPushConstant) {
            builder.addIncorporatedExtension(spv::E_SPV_KHR_8bit_storage, spv::Spv_1_5);
            builder.addCapability(spv::CapabilityStoragePushConstant8);
        } else if (storageClass == spv::StorageClassUniform) {
            builder.addIncorporatedExtension(spv::E_SPV_KHR_8bit_storage, spv::Spv_1_5);
            builder.addCapability(spv::CapabilityUniformAndStorageBuffer8BitAccess);
        } else if (storageClass == spv::StorageClassStorageBuffer) {
            builder.addIncorporatedExtension(spv::E_SPV_KHR_8bit_storage, spv::Spv_1_5);
            builder.addCapability(spv::CapabilityStorageBuffer8BitAccess);
        } else if (storageClass == spv::StorageClassWorkgroup &&
                   node->getType().getBasicType() == glslang::EbtBlock) {
            builder.addCapability(spv::CapabilityWorkgroupMemoryExplicitLayout8BitAccessKHR);
        } else {
            builder.addCapability(spv::CapabilityInt8);
        }
    }

    const char* name = node->getName().c_str();
    if (glslang::IsAnonymous(name))
        name = "";

    spv::Id initializer = spv::NoResult;

    if (node->getType().getQualifier().storage == glslang::EvqUniform && !node->getConstArray().empty()) {
        int nextConst = 0;
        initializer = createSpvConstantFromConstUnionArray(node->getType(),
                                                           node->getConstArray(),
                                                           nextConst,
                                                           false /* specConst */);
    } else if (node->getType().getQualifier().isNullInit()) {
        initializer = builder.makeNullConstant(spvType);
    }

    return builder.createVariable(spv::NoPrecision, storageClass, spvType, name, initializer, false);
}

// Return type Id of the sampled type.
spv::Id TGlslangToSpvTraverser::getSampledType(const glslang::TSampler& sampler)
{
    switch (sampler.type) {
        case glslang::EbtInt:      return builder.makeIntType(32);
        case glslang::EbtUint:     return builder.makeUintType(32);
        case glslang::EbtFloat:    return builder.makeFloatType(32);
        case glslang::EbtFloat16:
            builder.addExtension(spv::E_SPV_AMD_gpu_shader_half_float_fetch);
            builder.addCapability(spv::CapabilityFloat16ImageAMD);
            return builder.makeFloatType(16);
        case glslang::EbtInt64:
            builder.addExtension(spv::E_SPV_EXT_shader_image_int64);
            builder.addCapability(spv::CapabilityInt64ImageEXT);
            return builder.makeIntType(64);
        case glslang::EbtUint64:
            builder.addExtension(spv::E_SPV_EXT_shader_image_int64);
            builder.addCapability(spv::CapabilityInt64ImageEXT);
            return builder.makeUintType(64);
        default:
            assert(0);
            return builder.makeFloatType(32);
    }
}

// If node is a swizzle operation, return the type that should be used if
// the swizzle base is first consumed by another operation, before the swizzle
// is applied.
spv::Id TGlslangToSpvTraverser::getInvertedSwizzleType(const glslang::TIntermTyped& node)
{
    if (node.getAsOperator() &&
        node.getAsOperator()->getOp() == glslang::EOpVectorSwizzle)
        return convertGlslangToSpvType(node.getAsBinaryNode()->getLeft()->getType());
    else
        return spv::NoType;
}

// When inverting a swizzle with a parent op, this function
// will apply the swizzle operation to a completed parent operation.
spv::Id TGlslangToSpvTraverser::createInvertedSwizzle(spv::Decoration precision, const glslang::TIntermTyped& node,
    spv::Id parentResult)
{
    std::vector<unsigned> swizzle;
    convertSwizzle(*node.getAsBinaryNode()->getRight()->getAsAggregate(), swizzle);
    return builder.createRvalueSwizzle(precision, convertGlslangToSpvType(node.getType()), parentResult, swizzle);
}

// Convert a glslang AST swizzle node to a swizzle vector for building SPIR-V.
void TGlslangToSpvTraverser::convertSwizzle(const glslang::TIntermAggregate& node, std::vector<unsigned>& swizzle)
{
    const glslang::TIntermSequence& swizzleSequence = node.getSequence();
    for (int i = 0; i < (int)swizzleSequence.size(); ++i)
        swizzle.push_back(swizzleSequence[i]->getAsConstantUnion()->getConstArray()[0].getIConst());
}

// Convert from a glslang type to an SPV type, by calling into a
// recursive version of this function. This establishes the inherited
// layout state rooted from the top-level type.
spv::Id TGlslangToSpvTraverser::convertGlslangToSpvType(const glslang::TType& type, bool forwardReferenceOnly)
{
    return convertGlslangToSpvType(type, getExplicitLayout(type), type.getQualifier(), false, forwardReferenceOnly);
}

// Do full recursive conversion of an arbitrary glslang type to a SPIR-V Id.
// explicitLayout can be kept the same throughout the hierarchical recursive walk.
// Mutually recursive with convertGlslangStructToSpvType().
spv::Id TGlslangToSpvTraverser::convertGlslangToSpvType(const glslang::TType& type,
    glslang::TLayoutPacking explicitLayout, const glslang::TQualifier& qualifier,
    bool lastBufferBlockMember, bool forwardReferenceOnly)
{
    spv::Id spvType = spv::NoResult;

    switch (type.getBasicType()) {
    case glslang::EbtVoid:
        spvType = builder.makeVoidType();
        assert (! type.isArray());
        break;
    case glslang::EbtBool:
        // "transparent" bool doesn't exist in SPIR-V.  The GLSL convention is
        // a 32-bit int where non-0 means true.
        if (explicitLayout != glslang::ElpNone)
            spvType = builder.makeUintType(32);
        else
            spvType = builder.makeBoolType(false);
        break;
    case glslang::EbtInt:
        spvType = builder.makeIntType(32);
        break;
    case glslang::EbtUint:
        spvType = builder.makeUintType(32);
        break;
    case glslang::EbtFloat:
        spvType = builder.makeFloatType(32);
        break;
    case glslang::EbtDouble:
        spvType = builder.makeFloatType(64);
        break;
    case glslang::EbtFloat16:
        spvType = builder.makeFloatType(16);
        break;
    case glslang::EbtInt8:
        spvType = builder.makeIntType(8);
        break;
    case glslang::EbtUint8:
        spvType = builder.makeUintType(8);
        break;
    case glslang::EbtInt16:
        spvType = builder.makeIntType(16);
        break;
    case glslang::EbtUint16:
        spvType = builder.makeUintType(16);
        break;
    case glslang::EbtInt64:
        spvType = builder.makeIntType(64);
        break;
    case glslang::EbtUint64:
        spvType = builder.makeUintType(64);
        break;
    case glslang::EbtAtomicUint:
        builder.addCapability(spv::CapabilityAtomicStorage);
        spvType = builder.makeUintType(32);
        break;
    case glslang::EbtAccStruct:
        switch (glslangIntermediate->getStage()) {
        case EShLangRayGen:
        case EShLangIntersect:
        case EShLangAnyHit:
        case EShLangClosestHit:
        case EShLangMiss:
        case EShLangCallable:
            // these all should have the RayTracingNV/KHR capability already
            break;
        default:
            {
                auto& extensions = glslangIntermediate->getRequestedExtensions();
                if (extensions.find("GL_EXT_ray_query") != extensions.end()) {
                    builder.addExtension(spv::E_SPV_KHR_ray_query);
                    builder.addCapability(spv::CapabilityRayQueryKHR);
                }
            }
            break;
        }
        spvType = builder.makeAccelerationStructureType();
        break;
    case glslang::EbtRayQuery:
        {
            auto& extensions = glslangIntermediate->getRequestedExtensions();
            if (extensions.find("GL_EXT_ray_query") != extensions.end()) {
                builder.addExtension(spv::E_SPV_KHR_ray_query);
                builder.addCapability(spv::CapabilityRayQueryKHR);
            }
            spvType = builder.makeRayQueryType();
        }
        break;
    case glslang::EbtReference:
        {
            // Make the forward pointer, then recurse to convert the structure type, then
            // patch up the forward pointer with a real pointer type.
            if (forwardPointers.find(type.getReferentType()) == forwardPointers.end()) {
                spv::Id forwardId = builder.makeForwardPointer(spv::StorageClassPhysicalStorageBufferEXT);
                forwardPointers[type.getReferentType()] = forwardId;
            }
            spvType = forwardPointers[type.getReferentType()];
            if (!forwardReferenceOnly) {
                spv::Id referentType = convertGlslangToSpvType(*type.getReferentType());
                builder.makePointerFromForwardPointer(spv::StorageClassPhysicalStorageBufferEXT,
                                                      forwardPointers[type.getReferentType()],
                                                      referentType);
            }
        }
        break;
    case glslang::EbtSampler:
        {
            const glslang::TSampler& sampler = type.getSampler();
            if (sampler.isPureSampler()) {
                spvType = builder.makeSamplerType();
            } else {
                // an image is present, make its type
                spvType = builder.makeImageType(getSampledType(sampler), TranslateDimensionality(sampler),
                                                sampler.isShadow(), sampler.isArrayed(), sampler.isMultiSample(),
                                                sampler.isImageClass() ? 2 : 1, TranslateImageFormat(type));
                if (sampler.isCombined() &&
                    (!sampler.isBuffer() || glslangIntermediate->getSpv().spv < glslang::EShTargetSpv_1_6)) {
                    // Already has both image and sampler, make the combined type. Only combine sampler to
                    // buffer if before SPIR-V 1.6.
                    spvType = builder.makeSampledImageType(spvType);
                }
            }
        }
        break;
    case glslang::EbtStruct:
    case glslang::EbtBlock:
        {
            // If we've seen this struct type, return it
            const glslang::TTypeList* glslangMembers = type.getStruct();

            // Try to share structs for different layouts, but not yet for other
            // kinds of qualification (primarily not yet including interpolant qualification).
            if (! HasNonLayoutQualifiers(type, qualifier))
                spvType = structMap[explicitLayout][qualifier.layoutMatrix][glslangMembers];
            if (spvType != spv::NoResult)
                break;

            // else, we haven't seen it...
            if (type.getBasicType() == glslang::EbtBlock)
                memberRemapper[glslangTypeToIdMap[glslangMembers]].resize(glslangMembers->size());
            spvType = convertGlslangStructToSpvType(type, glslangMembers, explicitLayout, qualifier);
        }
        break;
    case glslang::EbtString:
        // no type used for OpString
        return 0;

    case glslang::EbtHitObjectNV: {
        builder.addExtension(spv::E_SPV_NV_shader_invocation_reorder);
        builder.addCapability(spv::CapabilityShaderInvocationReorderNV);
        spvType = builder.makeHitObjectNVType();
    }
    break;
    case glslang::EbtSpirvType: {
        // GL_EXT_spirv_intrinsics
        const auto& spirvType = type.getSpirvType();
        const auto& spirvInst = spirvType.spirvInst;

        std::vector<spv::IdImmediate> operands;
        for (const auto& typeParam : spirvType.typeParams) {
            if (typeParam.constant != nullptr) {
                // Constant expression
                if (typeParam.constant->isLiteral()) {
                    if (typeParam.constant->getBasicType() == glslang::EbtFloat) {
                        float floatValue = static_cast<float>(typeParam.constant->getConstArray()[0].getDConst());
                        unsigned literal;
                        static_assert(sizeof(literal) == sizeof(floatValue), "sizeof(unsigned) != sizeof(float)");
                        memcpy(&literal, &floatValue, sizeof(literal));
                        operands.push_back({false, literal});
                    } else if (typeParam.constant->getBasicType() == glslang::EbtInt) {
                        unsigned literal = typeParam.constant->getConstArray()[0].getIConst();
                        operands.push_back({false, literal});
                    } else if (typeParam.constant->getBasicType() == glslang::EbtUint) {
                        unsigned literal = typeParam.constant->getConstArray()[0].getUConst();
                        operands.push_back({false, literal});
                    } else if (typeParam.constant->getBasicType() == glslang::EbtBool) {
                        unsigned literal = typeParam.constant->getConstArray()[0].getBConst();
                        operands.push_back({false, literal});
                    } else if (typeParam.constant->getBasicType() == glslang::EbtString) {
                        auto str = typeParam.constant->getConstArray()[0].getSConst()->c_str();
                        unsigned literal = 0;
                        char* literalPtr = reinterpret_cast<char*>(&literal);
                        unsigned charCount = 0;
                        char ch = 0;
                        do {
                            ch = *(str++);
                            *(literalPtr++) = ch;
                            ++charCount;
                            if (charCount == 4) {
                                operands.push_back({false, literal});
                                literalPtr = reinterpret_cast<char*>(&literal);
                                charCount = 0;
                            }
                        } while (ch != 0);

                        // Partial literal is padded with 0
                        if (charCount > 0) {
                            for (; charCount < 4; ++charCount)
                                *(literalPtr++) = 0;
                            operands.push_back({false, literal});
                        }
                    } else
                        assert(0); // Unexpected type
                } else
                    operands.push_back({true, createSpvConstant(*typeParam.constant)});
            } else {
                // Type specifier
                assert(typeParam.type != nullptr);
                operands.push_back({true, convertGlslangToSpvType(*typeParam.type)});
            }
        }

        assert(spirvInst.set == ""); // Currently, couldn't be extended instructions.
        spvType = builder.makeGenericType(static_cast<spv::Op>(spirvInst.id), operands);

        break;
    }
    default:
        assert(0);
        break;
    }

    if (type.isMatrix())
        spvType = builder.makeMatrixType(spvType, type.getMatrixCols(), type.getMatrixRows());
    else {
        // If this variable has a vector element count greater than 1, create a SPIR-V vector
        if (type.getVectorSize() > 1)
            spvType = builder.makeVectorType(spvType, type.getVectorSize());
    }

    if (type.isCoopMatNV()) {
        builder.addCapability(spv::CapabilityCooperativeMatrixNV);
        builder.addExtension(spv::E_SPV_NV_cooperative_matrix);

        if (type.getBasicType() == glslang::EbtFloat16)
            builder.addCapability(spv::CapabilityFloat16);
        if (type.getBasicType() == glslang::EbtUint8 ||
            type.getBasicType() == glslang::EbtInt8) {
            builder.addCapability(spv::CapabilityInt8);
        }

        spv::Id scope = makeArraySizeId(*type.getTypeParameters()->arraySizes, 1);
        spv::Id rows = makeArraySizeId(*type.getTypeParameters()->arraySizes, 2);
        spv::Id cols = makeArraySizeId(*type.getTypeParameters()->arraySizes, 3);

        spvType = builder.makeCooperativeMatrixTypeNV(spvType, scope, rows, cols);
    }

    if (type.isCoopMatKHR()) {
        builder.addCapability(spv::CapabilityCooperativeMatrixKHR);
        builder.addExtension(spv::E_SPV_KHR_cooperative_matrix);

        if (type.getBasicType() == glslang::EbtFloat16)
            builder.addCapability(spv::CapabilityFloat16);
        if (type.getBasicType() == glslang::EbtUint8 || type.getBasicType() == glslang::EbtInt8) {
            builder.addCapability(spv::CapabilityInt8);
        }

        spv::Id scope = makeArraySizeId(*type.getTypeParameters()->arraySizes, 0);
        spv::Id rows = makeArraySizeId(*type.getTypeParameters()->arraySizes, 1);
        spv::Id cols = makeArraySizeId(*type.getTypeParameters()->arraySizes, 2);
        spv::Id use = builder.makeUintConstant(type.getCoopMatKHRuse());

        spvType = builder.makeCooperativeMatrixTypeKHR(spvType, scope, rows, cols, use);
    }

    if (type.isArray()) {
        int stride = 0;  // keep this 0 unless doing an explicit layout; 0 will mean no decoration, no stride

        // Do all but the outer dimension
        if (type.getArraySizes()->getNumDims() > 1) {
            // We need to decorate array strides for types needing explicit layout, except blocks.
            if (explicitLayout != glslang::ElpNone && type.getBasicType() != glslang::EbtBlock) {
                // Use a dummy glslang type for querying internal strides of
                // arrays of arrays, but using just a one-dimensional array.
                glslang::TType simpleArrayType(type, 0); // deference type of the array
                while (simpleArrayType.getArraySizes()->getNumDims() > 1)
                    simpleArrayType.getArraySizes()->dereference();

                // Will compute the higher-order strides here, rather than making a whole
                // pile of types and doing repetitive recursion on their contents.
                stride = getArrayStride(simpleArrayType, explicitLayout, qualifier.layoutMatrix);
            }

            // make the arrays
            for (int dim = type.getArraySizes()->getNumDims() - 1; dim > 0; --dim) {
                spvType = builder.makeArrayType(spvType, makeArraySizeId(*type.getArraySizes(), dim), stride);
                if (stride > 0)
                    builder.addDecoration(spvType, spv::DecorationArrayStride, stride);
                stride *= type.getArraySizes()->getDimSize(dim);
            }
        } else {
            // single-dimensional array, and don't yet have stride

            // We need to decorate array strides for types needing explicit layout, except blocks.
            if (explicitLayout != glslang::ElpNone && type.getBasicType() != glslang::EbtBlock)
                stride = getArrayStride(type, explicitLayout, qualifier.layoutMatrix);
        }

        // Do the outer dimension, which might not be known for a runtime-sized array.
        // (Unsized arrays that survive through linking will be runtime-sized arrays)
        if (type.isSizedArray())
            spvType = builder.makeArrayType(spvType, makeArraySizeId(*type.getArraySizes(), 0), stride);
        else {
            if (!lastBufferBlockMember) {
                builder.addIncorporatedExtension("SPV_EXT_descriptor_indexing", spv::Spv_1_5);
                builder.addCapability(spv::CapabilityRuntimeDescriptorArrayEXT);
            }
            spvType = builder.makeRuntimeArray(spvType);
        }
        if (stride > 0)
            builder.addDecoration(spvType, spv::DecorationArrayStride, stride);
    }

    return spvType;
}

// TODO: this functionality should exist at a higher level, in creating the AST
//
// Identify interface members that don't have their required extension turned on.
//
bool TGlslangToSpvTraverser::filterMember(const glslang::TType& member)
{
    auto& extensions = glslangIntermediate->getRequestedExtensions();

    if (member.getFieldName() == "gl_SecondaryViewportMaskNV" &&
        extensions.find("GL_NV_stereo_view_rendering") == extensions.end())
        return true;
    if (member.getFieldName() == "gl_SecondaryPositionNV" &&
        extensions.find("GL_NV_stereo_view_rendering") == extensions.end())
        return true;

    if (glslangIntermediate->getStage() != EShLangMesh) {
        if (member.getFieldName() == "gl_ViewportMask" &&
            extensions.find("GL_NV_viewport_array2") == extensions.end())
            return true;
        if (member.getFieldName() == "gl_PositionPerViewNV" &&
            extensions.find("GL_NVX_multiview_per_view_attributes") == extensions.end())
            return true;
        if (member.getFieldName() == "gl_ViewportMaskPerViewNV" &&
            extensions.find("GL_NVX_multiview_per_view_attributes") == extensions.end())
            return true;
    }

    return false;
};

// Do full recursive conversion of a glslang structure (or block) type to a SPIR-V Id.
// explicitLayout can be kept the same throughout the hierarchical recursive walk.
// Mutually recursive with convertGlslangToSpvType().
spv::Id TGlslangToSpvTraverser::convertGlslangStructToSpvType(const glslang::TType& type,
                                                              const glslang::TTypeList* glslangMembers,
                                                              glslang::TLayoutPacking explicitLayout,
                                                              const glslang::TQualifier& qualifier)
{
    // Create a vector of struct types for SPIR-V to consume
    std::vector<spv::Id> spvMembers;
    int memberDelta = 0;  // how much the member's index changes from glslang to SPIR-V, normally 0,
                          // except sometimes for blocks
    std::vector<std::pair<glslang::TType*, glslang::TQualifier> > deferredForwardPointers;
    for (int i = 0; i < (int)glslangMembers->size(); i++) {
        auto& glslangMember = (*glslangMembers)[i];
        if (glslangMember.type->hiddenMember()) {
            ++memberDelta;
            if (type.getBasicType() == glslang::EbtBlock)
                memberRemapper[glslangTypeToIdMap[glslangMembers]][i] = -1;
        } else {
            if (type.getBasicType() == glslang::EbtBlock) {
                if (filterMember(*glslangMember.type)) {
                    memberDelta++;
                    memberRemapper[glslangTypeToIdMap[glslangMembers]][i] = -1;
                    continue;
                }
                memberRemapper[glslangTypeToIdMap[glslangMembers]][i] = i - memberDelta;
            }
            // modify just this child's view of the qualifier
            glslang::TQualifier memberQualifier = glslangMember.type->getQualifier();
            InheritQualifiers(memberQualifier, qualifier);

            // manually inherit location
            if (! memberQualifier.hasLocation() && qualifier.hasLocation())
                memberQualifier.layoutLocation = qualifier.layoutLocation;

            // recurse
            bool lastBufferBlockMember = qualifier.storage == glslang::EvqBuffer &&
                                         i == (int)glslangMembers->size() - 1;

            // Make forward pointers for any pointer members.
            if (glslangMember.type->isReference() &&
                forwardPointers.find(glslangMember.type->getReferentType()) == forwardPointers.end()) {
                deferredForwardPointers.push_back(std::make_pair(glslangMember.type, memberQualifier));
            }

            // Create the member type.
            auto const spvMember = convertGlslangToSpvType(*glslangMember.type, explicitLayout, memberQualifier, lastBufferBlockMember,
                glslangMember.type->isReference());
            spvMembers.push_back(spvMember);

            // Update the builder with the type's location so that we can create debug types for the structure members.
            // There doesn't exist a "clean" entry point for this information to be passed along to the builder so, for now,
            // it is stored in the builder and consumed during the construction of composite debug types.
            // TODO: This probably warrants further investigation. This approach was decided to be the least ugly of the
            // quick and dirty approaches that were tried.
            // Advantages of this approach:
            //  + Relatively clean. No direct calls into debug type system.
            //  + Handles nested recursive structures.
            // Disadvantages of this approach:
            //  + Not as clean as desired. Traverser queries/sets persistent state. This is fragile.
            //  + Table lookup during creation of composite debug types. This really shouldn't be necessary.
            if(options.emitNonSemanticShaderDebugInfo) {
                builder.debugTypeLocs[spvMember].name = glslangMember.type->getFieldName().c_str();
                builder.debugTypeLocs[spvMember].line = glslangMember.loc.line;
                builder.debugTypeLocs[spvMember].column = glslangMember.loc.column;
            }
        }
    }

    // Make the SPIR-V type
    spv::Id spvType = builder.makeStructType(spvMembers, type.getTypeName().c_str(), false);
    if (! HasNonLayoutQualifiers(type, qualifier))
        structMap[explicitLayout][qualifier.layoutMatrix][glslangMembers] = spvType;

    // Decorate it
    decorateStructType(type, glslangMembers, explicitLayout, qualifier, spvType, spvMembers);

    for (int i = 0; i < (int)deferredForwardPointers.size(); ++i) {
        auto it = deferredForwardPointers[i];
        convertGlslangToSpvType(*it.first, explicitLayout, it.second, false);
    }

    return spvType;
}

void TGlslangToSpvTraverser::decorateStructType(const glslang::TType& type,
                                                const glslang::TTypeList* glslangMembers,
                                                glslang::TLayoutPacking explicitLayout,
                                                const glslang::TQualifier& qualifier,
                                                spv::Id spvType,
                                                const std::vector<spv::Id>& spvMembers)
{
    // Name and decorate the non-hidden members
    int offset = -1;
    bool memberLocationInvalid = type.isArrayOfArrays() ||
        (type.isArray() && (type.getQualifier().isArrayedIo(glslangIntermediate->getStage()) == false));
    for (int i = 0; i < (int)glslangMembers->size(); i++) {
        glslang::TType& glslangMember = *(*glslangMembers)[i].type;
        int member = i;
        if (type.getBasicType() == glslang::EbtBlock) {
            member = memberRemapper[glslangTypeToIdMap[glslangMembers]][i];
            if (filterMember(glslangMember))
                continue;
        }

        // modify just this child's view of the qualifier
        glslang::TQualifier memberQualifier = glslangMember.getQualifier();
        InheritQualifiers(memberQualifier, qualifier);

        // using -1 above to indicate a hidden member
        if (member < 0)
            continue;

        builder.addMemberName(spvType, member, glslangMember.getFieldName().c_str());
        builder.addMemberDecoration(spvType, member,
                                    TranslateLayoutDecoration(glslangMember, memberQualifier.layoutMatrix));
        builder.addMemberDecoration(spvType, member, TranslatePrecisionDecoration(glslangMember));
        // Add interpolation and auxiliary storage decorations only to
        // top-level members of Input and Output storage classes
        if (type.getQualifier().storage == glslang::EvqVaryingIn ||
            type.getQualifier().storage == glslang::EvqVaryingOut) {
            if (type.getBasicType() == glslang::EbtBlock ||
                glslangIntermediate->getSource() == glslang::EShSourceHlsl) {
                builder.addMemberDecoration(spvType, member, TranslateInterpolationDecoration(memberQualifier));
                builder.addMemberDecoration(spvType, member, TranslateAuxiliaryStorageDecoration(memberQualifier));
                addMeshNVDecoration(spvType, member, memberQualifier);
            }
        }
        builder.addMemberDecoration(spvType, member, TranslateInvariantDecoration(memberQualifier));

        if (type.getBasicType() == glslang::EbtBlock &&
            qualifier.storage == glslang::EvqBuffer) {
            // Add memory decorations only to top-level members of shader storage block
            std::vector<spv::Decoration> memory;
            TranslateMemoryDecoration(memberQualifier, memory, glslangIntermediate->usingVulkanMemoryModel());
            for (unsigned int i = 0; i < memory.size(); ++i)
                builder.addMemberDecoration(spvType, member, memory[i]);
        }

        // Location assignment was already completed correctly by the front end,
        // just track whether a member needs to be decorated.
        // Ignore member locations if the container is an array, as that's
        // ill-specified and decisions have been made to not allow this.
        if (!memberLocationInvalid && memberQualifier.hasLocation())
            builder.addMemberDecoration(spvType, member, spv::DecorationLocation, memberQualifier.layoutLocation);

        // component, XFB, others
        if (glslangMember.getQualifier().hasComponent())
            builder.addMemberDecoration(spvType, member, spv::DecorationComponent,
                                        glslangMember.getQualifier().layoutComponent);
        if (glslangMember.getQualifier().hasXfbOffset())
            builder.addMemberDecoration(spvType, member, spv::DecorationOffset,
                                        glslangMember.getQualifier().layoutXfbOffset);
        else if (explicitLayout != glslang::ElpNone) {
            // figure out what to do with offset, which is accumulating
            int nextOffset;
            updateMemberOffset(type, glslangMember, offset, nextOffset, explicitLayout, memberQualifier.layoutMatrix);
            if (offset >= 0)
                builder.addMemberDecoration(spvType, member, spv::DecorationOffset, offset);
            offset = nextOffset;
        }

        if (glslangMember.isMatrix() && explicitLayout != glslang::ElpNone)
            builder.addMemberDecoration(spvType, member, spv::DecorationMatrixStride,
                                        getMatrixStride(glslangMember, explicitLayout, memberQualifier.layoutMatrix));

        // built-in variable decorations
        spv::BuiltIn builtIn = TranslateBuiltInDecoration(glslangMember.getQualifier().builtIn, true);
        if (builtIn != spv::BuiltInMax)
            builder.addMemberDecoration(spvType, member, spv::DecorationBuiltIn, (int)builtIn);

        // nonuniform
        builder.addMemberDecoration(spvType, member, TranslateNonUniformDecoration(glslangMember.getQualifier()));

        if (glslangIntermediate->getHlslFunctionality1() && memberQualifier.semanticName != nullptr) {
            builder.addExtension("SPV_GOOGLE_hlsl_functionality1");
            builder.addMemberDecoration(spvType, member, (spv::Decoration)spv::DecorationHlslSemanticGOOGLE,
                                        memberQualifier.semanticName);
        }

        if (builtIn == spv::BuiltInLayer) {
            // SPV_NV_viewport_array2 extension
            if (glslangMember.getQualifier().layoutViewportRelative){
                builder.addMemberDecoration(spvType, member, (spv::Decoration)spv::DecorationViewportRelativeNV);
                builder.addCapability(spv::CapabilityShaderViewportMaskNV);
                builder.addExtension(spv::E_SPV_NV_viewport_array2);
            }
            if (glslangMember.getQualifier().layoutSecondaryViewportRelativeOffset != -2048){
                builder.addMemberDecoration(spvType, member,
                                            (spv::Decoration)spv::DecorationSecondaryViewportRelativeNV,
                                            glslangMember.getQualifier().layoutSecondaryViewportRelativeOffset);
                builder.addCapability(spv::CapabilityShaderStereoViewNV);
                builder.addExtension(spv::E_SPV_NV_stereo_view_rendering);
            }
        }
        if (glslangMember.getQualifier().layoutPassthrough) {
            builder.addMemberDecoration(spvType, member, (spv::Decoration)spv::DecorationPassthroughNV);
            builder.addCapability(spv::CapabilityGeometryShaderPassthroughNV);
            builder.addExtension(spv::E_SPV_NV_geometry_shader_passthrough);
        }

        //
        // Add SPIR-V decorations for members (GL_EXT_spirv_intrinsics)
        //
        if (glslangMember.getQualifier().hasSprivDecorate()) {
            const glslang::TSpirvDecorate& spirvDecorate = glslangMember.getQualifier().getSpirvDecorate();

            // Add spirv_decorate
            for (auto& decorate : spirvDecorate.decorates) {
                if (!decorate.second.empty()) {
                    std::vector<unsigned> literals;
                    TranslateLiterals(decorate.second, literals);
                    builder.addMemberDecoration(spvType, member, static_cast<spv::Decoration>(decorate.first), literals);
                }
                else
                    builder.addMemberDecoration(spvType, member, static_cast<spv::Decoration>(decorate.first));
            }

            // spirv_decorate_id not applied to members
            assert(spirvDecorate.decorateIds.empty());

            // Add spirv_decorate_string
            for (auto& decorateString : spirvDecorate.decorateStrings) {
                std::vector<const char*> strings;
                assert(!decorateString.second.empty());
                for (auto extraOperand : decorateString.second) {
                    const char* string = extraOperand->getConstArray()[0].getSConst()->c_str();
                    strings.push_back(string);
                }
                builder.addDecoration(spvType, static_cast<spv::Decoration>(decorateString.first), strings);
            }
        }
    }

    // Decorate the structure
    builder.addDecoration(spvType, TranslateLayoutDecoration(type, qualifier.layoutMatrix));
    const auto basicType = type.getBasicType();
    const auto typeStorageQualifier = type.getQualifier().storage;
    if (basicType == glslang::EbtBlock) {
        builder.addDecoration(spvType, TranslateBlockDecoration(typeStorageQualifier, glslangIntermediate->usingStorageBuffer()));
    } else if (basicType == glslang::EbtStruct && glslangIntermediate->getSpv().vulkan > 0) {
        const auto hasRuntimeArray = !spvMembers.empty() && builder.getOpCode(spvMembers.back()) == spv::OpTypeRuntimeArray;
        if (hasRuntimeArray) {
            builder.addDecoration(spvType, TranslateBlockDecoration(typeStorageQualifier, glslangIntermediate->usingStorageBuffer()));
        }
    }

    if (qualifier.hasHitObjectShaderRecordNV())
        builder.addDecoration(spvType, spv::DecorationHitObjectShaderRecordBufferNV);
}

// Turn the expression forming the array size into an id.
// This is not quite trivial, because of specialization constants.
// Sometimes, a raw constant is turned into an Id, and sometimes
// a specialization constant expression is.
spv::Id TGlslangToSpvTraverser::makeArraySizeId(const glslang::TArraySizes& arraySizes, int dim, bool allowZero)
{
    // First, see if this is sized with a node, meaning a specialization constant:
    glslang::TIntermTyped* specNode = arraySizes.getDimNode(dim);
    if (specNode != nullptr) {
        builder.clearAccessChain();
        SpecConstantOpModeGuard spec_constant_op_mode_setter(&builder);
        spec_constant_op_mode_setter.turnOnSpecConstantOpMode();
        specNode->traverse(this);
        return accessChainLoad(specNode->getAsTyped()->getType());
    }

    // Otherwise, need a compile-time (front end) size, get it:
    int size = arraySizes.getDimSize(dim);

    if (!allowZero)
        assert(size > 0);

    return builder.makeUintConstant(size);
}

// Wrap the builder's accessChainLoad to:
//  - localize handling of RelaxedPrecision
//  - use the SPIR-V inferred type instead of another conversion of the glslang type
//    (avoids unnecessary work and possible type punning for structures)
//  - do conversion of concrete to abstract type
spv::Id TGlslangToSpvTraverser::accessChainLoad(const glslang::TType& type)
{
    spv::Id nominalTypeId = builder.accessChainGetInferredType();

    spv::Builder::AccessChain::CoherentFlags coherentFlags = builder.getAccessChain().coherentFlags;
    coherentFlags |= TranslateCoherent(type);

    spv::MemoryAccessMask accessMask = spv::MemoryAccessMask(TranslateMemoryAccess(coherentFlags) & ~spv::MemoryAccessMakePointerAvailableKHRMask);
    // If the value being loaded is HelperInvocation, SPIR-V 1.6 is being generated (so that
    // SPV_EXT_demote_to_helper_invocation is in core) and the memory model is in use, add
    // the Volatile MemoryAccess semantic.
    if (type.getQualifier().builtIn == glslang::EbvHelperInvocation &&
        glslangIntermediate->usingVulkanMemoryModel() &&
        glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_6) {
        accessMask = spv::MemoryAccessMask(accessMask | spv::MemoryAccessVolatileMask);
    }

    unsigned int alignment = builder.getAccessChain().alignment;
    alignment |= type.getBufferReferenceAlignment();

    spv::Id loadedId = builder.accessChainLoad(TranslatePrecisionDecoration(type),
        TranslateNonUniformDecoration(builder.getAccessChain().coherentFlags),
        TranslateNonUniformDecoration(type.getQualifier()),
        nominalTypeId,
        accessMask,
        TranslateMemoryScope(coherentFlags),
        alignment);

    // Need to convert to abstract types when necessary
    if (type.getBasicType() == glslang::EbtBool) {
        loadedId = convertLoadedBoolInUniformToUint(type, nominalTypeId, loadedId);
    }

    return loadedId;
}

// Wrap the builder's accessChainStore to:
//  - do conversion of concrete to abstract type
//
// Implicitly uses the existing builder.accessChain as the storage target.
void TGlslangToSpvTraverser::accessChainStore(const glslang::TType& type, spv::Id rvalue)
{
    // Need to convert to abstract types when necessary
    if (type.getBasicType() == glslang::EbtBool) {
        spv::Id nominalTypeId = builder.accessChainGetInferredType();

        if (builder.isScalarType(nominalTypeId)) {
            // Conversion for bool
            spv::Id boolType = builder.makeBoolType();
            if (nominalTypeId != boolType) {
                // keep these outside arguments, for determinant order-of-evaluation
                spv::Id one = builder.makeUintConstant(1);
                spv::Id zero = builder.makeUintConstant(0);
                rvalue = builder.createTriOp(spv::OpSelect, nominalTypeId, rvalue, one, zero);
            } else if (builder.getTypeId(rvalue) != boolType)
                rvalue = builder.createBinOp(spv::OpINotEqual, boolType, rvalue, builder.makeUintConstant(0));
        } else if (builder.isVectorType(nominalTypeId)) {
            // Conversion for bvec
            int vecSize = builder.getNumTypeComponents(nominalTypeId);
            spv::Id bvecType = builder.makeVectorType(builder.makeBoolType(), vecSize);
            if (nominalTypeId != bvecType) {
                // keep these outside arguments, for determinant order-of-evaluation
                spv::Id one = makeSmearedConstant(builder.makeUintConstant(1), vecSize);
                spv::Id zero = makeSmearedConstant(builder.makeUintConstant(0), vecSize);
                rvalue = builder.createTriOp(spv::OpSelect, nominalTypeId, rvalue, one, zero);
            } else if (builder.getTypeId(rvalue) != bvecType)
                rvalue = builder.createBinOp(spv::OpINotEqual, bvecType, rvalue,
                                             makeSmearedConstant(builder.makeUintConstant(0), vecSize));
        }
    }

    spv::Builder::AccessChain::CoherentFlags coherentFlags = builder.getAccessChain().coherentFlags;
    coherentFlags |= TranslateCoherent(type);

    unsigned int alignment = builder.getAccessChain().alignment;
    alignment |= type.getBufferReferenceAlignment();

    builder.accessChainStore(rvalue, TranslateNonUniformDecoration(builder.getAccessChain().coherentFlags),
                             spv::MemoryAccessMask(TranslateMemoryAccess(coherentFlags) &
                                ~spv::MemoryAccessMakePointerVisibleKHRMask),
                             TranslateMemoryScope(coherentFlags), alignment);
}

// For storing when types match at the glslang level, but not might match at the
// SPIR-V level.
//
// This especially happens when a single glslang type expands to multiple
// SPIR-V types, like a struct that is used in a member-undecorated way as well
// as in a member-decorated way.
//
// NOTE: This function can handle any store request; if it's not special it
// simplifies to a simple OpStore.
//
// Implicitly uses the existing builder.accessChain as the storage target.
void TGlslangToSpvTraverser::multiTypeStore(const glslang::TType& type, spv::Id rValue)
{
    // we only do the complex path here if it's an aggregate
    if (! type.isStruct() && ! type.isArray()) {
        accessChainStore(type, rValue);
        return;
    }

    // and, it has to be a case of type aliasing
    spv::Id rType = builder.getTypeId(rValue);
    spv::Id lValue = builder.accessChainGetLValue();
    spv::Id lType = builder.getContainedTypeId(builder.getTypeId(lValue));
    if (lType == rType) {
        accessChainStore(type, rValue);
        return;
    }

    // Recursively (as needed) copy an aggregate type to a different aggregate type,
    // where the two types were the same type in GLSL. This requires member
    // by member copy, recursively.

    // SPIR-V 1.4 added an instruction to do help do this.
    if (glslangIntermediate->getSpv().spv >= glslang::EShTargetSpv_1_4) {
        // However, bool in uniform space is changed to int, so
        // OpCopyLogical does not work for that.
        // TODO: It would be more robust to do a full recursive verification of the types satisfying SPIR-V rules.
        bool rBool = builder.containsType(builder.getTypeId(rValue), spv::OpTypeBool, 0);
        bool lBool = builder.containsType(lType, spv::OpTypeBool, 0);
        if (lBool == rBool) {
            spv::Id logicalCopy = builder.createUnaryOp(spv::OpCopyLogical, lType, rValue);
            accessChainStore(type, logicalCopy);
            return;
        }
    }

    // If an array, copy element by element.
    if (type.isArray()) {
        glslang::TType glslangElementType(type, 0);
        spv::Id elementRType = builder.getContainedTypeId(rType);
        for (int index = 0; index < type.getOuterArraySize(); ++index) {
            // get the source member
            spv::Id elementRValue = builder.createCompositeExtract(rValue, elementRType, index);

            // set up the target storage
            builder.clearAccessChain();
            builder.setAccessChainLValue(lValue);
            builder.accessChainPush(builder.makeIntConstant(index), TranslateCoherent(type),
                type.getBufferReferenceAlignment());

            // store the member
            multiTypeStore(glslangElementType, elementRValue);
        }
    } else {
        assert(type.isStruct());

        // loop over structure members
        const glslang::TTypeList& members = *type.getStruct();
        for (int m = 0; m < (int)members.size(); ++m) {
            const glslang::TType& glslangMemberType = *members[m].type;

            // get the source member
            spv::Id memberRType = builder.getContainedTypeId(rType, m);
            spv::Id memberRValue = builder.createCompositeExtract(rValue, memberRType, m);

            // set up the target storage
            builder.clearAccessChain();
            builder.setAccessChainLValue(lValue);
            builder.accessChainPush(builder.makeIntConstant(m), TranslateCoherent(type),
                type.getBufferReferenceAlignment());

            // store the member
            multiTypeStore(glslangMemberType, memberRValue);
        }
    }
}

// Decide whether or not this type should be
// decorated with offsets and strides, and if so
// whether std140 or std430 rules should be applied.
glslang::TLayoutPacking TGlslangToSpvTraverser::getExplicitLayout(const glslang::TType& type) const
{
    // has to be a block
    if (type.getBasicType() != glslang::EbtBlock)
        return glslang::ElpNone;

    // has to be a uniform or buffer block or task in/out blocks
    if (type.getQualifier().storage != glslang::EvqUniform &&
        type.getQualifier().storage != glslang::EvqBuffer &&
        type.getQualifier().storage != glslang::EvqShared &&
        !type.getQualifier().isTaskMemory())
        return glslang::ElpNone;

    // return the layout to use
    switch (type.getQualifier().layoutPacking) {
    case glslang::ElpStd140:
    case glslang::ElpStd430:
    case glslang::ElpScalar:
        return type.getQualifier().layoutPacking;
    default:
        return glslang::ElpNone;
    }
}

// Given an array type, returns the integer stride required for that array
int TGlslangToSpvTraverser::getArrayStride(const glslang::TType& arrayType, glslang::TLayoutPacking explicitLayout,
    glslang::TLayoutMatrix matrixLayout)
{
    int size;
    int stride;
    glslangIntermediate->getMemberAlignment(arrayType, size, stride, explicitLayout,
        matrixLayout == glslang::ElmRowMajor);

    return stride;
}

// Given a matrix type, or array (of array) of matrixes type, returns the integer stride required for that matrix
// when used as a member of an interface block
int TGlslangToSpvTraverser::getMatrixStride(const glslang::TType& matrixType, glslang::TLayoutPacking explicitLayout,
    glslang::TLayoutMatrix matrixLayout)
{
    glslang::TType elementType;
    elementType.shallowCopy(matrixType);
    elementType.clearArraySizes();

    int size;
    int stride;
    glslangIntermediate->getMemberAlignment(elementType, size, stride, explicitLayout,
        matrixLayout == glslang::ElmRowMajor);

    return stride;
}

// Given a member type of a struct, realign the current offset for it, and compute
// the next (not yet aligned) offset for the next member, which will get aligned
// on the next call.
// 'currentOffset' should be passed in already initialized, ready to modify, and reflecting
// the migration of data from nextOffset -> currentOffset.  It should be -1 on the first call.
// -1 means a non-forced member offset (no decoration needed).
void TGlslangToSpvTraverser::updateMemberOffset(const glslang::TType& structType, const glslang::TType& memberType,
    int& currentOffset, int& nextOffset, glslang::TLayoutPacking explicitLayout, glslang::TLayoutMatrix matrixLayout)
{
    // this will get a positive value when deemed necessary
    nextOffset = -1;

    // override anything in currentOffset with user-set offset
    if (memberType.getQualifier().hasOffset())
        currentOffset = memberType.getQualifier().layoutOffset;

    // It could be that current linker usage in glslang updated all the layoutOffset,
    // in which case the following code does not matter.  But, that's not quite right
    // once cross-compilation unit GLSL validation is done, as the original user
    // settings are needed in layoutOffset, and then the following will come into play.

    if (explicitLayout == glslang::ElpNone) {
        if (! memberType.getQualifier().hasOffset())
            currentOffset = -1;

        return;
    }

    // Getting this far means we need explicit offsets
    if (currentOffset < 0)
        currentOffset = 0;

    // Now, currentOffset is valid (either 0, or from a previous nextOffset),
    // but possibly not yet correctly aligned.

    int memberSize;
    int dummyStride;
    int memberAlignment = glslangIntermediate->getMemberAlignment(memberType, memberSize, dummyStride, explicitLayout,
        matrixLayout == glslang::ElmRowMajor);

    // Adjust alignment for HLSL rules
    // TODO: make this consistent in early phases of code:
    //       adjusting this late means inconsistencies with earlier code, which for reflection is an issue
    // Until reflection is brought in sync with these adjustments, don't apply to $Global,
    // which is the most likely to rely on reflection, and least likely to rely implicit layouts
    if (glslangIntermediate->usingHlslOffsets() &&
        ! memberType.isArray() && memberType.isVector() && structType.getTypeName().compare("$Global") != 0) {
        int dummySize;
        int componentAlignment = glslangIntermediate->getBaseAlignmentScalar(memberType, dummySize);
        if (componentAlignment <= 4)
            memberAlignment = componentAlignment;
    }

    // Bump up to member alignment
    glslang::RoundToPow2(currentOffset, memberAlignment);

    // Bump up to vec4 if there is a bad straddle
    if (explicitLayout != glslang::ElpScalar && glslangIntermediate->improperStraddle(memberType, memberSize,
        currentOffset))
        glslang::RoundToPow2(currentOffset, 16);

    nextOffset = currentOffset + memberSize;
}

void TGlslangToSpvTraverser::declareUseOfStructMember(const glslang::TTypeList& members, int glslangMember)
{
    const glslang::TBuiltInVariable glslangBuiltIn = members[glslangMember].type->getQualifier().builtIn;
    switch (glslangBuiltIn)
    {
    case glslang::EbvPointSize:
    case glslang::EbvClipDistance:
    case glslang::EbvCullDistance:
    case glslang::EbvViewportMaskNV:
    case glslang::EbvSecondaryPositionNV:
    case glslang::EbvSecondaryViewportMaskNV:
    case glslang::EbvPositionPerViewNV:
    case glslang::EbvViewportMaskPerViewNV:
    case glslang::EbvTaskCountNV:
    case glslang::EbvPrimitiveCountNV:
    case glslang::EbvPrimitiveIndicesNV:
    case glslang::EbvClipDistancePerViewNV:
    case glslang::EbvCullDistancePerViewNV:
    case glslang::EbvLayerPerViewNV:
    case glslang::EbvMeshViewCountNV:
    case glslang::EbvMeshViewIndicesNV:
        // Generate the associated capability.  Delegate to TranslateBuiltInDecoration.
        // Alternately, we could just call this for any glslang built-in, since the
        // capability already guards against duplicates.
        TranslateBuiltInDecoration(glslangBuiltIn, false);
        break;
    default:
        // Capabilities were already generated when the struct was declared.
        break;
    }
}

bool TGlslangToSpvTraverser::isShaderEntryPoint(const glslang::TIntermAggregate* node)
{
    return node->getName().compare(glslangIntermediate->getEntryPointMangledName().c_str()) == 0;
}

// Does parameter need a place to keep writes, separate from the original?
// Assumes called after originalParam(), which filters out block/buffer/opaque-based
// qualifiers such that we should have only in/out/inout/constreadonly here.
bool TGlslangToSpvTraverser::writableParam(glslang::TStorageQualifier qualifier) const
{
    assert(qualifier == glslang::EvqIn ||
           qualifier == glslang::EvqOut ||
           qualifier == glslang::EvqInOut ||
           qualifier == glslang::EvqUniform ||
           qualifier == glslang::EvqConstReadOnly);
    return qualifier != glslang::EvqConstReadOnly &&
           qualifier != glslang::EvqUniform;
}

// Is parameter pass-by-original?
bool TGlslangToSpvTraverser::originalParam(glslang::TStorageQualifier qualifier, const glslang::TType& paramType,
                                           bool implicitThisParam)
{
    if (implicitThisParam)                                                                     // implicit this
        return true;
    if (glslangIntermediate->getSource() == glslang::EShSourceHlsl)
        return paramType.getBasicType() == glslang::EbtBlock;
    return (paramType.containsOpaque() && !glslangIntermediate->getBindlessMode()) ||       // sampler, etc.
           paramType.getQualifier().isSpirvByReference() ||                                    // spirv_by_reference
           (paramType.getBasicType() == glslang::EbtBlock && qualifier == glslang::EvqBuffer); // SSBO
}

// Make all the functions, skeletally, without actually visiting their bodies.
void TGlslangToSpvTraverser::makeFunctions(const glslang::TIntermSequence& glslFunctions)
{
    const auto getParamDecorations = [&](std::vector<spv::Decoration>& decorations, const glslang::TType& type,
        bool useVulkanMemoryModel) {
        spv::Decoration paramPrecision = TranslatePrecisionDecoration(type);
        if (paramPrecision != spv::NoPrecision)
            decorations.push_back(paramPrecision);
        TranslateMemoryDecoration(type.getQualifier(), decorations, useVulkanMemoryModel);
        if (type.isReference()) {
            // Original and non-writable params pass the pointer directly and
            // use restrict/aliased, others are stored to a pointer in Function
            // memory and use RestrictPointer/AliasedPointer.
            if (originalParam(type.getQualifier().storage, type, false) ||
                !writableParam(type.getQualifier().storage)) {
                decorations.push_back(type.getQualifier().isRestrict() ? spv::DecorationRestrict :
                                                                         spv::DecorationAliased);
            } else {
                decorations.push_back(type.getQualifier().isRestrict() ? spv::DecorationRestrictPointerEXT :
                                                                         spv::DecorationAliasedPointerEXT);
            }
        }
    };

    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* glslFunction = glslFunctions[f]->getAsAggregate();
        if (! glslFunction || glslFunction->getOp() != glslang::EOpFunction || isShaderEntryPoint(glslFunction))
            continue;

        // We're on a user function.  Set up the basic interface for the function now,
        // so that it's available to call.  Translating the body will happen later.
        //
        // Typically (except for a "const in" parameter), an address will be passed to the
        // function.  What it is an address of varies:
        //
        // - "in" parameters not marked as "const" can be written to without modifying the calling
        //   argument so that write needs to be to a copy, hence the address of a copy works.
        //
        // - "const in" parameters can just be the r-value, as no writes need occur.
        //
        // - "out" and "inout" arguments can't be done as pointers to the calling argument, because
        //   GLSL has copy-in/copy-out semantics.  They can be handled though with a pointer to a copy.

        std::vector<spv::Id> paramTypes;
        std::vector<char const*> paramNames;
        std::vector<std::vector<spv::Decoration>> paramDecorations; // list of decorations per parameter
        glslang::TIntermSequence& parameters = glslFunction->getSequence()[0]->getAsAggregate()->getSequence();

#ifdef ENABLE_HLSL
        bool implicitThis = (int)parameters.size() > 0 && parameters[0]->getAsSymbolNode()->getName() ==
                                                          glslangIntermediate->implicitThisName;
#else
        bool implicitThis = false;
#endif

        paramDecorations.resize(parameters.size());
        for (int p = 0; p < (int)parameters.size(); ++p) {
            const glslang::TType& paramType = parameters[p]->getAsTyped()->getType();
            spv::Id typeId = convertGlslangToSpvType(paramType);
            if (originalParam(paramType.getQualifier().storage, paramType, implicitThis && p == 0))
                typeId = builder.makePointer(TranslateStorageClass(paramType), typeId);
            else if (writableParam(paramType.getQualifier().storage))
                typeId = builder.makePointer(spv::StorageClassFunction, typeId);
            else
                rValueParameters.insert(parameters[p]->getAsSymbolNode()->getId());
            getParamDecorations(paramDecorations[p], paramType, glslangIntermediate->usingVulkanMemoryModel());
            paramTypes.push_back(typeId);
        }

        for (auto const parameter:parameters) {
            paramNames.push_back(parameter->getAsSymbolNode()->getName().c_str());
        }

        spv::Block* functionBlock;
        spv::Function *function = builder.makeFunctionEntry(TranslatePrecisionDecoration(glslFunction->getType()),
                                                            convertGlslangToSpvType(glslFunction->getType()),
                                                            glslFunction->getName().c_str(), paramTypes, paramNames,
                                                            paramDecorations, &functionBlock);
        if (implicitThis)
            function->setImplicitThis();

        // Track function to emit/call later
        functionMap[glslFunction->getName().c_str()] = function;

        // Set the parameter id's
        for (int p = 0; p < (int)parameters.size(); ++p) {
            symbolValues[parameters[p]->getAsSymbolNode()->getId()] = function->getParamId(p);
            // give a name too
            builder.addName(function->getParamId(p), parameters[p]->getAsSymbolNode()->getName().c_str());

            const glslang::TType& paramType = parameters[p]->getAsTyped()->getType();
            if (paramType.contains8BitInt())
                builder.addCapability(spv::CapabilityInt8);
            if (paramType.contains16BitInt())
                builder.addCapability(spv::CapabilityInt16);
            if (paramType.contains16BitFloat())
                builder.addCapability(spv::CapabilityFloat16);
        }
    }
}

// Process all the initializers, while skipping the functions and link objects
void TGlslangToSpvTraverser::makeGlobalInitializers(const glslang::TIntermSequence& initializers)
{
    builder.setBuildPoint(shaderEntry->getLastBlock());
    for (int i = 0; i < (int)initializers.size(); ++i) {
        glslang::TIntermAggregate* initializer = initializers[i]->getAsAggregate();
        if (initializer && initializer->getOp() != glslang::EOpFunction && initializer->getOp() !=
            glslang::EOpLinkerObjects) {

            // We're on a top-level node that's not a function.  Treat as an initializer, whose
            // code goes into the beginning of the entry point.
            initializer->traverse(this);
        }
    }
}
// Walk over all linker objects to create a map for payload and callable data linker objects
// and their location to be used during codegen for OpTraceKHR and OpExecuteCallableKHR
// This is done here since it is possible that these linker objects are not be referenced in the AST
void TGlslangToSpvTraverser::collectRayTracingLinkerObjects()
{
    glslang::TIntermAggregate* linkerObjects = glslangIntermediate->findLinkerObjects();
    for (auto& objSeq : linkerObjects->getSequence()) {
        auto objNode = objSeq->getAsSymbolNode();
        if (objNode != nullptr) {
            if (objNode->getQualifier().hasLocation()) {
                unsigned int location = objNode->getQualifier().layoutLocation;
                auto st = objNode->getQualifier().storage;
                int set;
                switch (st)
                {
                case glslang::EvqPayload:
                case glslang::EvqPayloadIn:
                    set = 0;
                    break;
                case glslang::EvqCallableData:
                case glslang::EvqCallableDataIn:
                    set = 1;
                    break;

                case glslang::EvqHitObjectAttrNV:
                    set = 2;
                    break;

                default:
                    set = -1;
                }
                if (set != -1)
                    locationToSymbol[set].insert(std::make_pair(location, objNode));
            }
        }
    }
}
// Process all the functions, while skipping initializers.
void TGlslangToSpvTraverser::visitFunctions(const glslang::TIntermSequence& glslFunctions)
{
    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* node = glslFunctions[f]->getAsAggregate();
        if (node && (node->getOp() == glslang::EOpFunction || node->getOp() == glslang::EOpLinkerObjects))
            node->traverse(this);
    }
}

void TGlslangToSpvTraverser::handleFunctionEntry(const glslang::TIntermAggregate* node)
{
    // SPIR-V functions should already be in the functionMap from the prepass
    // that called makeFunctions().
    currentFunction = functionMap[node->getName().c_str()];
    spv::Block* functionBlock = currentFunction->getEntryBlock();
    builder.setBuildPoint(functionBlock);
    builder.enterFunction(currentFunction);
}

void TGlslangToSpvTraverser::translateArguments(const glslang::TIntermAggregate& node, std::vector<spv::Id>& arguments,
    spv::Builder::AccessChain::CoherentFlags &lvalueCoherentFlags)
{
    const glslang::TIntermSequence& glslangArguments = node.getSequence();

    glslang::TSampler sampler = {};
    bool cubeCompare = false;
    bool f16ShadowCompare = false;
    if (node.isTexture() || node.isImage()) {
        sampler = glslangArguments[0]->getAsTyped()->getType().getSampler();
        cubeCompare = sampler.dim == glslang::EsdCube && sampler.arrayed && sampler.shadow;
        f16ShadowCompare = sampler.shadow &&
            glslangArguments[1]->getAsTyped()->getType().getBasicType() == glslang::EbtFloat16;
    }

    for (int i = 0; i < (int)glslangArguments.size(); ++i) {
        builder.clearAccessChain();
        glslangArguments[i]->traverse(this);

        // Special case l-value operands
        bool lvalue = false;
        switch (node.getOp()) {
        case glslang::EOpImageAtomicAdd:
        case glslang::EOpImageAtomicMin:
        case glslang::EOpImageAtomicMax:
        case glslang::EOpImageAtomicAnd:
        case glslang::EOpImageAtomicOr:
        case glslang::EOpImageAtomicXor:
        case glslang::EOpImageAtomicExchange:
        case glslang::EOpImageAtomicCompSwap:
        case glslang::EOpImageAtomicLoad:
        case glslang::EOpImageAtomicStore:
            if (i == 0)
                lvalue = true;
            break;
        case glslang::EOpSparseImageLoad:
            if ((sampler.ms && i == 3) || (! sampler.ms && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTexture:
            if (((cubeCompare || f16ShadowCompare) && i == 3) || (! (cubeCompare || f16ShadowCompare) && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureClamp:
            if (((cubeCompare || f16ShadowCompare) && i == 4) || (! (cubeCompare || f16ShadowCompare) && i == 3))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureLod:
        case glslang::EOpSparseTextureOffset:
            if  ((f16ShadowCompare && i == 4) || (! f16ShadowCompare && i == 3))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureFetch:
            if ((sampler.dim != glslang::EsdRect && i == 3) || (sampler.dim == glslang::EsdRect && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureFetchOffset:
            if ((sampler.dim != glslang::EsdRect && i == 4) || (sampler.dim == glslang::EsdRect && i == 3))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureLodOffset:
        case glslang::EOpSparseTextureGrad:
        case glslang::EOpSparseTextureOffsetClamp:
            if ((f16ShadowCompare && i == 5) || (! f16ShadowCompare && i == 4))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGradOffset:
        case glslang::EOpSparseTextureGradClamp:
            if ((f16ShadowCompare && i == 6) || (! f16ShadowCompare && i == 5))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGradOffsetClamp:
            if ((f16ShadowCompare && i == 7) || (! f16ShadowCompare && i == 6))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGather:
            if ((sampler.shadow && i == 3) || (! sampler.shadow && i == 2))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGatherOffset:
        case glslang::EOpSparseTextureGatherOffsets:
            if ((sampler.shadow && i == 4) || (! sampler.shadow && i == 3))
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGatherLod:
            if (i == 3)
                lvalue = true;
            break;
        case glslang::EOpSparseTextureGatherLodOffset:
        case glslang::EOpSparseTextureGatherLodOffsets:
            if (i == 4)
                lvalue = true;
            break;
        case glslang::EOpSparseImageLoadLod:
            if (i == 3)
                lvalue = true;
            break;
        case glslang::EOpImageSampleFootprintNV:
            if (i == 4)
                lvalue = true;
            break;
        case glslang::EOpImageSampleFootprintClampNV:
        case glslang::EOpImageSampleFootprintLodNV:
            if (i == 5)
                lvalue = true;
            break;
        case glslang::EOpImageSampleFootprintGradNV:
            if (i == 6)
                lvalue = true;
            break;
        case glslang::EOpImageSampleFootprintGradClampNV:
            if (i == 7)
                lvalue = true;
            break;
        case glslang::EOpRayQueryGetIntersectionTriangleVertexPositionsEXT:
            if (i == 2)
                lvalue = true;
            break;
        default:
            break;
        }

        if (lvalue) {
            spv::Id lvalue_id = builder.accessChainGetLValue();
            arguments.push_back(lvalue_id);
            lvalueCoherentFlags = builder.getAccessChain().coherentFlags;
            builder.addDecoration(lvalue_id, TranslateNonUniformDecoration(lvalueCoherentFlags));
            lvalueCoherentFlags |= TranslateCoherent(glslangArguments[i]->getAsTyped()->getType());
        } else
            arguments.push_back(accessChainLoad(glslangArguments[i]->getAsTyped()->getType()));
    }
}

void TGlslangToSpvTraverser::translateArguments(glslang::TIntermUnary& node, std::vector<spv::Id>& arguments)
{
    builder.clearAccessChain();
    node.getOperand()->traverse(this);
    arguments.push_back(accessChainLoad(node.getOperand()->getType()));
}

spv::Id TGlslangToSpvTraverser::createImageTextureFunctionCall(glslang::TIntermOperator* node)
{
    if (! node->isImage() && ! node->isTexture())
        return spv::NoResult;

    builder.setLine(node->getLoc().line, node->getLoc().getFilename());

    // Process a GLSL texturing op (will be SPV image)

    const glslang::TType &imageType = node->getAsAggregate()
                                        ? node->getAsAggregate()->getSequence()[0]->getAsTyped()->getType()
                                        : node->getAsUnaryNode()->getOperand()->getAsTyped()->getType();
    const glslang::TSampler sampler = imageType.getSampler();
    bool f16ShadowCompare = (sampler.shadow && node->getAsAggregate())
            ? node->getAsAggregate()->getSequence()[1]->getAsTyped()->getType().getBasicType() == glslang::EbtFloat16
            : false;

    const auto signExtensionMask = [&]() {
        if (builder.getSpvVersion() >= spv::Spv_1_4) {
            if (sampler.type == glslang::EbtUint)
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       