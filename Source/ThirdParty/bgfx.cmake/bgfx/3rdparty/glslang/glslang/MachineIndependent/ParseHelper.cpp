//
// Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
// Copyright (C) 2012-2015 LunarG, Inc.
// Copyright (C) 2015-2018 Google, Inc.
// Copyright (C) 2017, 2019 ARM Limited.
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

#include "ParseHelper.h"
#include "Scan.h"

#include "../OSDependent/osinclude.h"
#include <algorithm>

#include "preprocessor/PpContext.h"

extern int yyparse(glslang::TParseContext*);

namespace glslang {

TParseContext::TParseContext(TSymbolTable& symbolTable, TIntermediate& interm, bool parsingBuiltins,
                             int version, EProfile profile, const SpvVersion& spvVersion, EShLanguage language,
                             TInfoSink& infoSink, bool forwardCompatible, EShMessages messages,
                             const TString* entryPoint) :
            TParseContextBase(symbolTable, interm, parsingBuiltins, version, profile, spvVersion, language,
                              infoSink, forwardCompatible, messages, entryPoint),
            inMain(false),
            blockName(nullptr),
            limits(resources.limits),
            atomicUintOffsets(nullptr), anyIndexLimits(false)
{
    // decide whether precision qualifiers should be ignored or respected
    if (isEsProfile() || spvVersion.vulkan > 0) {
        precisionManager.respectPrecisionQualifiers();
        if (! parsingBuiltins && language == EShLangFragment && !isEsProfile() && spvVersion.vulkan > 0)
            precisionManager.warnAboutDefaults();
    }

    setPrecisionDefaults();

    globalUniformDefaults.clear();
    globalUniformDefaults.layoutMatrix = ElmColumnMajor;
    globalUniformDefaults.layoutPacking = spvVersion.spv != 0 ? ElpStd140 : ElpShared;

    globalBufferDefaults.clear();
    globalBufferDefaults.layoutMatrix = ElmColumnMajor;
    globalBufferDefaults.layoutPacking = spvVersion.spv != 0 ? ElpStd430 : ElpShared;

    globalInputDefaults.clear();
    globalOutputDefaults.clear();

    globalSharedDefaults.clear();
    globalSharedDefaults.layoutMatrix = ElmColumnMajor;
    globalSharedDefaults.layoutPacking = ElpStd430;

    // "Shaders in the transform
    // feedback capturing mode have an initial global default of
    //     layout(xfb_buffer = 0) out;"
    if (language == EShLangVertex ||
        language == EShLangTessControl ||
        language == EShLangTessEvaluation ||
        language == EShLangGeometry)
        globalOutputDefaults.layoutXfbBuffer = 0;

    if (language == EShLangGeometry)
        globalOutputDefaults.layoutStream = 0;

    if (entryPoint != nullptr && entryPoint->size() > 0 && *entryPoint != "main")
        infoSink.info.message(EPrefixError, "Source entry point must be \"main\"");
}

TParseContext::~TParseContext()
{
    delete [] atomicUintOffsets;
}

// Set up all default precisions as needed by the current environment.
// Intended just as a TParseContext constructor helper.
void TParseContext::setPrecisionDefaults()
{
    // Set all precision defaults to EpqNone, which is correct for all types
    // when not obeying precision qualifiers, and correct for types that don't
    // have defaults (thus getting an error on use) when obeying precision
    // qualifiers.

    for (int type = 0; type < EbtNumTypes; ++type)
        defaultPrecision[type] = EpqNone;

    for (int type = 0; type < maxSamplerIndex; ++type)
        defaultSamplerPrecision[type] = EpqNone;

    // replace with real precision defaults for those that have them
    if (obeyPrecisionQualifiers()) {
        if (isEsProfile()) {
            // Most don't have defaults, a few default to lowp.
            TSampler sampler;
            sampler.set(EbtFloat, Esd2D);
            defaultSamplerPrecision[computeSamplerTypeIndex(sampler)] = EpqLow;
            sampler.set(EbtFloat, EsdCube);
            defaultSamplerPrecision[computeSamplerTypeIndex(sampler)] = EpqLow;
            sampler.set(EbtFloat, Esd2D);
            sampler.setExternal(true);
            defaultSamplerPrecision[computeSamplerTypeIndex(sampler)] = EpqLow;
        }

        // If we are parsing built-in computational variables/functions, it is meaningful to record
        // whether the built-in has no precision qualifier, as that ambiguity
        // is used to resolve the precision from the supplied arguments/operands instead.
        // So, we don't actually want to replace EpqNone with a default precision for built-ins.
        if (! parsingBuiltins) {
            if (isEsProfile() && language == EShLangFragment) {
                defaultPrecision[EbtInt] = EpqMedium;
                defaultPrecision[EbtUint] = EpqMedium;
            } else {
                defaultPrecision[EbtInt] = EpqHigh;
                defaultPrecision[EbtUint] = EpqHigh;
                defaultPrecision[EbtFloat] = EpqHigh;
            }

            if (!isEsProfile()) {
                // Non-ES profile
                // All sampler precisions default to highp.
                for (int type = 0; type < maxSamplerIndex; ++type)
                    defaultSamplerPrecision[type] = EpqHigh;
            }
        }

        defaultPrecision[EbtSampler] = EpqLow;
        defaultPrecision[EbtAtomicUint] = EpqHigh;
    }
}

void TParseContext::setLimits(const TBuiltInResource& r)
{
    resources = r;
    intermediate.setLimits(r);

    anyIndexLimits = ! limits.generalAttributeMatrixVectorIndexing ||
                     ! limits.generalConstantMatrixVectorIndexing ||
                     ! limits.generalSamplerIndexing ||
                     ! limits.generalUniformIndexing ||
                     ! limits.generalVariableIndexing ||
                     ! limits.generalVaryingIndexing;


    // "Each binding point tracks its own current default offset for
    // inheritance of subsequent variables using the same binding. The initial state of compilation is that all
    // binding points have an offset of 0."
    atomicUintOffsets = new int[resources.maxAtomicCounterBindings];
    for (int b = 0; b < resources.maxAtomicCounterBindings; ++b)
        atomicUintOffsets[b] = 0;
}

//
// Parse an array of strings using yyparse, going through the
// preprocessor to tokenize the shader strings, then through
// the GLSL scanner.
//
// Returns true for successful acceptance of the shader, false if any errors.
//
bool TParseContext::parseShaderStrings(TPpContext& ppContext, TInputScanner& input, bool versionWillBeError)
{
    currentScanner = &input;
    ppContext.setInput(input, versionWillBeError);
    yyparse(this);

    finish();

    return numErrors == 0;
}

// This is called from bison when it has a parse (syntax) error
// Note though that to stop cascading errors, we set EOF, which
// will usually cause a syntax error, so be more accurate that
// compilation is terminating.
void TParseContext::parserError(const char* s)
{
    if (! getScanner()->atEndOfInput() || numErrors == 0)
        error(getCurrentLoc(), "", "", s, "");
    else
        error(getCurrentLoc(), "compilation terminated", "", "");
}

void TParseContext::growGlobalUniformBlock(const TSourceLoc& loc, TType& memberType, const TString& memberName, TTypeList* typeList)
{
    bool createBlock = globalUniformBlock == nullptr;

    if (createBlock) {
        globalUniformBinding = intermediate.getGlobalUniformBinding();
        globalUniformSet = intermediate.getGlobalUniformSet();
    }

    // use base class function to create/expand block
    TParseContextBase::growGlobalUniformBlock(loc, memberType, memberName, typeList);

    if (spvVersion.vulkan > 0 && spvVersion.vulkanRelaxed) {
        // check for a block storage override
        TBlockStorageClass storageOverride = intermediate.getBlockStorageOverride(getGlobalUniformBlockName());
        TQualifier& qualifier = globalUniformBlock->getWritableType().getQualifier();
        qualifier.defaultBlock = true;

        if (storageOverride != EbsNone) {
            if (createBlock) {
                // Remap block storage
                qualifier.setBlockStorage(storageOverride);

                // check that the change didn't create errors
                blockQualifierCheck(loc, qualifier, false);
            }

            // remap meber storage as well
            memberType.getQualifier().setBlockStorage(storageOverride);
        }
    }
}

void TParseContext::growAtomicCounterBlock(int binding, const TSourceLoc& loc, TType& memberType, const TString& memberName, TTypeList* typeList)
{
    bool createBlock = atomicCounterBuffers.find(binding) == atomicCounterBuffers.end();

    if (createBlock) {
        atomicCounterBlockSet = intermediate.getAtomicCounterBlockSet();
    }

    // use base class function to create/expand block
    TParseContextBase::growAtomicCounterBlock(binding, loc, memberType, memberName, typeList);
    TQualifier& qualifier = atomicCounterBuffers[binding]->getWritableType().getQualifier();
    qualifier.defaultBlock = true;

    if (spvVersion.vulkan > 0 && spvVersion.vulkanRelaxed) {
        // check for a Block storage override
        TBlockStorageClass storageOverride = intermediate.getBlockStorageOverride(getAtomicCounterBlockName());

        if (storageOverride != EbsNone) {
            if (createBlock) {
                // Remap block storage

                qualifier.setBlockStorage(storageOverride);

                // check that the change didn't create errors
                blockQualifierCheck(loc, qualifier, false);
            }

            // remap meber storage as well
            memberType.getQualifier().setBlockStorage(storageOverride);
        }
    }
}

const char* TParseContext::getGlobalUniformBlockName() const
{
    const char* name = intermediate.getGlobalUniformBlockName();
    if (std::string(name) == "")
        return "gl_DefaultUniformBlock";
    else
        return name;
}
void TParseContext::finalizeGlobalUniformBlockLayout(TVariable&)
{
}
void TParseContext::setUniformBlockDefaults(TType& block) const
{
    block.getQualifier().layoutPacking = ElpStd140;
    block.getQualifier().layoutMatrix = ElmColumnMajor;
}


const char* TParseContext::getAtomicCounterBlockName() const
{
    const char* name = intermediate.getAtomicCounterBlockName();
    if (std::string(name) == "")
        return "gl_AtomicCounterBlock";
    else
        return name;
}
void TParseContext::finalizeAtomicCounterBlockLayout(TVariable&)
{
}

void TParseContext::setAtomicCounterBlockDefaults(TType& block) const
{
    block.getQualifier().layoutPacking = ElpStd430;
    block.getQualifier().layoutMatrix = ElmRowMajor;
}

void TParseContext::setInvariant(const TSourceLoc& loc, const char* builtin) {
    TSymbol* symbol = symbolTable.find(builtin);
    if (symbol && symbol->getType().getQualifier().isPipeOutput()) {
        if (intermediate.inIoAccessed(builtin))
            warn(loc, "changing qualification after use", "invariant", builtin);
        TSymbol* csymbol = symbolTable.copyUp(symbol);
        csymbol->getWritableType().getQualifier().invariant = true;
    }
}

void TParseContext::handlePragma(const TSourceLoc& loc, const TVector<TString>& tokens)
{
    if (pragmaCallback)
        pragmaCallback(loc.line, tokens);

    if (tokens.size() == 0)
        return;

    if (tokens[0].compare("optimize") == 0) {
        if (tokens.size() != 4) {
            error(loc, "optimize pragma syntax is incorrect", "#pragma", "");
            return;
        }

        if (tokens[1].compare("(") != 0) {
            error(loc, "\"(\" expected after 'optimize' keyword", "#pragma", "");
            return;
        }

        if (tokens[2].compare("on") == 0)
            contextPragma.optimize = true;
        else if (tokens[2].compare("off") == 0)
            contextPragma.optimize = false;
        else {
            if(relaxedErrors())
                //  If an implementation does not recognize the tokens following #pragma, then it will ignore that pragma.
                warn(loc, "\"on\" or \"off\" expected after '(' for 'optimize' pragma", "#pragma", "");
            return;
        }

        if (tokens[3].compare(")") != 0) {
            error(loc, "\")\" expected to end 'optimize' pragma", "#pragma", "");
            return;
        }
    } else if (tokens[0].compare("debug") == 0) {
        if (tokens.size() != 4) {
            error(loc, "debug pragma syntax is incorrect", "#pragma", "");
            return;
        }

        if (tokens[1].compare("(") != 0) {
            error(loc, "\"(\" expected after 'debug' keyword", "#pragma", "");
            return;
        }

        if (tokens[2].compare("on") == 0)
            contextPragma.debug = true;
        else if (tokens[2].compare("off") == 0)
            contextPragma.debug = false;
        else {
            if(relaxedErrors())
                //  If an implementation does not recognize the tokens following #pragma, then it will ignore that pragma.
                warn(loc, "\"on\" or \"off\" expected after '(' for 'debug' pragma", "#pragma", "");
            return;
        }

        if (tokens[3].compare(")") != 0) {
            error(loc, "\")\" expected to end 'debug' pragma", "#pragma", "");
            return;
        }
    } else if (spvVersion.spv > 0 && tokens[0].compare("use_storage_buffer") == 0) {
        if (tokens.size() != 1)
            error(loc, "extra tokens", "#pragma", "");
        intermediate.setUseStorageBuffer();
    } else if (spvVersion.spv > 0 && tokens[0].compare("use_vulkan_memory_model") == 0) {
        if (tokens.size() != 1)
            error(loc, "extra tokens", "#pragma", "");
        intermediate.setUseVulkanMemoryModel();
    } else if (spvVersion.spv > 0 && tokens[0].compare("use_variable_pointers") == 0) {
        if (tokens.size() != 1)
            error(loc, "extra tokens", "#pragma", "");
        if (spvVersion.spv < glslang::EShTargetSpv_1_3)
            error(loc, "requires SPIR-V 1.3", "#pragma use_variable_pointers", "");
        intermediate.setUseVariablePointers();
    } else if (tokens[0].compare("once") == 0) {
        warn(loc, "not implemented", "#pragma once", "");
    } else if (tokens[0].compare("glslang_binary_double_output") == 0) {
        intermediate.setBinaryDoubleOutput();
    } else if (spvVersion.spv > 0 && tokens[0].compare("STDGL") == 0 &&
               tokens[1].compare("invariant") == 0 && tokens[3].compare("all") == 0) {
        intermediate.setInvariantAll();
        // Set all builtin out variables invariant if declared
        setInvariant(loc, "gl_Position");
        setInvariant(loc, "gl_PointSize");
        setInvariant(loc, "gl_ClipDistance");
        setInvariant(loc, "gl_CullDistance");
        setInvariant(loc, "gl_TessLevelOuter");
        setInvariant(loc, "gl_TessLevelInner");
        setInvariant(loc, "gl_PrimitiveID");
        setInvariant(loc, "gl_Layer");
        setInvariant(loc, "gl_ViewportIndex");
        setInvariant(loc, "gl_FragDepth");
        setInvariant(loc, "gl_SampleMask");
        setInvariant(loc, "gl_ClipVertex");
        setInvariant(loc, "gl_FrontColor");
        setInvariant(loc, "gl_BackColor");
        setInvariant(loc, "gl_FrontSecondaryColor");
        setInvariant(loc, "gl_BackSecondaryColor");
        setInvariant(loc, "gl_TexCoord");
        setInvariant(loc, "gl_FogFragCoord");
        setInvariant(loc, "gl_FragColor");
        setInvariant(loc, "gl_FragData");
    }
}

//
// Handle seeing a variable identifier in the grammar.
//
TIntermTyped* TParseContext::handleVariable(const TSourceLoc& loc, TSymbol* symbol, const TString* string)
{
    TIntermTyped* node = nullptr;

    // Error check for requiring specific extensions present.
    if (symbol && symbol->getNumExtensions())
        requireExtensions(loc, symbol->getNumExtensions(), symbol->getExtensions(), symbol->getName().c_str());

    if (symbol && symbol->isReadOnly()) {
        // All shared things containing an unsized array must be copied up
        // on first use, so that all future references will share its array structure,
        // so that editing the implicit size will effect all nodes consuming it,
        // and so that editing the implicit size won't change the shared one.
        //
        // If this is a variable or a block, check it and all it contains, but if this
        // is a member of an anonymous block, check the whole block, as the whole block
        // will need to be copied up if it contains an unsized array.
        //
        // This check is being done before the block-name check further down, so guard
        // for that too.
        if (!symbol->getType().isUnusableName()) {
            if (symbol->getType().containsUnsizedArray() ||
                (symbol->getAsAnonMember() &&
                 symbol->getAsAnonMember()->getAnonContainer().getType().containsUnsizedArray()))
                makeEditable(symbol);
        }
    }

    const TVariable* variable;
    const TAnonMember* anon = symbol ? symbol->getAsAnonMember() : nullptr;
    if (anon) {
        // It was a member of an anonymous container.

        // Create a subtree for its dereference.
        variable = anon->getAnonContainer().getAsVariable();
        TIntermTyped* container = intermediate.addSymbol(*variable, loc);
        TIntermTyped* constNode = intermediate.addConstantUnion(anon->getMemberNumber(), loc);
        node = intermediate.addIndex(EOpIndexDirectStruct, container, constNode, loc);

        node->setType(*(*variable->getType().getStruct())[anon->getMemberNumber()].type);
        if (node->getType().hiddenMember())
            error(loc, "member of nameless block was not redeclared", string->c_str(), "");
    } else {
        // Not a member of an anonymous container.

        // The symbol table search was done in the lexical phase.
        // See if it was a variable.
        variable = symbol ? symbol->getAsVariable() : nullptr;
        if (variable) {
            if (variable->getType().isUnusableName()) {
                error(loc, "cannot be used (maybe an instance name is needed)", string->c_str(), "");
                variable = nullptr;
            }

            if (language == EShLangMesh && variable) {
                TLayoutGeometry primitiveType = intermediate.getOutputPrimitive();
                if ((variable->getMangledName() == "gl_PrimitiveTriangleIndicesEXT" && primitiveType != ElgTriangles) ||
                    (variable->getMangledName() == "gl_PrimitiveLineIndicesEXT" && primitiveType != ElgLines) ||
                    (variable->getMangledName() == "gl_PrimitivePointIndicesEXT" && primitiveType != ElgPoints)) {
                    error(loc, "cannot be used (ouput primitive type mismatch)", string->c_str(), "");
                    variable = nullptr;
                }
            }
        } else {
            if (symbol)
                error(loc, "variable name expected", string->c_str(), "");
        }

        // Recovery, if it wasn't found or was not a variable.
        if (! variable)
            variable = new TVariable(string, TType(EbtVoid));

        if (variable->getType().getQualifier().isFrontEndConstant())
            node = intermediate.addConstantUnion(variable->getConstArray(), variable->getType(), loc);
        else
            node = intermediate.addSymbol(*variable, loc);
    }

    if (variable->getType().getQualifier().isIo())
        intermediate.addIoAccessed(*string);

    if (variable->getType().isReference() &&
        variable->getType().getQualifier().bufferReferenceNeedsVulkanMemoryModel()) {
        intermediate.setUseVulkanMemoryModel();
    }

    return node;
}

//
// Handle seeing a base[index] dereference in the grammar.
//
TIntermTyped* TParseContext::handleBracketDereference(const TSourceLoc& loc, TIntermTyped* base, TIntermTyped* index)
{
    int indexValue = 0;
    if (index->getQualifier().isFrontEndConstant())
        indexValue = index->getAsConstantUnion()->getConstArray()[0].getIConst();

    // basic type checks...
    variableCheck(base);

    if (! base->isArray() && ! base->isMatrix() && ! base->isVector() && ! base->getType().isCoopMat() &&
        ! base->isReference()) {
        if (base->getAsSymbolNode())
            error(loc, " left of '[' is not of type array, matrix, or vector ", base->getAsSymbolNode()->getName().c_str(), "");
        else
            error(loc, " left of '[' is not of type array, matrix, or vector ", "expression", "");

        // Insert dummy error-recovery result
        return intermediate.addConstantUnion(0.0, EbtFloat, loc);
    }

    if (!base->isArray() && base->isVector()) {
        if (base->getType().contains16BitFloat())
            requireFloat16Arithmetic(loc, "[", "does not operate on types containing float16");
        if (base->getType().contains16BitInt())
            requireInt16Arithmetic(loc, "[", "does not operate on types containing (u)int16");
        if (base->getType().contains8BitInt())
            requireInt8Arithmetic(loc, "[", "does not operate on types containing (u)int8");
    }

    // check for constant folding
    if (base->getType().getQualifier().isFrontEndConstant() && index->getQualifier().isFrontEndConstant()) {
        // both base and index are front-end constants
        checkIndex(loc, base->getType(), indexValue);
        return intermediate.foldDereference(base, indexValue, loc);
    }

    // at least one of base and index is not a front-end constant variable...
    TIntermTyped* result = nullptr;

    if (base->isReference() && ! base->isArray()) {
        requireExtensions(loc, 1, &E_GL_EXT_buffer_reference2, "buffer reference indexing");
        if (base->getType().getReferentType()->containsUnsizedArray()) {
            error(loc, "cannot index reference to buffer containing an unsized array", "", "");
            result = nullptr;
        } else {
            result = intermediate.addBinaryMath(EOpAdd, base, index, loc);
            if (result != nullptr)
                result->setType(base->getType());
        }
        if (result == nullptr) {
            error(loc, "cannot index buffer reference", "", "");
            result = intermediate.addConstantUnion(0.0, EbtFloat, loc);
        }
        return result;
    }
    if (base->getAsSymbolNode() && isIoResizeArray(base->getType()))
        handleIoResizeArrayAccess(loc, base);

    if (index->getQualifier().isFrontEndConstant())
        checkIndex(loc, base->getType(), indexValue);

    if (index->getQualifier().isFrontEndConstant()) {
        if (base->getType().isUnsizedArray()) {
            base->getWritableType().updateImplicitArraySize(indexValue + 1);
            base->getWritableType().setImplicitlySized(true);
            if (base->getQualifier().builtIn == EbvClipDistance &&
                indexValue >= resources.maxClipDistances) {
                error(loc, "gl_ClipDistance", "[", "array index out of range '%d'", indexValue);
            }
            else if (base->getQualifier().builtIn == EbvCullDistance &&
                indexValue >= resources.maxCullDistances) {
                error(loc, "gl_CullDistance", "[", "array index out of range '%d'", indexValue);
            }
            // For 2D per-view builtin arrays, update the inner dimension size in parent type
            if (base->getQualifier().isPerView() && base->getQualifier().builtIn != EbvNone) {
                TIntermBinary* binaryNode = base->getAsBinaryNode();
                if (binaryNode) {
                    TType& leftType = binaryNode->getLeft()->getWritableType();
                    TArraySizes& arraySizes = *leftType.getArraySizes();
                    assert(arraySizes.getNumDims() == 2);
                    arraySizes.setDimSize(1, std::max(arraySizes.getDimSize(1), indexValue + 1));
                }
            }
        } else
            checkIndex(loc, base->getType(), indexValue);
        result = intermediate.addIndex(EOpIndexDirect, base, index, loc);
    } else {
        if (base->getType().isUnsizedArray()) {
            // we have a variable index into an unsized array, which is okay,
            // depending on the situation
            if (base->getAsSymbolNode() && isIoResizeArray(base->getType()))
                error(loc, "", "[", "array must be sized by a redeclaration or layout qualifier before being indexed with a variable");
            else {
                // it is okay for a run-time sized array
                checkRuntimeSizable(loc, *base);
            }
            base->getWritableType().setArrayVariablyIndexed();
        }
        if (base->getBasicType() == EbtBlock) {
            if (base->getQualifier().storage == EvqBuffer)
                requireProfile(base->getLoc(), ~EEsProfile, "variable indexing buffer block array");
            else if (base->getQualifier().storage == EvqUniform)
                profileRequires(base->getLoc(), EEsProfile, 320, Num_AEP_gpu_shader5, AEP_gpu_shader5,
                                "variable indexing uniform block array");
            else {
                // input/output blocks either don't exist or can't be variably indexed
            }
        } else if (language == EShLangFragment && base->getQualifier().isPipeOutput())
            requireProfile(base->getLoc(), ~EEsProfile, "variable indexing fragment shader output array");
        else if (base->getBasicType() == EbtSampler && version >= 130) {
            const char* explanation = "variable indexing sampler array";
            requireProfile(base->getLoc(), EEsProfile | ECoreProfile | ECompatibilityProfile, explanation);
            profileRequires(base->getLoc(), EEsProfile, 320, Num_AEP_gpu_shader5, AEP_gpu_shader5, explanation);
            profileRequires(base->getLoc(), ECoreProfile | ECompatibilityProfile, 400, nullptr, explanation);
        }

        result = intermediate.addIndex(EOpIndexIndirect, base, index, loc);
    }

    // Insert valid dereferenced result type
    TType newType(base->getType(), 0);
    if (base->getType().getQualifier().isConstant() && index->getQualifier().isConstant()) {
        newType.getQualifier().storage = EvqConst;
        // If base or index is a specialization constant, the result should also be a specialization constant.
        if (base->getType().getQualifier().isSpecConstant() || index->getQualifier().isSpecConstant()) {
            newType.getQualifier().makeSpecConstant();
        }
    } else {
        newType.getQualifier().storage = EvqTemporary;
        newType.getQualifier().specConstant = false;
    }
    result->setType(newType);

    inheritMemoryQualifiers(base->getQualifier(), result->getWritableType().getQualifier());

    // Propagate nonuniform
    if (base->getQualifier().isNonUniform() || index->getQualifier().isNonUniform())
        result->getWritableType().getQualifier().nonUniform = true;

    if (anyIndexLimits)
        handleIndexLimits(loc, base, index);

    return result;
}

// for ES 2.0 (version 100) limitations for almost all index operations except vertex-shader uniforms
void TParseContext::handleIndexLimits(const TSourceLoc& /*loc*/, TIntermTyped* base, TIntermTyped* index)
{
    if ((! limits.generalSamplerIndexing && base->getBasicType() == EbtSampler) ||
        (! limits.generalUniformIndexing && base->getQualifier().isUniformOrBuffer() && language != EShLangVertex) ||
        (! limits.generalAttributeMatrixVectorIndexing && base->getQualifier().isPipeInput() && language == EShLangVertex && (base->getType().isMatrix() || base->getType().isVector())) ||
        (! limits.generalConstantMatrixVectorIndexing && base->getAsConstantUnion()) ||
        (! limits.generalVariableIndexing && ! base->getType().getQualifier().isUniformOrBuffer() &&
                                             ! base->getType().getQualifier().isPipeInput() &&
                                             ! base->getType().getQualifier().isPipeOutput() &&
                                             ! base->getType().getQualifier().isConstant()) ||
        (! limits.generalVaryingIndexing && (base->getType().getQualifier().isPipeInput() ||
                                                base->getType().getQualifier().isPipeOutput()))) {
        // it's too early to know what the inductive variables are, save it for post processing
        needsIndexLimitationChecking.push_back(index);
    }
}

// Make a shared symbol have a non-shared version that can be edited by the current
// compile, such that editing its type will not change the shared version and will
// effect all nodes sharing it.
void TParseContext::makeEditable(TSymbol*& symbol)
{
    TParseContextBase::makeEditable(symbol);

    // See if it's tied to IO resizing
    if (isIoResizeArray(symbol->getType()))
        ioArraySymbolResizeList.push_back(symbol);
}

// Return true if this is a geometry shader input array or tessellation control output array
// or mesh shader output array.
bool TParseContext::isIoResizeArray(const TType& type) const
{
    return type.isArray() &&
           ((language == EShLangGeometry    && type.getQualifier().storage == EvqVaryingIn) ||
            (language == EShLangTessControl && type.getQualifier().storage == EvqVaryingOut &&
                ! type.getQualifier().patch) ||
            (language == EShLangFragment && type.getQualifier().storage == EvqVaryingIn &&
                (type.getQualifier().pervertexNV || type.getQualifier().pervertexEXT)) ||
            (language == EShLangMesh && type.getQualifier().storage == EvqVaryingOut &&
                !type.getQualifier().perTaskNV));
}

// If an array is not isIoResizeArray() but is an io array, make sure it has the right size
void TParseContext::fixIoArraySize(const TSourceLoc& loc, TType& type)
{
    if (! type.isArray() || type.getQualifier().patch || symbolTable.atBuiltInLevel())
        return;

    assert(! isIoResizeArray(type));

    if (type.getQualifier().storage != EvqVaryingIn || type.getQualifier().patch)
        return;

    if (language == EShLangTessControl || language == EShLangTessEvaluation) {
        if (type.getOuterArraySize() != resources.maxPatchVertices) {
            if (type.isSizedArray())
                error(loc, "tessellation input array size must be gl_MaxPatchVertices or implicitly sized", "[]", "");
            type.changeOuterArraySize(resources.maxPatchVertices);
        }
    }
}

// Issue any errors if the non-array object is missing arrayness WRT
// shader I/O that has array requirements.
// All arrayness checking is handled in array paths, this is for
void TParseContext::ioArrayCheck(const TSourceLoc& loc, const TType& type, const TString& identifier)
{
    if (! type.isArray() && ! symbolTable.atBuiltInLevel()) {
        if (type.getQualifier().isArrayedIo(language) && !type.getQualifier().layoutPassthrough)
            error(loc, "type must be an array:", type.getStorageQualifierString(), identifier.c_str());
    }
}

// Handle a dereference of a geometry shader input array or tessellation control output array.
// See ioArraySymbolResizeList comment in ParseHelper.h.
//
void TParseContext::handleIoResizeArrayAccess(const TSourceLoc& /*loc*/, TIntermTyped* base)
{
    TIntermSymbol* symbolNode = base->getAsSymbolNode();
    assert(symbolNode);
    if (! symbolNode)
        return;

    // fix array size, if it can be fixed and needs to be fixed (will allow variable indexing)
    if (symbolNode->getType().isUnsizedArray()) {
        int newSize = getIoArrayImplicitSize(symbolNode->getType().getQualifier());
        if (newSize > 0)
            symbolNode->getWritableType().changeOuterArraySize(newSize);
    }
}

// If there has been an input primitive declaration (geometry shader) or an output
// number of vertices declaration(tessellation shader), make sure all input array types
// match it in size.  Types come either from nodes in the AST or symbols in the
// symbol table.
//
// Types without an array size will be given one.
// Types already having a size that is wrong will get an error.
//
void TParseContext::checkIoArraysConsistency(const TSourceLoc &loc, bool tailOnly)
{
    int requiredSize = 0;
    TString featureString;
    size_t listSize = ioArraySymbolResizeList.size();
    size_t i = 0;

    // If tailOnly = true, only check the last array symbol in the list.
    if (tailOnly) {
        i = listSize - 1;
    }
    for (bool firstIteration = true; i < listSize; ++i) {
        TType &type = ioArraySymbolResizeList[i]->getWritableType();

        // As I/O array sizes don't change, fetch requiredSize only once,
        // except for mesh shaders which could have different I/O array sizes based on type qualifiers.
        if (firstIteration || (language == EShLangMesh)) {
            requiredSize = getIoArrayImplicitSize(type.getQualifier(), &featureString);
            if (requiredSize == 0)
                break;
            firstIteration = false;
        }

        checkIoArrayConsistency(loc, requiredSize, featureString.c_str(), type,
                                ioArraySymbolResizeList[i]->getName());
    }
}

int TParseContext::getIoArrayImplicitSize(const TQualifier &qualifier, TString *featureString) const
{
    int expectedSize = 0;
    TString str = "unknown";
    unsigned int maxVertices = intermediate.getVertices() != TQualifier::layoutNotSet ? intermediate.getVertices() : 0;

    if (language == EShLangGeometry) {
        expectedSize = TQualifier::mapGeometryToSize(intermediate.getInputPrimitive());
        str = TQualifier::getGeometryString(intermediate.getInputPrimitive());
    }
    else if (language == EShLangTessControl) {
        expectedSize = maxVertices;
        str = "vertices";
    } else if (language == EShLangFragment) {
        // Number of vertices for Fragment shader is always three.
        expectedSize = 3;
        str = "vertices";
    } else if (language == EShLangMesh) {
        unsigned int maxPrimitives =
            intermediate.getPrimitives() != TQualifier::layoutNotSet ? intermediate.getPrimitives() : 0;
        if (qualifier.builtIn == EbvPrimitiveIndicesNV) {
            expectedSize = maxPrimitives * TQualifier::mapGeometryToSize(intermediate.getOutputPrimitive());
            str = "max_primitives*";
            str += TQualifier::getGeometryString(intermediate.getOutputPrimitive());
        }
        else if (qualifier.builtIn == EbvPrimitiveTriangleIndicesEXT || qualifier.builtIn == EbvPrimitiveLineIndicesEXT ||
                 qualifier.builtIn == EbvPrimitivePointIndicesEXT) {
            expectedSize = maxPrimitives;
            str = "max_primitives";
        }
        else if (qualifier.isPerPrimitive()) {
            expectedSize = maxPrimitives;
            str = "max_primitives";
        }
        else {
            expectedSize = maxVertices;
            str = "max_vertices";
        }
    }
    if (featureString)
        *featureString = str;
    return expectedSize;
}

void TParseContext::checkIoArrayConsistency(const TSourceLoc& loc, int requiredSize, const char* feature, TType& type, const TString& name)
{
    if (type.isUnsizedArray())
        type.changeOuterArraySize(requiredSize);
    else if (type.getOuterArraySize() != requiredSize) {
        if (language == EShLangGeometry)
            error(loc, "inconsistent input primitive for array size of", feature, name.c_str());
        else if (language == EShLangTessControl)
            error(loc, "inconsistent output number of vertices for array size of", feature, name.c_str());
        else if (language == EShLangFragment) {
            if (type.getOuterArraySize() > requiredSize)
                error(loc, " cannot be greater than 3 for pervertexEXT", feature, name.c_str());
        }
        else if (language == EShLangMesh)
            error(loc, "inconsistent output array size of", feature, name.c_str());
        else
            assert(0);
    }
}

// Handle seeing a binary node with a math operation.
// Returns nullptr if not semantically allowed.
TIntermTyped* TParseContext::handleBinaryMath(const TSourceLoc& loc, const char* str, TOperator op, TIntermTyped* left, TIntermTyped* right)
{
    rValueErrorCheck(loc, str, left->getAsTyped());
    rValueErrorCheck(loc, str, right->getAsTyped());

    bool allowed = true;
    switch (op) {
    // TODO: Bring more source language-specific checks up from intermediate.cpp
    // to the specific parse helpers for that source language.
    case EOpLessThan:
    case EOpGreaterThan:
    case EOpLessThanEqual:
    case EOpGreaterThanEqual:
        if (! left->isScalar() || ! right->isScalar())
            allowed = false;
        break;
    default:
        break;
    }

    if (((left->getType().contains16BitFloat() || right->getType().contains16BitFloat()) && !float16Arithmetic()) ||
        ((left->getType().contains16BitInt() || right->getType().contains16BitInt()) && !int16Arithmetic()) ||
        ((left->getType().contains8BitInt() || right->getType().contains8BitInt()) && !int8Arithmetic())) {
        allowed = false;
    }

    TIntermTyped* result = nullptr;
    if (allowed) {
        if ((left->isReference() || right->isReference()))
            requireExtensions(loc, 1, &E_GL_EXT_buffer_reference2, "buffer reference math");
        result = intermediate.addBinaryMath(op, left, right, loc);
    }

    if (result == nullptr) {
        bool enhanced = intermediate.getEnhancedMsgs();
        binaryOpError(loc, str, left->getCompleteString(enhanced), right->getCompleteString(enhanced));
    }

    return result;
}

// Handle seeing a unary node with a math operation.
TIntermTyped* TParseContext::handleUnaryMath(const TSourceLoc& loc, const char* str, TOperator op, TIntermTyped* childNode)
{
    rValueErrorCheck(loc, str, childNode);

    bool allowed = true;
    if ((childNode->getType().contains16BitFloat() && !float16Arithmetic()) ||
        (childNode->getType().contains16BitInt() && !int16Arithmetic()) ||
        (childNode->getType().contains8BitInt() && !int8Arithmetic())) {
        allowed = false;
    }

    TIntermTyped* result = nullptr;
    if (allowed)
        result = intermediate.addUnaryMath(op, childNode, loc);

    if (result)
        return result;
    else {
        bool enhanced = intermediate.getEnhancedMsgs();
        unaryOpError(loc, str, childNode->getCompleteString(enhanced));
    }

    return childNode;
}

//
// Handle seeing a base.field dereference in the grammar.
//
TIntermTyped* TParseContext::handleDotDereference(const TSourceLoc& loc, TIntermTyped* base, const TString& field)
{
    variableCheck(base);

    //
    // .length() can't be resolved until we later see the function-calling syntax.
    // Save away the name in the AST for now.  Processing is completed in
    // handleLengthMethod().
    //
    if (field == "length") {
        if (base->isArray()) {
            profileRequires(loc, ENoProfile, 120, E_GL_3DL_array_objects, ".length");
            profileRequires(loc, EEsProfile, 300, nullptr, ".length");
        } else if (base->isVector() || base->isMatrix()) {
            const char* feature = ".length() on vectors and matrices";
            requireProfile(loc, ~EEsProfile, feature);
            profileRequires(loc, ~EEsProfile, 420, E_GL_ARB_shading_language_420pack, feature);
        } else if (!base->getType().isCoopMat()) {
            bool enhanced = intermediate.getEnhancedMsgs();
            error(loc, "does not operate on this type:", field.c_str(), base->getType().getCompleteString(enhanced).c_str());
            return base;
        }

        return intermediate.addMethod(base, TType(EbtInt), &field, loc);
    }

    // It's not .length() if we get to here.

    if (base->isArray()) {
        error(loc, "cannot apply to an array:", ".", field.c_str());

        return base;
    }

    if (base->getType().isCoopMat()) {
        error(loc, "cannot apply to a cooperative matrix type:", ".", field.c_str());
        return base;
    }

    // It's neither an array nor .length() if we get here,
    // leaving swizzles and struct/block dereferences.

    TIntermTyped* result = base;
    if ((base->isVector() || base->isScalar()) &&
        (base->isFloatingDomain() || base->isIntegerDomain() || base->getBasicType() == EbtBool)) {
        result = handleDotSwizzle(loc, base, field);
    } else if (base->isStruct() || base->isReference()) {
        const TTypeList* fields = base->isReference() ?
                                  base->getType().getReferentType()->getStruct() :
                                  base->getType().getStruct();
        bool fieldFound = false;
        int member;
        for (member = 0; member < (int)fields->size(); ++member) {
            if ((*fields)[member].type->getFieldName() == field) {
                fieldFound = true;
                break;
            }
        }
        if (fieldFound) {
            if (base->getType().getQualifier().isFrontEndConstant())
                result = intermediate.foldDereference(base, member, loc);
            else {
                blockMemberExtensionCheck(loc, base, member, field);
                TIntermTyped* index = intermediate.addConstantUnion(member, loc);
                result = intermediate.addIndex(EOpIndexDirectStruct, base, index, loc);
                result->setType(*(*fields)[member].type);
                if ((*fields)[member].type->getQualifier().isIo())
                    intermediate.addIoAccessed(field);
            }
            inheritMemoryQualifiers(base->getQualifier(), result->getWritableType().getQualifier());
        } else {
            auto baseSymbol = base;
            while (baseSymbol->getAsSymbolNode() == nullptr) {
                auto binaryNode = baseSymbol->getAsBinaryNode();
                if (binaryNode == nullptr) break;
                baseSymbol = binaryNode->getLeft();
            }
            if (baseSymbol->getAsSymbolNode() != nullptr) {
                TString structName;
                structName.append("\'").append(baseSymbol->getAsSymbolNode()->getName().c_str()).append("\'");
                error(loc, "no such field in structure", field.c_str(), structName.c_str());
            } else {
                error(loc, "no such field in structure", field.c_str(), "");
            }
        }
    } else
        error(loc, "does not apply to this type:", field.c_str(),
          base->getType().getCompleteString(intermediate.getEnhancedMsgs()).c_str());

    // Propagate noContraction up the dereference chain
    if (base->getQualifier().isNoContraction())
        result->getWritableType().getQualifier().setNoContraction();

    // Propagate nonuniform
    if (base->getQualifier().isNonUniform())
        result->getWritableType().getQualifier().nonUniform = true;

    return result;
}

//
// Handle seeing a base.swizzle, a subset of base.identifier in the grammar.
//
TIntermTyped* TParseContext::handleDotSwizzle(const TSourceLoc& loc, TIntermTyped* base, const TString& field)
{
    TIntermTyped* result = base;
    if (base->isScalar()) {
        const char* dotFeature = "scalar swizzle";
        requireProfile(loc, ~EEsProfile, dotFeature);
        profileRequires(loc, ~EEsProfile, 420, E_GL_ARB_shading_language_420pack, dotFeature);
    }

    TSwizzleSelectors<TVectorSelector> selectors;
    parseSwizzleSelector(loc, field, base->getVectorSize(), selectors);

    if (base->isVector() && selectors.size() != 1 && base->getType().contains16BitFloat())
        requireFloat16Arithmetic(loc, ".", "can't swizzle types containing float16");
    if (base->isVector() && selectors.size() != 1 && base->getType().contains16BitInt())
        requireInt16Arithmetic(loc, ".", "can't swizzle types containing (u)int16");
    if (base->isVector() && selectors.size() != 1 && base->getType().contains8BitInt())
        requireInt8Arithmetic(loc, ".", "can't swizzle types containing (u)int8");

    if (base->isScalar()) {
        if (selectors.size() == 1)
            return result;
        else {
            TType type(base->getBasicType(), EvqTemporary, selectors.size());
            // Swizzle operations propagate specialization-constantness
            if (base->getQualifier().isSpecConstant())
                type.getQualifier().makeSpecConstant();
            return addConstructor(loc, base, type);
        }
    }

    if (base->getType().getQualifier().isFrontEndConstant())
        result = intermediate.foldSwizzle(base, selectors, loc);
    else {
        if (selectors.size() == 1) {
            TIntermTyped* index = intermediate.addConstantUnion(selectors[0], loc);
            result = intermediate.addIndex(EOpIndexDirect, base, index, loc);
            result->setType(TType(base->getBasicType(), EvqTemporary, base->getType().getQualifier().precision));
        } else {
            TIntermTyped* index = intermediate.addSwizzle(selectors, loc);
            result = intermediate.addIndex(EOpVectorSwizzle, base, index, loc);
            result->setType(TType(base->getBasicType(), EvqTemporary, base->getType().getQualifier().precision, selectors.size()));
        }
        // Swizzle operations propagate specialization-constantness
        if (base->getType().getQualifier().isSpecConstant())
            result->getWritableType().getQualifier().makeSpecConstant();
    }

    return result;
}

void TParseContext::blockMemberExtensionCheck(const TSourceLoc& loc, const TIntermTyped* base, int member, const TString& memberName)
{
    // a block that needs extension checking is either 'base', or if arrayed,
    // one level removed to the left
    const TIntermSymbol* baseSymbol = nullptr;
    if (base->getAsBinaryNode() == nullptr)
        baseSymbol = base->getAsSymbolNode();
    else
        baseSymbol = base->getAsBinaryNode()->getLeft()->getAsSymbolNode();
    if (baseSymbol == nullptr)
        return;
    const TSymbol* symbol = symbolTable.find(baseSymbol->getName());
    if (symbol == nullptr)
        return;
    const TVariable* variable = symbol->getAsVariable();
    if (variable == nullptr)
        return;
    if (!variable->hasMemberExtensions())
        return;

    // We now have a variable that is the base of a dot reference
    // with members that need extension checking.
    if (variable->getNumMemberExtensions(member) > 0)
        requireExtensions(loc, variable->getNumMemberExtensions(member), variable->getMemberExtensions(member), memberName.c_str());
}

//
// Handle seeing a function declarator in the grammar.  This is the precursor
// to recognizing a function prototype or function definition.
//
TFunction* TParseContext::handleFunctionDeclarator(const TSourceLoc& loc, TFunction& function, bool prototype)
{
    // ES can't declare prototypes inside functions
    if (! symbolTable.atGlobalLevel())
        requireProfile(loc, ~EEsProfile, "local function declaration");

    //
    // Multiple declarations of the same function name are allowed.
    //
    // If this is a definition, the definition production code will check for redefinitions
    // (we don't know at this point if it's a definition or not).
    //
    // Redeclarations (full signature match) are allowed.  But, return types and parameter qualifiers must also match.
    //  - except ES 100, which only allows a single prototype
    //
    // ES 100 does not allow redefining, but does allow overloading of built-in functions.
    // ES 300 does not allow redefining or overloading of built-in functions.
    //
    bool builtIn;
    TSymbol* symbol = symbolTable.find(function.getMangledName(), &builtIn);
    if (symbol && symbol->getAsFunction() && builtIn)
        requireProfile(loc, ~EEsProfile, "redefinition of built-in function");
    // Check the validity of using spirv_literal qualifier
    for (int i = 0; i < function.getParamCount(); ++i) {
        if (function[i].type->getQualifier().isSpirvLiteral() && function.getBuiltInOp() != EOpSpirvInst)
            error(loc, "'spirv_literal' can only be used on functions defined with 'spirv_instruction' for argument",
                  function.getName().c_str(), "%d", i + 1);
    }

    // For function declaration with SPIR-V instruction qualifier, always ignore the built-in function and
    // respect this redeclared one.
    if (symbol && builtIn && function.getBuiltInOp() == EOpSpirvInst)
        symbol = nullptr;
    const TFunction* prevDec = symbol ? symbol->getAsFunction() : nullptr;
    if (prevDec) {
        if (prevDec->isPrototyped() && prototype)
            profileRequires(loc, EEsProfile, 300, nullptr, "multiple prototypes for same function");
        if (prevDec->getType() != function.getType())
            error(loc, "overloaded functions must have the same return type", function.getName().c_str(), "");
        if (prevDec->getSpirvInstruction() != function.getSpirvInstruction()) {
            error(loc, "overloaded functions must have the same qualifiers", function.getName().c_str(),
                  "spirv_instruction");
        }
        for (int i = 0; i < prevDec->getParamCount(); ++i) {
            if ((*prevDec)[i].type->getQualifier().storage != function[i].type->getQualifier().storage)
                error(loc, "overloaded functions must have the same parameter storage qualifiers for argument", function[i].type->getStorageQualifierString(), "%d", i+1);

            if ((*prevDec)[i].type->getQualifier().precision != function[i].type->getQualifier().precision)
                error(loc, "overloaded functions must have the same parameter precision qualifiers for argument", function[i].type->getPrecisionQualifierString(), "%d", i+1);
        }
    }

    arrayObjectCheck(loc, function.getType(), "array in function return type");

    if (prototype) {
        // All built-in functions are defined, even though they don't have a body.
        // Count their prototype as a definition instead.
        if (symbolTable.atBuiltInLevel())
            function.setDefined();
        else {
            if (prevDec && ! builtIn)
                symbol->getAsFunction()->setPrototyped();  // need a writable one, but like having prevDec as a const
            function.setPrototyped();
        }
    }

    // This insert won't actually insert it if it's a duplicate signature, but it will still check for
    // other forms of name collisions.
    if (! symbolTable.insert(function))
        error(loc, "function name is redeclaration of existing name", function.getName().c_str(), "");

    //
    // If this is a redeclaration, it could also be a definition,
    // in which case, we need to use the parameter names from this one, and not the one that's
    // being redeclared.  So, pass back this declaration, not the one in the symbol table.
    //
    return &function;
}

//
// Handle seeing the function prototype in front of a function definition in the grammar.
// The body is handled after this function returns.
//
TIntermAggregate* TParseContext::handleFunctionDefinition(const TSourceLoc& loc, TFunction& function)
{
    currentCaller = function.getMangledName();
    TSymbol* symbol = symbolTable.find(function.getMangledName());
    TFunction* prevDec = symbol ? symbol->getAsFunction() : nullptr;

    if (! prevDec)
        error(loc, "can't find function", function.getName().c_str(), "");
    // Note:  'prevDec' could be 'function' if this is the first time we've seen function
    // as it would have just been put in the symbol table.  Otherwise, we're looking up
    // an earlier occurrence.

    if (prevDec && prevDec->isDefined()) {
        // Then this function already has a body.
        error(loc, "function already has a body", function.getName().c_str(), "");
    }
    if (prevDec && ! prevDec->isDefined()) {
        prevDec->setDefined();

        // Remember the return type for later checking for RETURN statements.
        currentFunctionType = &(prevDec->getType());
    } else
        currentFunctionType = new TType(EbtVoid);
    functionReturnsValue = false;

    // Check for entry point
    if (function.getName().compare(intermediate.getEntryPointName().c_str()) == 0) {
        intermediate.setEntryPointMangledName(function.getMangledName().c_str());
        intermediate.incrementEntryPointCount();
        inMain = true;
    } else
        inMain = false;

    //
    // Raise error message if main function takes any parameters or returns anything other than void
    //
    if (inMain) {
        if (function.getParamCount() > 0)
            error(loc, "function cannot take any parameter(s)", function.getName().c_str(), "");
        if (function.getType().getBasicType() != EbtVoid)
            error(loc, "", function.getType().getBasicTypeString().c_str(), "entry point cannot return a value");
    }

    //
    // New symbol table scope for body of function plus its arguments
    //
    symbolTable.push();

    //
    // Insert parameters into the symbol table.
    // If the parameter has no name, it's not an error, just don't insert it
    // (could be used for unused args).
    //
    // Also, accumulate the list of parameters into the HIL, so lower level code
    // knows where to find parameters.
    //
    TIntermAggregate* paramNodes = new TIntermAggregate;
    for (int i = 0; i < function.getParamCount(); i++) {
        TParameter& param = function[i];
        if (param.name != nullptr) {
            TVariable *variable = new TVariable(param.name, *param.type);

            // Insert the parameters with name in the symbol table.
            if (! symbolTable.insert(*variable))
                error(loc, "redefinition", variable->getName().c_str(), "");
            else {
                // Transfer ownership of name pointer to symbol table.
                param.name = nullptr;

                // Add the parameter to the HIL
                paramNodes = intermediate.growAggregate(paramNodes,
                                                        intermediate.addSymbol(*variable, loc),
                                                        loc);
            }
        } else
            paramNodes = intermediate.growAggregate(paramNodes, intermediate.addSymbol(*param.type, loc), loc);
    }
    intermediate.setAggregateOperator(paramNodes, EOpParameters, TType(EbtVoid), loc);
    loopNestingLevel = 0;
    statementNestingLevel = 0;
    controlFlowNestingLevel = 0;
    postEntryPointReturn = false;

    return paramNodes;
}

//
// Handle seeing function call syntax in the grammar, which could be any of
//  - .length() method
//  - constructor
//  - a call to a built-in function mapped to an operator
//  - a call to a built-in function that will remain a function call (e.g., texturing)
//  - user function
//  - subroutine call (not implemented yet)
//
TIntermTyped* TParseContext::handleFunctionCall(const TSourceLoc& loc, TFunction* function, TIntermNode* arguments)
{
    TIntermTyped* result = nullptr;

    if (spvVersion.vulkan != 0 && spvVersion.vulkanRelaxed) {
        // allow calls that are invalid in Vulkan Semantics to be invisibily
        // remapped to equivalent valid functions
        result = vkRelaxedRemapFunctionCall(loc, function, arguments);
        if (result)
            return result;
    }

    if (function->getBuiltInOp() == EOpArrayLength)
        result = handleLengthMethod(loc, function, arguments);
    else if (function->getBuiltInOp() != EOpNull) {
        //
        // Then this should be a constructor.
        // Don't go through the symbol table for constructors.
        // Their parameters will be verified algorithmically.
        //
        TType type(EbtVoid);  // use this to get the type back
        if (! constructorError(loc, arguments, *function, function->getBuiltInOp(), type)) {
            //
            // It's a constructor, of type 'type'.
            //
            result = addConstructor(loc, arguments, type);
            if (result == nullptr)
                error(loc, "cannot construct with these arguments", type.getCompleteString(intermediate.getEnhancedMsgs()).c_str(), "");
        }
    } else {
        //
        // Find it in the symbol table.
        //
        const TFunction* fnCandidate;
        bool builtIn {false};
        fnCandidate = findFunction(loc, *function, builtIn);
        if (fnCandidate) {
            // This is a declared function that might map to
            //  - a built-in operator,
            //  - a built-in function not mapped to an operator, or
            //  - a user function.

            // Error check for a function requiring specific extensions present.
            if (builtIn && fnCandidate->getNumExtensions())
                requireExtensions(loc, fnCandidate->getNumExtensions(), fnCandidate->getExtensions(), fnCandidate->getName().c_str());

            if (builtIn && fnCandidate->getType().contains16BitFloat())
                requireFloat16Arithmetic(loc, "built-in function", "float16 types can only be in uniform block or buffer storage");
            if (builtIn && fnCandidate->getType().contains16BitInt())
                requireInt16Arithmetic(loc, "built-in function", "(u)int16 types can only be in uniform block or buffer storage");
            if (builtIn && fnCandidate->getType().contains8BitInt())
                requireInt8Arithmetic(loc, "built-in function", "(u)int8 types can only be in uniform block or buffer storage");

            if (arguments != nullptr) {
                // Make sure qualifications work for these arguments.
                TIntermAggregate* aggregate = arguments->getAsAggregate();
                for (int i = 0; i < fnCandidate->getParamCount(); ++i) {
                    // At this early point there is a slight ambiguity between whether an aggregate 'arguments'
                    // is the single argument itself or its children are the arguments.  Only one argument
                    // means take 'arguments' itself as the one argument.
                    TIntermNode* arg = fnCandidate->getParamCount() == 1 ? arguments : (aggregate ? aggregate->getSequence()[i] : arguments);
                    TQualifier& formalQualifier = (*fnCandidate)[i].type->getQualifier();
                    if (formalQualifier.isParamOutput()) {
                        if (lValueErrorCheck(arguments->getLoc(), "assign", arg->getAsTyped()))
                            error(arguments->getLoc(), "Non-L-value cannot be passed for 'out' or 'inout' parameters.", "out", "");
                    }
                    if (formalQualifier.isSpirvLiteral()) {
                        if (!arg->getAsTyped()->getQualifier().isFrontEndConstant()) {
                            error(arguments->getLoc(),
                                  "Non front-end constant expressions cannot be passed for 'spirv_literal' parameters.",
                                  "spirv_literal", "");
                        }
                    }
                    const TType& argType = arg->getAsTyped()->getType();
                    const TQualifier& argQualifier = argType.getQualifier();
                    bool containsBindlessSampler = intermediate.getBindlessMode() && argType.containsSampler();
                    if (argQualifier.isMemory() && !containsBindlessSampler && (argType.containsOpaque() || argType.isReference())) {
                        const char* message = "argument cannot drop memory qualifier when passed to formal parameter";
                        if (argQualifier.volatil && ! formalQualifier.volatil)
                            error(arguments->getLoc(), message, "volatile", "");
                        if (argQualifier.coherent && ! (formalQualifier.devicecoherent || formalQualifier.coherent))
                            error(arguments->getLoc(), message, "coherent", "");
                        if (argQualifier.devicecoherent && ! (formalQualifier.devicecoherent || formalQualifier.coherent))
                            error(arguments->getLoc(), message, "devicecoherent", "");
                        if (argQualifier.queuefamilycoherent && ! (formalQualifier.queuefamilycoherent || formalQualifier.devicecoherent || formalQualifier.coherent))
                            error(arguments->getLoc(), message, "queuefamilycoherent", "");
                        if (argQualifier.workgroupcoherent && ! (formalQualifier.workgroupcoherent || formalQualifier.queuefamilycoherent || formalQualifier.devicecoherent || formalQualifier.coherent))
                            error(arguments->getLoc(), message, "workgroupcoherent", "");
                        if (argQualifier.subgroupcoherent && ! (formalQualifier.subgroupcoherent || formalQualifier.workgroupcoherent || formalQualifier.queuefamilycoherent || formalQualifier.devicecoherent || formalQualifier.coherent))
                            error(arguments->getLoc(), message, "subgroupcoherent", "");
                        if (argQualifier.readonly && ! formalQualifier.readonly)
                            error(arguments->getLoc(), message, "readonly", "");
                        if (argQualifier.writeonly && ! formalQualifier.writeonly)
                            error(arguments->getLoc(), message, "writeonly", "");
                        // Don't check 'restrict', it is different than the rest:
                        // "...but only restrict can be taken away from a calling argument, by a formal parameter that
                        // lacks the restrict qualifier..."
                    }
                    if (!builtIn && argQualifier.getFormat() != formalQualifier.getFormat()) {
                        // we have mismatched formats, which should only be allowed if writeonly
                        // and at least one format is unknown
                        if (!formalQualifier.isWriteOnly() || (formalQualifier.getFormat() != ElfNone &&
                                                                  argQualifier.getFormat() != ElfNone))
                            error(arguments->getLoc(), "image formats must match", "format", "");
                    }
                    if (builtIn && arg->getAsTyped()->getType().contains16BitFloat())
                        requireFloat16Arithmetic(arguments->getLoc(), "built-in function", "float16 types can only be in uniform block or buffer storage");
                    if (builtIn && arg->getAsTyped()->getType().contains16BitInt())
                        requireInt16Arithmetic(arguments->getLoc(), "built-in function", "(u)int16 types can only be in uniform block or buffer storage");
                    if (builtIn && arg->getAsTyped()->getType().contains8BitInt())
                        requireInt8Arithmetic(arguments->getLoc(), "built-in function", "(u)int8 types can only be in uniform block or buffer storage");

                    // TODO 4.5 functionality:  A shader will fail to compile
                    // if the value passed to the memargument of an atomic memory function does not correspond to a buffer or
                    // shared variable. It is acceptable to pass an element of an array or a single component of a vector to the
                    // memargument of an atomic memory function, as long as the underlying array or vector is a buffer or
                    // shared variable.
                }

                // Convert 'in' arguments
                addInputArgumentConversions(*fnCandidate, arguments);  // arguments may be modified if it's just a single argument node
            }

            if (builtIn && fnCandidate->getBuiltInOp() != EOpNull) {
                // A function call mapped to a built-in operation.
                result = handleBuiltInFunctionCall(loc, arguments, *fnCandidate);
            } else if (fnCandidate->getBuiltInOp() == EOpSpirvInst) {
                // When SPIR-V instruction qualifier is specified, the function call is still mapped to a built-in operation.
                result = handleBuiltInFunctionCall(loc, arguments, *fnCandidate);
            } else {
                // This is a function call not mapped to built-in operator.
                // It could still be a built-in function, but only if PureOperatorBuiltins == false.
                result = intermediate.setAggregateOperator(arguments, EOpFunctionCall, fnCandidate->getType(), loc);
                TIntermAggregate* call = result->getAsAggregate();
                call->setName(fnCandidate->getMangledName());

                // this is how we know whether the given function is a built-in function or a user-defined function
                // if builtIn == false, it's a userDefined -> could be an overloaded built-in function also
                // if builtIn == true, it's definitely a built-in function with EOpNull
                if (! builtIn) {
                    call->setUserDefined();
                    if (symbolTable.atGlobalLevel()) {
                        requireProfile(loc, ~EEsProfile, "calling user function from global scope");
                        intermediate.addToCallGraph(infoSink, "main(", fnCandidate->getMangledName());
                    } else
                        intermediate.addToCallGraph(infoSink, currentCaller, fnCandidate->getMangledName());
                }

                if (builtIn)
                    nonOpBuiltInCheck(loc, *fnCandidate, *call);
                else
                    userFunctionCallCheck(loc, *call);
            }

            // Convert 'out' arguments.  If it was a constant folded built-in, it won't be an aggregate anymore.
            // Built-ins with a single argument aren't called with an aggregate, but they also don't have an output.
            // Also, build the qualifier list for user function calls, which are always called with an aggregate.
            if (result->getAsAggregate()) {
                TQualifierList& qualifierList = result->getAsAggregate()->getQualifierList();
                for (int i = 0; i < fnCandidate->getParamCount(); ++i) {
                    TStorageQualifier qual = (*fnCandidate)[i].type->getQualifier().storage;
                    qualifierList.push_back(qual);
                }
                result = addOutputArgumentConversions(*fnCandidate, *result->getAsAggregate());
            }

            if (result->getAsTyped()->getType().isCoopMat() &&
               !result->getAsTyped()->getType().isParameterized()) {
                assert(fnCandidate->getBuiltInOp() == EOpCooperativeMatrixMulAdd ||
                       fnCandidate->getBuiltInOp() == EOpCooperativeMatrixMulAddNV);

                result->setType(result->getAsAggregate()->getSequence()[2]->getAsTyped()->getType());
            }
        }
    }

    // generic error recovery
    // TODO: simplification: localize all the error recoveries that look like this, and taking type into account to reduce cascades
    if (result == nullptr)
        result = intermediate.addConstantUnion(0.0, EbtFloat, loc);

    return result;
}

TIntermTyped* TParseContext::handleBuiltInFunctionCall(TSourceLoc loc, TIntermNode* arguments,
                                                       const TFunction& function)
{
    checkLocation(loc, function.getBuiltInOp());
    TIntermTyped *result = intermediate.addBuiltInFunctionCall(loc, function.getBuiltInOp(),
                                                               function.getParamCount() == 1,
                                                               arguments, function.getType());
    if (result != nullptr && obeyPrecisionQualifiers())
        computeBuiltinPrecisions(*result, function);

    if (result == nullptr) {
        if (arguments == nullptr)
            error(loc, " wrong operand type", "Internal Error",
                                      "built in unary operator function.  Type: %s", "");
        else
            error(arguments->getLoc(), " wrong operand type", "Internal Error",
                                      "built in unary operator function.  Type: %s",
                                      static_cast<TIntermTyped*>(arguments)->getCompleteString(intermediate.getEnhancedMsgs()).c_str());
    } else if (result->getAsOperator())
        builtInOpCheck(loc, function, *result->getAsOperator());

    // Special handling for function call with SPIR-V instruction qualifier specified
    if (function.getBuiltInOp() == EOpSpirvInst) {
        if (auto agg = result->getAsAggregate()) {
            // Propogate spirv_by_reference/spirv_literal from parameters to arguments
            auto& sequence = agg->getSequence();
            for (unsigned i = 0; i < sequence.size(); ++i) {
                if (function[i].type->getQualifier().isSpirvByReference())
                    sequence[i]->getAsTyped()->getQualifier().setSpirvByReference();
                if (function[i].type->getQualifier().isSpirvLiteral())
                    sequence[i]->getAsTyped()->getQualifier().setSpirvLiteral();
            }

            // Attach the function call to SPIR-V intruction
            agg->setSpirvInstruction(function.getSpirvInstruction());
        } else if (auto unaryNode = result->getAsUnaryNode()) {
            // Propogate spirv_by_reference/spirv_literal from parameters to arguments
            if (function[0].type->getQualifier().isSpirvByReference())
                unaryNode->getOperand()->getQualifier().setSpirvByReference();
            if (function[0].type->getQualifier().isSpirvLiteral())
                unaryNode->getOperand()->getQualifier().setSpirvLiteral();

            // Attach the function call to SPIR-V intruction
            unaryNode->setSpirvInstruction(function.getSpirvInstruction());
        } else
            assert(0);
    }

    return result;
}

// "The operation of a built-in function can have a different precision
// qualification than the precision qualification of the resulting value.
// These two precision qualifications are established as follows.
//
// The precision qualification of the operation of a built-in function is
// based on the precision qualification of its input arguments and formal
// parameters:  When a formal parameter specifies a precision qualifier,
// that is used, otherwise, the precision qualification of the calling
// argument is used.  The highest precision of these will be the precision
// qualification of the operation of the built-in function. Generally,
// this is applied across all arguments to a built-in function, with the
// exceptions being:
//   - bitfieldExtract and bitfieldInsert ignore the 'offset' and 'bits'
//     arguments.
//   - interpolateAt* functions only look at the 'interpolant' argument.
//
// The precision qualification of the result of a built-in function is
// determined in one of the following ways:
//
//   - For the texture sampling, image load, and image store functions,
//     the precision of the return type matches the precision of the
//     sampler type
//
//   Otherwise:
//
//   - For prototypes that do not specify a resulting precision qualifier,
//     the precision will be the same as the precision of the operation.
//
//   - For prototypes that do specify a resulting precision qualifier,
//     the specified precision qualifier is the precision qualification of
//     the result."
//
void TParseContext::computeBuiltinPrecisions(TIntermTyped& node, const TFunction& function)
{
    TPrecisionQualifier operationPrecision = EpqNone;
    TPrecisionQualifier resultPrecision = EpqNone;

    TIntermOperator* opNode = node.getAsOperator();
    if (opNode == nullptr)
        return;

    if (TIntermUnary* unaryNode = node.getAsUnaryNode()) {
        operationPrecision = std::max(function[0].type->getQualifier().precision,
                                      unaryNode->getOperand()->getType().getQualifier().precision);
        if (function.getType().getBasicType() != EbtBool)
            resultPrecision = function.getType().getQualifier().precision == EpqNone ?
                                        operationPrecision :
                                        function.getType().getQualifier().precision;
    } else if (TIntermAggregate* agg = node.getAsAggregate()) {
        TIntermSequence& sequence = agg->getSequence();
        unsigned int numArgs = (unsigned int)sequence.size();
        switch (agg->getOp()) {
        case EOpBitfieldExtract:
            numArgs = 1;
            break;
        case EOpBitfieldInsert:
            numArgs = 2;
            break;
        case EOpInterpolateAtCentroid:
        case EOpInterpolateAtOffset:
        case EOpInterpolateAtSample:
            numArgs = 1;
            break;
        case EOpDebugPrintf:
            numArgs = 0;
            break;
        default:
            break;
        }
        // find the maximum precision from the arguments and parameters
        for (unsigned int arg = 0; arg < numArgs; ++arg) {
            operationPrecision = std::max(operationPrecision, sequence[arg]->getAsTyped()->getQualifier().precision);
            operationPrecision = std::max(operationPrecision, function[arg].type->getQualifier().precision);
        }
        // compute the result precision
        if (agg->isSampling() ||
            agg->getOp() == EOpImageLoad || agg->getOp() == EOpImageStore ||
            agg->getOp() == EOpImageLoadLod || agg->getOp() == EOpImageStoreLod)
            resultPrecision = sequence[0]->getAsTyped()->getQualifier().precision;
        else if (function.getType().getBasicType() != EbtBool)
            resultPrecision = function.getType().getQualifier().precision == EpqNone ?
                                        operationPrecision :
                                        function.getType().getQualifier().precision;
    }

    // Propagate precision through this node and its children. That algorithm stops
    // when a precision is found, so start by clearing this subroot precision
    opNode->getQualifier().precision = EpqNone;
    if (operationPrecision != EpqNone) {
        opNode->propagatePrecision(operationPrecision);
        opNode->setOperationPrecision(operationPrecision);
    }
    // Now, set the result precision, which might not match
    opNode->getQualifier().precision = resultPrecision;
}

TIntermNode* TParseContext::handleReturnValue(const TSourceLoc& loc, TIntermTyped* value)
{
    storage16BitAssignmentCheck(loc, value->getType(), "return");

    functionReturnsValue = true;
    TIntermBranch* branch = nullptr;
    if (currentFunctionType->getBasicType() == EbtVoid) {
        error(loc, "void function cannot return a value", "return", "");
        branch = intermediate.addBranch(EOpReturn, loc);
    } else if (*currentFunctionType != value->getType()) {
        TIntermTyped* converted = intermediate.addConversion(EOpReturn, *currentFunctionType, value);
        if (converted) {
            if (*currentFunctionType != converted->getType())
                error(loc, "cannot convert return value to function return type", "return", "");
            if (version < 420)
                warn(loc, "type conversion on return values was not explicitly allowed until version 420",
                     "return", "");
            branch = intermediate.addBranch(EOpReturn, converted, loc);
        } else {
            error(loc, "type does not match, or is not convertible to, the function's return type", "return", "");
            branch = intermediate.addBranch(EOpReturn, value, loc);
        }
    } else {
        if (value->getType().isTexture() || value->getType().isImage()) {
            if (!extensionTurnedOn(E_GL_ARB_bindless_texture))
                error(loc, "sampler or image can be used as return type only when the extension GL_ARB_bindless_texture enabled", "return", "");
        }
        branch = intermediate.addBranch(EOpReturn, value, loc);
    }
    branch->updatePrecision(currentFunctionType->getQualifier().precision);
    return branch;
}

// See if the operation is being done in an illegal location.
void TParseContext::checkLocation(const TSourceLoc& loc, TOperator op)
{
    switch (op) {
    case EOpBarrier:
        if (language == EShLangTessControl) {
            if (controlFlowNestingLevel > 0)
                error(loc, "tessellation control barrier() cannot be placed within flow control", "", "");
            if (! inMain)
                error(loc, "tessellation control barrier() must be in main()", "", "");
            else if (postEntryPointReturn)
                error(loc, "tessellation control barrier() cannot be placed after a return from main()", "", "");
        }
        break;
    case EOpBeginInvocationInterlock:
        if (language != EShLangFragment)
            error(loc, "beginInvocationInterlockARB() must be in a fragment shader", "", "");
        if (! inMain)
            error(loc, "beginInvocationInterlockARB() must be in main()", "", "");
        else if (postEntryPointReturn)
            error(loc, "beginInvocationInterlockARB() cannot be placed after a return from main()", "", "");
        if (controlFlowNestingLevel > 0)
            error(loc, "beginInvocationInterlockARB() cannot be placed within flow control", "", "");

        if (beginInvocationInterlockCount > 0)
            error(loc, "beginInvocationInterlockARB() must only be called once", "", "");
        if (endInvocationInterlockCount > 0)
            error(loc, "beginInvocationInterlockARB() must be called before endInvocationInterlockARB()", "", "");

        beginInvocationInterlockCount++;

        // default to pixel_interlock_ordered
        if (intermediate.getInterlockOrdering() == EioNone)
            intermediate.setInterlockOrdering(EioPixelInterlockOrdered);
        break;
    case EOpEndInvocationInterlock:
        if (language != EShLangFragment)
            error(loc, "endInvocationInterlockARB() must be in a fragment shader", "", "");
        if (! inMain)
            error(loc, "endInvocationInterlockARB() must be in main()", "", "");
        else if (postEntryPointReturn)
            error(loc, "endInvocationInterlockARB() cannot be placed after a return from main()", "", "");
        if (controlFlowNestingLevel > 0)
            error(loc, "endInvocationInterlockARB() cannot be placed within flow control", "", "");

        if (endInvocationInterlockCount > 0)
            error(loc, "endInvocationInterlockARB() must only be called once", "", "");
        if (beginInvocationInterlockCount == 0)
            error(loc, "beginInvocationInterlockARB() must be called before endInvocationInterlockARB()", "", "");

        endInvocationInterlockCount++;
        break;
    default:
        break;
    }
}

// Finish processing object.length(). This started earlier in handleDotDereference(), where
// the ".length" part was recognized and semantically checked, and finished here where the
// function syntax "()" is recognized.
//
// Return resulting tree node.
TIntermTyped* TParseContext::handleLengthMethod(const TSourceLoc& loc, TFunction* function, TIntermNode* intermNode)
{
    int length = 0;

    if (function->getParamCount() > 0)
        error(loc, "method does not accept any arguments", function->getName().c_str(), "");
    else {
        const TType& type = intermNode->getAsTyped()->getType();
        if (type.isArray()) {
            if (type.isUnsizedArray()) {
                if (intermNode->getAsSymbolNode() && isIoResizeArray(type)) {
                    // We could be between a layout declaration that gives a built-in io array implicit size and
                    // a user redeclaration of that array, meaning we have to substitute its implicit size here
                    // without actually redeclaring the array.  (It is an error to use a member before the
                    // redeclaration, but not an error to use the array name itself.)
                    const TString& name = intermNode->getAsSymbolNode()->getName();
                    if (name == "gl_in" || name == "gl_out" || name == "gl_MeshVerticesNV" ||
                        name == "gl_MeshPrimitivesNV") {
                        length = getIoArrayImplicitSize(type.getQualifier());
                    }
                }
                if (length == 0) {
                    if (intermNode->getAsSymbolNode() && isIoResizeArray(type))
                        error(loc, "", function->getName().c_str(), "array must first be sized by a redeclaration or layout qualifier");
                    else if (isRuntimeLength(*intermNode->getAsTyped())) {
                        // Create a unary op and let the back end handle it
                        return intermediate.addBuiltInFunctionCall(loc, EOpArrayLength, true, intermNode, TType(EbtInt));
                    } else
                        error(loc, "", function->getName().c_str(), "array must be declared with a size before using this method");
                }
            } else if (type.getOuterArrayNode()) {
                // If the array's outer size is specified by an intermediate node, it means the array's length
                // was specified by a specialization constant. In such a case, we should return the node of the
                // specialization constants to represent the length.
                return type.getOuterArrayNode();
            } else
                length = type.getOuterArraySize();
        } else if (type.isMatrix())
            length = type.getMatrixCols();
        else if (type.isVector())
            length = type.getVectorSize();
        else if (type.isCoopMat())
            return intermediate.addBuiltInFunctionCall(loc, EOpArrayLength, true, intermNode, TType(EbtInt));
        else {
            // we should not get here, because earlier semantic checking should have prevented this path
            error(loc, ".length()", "unexpected use of .length()", "");
        }
    }

    if (length == 0)
        length = 1;

    return intermediate.addConstantUnion(length, loc);
}

//
// Add any needed implicit conversions for function-call arguments to input parameters.
//
void TParseContext::addInputArgumentConversions(const TFunction& function, TIntermNode*& arguments) const
{
    TIntermAggregate* aggregate = arguments->getAsAggregate();

    // Process each argument's conversion
    for (int i = 0; i < function.getParamCount(); ++i) {
        // At this early point there is a slight ambiguity between whether an aggregate 'arguments'
        // is the single argument itself or its children are the arguments.  Only one argument
        // means take 'arguments' itself as the one argument.
        TIntermTyped* arg = function.getParamCount() == 1 ? arguments->getAsTyped() : (aggregate ? aggregate->getSequence()[i]->getAsTyped() : arguments->getAsTyped());
        if (*function[i].type != arg->getType()) {
            if (function[i].type->getQualifier().isParamInput() &&
               !function[i].type->isCoopMat()) {
                // In-qualified arguments just need an extra node added above the argument to
                // convert to the correct type.
                arg = intermediate.addConversion(EOpFunctionCall, *function[i].type, arg);
                if (arg) {
                    if (function.getParamCount() == 1)
                        arguments = arg;
                    else {
                        if (aggregate)
                            aggregate->getSequence()[i] = arg;
                        else
                            arguments = arg;
                    }
                }
            }
        }
    }
}

//
// Add any needed implicit output conversions for function-call arguments.  This
// can require a new tree topology, complicated further by whether the function
// has a return value.
//
// Returns a node of a subtree that evaluates to the return value of the function.
//
TIntermTyped* TParseContext::addOutputArgumentConversions(const TFunction& function, TIntermAggregate& intermNode) const
{
    TIntermSequence& arguments = intermNode.getSequence();

    // Will there be any output conversions?
    bool outputConversions = false;
    for (int i = 0; i < function.getParamCount(); ++i) {
        if (*function[i].type != arguments[i]->getAsTyped()->getType() && function[i].type->getQualifier().isParamOutput()) {
            outputConversions = true;
            break;
        }
    }

    if (! outputConversions)
        return &intermNode;

    // Setup for the new tree, if needed:
    //
    // Output conversions need a different tree topology.
    // Out-qualified arguments need a temporary of the correct type, with the call
    // followed by an assignment of the temporary to the original argument:
    //     void: function(arg, ...)  ->        (          function(tempArg, ...), arg = tempArg, ...)
    //     ret = function(arg, ...)  ->  ret = (tempRet = function(tempArg, ...), arg = tempArg, ..., tempRet)
    // Where the "tempArg" type needs no conversion as an argument, but will convert on assignment.
    TIntermTyped* conversionTree = nullptr;
    TVariable* tempRet = nullptr;
    if (intermNode.getBasicType() != EbtVoid) {
        // do the "tempRet = function(...), " bit from above
        tempRet = makeInternalVariable("tempReturn", intermNode.getType());
        TIntermSymbol* tempRetNode = intermediate.addSymbol(*tempRet, intermNode.getLoc());
        conversionTree = intermediate.addAssign(EOpAssign, tempRetNode, &intermNode, intermNode.getLoc());
    } else
        conversionTree = &intermNode;

    conversionTree = intermediate.makeAggregate(conversionTree);

    // Process each argument's conversion
    for (int i = 0; i < function.getParamCount(); ++i) {
        if (*function[i].type != arguments[i]->getAsTyped()->getType()) {
            if (function[i].type->getQualifier().isParamOutput()) {
                // Out-qualified arguments need to use the topology set up above.
                // do the " ...(tempArg, ...), arg = tempArg" bit from above
                TType paramType;
                paramType.shallowCopy(*function[i].type);
                if (arguments[i]->getAsTyped()->getType().isParameterized() &&
                    !paramType.isParameterized()) {
                    paramType.shallowCopy(arguments[i]->getAsTyped()->getType());
                    paramType.copyTypeParameters(*arguments[i]->getAsTyped()->getType().getTypeParameters());
                }
                TVariable* tempArg = makeInternalVariable("tempArg", paramType);
                tempArg->getWritableType().getQualifier().makeTemporary();
                TIntermSymbol* tempArgNode = intermediate.addSymbol(*tempArg, intermNode.getLoc());
                TIntermTyped* tempAssign = intermediate.addAssign(EOpAssign, arguments[i]->getAsTyped(), tempArgNode, arguments[i]->getLoc());
                conversionTree = intermediate.growAggregate(conversionTree, tempAssign, arguments[i]->getLoc());
                // replace the argument with another node for the same tempArg variable
                arguments[i] = intermediate.addSymbol(*tempArg, intermNode.getLoc());
            }
        }
    }

    // Finalize the tree topology (see bigger comment above).
    if (tempRet) {
        // do the "..., tempRet" bit from above
        TIntermSymbol* tempRetNode = intermediate.addSymbol(*tempRet, intermNode.getLoc());
        conversionTree = intermediate.growAggregate(conversionTree, tempRetNode, intermNode.getLoc());
    }
    conversionTree = intermediate.setAggregateOperator(conversionTree, EOpComma, intermNode.getType(), intermNode.getLoc());

    return conversionTree;
}

TIntermTyped* TParseContext::addAssign(const TSourceLoc& loc, TOperator op, TIntermTyped* left, TIntermTyped* right)
{
    if ((op == EOpAddAssign || op == EOpSubAssign) && left->isReference())
        requireExtensions(loc, 1, &E_GL_EXT_buffer_reference2, "+= and -= on a buffer reference");

    if (op == EOpAssign && left->getBasicType() == EbtSampler && right->getBasicType() == EbtSampler)
        requireExtensions(loc, 1, &E_GL_ARB_bindless_texture, "sampler assignment for bindless texture");

    return intermediate.addAssign(op, left, right, loc);
}

void TParseContext::memorySemanticsCheck(const TSourceLoc& loc, const TFunction& fnCandidate, const TIntermOperator& callNode)
{
    const TIntermSequence* argp = &callNode.getAsAggregate()->getSequence();

    //const int gl_SemanticsRelaxed         = 0x0;
    const int gl_SemanticsAcquire         = 0x2;
    const int gl_SemanticsRelease         = 0x4;
    const int gl_SemanticsAcquireRelease  = 0x8;
    const int gl_SemanticsMakeAvailable   = 0x2000;
    const int gl_SemanticsMakeVisible     = 0x4000;
    const int gl_SemanticsVolatile        = 0x8000;

    //const int gl_StorageSemanticsNone     = 0x0;
    const int gl_StorageSemanticsBuffer   = 0x40;
    const int gl_StorageSemanticsShared   = 0x100;
    const int gl_StorageSemanticsImage    = 0x800;
    const int gl_StorageSemanticsOutput   = 0x1000;


    unsigned int semantics = 0, storageClassSemantics = 0;
    unsigned int semantics2 = 0, storageClassSemantics2 = 0;

    const TIntermTyped* arg0 = (*argp)[0]->getAsTyped();
    const bool isMS = arg0->getBasicType() == EbtSampler && arg0->getType().getSampler().isMultiSample();

    // Grab the semantics and storage class semantics from the operands, based on opcode
    switch (callNode.getOp()) {
    case EOpAtomicAdd:
    case EOpAtomicSubtract:
    case EOpAtomicMin:
    case EOpAtomicMax:
    case EOpAtomicAnd:
    case EOpAtomicOr:
    case EOpAtomicXor:
    case EOpAtomicExchange:
    case EOpAtomicStore:
        storageClassSemantics = (*argp)[3]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[4]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;
    case EOpAtomicLoad:
        storageClassSemantics = (*argp)[2]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[3]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;
    case EOpAtomicCompSwap:
        storageClassSemantics = (*argp)[4]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[5]->getAsConstantUnion()->getConstArray()[0].getIConst();
        storageClassSemantics2 = (*argp)[6]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics2 = (*argp)[7]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;

    case EOpImageAtomicAdd:
    case EOpImageAtomicMin:
    case EOpImageAtomicMax:
    case EOpImageAtomicAnd:
    case EOpImageAtomicOr:
    case EOpImageAtomicXor:
    case EOpImageAtomicExchange:
    case EOpImageAtomicStore:
        storageClassSemantics = (*argp)[isMS ? 5 : 4]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[isMS ? 6 : 5]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;
    case EOpImageAtomicLoad:
        storageClassSemantics = (*argp)[isMS ? 4 : 3]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[isMS ? 5 : 4]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;
    case EOpImageAtomicCompSwap:
        storageClassSemantics = (*argp)[isMS ? 6 : 5]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[isMS ? 7 : 6]->getAsConstantUnion()->getConstArray()[0].getIConst();
        storageClassSemantics2 = (*argp)[isMS ? 8 : 7]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics2 = (*argp)[isMS ? 9 : 8]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;

    case EOpBarrier:
        storageClassSemantics = (*argp)[2]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[3]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;
    case EOpMemoryBarrier:
        storageClassSemantics = (*argp)[1]->getAsConstantUnion()->getConstArray()[0].getIConst();
        semantics = (*argp)[2]->getAsConstantUnion()->getConstArray()[0].getIConst();
        break;
    default:
        break;
    }

    if ((semantics & gl_SemanticsAcquire) &&
        (callNode.getOp() == EOpAtomicStore || callNode.getOp() == EOpImageAtomicStore)) {
        error(loc, "gl_SemanticsAcquire must not be used with (image) atomic store",
              fnCandidate.getName().c_str(), "");
    }
    if ((semantics & gl_SemanticsRelease) &&
        (callNode.getOp() == EOpAtomicLoad || callNode.getOp() == EOpImageAtomicLoad)) {
        error(loc, "gl_SemanticsRelease must not be used with (image) atomic load",
              fnCandidate.getName().c_str(), "");
    }
    if ((semantics & gl_SemanticsAcquireRelease) &&
        (callNode.getOp() == EOpAtomicStore || callNode.getOp() == EOpImageAtomicStore ||
         callNode.getOp() == EOpAtomicLoad  || callNode.getOp() == EOpImageAtomicLoad)) {
        error(loc, "gl_SemanticsAcquireRelease must not be used with (image) atomic load/store",
              fnCandidate.getName().c_str(), "");
    }
    if (((semantics | semantics2) & ~(gl_SemanticsAcquire |
                                      gl_SemanticsRelease |
                                      gl_SemanticsAcquireRelease |
                                      gl_SemanticsMakeAvailable |
                                      gl_SemanticsMakeVisible |
                                      gl_SemanticsVolatile))) {
        error(loc, "Invalid semantics value", fnCandidate.getName().c_str(), "");
    }
    if (((storageClassSemantics | storageClassSemantics2) & ~(gl_StorageSemanticsBuffer |
                                                              gl_StorageSemanticsShared |
                                                              gl_StorageSemanticsImage |
                                                              gl_StorageSemanticsOutput))) {
        error(loc, "Invalid storage class semantics value", fnCandidate.getName().c_str(), "");
    }

    if (callNode.getOp() == EOpMemoryBarrier) {
        if (!IsPow2(semantics & (gl_SemanticsAcquire | gl_SemanticsRelease | gl_SemanticsAcquireRelease))) {
            error(loc, "Semantics must include exactly one of gl_SemanticsRelease, gl_SemanticsAcquire, or "
                       "gl_SemanticsAcquireRelease", fnCandidate.getName().c_str(), "");
        }
    } else {
        if (semantics & (gl_SemanticsAcquire | gl_SemanticsRelease | gl_SemanticsAcquireRelease)) {
            if (!IsPow2(semantics & (gl_SemanticsAcquire | gl_SemanticsRelease | gl_SemanticsAcquireRelease))) {
                error(loc, "Semantics must not include multiple of gl_SemanticsRelease, gl_SemanticsAcquire, or "
                           "gl_SemanticsAcquireRelease", fnCandidate.getName().c_str(), "");
            }
        }
        if (semantics2 & (gl_SemanticsAcquire | gl_SemanticsRelease | gl_SemanticsAcquireRelease)) {
            if (!IsPow2(semantics2 & (gl_SemanticsAcquire | gl_SemanticsRelease | gl_SemanticsAcquireRelease))) {
                error(loc, "semUnequal must not include multiple of gl_SemanticsRelease, gl_SemanticsAcquire, or "
                           "gl_SemanticsAcquireRelease", fnCandidate.getName().c_str(), "");
            }
        }
    }
    if (callNode.getOp() == EOpMemoryBarrier) {
        if (storageClassSemantics == 0) {
            error(loc, "Storage class semantics must not be zero", fnCandidate.getName().c_str(), "");
        }
    }
    if (callNode.getOp() == EOpBarrier && semantics != 0 && storageClassSemantics == 0) {
        error(loc, "Storage class semantics must not be zero", fnCandidate.getName().c_str(), "");
    }
    if ((callNode.getOp() == EOpAtomicCompSwap || callNode.getOp() == EOpImageAtomicCompSwap) &&
        (semantics2 & (gl_SemanticsRelease | gl_SemanticsAcquireRelease))) {
        error(loc, "semUnequal must not be gl_SemanticsRelease or gl_SemanticsAcquireRelease",
              fnCandidate.getName().c_str(), "");
    }
    if ((semantics & gl_SemanticsMakeAvailable) &&
        !(semantics & (gl_SemanticsRelease | gl_SemanticsAcquireRelease))) {
        error(loc, "gl_SemanticsMakeAvailable requires gl_SemanticsRelease or gl_SemanticsAcquireRelease",
              fnCandidate.getName().c_str(), "");
    }
    if ((semantics & gl_SemanticsMakeVisible) &&
        !(semantics & (gl_SemanticsAcquire | gl_SemanticsAcquireRelease))) {
        error(loc, "gl_SemanticsMakeVisible requires gl_SemanticsAcquire or gl_SemanticsAcquireRelease",
              fnCandidate.getName().c_str(), "");
    }
    if ((semantics & gl_SemanticsVolatile) &&
        (callNode.getOp() == EOpMemoryBarrier || callNode.getOp() == EOpBarrier)) {
        error(loc, "gl_SemanticsVolatile must not be used with memoryBarrier or controlBarrier",
              fnCandidate.getName().c_str(), "");
    }
    if ((callNode.getOp() == EOpAtomicCompSwap || callNode.getOp() == EOpImageAtomicCompSwap) &&
        ((semantics ^ semantics2) & gl_SemanticsVolatile)) {
        error(loc, "semEqual and semUnequal must either both include gl_SemanticsVolatile or neither",
              fnCandidate.getName().c_str(), "");
    }
}

//
// Do additional checking of built-in function calls that is not caught
// by normal semantic checks on argument type, extension tagging, etc.
//
// Assumes there has been a semantically correct match to a built-in function prototype.
//
void TParseContext::builtInOpCheck(const TSourceLoc& loc, const TFunction& fnCandidate, TIntermOperator& callNode)
{
    // Set up convenience accessors to the argument(s).  There is almost always
    // multiple arguments for the cases below, but when there might be one,
    // check the unaryArg first.
    const TIntermSequence* argp = nullptr;   // confusing to use [] syntax on a pointer, so this is to help get a reference
    const TIntermTyped* unaryArg = nullptr;
    const TIntermTyped* arg0 = nullptr;
    if (callNode.getAsAggregate()) {
        argp = &callNode.getAsAggregate()->getSequence();
        if (argp->size() > 0)
            arg0 = (*argp)[0]->getAsTyped();
    } else {
        assert(callNode.getAsUnaryNode());
        unaryArg = callNode.getAsUnaryNode()->getOperand();
        arg0 = unaryArg;
    }

    TString featureString;
    const char* feature = nullptr;
    switch (callNode.getOp()) {
    case EOpTextureGather:
    case EOpTextureGatherOffset:
    case EOpTextureGatherOffsets:
    {
        // Figure out which variants are allowed by what extensions,
        // and what arguments must be constant for which situations.

        featureString = fnCandidate.getName();
        featureString += "(...)";
        feature = featureString.c_str();
        profileRequires(loc, EEsProfile, 310, nullptr, feature);
        int compArg = -1;  // track which argument, if any, is the constant component argument
        switch (callNode.getOp()) {
        case EOpTextureGather:
            // More than two arguments needs gpu_shader5, and rectangular or shadow needs gpu_shader5,
            // otherwise, need GL_ARB_texture_gather.
            if (fnCandidate.getParamCount() > 2 || fnCandidate[0].type->getSampler().dim == EsdRect || fnCandidate[0].type->getSampler().shadow) {
                profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_gpu_shader5, feature);
                if (! fnCandidate[0].type->getSampler().shadow)
                    compArg = 2;
            } else
                profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_texture_gather, feature);
            break;
        case EOpTextureGatherOffset:
            // GL_ARB_texture_gather is good enough for 2D non-shadow textures with no component argument
            if (fnCandidate[0].type->getSampler().dim == Esd2D && ! fnCandidate[0].type->getSampler().shadow && fnCandidate.getParamCount() == 3)
                profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_texture_gather, feature);
            else
                profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_gpu_shader5, feature);
            if (! (*argp)[fnCandidate[0].type->getSampler().shadow ? 3 : 2]->getAsConstantUnion())
                profileRequires(loc, EEsProfile, 320, Num_AEP_gpu_shader5, AEP_gpu_shader5,
                                "non-constant offset argument");
            if (! fnCandidate[0].type->getSampler().shadow)
                compArg = 3;
            break;
        case EOpTextureGatherOffsets:
            profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_gpu_shader5, feature);
            if (! fnCandidate[0].type->getSampler().shadow)
                compArg = 3;
            // check for constant offsets
            if (! (*argp)[fnCandidate[0].type->getSampler().shadow ? 3 : 2]->getAsConstantUnion())
                error(loc, "must be a compile-time constant:", feature, "offsets argument");
            break;
        default:
            break;
        }

        if (compArg > 0 && compArg < fnCandidate.getParamCount()) {
            if ((*argp)[compArg]->getAsConstantUnion()) {
                int value = (*argp)[compArg]->getAsConstantUnion()->getConstArray()[0].getIConst();
                if (value < 0 || value > 3)
                    error(loc, "must be 0, 1, 2, or 3:", feature, "component argument");
            } else
                error(loc, "must be a compile-time constant:", feature, "component argument");
        }

        bool bias = false;
        if (callNode.getOp() == EOpTextureGather)
            bias = fnCandidate.getParamCount() > 3;
        else if (callNode.getOp() == EOpTextureGatherOffset ||
                 callNode.getOp() == EOpTextureGatherOffsets)
            bias = fnCandidate.getParamCount() > 4;

        if (bias) {
            featureString = fnCandidate.getName();
            featureString += "with bias argument";
            feature = featureString.c_str();
            profileRequires(loc, ~EEsProfile, 450, nullptr, feature);
            requireExtensions(loc, 1, &E_GL_AMD_texture_gather_bias_lod, feature);
        }
        break;
    }
    case EOpSparseTextureGather:
    case EOpSparseTextureGatherOffset:
    case EOpSparseTextureGatherOffsets:
    {
        bool bias = false;
        if (callNode.getOp() == EOpSparseTextureGather)
            bias = fnCandidate.getParamCount() > 4;
        else if (callNode.getOp() == EOpSparseTextureGatherOffset ||
                 callNode.getOp() == EOpSparseTextureGatherOffsets)
            bias = fnCandidate.getParamCount() > 5;

        if (bias) {
            featureString = fnCandidate.getName();
            featureString += "with bias argument";
            feature = featureString.c_str();
            profileRequires(loc, ~EEsProfile, 450, nullptr, feature);
            requireExtensions(loc, 1, &E_GL_AMD_texture_gather_bias_lod, feature);
        }
        // As per GL_ARB_sparse_texture2 extension "Offsets" parameter must be constant integral expression
        // for sparseTextureGatherOffsetsARB just as textureGatherOffsets
        if (callNode.getOp() == EOpSparseTextureGatherOffsets) {
            int offsetsArg = arg0->getType().getSampler().shadow ? 3 : 2;
            if (!(*argp)[offsetsArg]->getAsConstantUnion())
                error(loc, "argument must be compile-time constant", "offsets", "");
        }
        break;
    }

    case EOpSparseTextureGatherLod:
    case EOpSparseTextureGatherLodOffset:
    case EOpSparseTextureGatherLodOffsets:
    {
        requireExtensions(loc, 1, &E_GL_ARB_sparse_texture2, fnCandidate.getName().c_str());
        break;
    }

    case EOpSwizzleInvocations:
    {
        if (! (*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "offset", "");
        else {
            unsigned offset[4] = {};
            offset[0] = (*argp)[1]->getAsConstantUnion()->getConstArray()[0].getUConst();
            offset[1] = (*argp)[1]->getAsConstantUnion()->getConstArray()[1].getUConst();
            offset[2] = (*argp)[1]->getAsConstantUnion()->getConstArray()[2].getUConst();
            offset[3] = (*argp)[1]->getAsConstantUnion()->getConstArray()[3].getUConst();
            if (offset[0] > 3 || offset[1] > 3 || offset[2] > 3 || offset[3] > 3)
                error(loc, "components must be in the range [0, 3]", "offset", "");
        }

        break;
    }

    case EOpSwizzleInvocationsMasked:
    {
        if (! (*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "mask", "");
        else {
            unsigned mask[3] = {};
            mask[0] = (*argp)[1]->getAsConstantUnion()->getConstArray()[0].getUConst();
            mask[1] = (*argp)[1]->getAsConstantUnion()->getConstArray()[1].getUConst();
            mask[2] = (*argp)[1]->getAsConstantUnion()->getConstArray()[2].getUConst();
            if (mask[0] > 31 || mask[1] > 31 || mask[2] > 31)
                error(loc, "components must be in the range [0, 31]", "mask", "");
        }

        break;
    }

    case EOpTextureOffset:
    case EOpTextureFetchOffset:
    case EOpTextureProjOffset:
    case EOpTextureLodOffset:
    case EOpTextureProjLodOffset:
    case EOpTextureGradOffset:
    case EOpTextureProjGradOffset:
    {
        // Handle texture-offset limits checking
        // Pick which argument has to hold constant offsets
        int arg = -1;
        switch (callNode.getOp()) {
        case EOpTextureOffset:          arg = 2;  break;
        case EOpTextureFetchOffset:     arg = (arg0->getType().getSampler().isRect()) ? 2 : 3; break;
        case EOpTextureProjOffset:      arg = 2;  break;
        case EOpTextureLodOffset:       arg = 3;  break;
        case EOpTextureProjLodOffset:   arg = 3;  break;
        case EOpTextureGradOffset:      arg = 4;  break;
        case EOpTextureProjGradOffset:  arg = 4;  break;
        default:
            assert(0);
            break;
        }

        if (arg > 0) {

            bool f16ShadowCompare = (*argp)[1]->getAsTyped()->getBasicType() == EbtFloat16 &&
                                    arg0->getType().getSampler().shadow;
            if (f16ShadowCompare)
                ++arg;
            if (! (*argp)[arg]->getAsTyped()->getQualifier().isConstant())
                error(loc, "argument must be compile-time constant", "texel offset", "");
            else if ((*argp)[arg]->getAsConstantUnion()) {
                const TType& type = (*argp)[arg]->getAsTyped()->getType();
                for (int c = 0; c < type.getVectorSize(); ++c) {
                    int offset = (*argp)[arg]->getAsConstantUnion()->getConstArray()[c].getIConst();
                    if (offset > resources.maxProgramTexelOffset || offset < resources.minProgramTexelOffset)
                        error(loc, "value is out of range:", "texel offset",
                              "[gl_MinProgramTexelOffset, gl_MaxProgramTexelOffset]");
                }
            }

            if (callNode.getOp() == EOpTextureOffset) {
                TSampler s = arg0->getType().getSampler();
                if (s.is2D() && s.isArrayed() && s.isShadow()) {
                    if (isEsProfile())
                        error(loc, "TextureOffset does not support sampler2DArrayShadow : ", "sampler", "ES Profile");
                    else if (version <= 420)
                        error(loc, "TextureOffset does not support sampler2DArrayShadow : ", "sampler", "version <= 420");
                }
            }
        }

        break;
    }

    case EOpTraceNV:
        if (!(*argp)[10]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "payload number", "a");
        break;
    case EOpTraceRayMotionNV:
        if (!(*argp)[11]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "payload number", "a");
        break;
    case EOpTraceKHR:
        if (!(*argp)[10]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "payload number", "a");
        else {
            unsigned int location = (*argp)[10]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(0, location) < 0)
                error(loc, "with layout(location =", "no rayPayloadEXT/rayPayloadInEXT declared", "%d)", location);
        }
        break;
    case EOpExecuteCallableNV:
        if (!(*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "callable data number", "");
        break;
    case EOpExecuteCallableKHR:
        if (!(*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "callable data number", "");
        else {
            unsigned int location = (*argp)[1]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(1, location) < 0)
                error(loc, "with layout(location =", "no callableDataEXT/callableDataInEXT declared", "%d)", location);
        }
        break;

    case EOpHitObjectTraceRayNV:
        if (!(*argp)[11]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "payload number", "");
        else {
            unsigned int location = (*argp)[11]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(0, location) < 0)
                error(loc, "with layout(location =", "no rayPayloadEXT/rayPayloadInEXT declared", "%d)", location);
        }
        break;
    case EOpHitObjectTraceRayMotionNV:
        if (!(*argp)[12]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "payload number", "");
        else {
            unsigned int location = (*argp)[12]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(0, location) < 0)
                error(loc, "with layout(location =", "no rayPayloadEXT/rayPayloadInEXT declared", "%d)", location);
        }
        break;
    case EOpHitObjectExecuteShaderNV:
        if (!(*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "payload number", "");
        else {
            unsigned int location = (*argp)[1]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(0, location) < 0)
                error(loc, "with layout(location =", "no rayPayloadEXT/rayPayloadInEXT declared", "%d)", location);
        }
        break;
    case EOpHitObjectRecordHitNV:
        if (!(*argp)[12]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "hitobjectattribute number", "");
        else {
            unsigned int location = (*argp)[12]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(2, location) < 0)
                error(loc, "with layout(location =", "no hitObjectAttributeNV declared", "%d)", location);
        }
        break;
    case EOpHitObjectRecordHitMotionNV:
        if (!(*argp)[13]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "hitobjectattribute number", "");
        else {
            unsigned int location = (*argp)[13]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(2, location) < 0)
                error(loc, "with layout(location =", "no hitObjectAttributeNV declared", "%d)", location);
        }
        break;
    case EOpHitObjectRecordHitWithIndexNV:
        if (!(*argp)[11]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "hitobjectattribute number", "");
        else {
            unsigned int location = (*argp)[11]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(2, location) < 0)
                error(loc, "with layout(location =", "no hitObjectAttributeNV declared", "%d)", location);
        }
        break;
    case EOpHitObjectRecordHitWithIndexMotionNV:
        if (!(*argp)[12]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "hitobjectattribute number", "");
        else {
            unsigned int location = (*argp)[12]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(2, location) < 0)
                error(loc, "with layout(location =", "no hitObjectAttributeNV declared", "%d)", location);
        }
        break;
    case EOpHitObjectGetAttributesNV:
        if (!(*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "hitobjectattribute number", "");
        else {
            unsigned int location = (*argp)[1]->getAsConstantUnion()->getAsConstantUnion()->getConstArray()[0].getUConst();
            if (!extensionTurnedOn(E_GL_EXT_spirv_intrinsics) && intermediate.checkLocationRT(2, location) < 0)
                error(loc, "with layout(location =", "no hitObjectAttributeNV declared", "%d)", location);
        }
        break;

    case EOpRayQueryGetIntersectionType:
    case EOpRayQueryGetIntersectionT:
    case EOpRayQueryGetIntersectionInstanceCustomIndex:
    case EOpRayQueryGetIntersectionInstanceId:
    case EOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffset:
    case EOpRayQueryGetIntersectionGeometryIndex:
    case EOpRayQueryGetIntersectionPrimitiveIndex:
    case EOpRayQueryGetIntersectionBarycentrics:
    case EOpRayQueryGetIntersectionFrontFace:
    case EOpRayQueryGetIntersectionObjectRayDirection:
    case EOpRayQueryGetIntersectionObjectRayOrigin:
    case EOpRayQueryGetIntersectionObjectToWorld:
    case EOpRayQueryGetIntersectionWorldToObject:
    case EOpRayQueryGetIntersectionTriangleVertexPositionsEXT:
        if (!(*argp)[1]->getAsConstantUnion())
            error(loc, "argument must be compile-time constant", "committed", "");
        break;

    case EOpTextureQuerySamples:
    case EOpImageQuerySamples:
        // GL_ARB_shader_texture_image_samples
        profileRequires(loc, ~EEsProfile, 450, E_GL_ARB_shader_texture_image_samples, "textureSamples and imageSamples");
        break;

    case EOpImageAtomicAdd:
    case EOpImageAtomicMin:
    case EOpImageAtomicMax:
    case EOpImageAtomicAnd:
    case EOpImageAtomicOr:
    case EOpImageAtomicXor:
    case EOpImageAtomicExchange:
    case EOpImageAtomicCompSwap:
    case EOpImageAtomicLoad:
    case EOpImageAtomicStore:
    {
        // Make sure the image types have the correct layout() format and correct argument types
        const TType& imageType = arg0->getType();
        if (imageType.getSampler().type == EbtInt || imageType.getSampler().type == EbtUint ||
            imageType.getSampler().type == EbtInt64 || imageType.getSampler().type == EbtUint64) {
            if (imageType.getQualifier().getFormat() != ElfR32i && imageType.getQualifier().getFormat() != ElfR32ui &&
                imageType.getQualifier().getFormat() != ElfR64i && imageType.getQualifier().getFormat() != ElfR64ui)
                error(loc, "only supported on image with format r32i or r32ui", fnCandidate.getName().c_str(), "");
            if (callNode.getType().getBasicType() == EbtInt64 && imageType.getQualifier().getFormat() != ElfR64i)
                error(loc, "only supported on image with format r64i", fnCandidate.getName().c_str(), "");
            else if (callNode.getType().getBasicType() == EbtUint64 && imageType.getQualifier().getFormat() != ElfR64ui)
                error(loc, "only supported on image with format r64ui", fnCandidate.getName().c_str(), "");
        } else if (imageType.getSampler().type == EbtFloat) {
            if (fnCandidate.getName().compare(0, 19, "imageAtomicExchange") == 0) {
                // imageAtomicExchange doesn't require an extension
            } else if ((fnCandidate.getName().compare(0, 14, "imageAtomicAdd") == 0) ||
                       (fnCandidate.getName().compare(0, 15, "imageAtomicLoad") == 0) ||
                       (fnCandidate.getName().compare(0, 16, "imageAtomicStore") == 0)) {
                requireExtensions(loc, 1, &E_GL_EXT_shader_atomic_float, fnCandidate.getName().c_str());
            } else if ((fnCandidate.getName().compare(0, 14, "imageAtomicMin") == 0) ||
                       (fnCandidate.getName().compare(0, 14, "imageAtomicMax") == 0)) {
                requireExtensions(loc, 1, &E_GL_EXT_shader_atomic_float2, fnCandidate.getName().c_str());
            } else {
                error(loc, "only supported on integer images", fnCandidate.getName().c_str(), "");
            }
            if (imageType.getQualifier().getFormat() != ElfR32f && isEsProfile())
                error(loc, "only supported on image with format r32f", fnCandidate.getName().c_str(), "");
        } else {
            error(loc, "not supported on this image type", fnCandidate.getName().c_str(), "");
        }

        const size_t maxArgs = imageType.getSampler().isMultiSample() ? 5 : 4;
        if (argp->size() > maxArgs) {
            requireExtensions(loc, 1, &E_GL_KHR_memory_scope_semantics, fnCandidate.getName().c_str());
            memorySemanticsCheck(loc, fnCandidate, callNode);
        }

        break;
    }

    case EOpAtomicAdd:
    case EOpAtomicSubtract:
    case EOpAtomicMin:
    case EOpAtomicMax:
    case EOpAtomicAnd:
    case EOpAtomicOr:
    case EOpAtomicXor:
    case EOpAtomicExchange:
    case EOpAtomicCompSwap:
    case EOpAtomicLoad:
    case EOpAtomicStore:
    {
        if (argp->size() > 3) {
            requireExtensions(loc, 1, &E_GL_KHR_memory_scope_semantics, fnCandidate.getName().c_str());
            memorySemanticsCheck(loc, fnCandidate, callNode);
            if ((callNode.getOp() == EOpAtomicAdd || callNode.getOp() == EOpAtomicExchange ||
                callNode.getOp() == EOpAtomicLoad || callNode.getOp() == EOpAtomicStore) &&
                (arg0->getType().getBasicType() == EbtFloat ||
                 arg0->getType().getBasicType() == EbtDouble)) {
                requireExtensions(loc, 1, &E_GL_EXT_shader_atomic_float, fnCandidate.getName().c_str());
            } else if ((callNode.getOp() == EOpAtomicAdd || callNode.getOp() == EOpAtomicExchange ||
                        callNode.getOp() == EOpAtomicLoad || callNode.getOp() == EOpAtomicStore ||
                        callNode.getOp() == EOpAtomicMin || callNode.getOp() == EOpAtomicMax) &&
                       arg0->getType().isFloatingDomain()) {
                requireExtensions(loc, 1, &E_GL_EXT_shader_atomic_float2, fnCandidate.getName().c_str());
            }
        } else if (arg0->getType().getBasicType() == EbtInt64 || arg0->getType().getBasicType() == EbtUint64) {
            const char* const extensions[2] = { E_GL_NV_shader_atomic_int64,
                                                E_GL_EXT_shader_atomic_int64 };
            requireExtensions(loc, 2, extensions, fnCandidate.getName().c_str());
        } else if ((callNode.getOp() == EOpAtomicAdd || callNode.getOp() == EOpAtomicExchange) &&
                   (arg0->getType().getBasicType() == EbtFloat ||
                    arg0->getType().getBasicType() == EbtDouble)) {
            requireExtensions(loc, 1, &E_GL_EXT_shader_atomic_float, fnCandidate.getName().c_str());
        } else if ((callNode.getOp() == EOpAtomicAdd || callNode.getOp() == EOpAtomicExchange ||
                    callNode.getOp() == EOpAtomicLoad || callNode.getOp() == EOpAtomicStore ||
                    callNode.getOp() == EOpAtomicMin || callNode.getOp() == EOpAtomicMax) &&
                   arg0->getType().isFloatingDomain()) {
            requireExtensions(loc, 1, &E_GL_EXT_shader_atomic_float2, fnCandidate.getName().c_str());
        }

        const TIntermTyped* base = TIntermediate::findLValueBase(arg0, true , true);
        const TType* refType = (base->getType().isReference()) ? base->getType().getReferentType() : nullptr;
        const TQualifier& qualifier = (refType != nullptr) ? refType->getQualifier() : base->getType().getQualifier();
        if (qualifier.storage != EvqShared && qualifier.storage != EvqBuffer && qualifier.storage != EvqtaskPayloadSharedEXT)
            error(loc,"Atomic memory function can only be used for shader storage block member or shared variable.",
            fnCandidate.getName().c_str(), "");

        break;
    }

    case EOpInterpolateAtCentroid:
    case EOpInterpolateAtSample:
    case EOpInterpolateAtOffset:
    case EOpInterpolateAtVertex:
        // Make sure the first argument is an interpolant, or an array element of an interpolant
        if (arg0->getType().getQualifier().storage != EvqVaryingIn) {
            // It might still be an array element.
            //
            // We could check more, but the semantics of the first argument are already met; the
            // only way to turn an array into a float/vec* is array dereference and swizzle.
            //
            // ES and desktop 4.3 and earlier:  swizzles may not be used
            // desktop 4.4 and later: swizzles may be used
            bool swizzleOkay = (!isEsProfile()) && (version >= 440);
            const TIntermTyped* base = TIntermediate::findLValueBase(arg0, swizzleOkay);
            if (base == nullptr || base->getType().getQualifier().storage != EvqVaryingIn)
                error(loc, "first argument must be an interpolant, or interpolant-array element", fnCandidate.getName().c_str(), "");
        }

        if (callNode.getOp() == EOpInterpolateAtVertex) {
            if (!arg0->getType().getQualifier().isExplicitInterpolation())
                error(loc, "argument must be qualified as __explicitInterpAMD in", "interpolant", "");
            else {
                if (! (*argp)[1]->getAsConstantUnion())
                    error(loc, "argument must be compile-time constant", "vertex index", "");
                else {
                    unsigned vertexIdx = (*argp)[1]->getAsConstantUnion()->getConstArray()[0].getUConst();
                    if (vertexIdx > 2)
                        error(loc, "must be in the range [0, 2]", "vertex index", "");
                }
            }
        }
        break;

    case EOpEmitStreamVertex:
    case EOpEndStreamPrimitive:
        if (version == 150)
            requireExtensions(loc, 1, &E_GL_ARB_gpu_shader5, "if the verison is 150 , the EmitStreamVertex and EndStreamPrimitive only support at extension GL_ARB_gpu_shader5");
        intermediate.setMultiStream();
        break;

    case EOpSubgroupClusteredAdd:
    case EOpSubgroupClusteredMul:
    case EOpSubgroupClusteredMin:
    case EOpSubgroupClusteredMax:
    case EOpSubgroupClusteredAnd:
    case EOpSubgroupClusteredOr:
    case EOpSubgroupClusteredXor:
        // The <clusterSize> as used in the subgroupClustered<op>() operations must be:
        // - An integral constant expression.
        // - At least 1.
        // - A power of 2.
        if ((*argp)[1]->getAsConstantUnion() == nullptr)
            error(loc, "argument must be compile-time constant", "cluster size", "");
        else {
            int size = (*argp)[1]->getAsConstantUnion()->getConstArray()[0].getIConst();
            if (size < 1)
                error(loc, "argument must be at least 1", "cluster size", "");
            else if (!IsPow2(size))
                error(loc, "argument must be a power of 2", "cluster size", "");
        }
        break;

    case EOpSubgroupBroadcast:
    case EOpSubgroupQuadBroadcast:
        if (spvVersion.spv < EShTargetSpv_1_5) {
            // <id> must be an integral constant expression.
            if ((*argp)[1]->getAsConstantUnion() == nullptr)
                error(loc, "argument must be compile-time constant", "id", "");
        }
        break;

    case EOpBarrier:
    case EOpMemoryBarrier:
        if (argp->size() > 0) {
            requireExtensions(loc, 1, &E_GL_KHR_memory_scope_semantics, fnCandidate.getName().c_str());
            memorySemanticsCheck(loc, fnCandidate, callNode);
        }
        break;

    case EOpMix:
        if (profile == EEsProfile && version < 310) {
            // Look for specific signatures
            if ((*argp)[0]->getAsTyped()->getBasicType() != EbtFloat &&
                (*argp)[1]->getAsTyped()->getBasicType() != EbtFloat &&
                (*argp)[2]->getAsTyped()->getBasicType() == EbtBool) {
                requireExtensions(loc, 1, &E_GL_EXT_shader_integer_mix, "specific signature of builtin mix");
            }
        }

        if (profile != EEsProfile && version < 450) {
            if ((*argp)[0]->getAsTyped()->getBasicType() != EbtFloat &&
                (*argp)[0]->getAsTyped()->getBasicType() != EbtDouble &&
                (*argp)[1]->getAsTyped()->getBasicType() != EbtFloat &&
                (*argp)[1]->getAsTyped()->getBasicType() != EbtDouble &&
                (*argp)[2]->getAsTyped()->getBasicType() == EbtBool) {
                requireExtensions(loc, 1, &E_GL_EXT_shader_integer_mix, fnCandidate.getName().c_str());
            }
        }

        break;

    default:
        break;
    }

    // Texture operations on texture objects (aside from texelFetch on a
    // textureBuffer) require EXT_samplerless_texture_functions.
    switch (callNode.getOp()) {
    case EOpTextureQuerySize:
    case EOpTextureQueryLevels:
    case EOpTextureQuerySamples:
    case EOpTextureFetch:
    case EOpTextureFetchOffset:
    {
        const TSampler& sampler = fnCandidate[0].type->getSampler();

        const bool isTexture = sampler.isTexture() && !sampler.isCombined();
        const bool isBuffer = sampler.isBuffer();
        const bool isFetch = callNode.getOp() == EOpTextureFetch || callNode.getOp() == EOpTextureFetchOffset;

        if (isTexture && (!isBuffer || !isFetch))
            requireExtensions(loc, 1, &E_GL_EXT_samplerless_texture_functions, fnCandidate.getName().c_str());

        break;
    }

    default:
        break;
    }

    if (callNode.isSubgroup()) {
        // these require SPIR-V 1.3
        if (spvVersion.spv > 0 && spvVersion.spv < EShTargetSpv_1_3)
            error(loc, "requires SPIR-V 1.3", "subgroup op", "");

        // Check that if extended types are being used that the correct extensions are enabled.
        if (arg0 != nullptr) {
            const TType& type = arg0->getType();
            bool enhanced = intermediate.getEnhancedMsgs();
            switch (type.getBasicType()) {
            default:
                break;
            case EbtInt8:
            case EbtUint8:
                requireExtensions(loc, 1, &E_GL_EXT_shader_subgroup_extended_types_int8, type.getCompleteString(enhanced).c_str());
                break;
            case EbtInt16:
            case EbtUint16:
                requireExtensions(loc, 1, &E_GL_EXT_shader_subgroup_extended_types_int16, type.getCompleteString(enhanced).c_str());
                break;
            case EbtInt64:
            case EbtUint64:
                requireExtensions(loc, 1, &E_GL_EXT_shader_subgroup_extended_types_int64, type.getCompleteString(enhanced).c_str());
                break;
            case EbtFloat16:
                requireExtensions(loc, 1, &E_GL_EXT_shader_subgroup_extended_types_float16, type.getCompleteString(enhanced).c_str());
                break;
            }
        }
    }
}

extern bool PureOperatorBuiltins;

// Deprecated!  Use PureOperatorBuiltins == true instead, in which case this
// functionality is handled in builtInOpCheck() instead of here.
//
// Do additional checking of built-in function calls that were not mapped
// to built-in operations (e.g., texturing functions).
//
// Assumes there has been a semantically correct match to a built-in function.
//
void TParseContext::nonOpBuiltInCheck(const TSourceLoc& loc, const TFunction& fnCandidate, TIntermAggregate& callNode)
{
    // Further maintenance of this function is deprecated, because the "correct"
    // future-oriented design is to not have to do string compares on function names.

    // If PureOperatorBuiltins == true, then all built-ins should be mapped
    // to a TOperator, and this function would then never get called.

    assert(PureOperatorBuiltins == false);

    // built-in texturing functions get their return value precision from the precision of the sampler
    if (fnCandidate.getType().getQualifier().precision == EpqNone &&
        fnCandidate.getParamCount() > 0 && fnCandidate[0].type->getBasicType() == EbtSampler)
        callNode.getQualifier().precision = callNode.getSequence()[0]->getAsTyped()->getQualifier().precision;

    if (fnCandidate.getName().compare(0, 7, "texture") == 0) {
        if (fnCandidate.getName().compare(0, 13, "textureGather") == 0) {
            TString featureString = fnCandidate.getName() + "(...)";
            const char* feature = featureString.c_str();
            profileRequires(loc, EEsProfile, 310, nullptr, feature);

            int compArg = -1;  // track which argument, if any, is the constant component argument
            if (fnCandidate.getName().compare("textureGatherOffset") == 0) {
                // GL_ARB_texture_gather is good enough for 2D non-shadow textures with no component argument
                if (fnCandidate[0].type->getSampler().dim == Esd2D && ! fnCandidate[0].type->getSampler().shadow && fnCandidate.getParamCount() == 3)
                    profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_texture_gather, feature);
                else
                    profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_gpu_shader5, feature);
                int offsetArg = fnCandidate[0].type->getSampler().shadow ? 3 : 2;
                if (! callNode.getSequence()[offsetArg]->getAsConstantUnion())
                    profileRequires(loc, EEsProfile, 320, Num_AEP_gpu_shader5, AEP_gpu_shader5,
                                    "non-constant offset argument");
                if (! fnCandidate[0].type->getSampler().shadow)
                    compArg = 3;
            } else if (fnCandidate.getName().compare("textureGatherOffsets") == 0) {
                profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_gpu_shader5, feature);
                if (! fnCandidate[0].type->getSampler().shadow)
                    compArg = 3;
                // check for constant offsets
                int offsetArg = fnCandidate[0].type->getSampler().shadow ? 3 : 2;
                if (! callNode.getSequence()[offsetArg]->getAsConstantUnion())
                    error(loc, "must be a compile-time constant:", feature, "offsets argument");
            } else if (fnCandidate.getName().compare("textureGather") == 0) {
                // More than two arguments needs gpu_shader5, and rectangular or shadow needs gpu_shader5,
                // otherwise, need GL_ARB_texture_gather.
                if (fnCandidate.getParamCount() > 2 || fnCandidate[0].type->getSampler().dim == EsdRect || fnCandidate[0].type->getSampler().shadow) {
                    profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_gpu_shader5, feature);
                    if (! fnCandidate[0].type->getSampler().shadow)
                        compArg = 2;
                } else
                    profileRequires(loc, ~EEsProfile, 400, E_GL_ARB_texture_gather, feature);
            }

            if (compArg > 0 && compArg < fnCandidate.getParamCount()) {
                if (callNode.getSequence()[compArg]->getAsConstantUnion()) {
                    int value = callNode.getSequence()[compArg]->getAsConstantUnion()->getConstArray()[0].getIConst();
                    if (value < 0 || value > 3)
                        error(loc, "must be 0, 1, 2, or 3:", feature, "component argument");
                } else
                    error(loc, "must be a compile-time constant:", feature, "component argument");
            }
        } else {
            // this is only for functions not starting "textureGather"...
            if (fnCandidate.getName().find("Offset") != TString::npos) {

                // Handle texture-offset limits checking
                int arg = -1;
                if (fnCandidate.getName().compare("textureOffset") == 0)
                    arg = 2;
                else if (fnCandidate.getName().compare("texelFetchOffset") == 0)
                    arg = 3;
                else if (fnCandidate.getName().compare("textureProjOffset") == 0)
                    arg = 2;
                else if (fnCandidate.getName().compare("textureLodOffset") == 0)
                    arg = 3;
                else if (fnCandidate.getName().compare("textureProjLodOffset") == 0)
                    arg = 3;
                else if (fnCandidate.getName().compare("textureGradOffset") == 0)
                    arg = 4;
                else if (fnCandidate.getName().compare("textureProjGradOffset") == 0)
                    arg = 4;

                if (arg > 0) {
                    if (! callNode.getSequence()[arg]->getAsConstantUnion())
                        error(loc, "argument must be compile-time constant", "texel offset", "");
                    else {
                        const TType& type = callNode.getSequence()[arg]->getAsTyped()->getType();
                        for (int c = 0; c < type.getVectorSize(); ++c) {
                            int offset = callNode.getSequence()[arg]->getAsConstantUnion()->getConstArray()[c].getIConst();
                            if (offset > resources.maxProgramTexelOffset || offset < resources.minProgramTexelOffset)
                                error(loc, "value is out of range:", "texel offset", "[gl_MinProgramTexelOffset, gl_MaxProgramTexelOffset]");
                        }
                    }
                }
            }
        }
    }

    // GL_ARB_shader_texture_image_samples
    if (fnCandidate.getName().compare(0, 14, "textureSamples") == 0 || fnCandidate.getName().compare(0, 12, "imageSamples") == 0)
        profileRequires(loc, ~EEsProfile, 450, E_GL_ARB_shader_texture_image_samples, "textureSamples and imageSamples");

    if (fnCandidate.getName().compare(0, 11, "imageAtomic") == 0) {
        const TType& imageType = callNode.getSequence()[0]->getAsTyped()->getType();
        if (imageType.getSampler().type == EbtInt || imageType.getSampler().type == EbtUint) {
            if (imageType.getQualifier().getFormat() != ElfR32i && imageType.getQualifier().getFormat() != ElfR32ui)
                error(loc, "only supported on image with format r32i or r32ui", fnCandidate.getName().c_str(), "");
        } else {
            if (fnCandidate.getName().compare(0, 19, "imageAtomicExchange") != 0)
                error(loc, "only supported on integer images", fnCandidate.getName().c_str(), "");
            else if (imageType.getQualifier().getFormat() != ElfR32f && isEsProfile())
                error(loc, "only supported on image with format r32f", fnCandidate.getName().c_str(), "");
        }
    }
}

//
// Do any extra checking for a user function call.
//
void TParseContext::userFunctionCallCheck(const TSourceLoc& loc, TIntermAggregate& callNode)
{
    TIntermSequence& arguments = callNode.getSequence();

    for (int i = 0; i < (int)arguments.size(); ++i)
        samplerConstructorLocationCheck(loc, "call argument", arguments[i]);
}

//
// Emit an error if this is a sampler constructor
//
void TParseContext::samplerConstructorLocationCheck(const TSourceLoc& loc, const char* token, TIntermNode* node)
{
    if (node->getAsOperator() && node->getAsOperator()->getOp() == EOpConstructTextureSampler)
        error(loc, "sampler constructor must appear at point of use", token, "");
}

//
// Handle seeing a built-in constructor in a grammar production.
//
TFunction* TParseContext::handleConstructorCall(const TSourceLoc& loc, const TPublicType& publicType)
{
    TType type(publicType);
    type.getQualifier().precision = EpqNone;

    if (type.isArray()) {
        profileRequires(loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed constructor");
        profileRequires(loc, EEsProfile, 300, nullptr, "arrayed constructor");
    }

    // Reuse EOpConstructTextureSampler for bindless image constructor
    // uvec2 imgHandle;
    // imageLoad(image1D(imgHandle), 0);
    if (type.isImage() && extensionTurnedOn(E_GL_ARB_bindless_texture))
    {
        intermediate.setBindlessImageMode(currentCaller, AstRefTypeFunc);
    }

    TOperator op = intermediate.mapTypeToConstructorOp(type);

    if (op == EOpNull) {
      if (intermediate.getEnhancedMsgs() && type.getBasicType() == EbtSampler)
            error(loc, "function not supported in this version; use texture() instead", "texture*D*", "");
        else
            error(loc, "cannot construct this type", type.getBasicString(), "");
        op = EOpConstructFloat;
        TType errorType(EbtFloat);
        type.shallowCopy(errorType);
    }

    TString empty("");

    return new TFunction(&empty, type, op);
}

// Handle seeing a precision qualifier in the grammar.
void TParseContext::handlePrecisionQualifier(const TSourceLoc& /*loc*/, TQualifier& qualifier, TPrecisionQualifier precision)
{
    if (obeyPrecisionQualifiers())
        qualifier.precision = precision;
}

// Check for messages to give on seeing a precision qualifier used in a
// declaration in the grammar.
void TParseContext::checkPrecisionQualifier(const TSourceLoc& loc, TPrecisionQualifier)
{
    if (precisionManager.shouldWarnAboutDefaults()) {
        warn(loc, "all default precisions are highp; use precision statements to quiet warning, e.g.:\n"
                  "         \"precision mediump int; precision highp float;\"", "", "");
        precisionManager.defaultWarningGiven();
    }
}

//
// Same error message for all places assignments don't work.
//
void TParseContext::assignError(const TSourceLoc& loc, const char* op, TString left, TString right)
{
    error(loc, "", op, "cannot convert from '%s' to '%s'",
          right.c_str(), left.c_str());
}

//
// Same error message for all places unary operations don't work.
//
void TParseContext::unaryOpError(const TSourceLoc& loc, const char* op, TString operand)
{
   error(loc, " wrong operand type", op,
          "no operation '%s' exists that takes an operand of type %s (or there is no acceptable conversion)",
          op, operand.c_str());
}

//
// Same error message for all binary operations don't work.
//
void TParseContext::binaryOpError(const TSourceLoc& loc, const char* op, TString left, TString right)
{
    error(loc, " wrong operand types:", op,
            "no operation '%s' exists that takes a left-hand operand of type '%s' and "
            "a right operand of type '%s' (or there is no acceptable conversion)",
            op, left.c_str(), right.c_str());
}

//
// A basic type of EbtVoid is a key that the name string was seen in the source, but
// it was not found as a variable in the symbol table.  If so, give the error
// message and insert a dummy variable in the symbol table to prevent future errors.
//
void TParseContext::variableCheck(TIntermTyped*& nodePtr)
{
    TIntermSymbol* symbol = nodePtr->getAsSymbolNode();
    if (! symbol)
        return;

    if (symbol->getType().getBasicType() == EbtVoid) {
        const char *extraInfoFormat = "";
        if (spvVersion.vulkan != 0 && symbol->getName() == "gl_VertexID") {
          extraInfoFormat = "(Did you mean gl_VertexIndex?)";
        } else if (spvVersion.vulkan != 0 && symbol->getName() == "gl_InstanceID") {
          extraInfoFormat = "(Did you mean gl_InstanceIndex?)";
        }
        error(symbol->getLoc(), "undeclared identifier", symbol->getName().c_str(), extraInfoFormat);

        // Add to symbol table to prevent future error messages on the same name
        if (symbol->getName().size() > 0) {
            TVariable* fakeVariable = new TVariable(&symbol->getName(), TType(EbtFloat));
            symbolTable.insert(*fakeVariable);

            // substitute a symbol node for this new variable
            nodePtr = intermediate.addSymbol(*fakeVariable, symbol->getLoc());
        }
    } else {
        switch (symbol->getQualifier().storage) {
        case EvqPointCoord:
            profileRequires(symbol->getLoc(), ENoProfile, 120, nullptr, "gl_PointCoord");
            break;
        default: break; // some compilers want this
        }
    }
}

//
// Both test and if necessary, spit out an error, to see if the node is really
// an l-value that can be operated on this way.
//
// Returns true if there was an error.
//
bool TParseContext::lValueErrorCheck(const TSourceLoc& loc, const char* op, TIntermTyped* node)
{
    TIntermBinary* binaryNode = node->getAsBinaryNode();

    if (binaryNode) {
        bool errorReturn = false;

        switch(binaryNode->getOp()) {
        case EOpIndexDirect:
        case EOpIndexIndirect:
            // ...  tessellation control shader ...
            // If a per-vertex output variable is used as an l-value, it is a
            // compile-time or link-time error if the expression indicating the
            // vertex index is not the identifier gl_InvocationID.
            if (language == EShLangTessControl) {
                const TType& leftType = binaryNode->getLeft()->getType();
                if (leftType.getQualifier().storage == EvqVaryingOut && ! leftType.getQualifier().patch && binaryNode->getLeft()->getAsSymbolNode()) {
                    // we have a per-vertex output
                    const TIntermSymbol* rightSymbol = binaryNode->getRight()->getAsSymbolNode();
                    if (! rightSymbol || rightSymbol->getQualifier().builtIn != EbvInvocationId)
                        error(loc, "tessellation-control per-vertex output l-value must be indexed with gl_InvocationID", "[]", "");
                }
            }
            break; // left node is checked by base class
        case EOpVectorSwizzle:
            errorReturn = lValueErrorCheck(loc, op, binaryNode->getLeft());
            if (!errorReturn) {
                int offset[4] = {0,0,0,0};

                TIntermTyped* rightNode = binaryNode->getRight();
                TIntermAggregate *aggrNode = rightNode->getAsAggregate();

                for (TIntermSequence::iterator p = aggrNode->getSequence().begin();
                                               p != aggrNode->getSequence().end(); p++) {
                    int value = (*p)->getAsTyped()->getAsConstantUnion()->getConstArray()[0].getIConst();
                    offset[value]++;
                    if (offset[value] > 1) {
                        error(loc, " l-value of swizzle cannot have duplicate components", op, "", "");

                        return true;
                    }
                }
            }

            return errorReturn;
        default:
            break;
        }

        if (errorReturn) {
            error(loc, " l-value required", op, "", "");
            return true;
        }
    }

    if (binaryNode && binaryNode->getOp() == EOpIndexDirectStruct && binaryNode->getLeft()->isReference())
        return false;

    // Let the base class check errors
    if (TParseContextBase::lValueErrorCheck(loc, op, node))
        return true;

    const char* symbol = nullptr;
    TIntermSymbol* symNode = node->getAsSymbolNode();
    if (symNode != nullptr)
        symbol = symNode->getName().c_str();

    const char* message = nullptr;
    switch (node->getQualifier().storage) {
    case EvqVaryingIn:      message = "can't modify shader input";   break;
    case EvqInstanceId:     message = "can't modify gl_InstanceID";  break;
    case EvqVertexId:       message = "can't modify gl_VertexID";    break;
    case EvqFace:           message = "can't modify gl_FrontFace";   break;
    case EvqFragCoord:      message = "can't modify gl_FragCoord";   break;
    case EvqPointCoord:     message = "can't modify gl_PointCoord";  break;
    case EvqFragDepth:
        intermediate.setDepthReplacing();
        // "In addition, it is an error to statically write to gl_FragDepth in the fragment shader."
        if (isEsProfile() && intermediate.getEarlyFragmentTests())
            message = "can't modify gl_FragDepth if using early_fragment_tests";
        break;
    case EvqFragStencil:
        intermediate.setStencilReplacing();
        // "In addition, it is an error to statically write to gl_FragDepth in the fragment shader."
        if (isEsProfile() && intermediate.getEarlyFragmentTests())
            message = "can't modify EvqFragStencil if using early_fragment_tests";
        break;

    case EvqtaskPayloadSharedEXT:
        if (language == EShLangMesh)
            message = "can't modify variable with storage qualifier taskPayloadSharedEXT in mesh shaders";
        break;
    default:
        break;
    }

    if (message == nullptr && binaryNode == nullptr && symNode == nullptr) {
        error(loc, " l-value required", op, "", "");

        return true;
    }

    //
    // Everything else is okay, no error.
    //
    if (message == nullptr)
        return false;

    //
    // If we get here, we have an error and a message.
    //
    if (symNode)
        error(loc, " l-value required", op, "\"%s\" (%s)", symbol, message);
    else
        error(loc, " l-value required", op, "(%s)", message);

    return true;
}

// Test for and give an error if the node can't be read from.
void TParseContext::rValueErrorCheck(const TSourceLoc& loc, const char* op, TIntermTyped* node)
{
    // Let the base class check errors
    TParseContextBase::rValueErrorCheck(loc, op, node);

    TIntermSymbol* symNode = node->getAsSymbolNode();
    if (!(symNode && symNode->getQualifier().isWriteOnly())) // base class checks
        if (symNode && symNode->getQualifier().isExplicitInterpolation())
            error(loc, "can't read from explicitly-interpolated object: ", op, symNode->getName().c_str());

    // local_size_{xyz} must be assigned or specialized before gl_WorkGroupSize can be assigned.
    if(node->getQualifier().builtIn == EbvWorkGroupSize &&
       !(intermediate.isLocalSizeSet() || intermediate.isLocalSizeSpecialized()))
        error(loc, "can't read from gl_WorkGroupSize before a fixed workgroup size has been declared", op, "");
}

//
// Both test, and if necessary spit out an error, to see if the node is really
// a constant.
//
void TParseContext::constantValueCheck(TIntermTyped* node, const char* token)
{
    if (! node->getQualifier().isConstant())
        error(node->getLoc(), "constant expression required", token, "");
}

//
// Both test, and if necessary spit out an error, to see if the node is really
// a 32-bit integer or can implicitly convert to one.
//
void TParseContext::integerCheck(const TIntermTyped* node, const char* token)
{
    auto from_type = node->getBasicType();
    if ((from_type == EbtInt || from_type == EbtUint ||
         intermediate.canImplicitlyPromote(from_type, EbtInt, EOpNull) ||
         intermediate.canImplicitlyPromote(from_type, EbtUint, EOpNull)) && node->isScalar())
        return;

    error(node->getLoc(), "scalar integer expression required", token, "");
}

//
// Both test, and if necessary spit out an error, to see if we are currently
// globally scoped.
//
void TParseContext::globalCheck(const TSourceLoc& loc, const char* token)
{
    if (! symbolTable.atGlobalLevel())
        error(loc, "not allowed in nested scope", token, "");
}

//
// Reserved errors for GLSL.
//
void TParseContext::reservedErrorCheck(const TSourceLoc& loc, const TString& identifier)
{
    // "Identifiers starting with "gl_" are reserved for use by OpenGL, and may not be
    // declared in a shader; this results in a compile-time error."
    if (! symbolTable.atBuiltInLevel()) {
        if (builtInName(identifier) && !extensionTurnedOn(E_GL_EXT_spirv_intrinsics))
            // The extension GL_EXT_spirv_intrinsics allows us to declare identifiers starting with "gl_".
            error(loc, "identifiers starting with \"gl_\" are reserved", identifier.c_str(), "");

        // "__" are not supposed to be an error.  ES 300 (and desktop) added the clarification:
        // "In addition, all identifiers containing two consecutive underscores (__) are
        // reserved; using such a name does not itself result in an error, but may result
        // in undefined behavior."
        // however, before that, ES tests required an error.
        if (identifier.find("__") != TString::npos && !extensionTurnedOn(E_GL_EXT_spirv_intrinsics)) {
            // The extension GL_EXT_spirv_intrinsics allows us to declare identifiers starting with "__".
            if (isEsProfile() && version < 300)
                error(loc, "identifiers containing consecutive underscores (\"__\") are reserved, and an error if version < 300", identifier.c_str(), "");
            else
                warn(loc, "identifiers containing consecutive underscores (\"__\") are reserved", identifier.c_str(), "");
        }
    }
}

//
// Reserved errors for the preprocessor.
//
void TParseContext::reservedPpErrorCheck(const TSourceLoc& loc, const char* identifier, const char* op)
{
    // "__" are not supposed to be an error.  ES 300 (and desktop) added the clarification:
    // "All macro names containing two consecutive underscores ( __ ) are reserved;
    // defining such a name does not itself result in an error, but may result in
    // undefined behavior.  All macro names prefixed with "GL_" ("GL" followed by a
    // single underscore) are also reserved, and defining such a name results in a
    // compile-time error."
    // however, before that, ES tests required an error.
    if (strncmp(identifier, "GL_", 3) == 0 && !extensionTurnedOn(E_GL_EXT_spirv_intrinsics))
        // The extension GL_EXT_spirv_intrinsics allows us to declare macros prefixed with "GL_".
        ppError(loc, "names beginning with \"GL_\" can't be (un)defined:", op,  identifier);
    else if (strncmp(identifier, "defined", 8) == 0)
        if (relaxedErrors())
            ppWarn(loc, "\"defined\" is (un)defined:", op,  identifier);
        else
            ppError(loc, "\"defined\" can't be (un)defined:", op,  identifier);
    else if (strstr(identifier, "__") != nullptr && !extensionTurnedOn(E_GL_EXT_spirv_intrinsics)) {
        // The extension GL_EXT_spirv_intrinsics allows us to declare macros prefixed with "__".
        if (isEsProfile() && version >= 300 &&
            (strcmp(identifier, "__LINE__") == 0 ||
             strcmp(identifier, "__FILE__") == 0 ||
             strcmp(identifier, "__VERSION__") == 0))
            ppError(loc, "predefined names can't be (un)defined:", op,  identifier);
        else {
            if (isEsProfile() && version < 300 && !relaxedErrors())
                ppError(loc, "names containing consecutive underscores are reserved, and an error if version < 300:", op, identifier);
            else
                ppWarn(loc, "names containing consecutive underscores are reserved:", op, identifier);
        }
    }
}

//
// See if this version/profile allows use of the line-continuation character '\'.
//
// Returns true if a line continuation should be done.
//
bool TParseContext::lineContinuationCheck(const TSourceLoc& loc, bool endOfComment)
{
    const char* message = "line continuation";

    bool lineContinuationAllowed = (isEsProfile() && version >= 300) ||
                                   (!isEsProfile() && (version >= 420 || extensionTurnedOn(E_GL_ARB_shading_language_420pack)));

    if (endOfComment) {
        if (lineContinuationAllowed)
            warn(loc, "used at end of comment; the following line is still part of the comment", message, "");
        else
            warn(loc, "used at end of comment, but this version does not provide line continuation", message, "");

        return lineContinuationAllowed;
    }

    if (relaxedErrors()) {
        if (! lineContinuationAllowed)
            warn(loc, "not allowed in this version", message, "");
        return true;
    } else {
        profileRequires(loc, EEsProfile, 300, nullptr, message);
        profileRequires(loc, ~EEsProfile, 420, E_GL_ARB_shading_language_420pack, message);
    }

    return lineContinuationAllowed;
}

bool TParseContext::builtInName(const TString& identifier)
{
    return identifier.compare(0, 3, "gl_") == 0;
}

//
// Make sure there is enough data and not too many arguments provided to the
// constructor to build something of the type of the constructor.  Also returns
// the type of the constructor.
//
// Part of establishing type is establishing specialization-constness.
// We don't yet know "top down" whether type is a specialization constant,
// but a const constructor can becomes a specialization constant if any of
// its children are, subject to KHR_vulkan_glsl rules:
//
//     - int(), uint(), and bool() constructors for type conversions
//       from any of the following types to any of the following types:
//         * int
//         * uint
//         * bool
//     - vector versions of the above conversion constructors
//
// Returns true if there was an error in construction.
//
bool TParseContext::constructorError(const TSourceLoc& loc, TIntermNode* node, TFunction& function, TOperator op, TType& type)
{
    // See if the constructor does not establish the main type, only requalifies
    // it, in which case the type comes from the argument instead of from the
    // constructor function.
    switch (op) {
    case EOpConstructNonuniform:
        if (node != nullptr && node->getAsTyped() != nullptr) {
            type.shallowCopy(node->getAsTyped()->getType());
            type.getQualifier().makeTemporary();
            type.getQualifier().nonUniform = true;
        }
        break;
    default:
        type.shallowCopy(function.getType());
        break;
    }

    TString constructorString;
    if (intermediate.getEnhancedMsgs())
        constructorString.append(type.getCompleteString(true, false, false, true)).append(" constructor");
    else
        constructorString.append("constructor");

    // See if it's a matrix
    bool constructingMatrix = false;
    switch (op) {
    case EOpConstructTextureSampler:
        return constructorTextureSamplerError(loc, function);
    case EOpConstructMat2x2:
    case EOpConstructMat2x3:
    case EOpConstructMat2x4:
    case EOpConstructMat3x2:
    case EOpConstructMat3x3:
    case EOpConstructMat3x4:
    case EOpConstructMat4x2:
    case EOpConstructMat4x3:
    case EOpConstructMat4x4:
    case EOpConstructDMat2x2:
    case EOpConstructDMat2x3:
    case EOpConstructDMat2x4:
    case EOpConstructDMat3x2:
    case EOpConstructDMat3x3:
    case EOpConstructDMat3x4:
    case EOpConstructDMat4x2:
    case EOpConstructDMat4x3:
    case EOpConstructDMat4x4:
    case EOpConstructF16Mat2x2:
    case EOpConstructF16Mat2x3:
    case EOpConstructF16Mat2x4:
    case EOpConstructF16Mat3x2:
    case EOpConstructF16Mat3x3:
    case EOpConstructF16Mat3x4:
    case EOpConstructF16Mat4x2:
    case EOpConstructF16Mat4x3:
    case EOpConstructF16Mat4x4:
        constructingMatrix = true;
        break;
    default:
        break;
    }

    //
    // Walk the arguments for first-pass checks and collection of information.
    //

    int size = 0;
    bool constType = true;
    bool specConstType = false;   // value is only valid if constType is true
    bool full = false;
    bool overFull = false;
    bool matrixInMatrix = false;
    bool arrayArg = false;
    bool floatArgument = false;
    bool intArgument = false;
    for (int arg = 0; arg < function.getParamCount(); ++arg) {
        if (function[arg].type->isArray()) {
            if (function[arg].type->isUnsizedArray()) {
                // Can't construct from an unsized array.
                error(loc, "array argument must be sized", constructorString.c_str(), "");
                return true;
            }
            arrayArg = true;
        }
        if (constructingMatrix && function[arg].type->isMatrix())
            matrixInMatrix = true;

        // 'full' will go to true when enough args have been seen.  If we loop
        // again, there is an extra argument.
        if (full) {
            // For vectors and matrices, it's okay to have too many components
            // available, but not okay to have unused arguments.
            overFull = true;
        }

        size += function[arg].type->computeNumComponents();
        if (op != EOpConstructStruct && ! type.isArray() && size >= type.computeNumComponents())
            full = true;

        if (! function[arg].type->getQualifier().isConstant())
            constType = false;
        if (function[arg].type->getQualifier().isSpecConstant())
            specConstType = true;
        if (function[arg].type->isFloatingDomain())
            floatArgument = true;
        if (function[arg].type->isIntegerDomain())
            intArgument = true;
        if (type.isStruct()) {
            if (function[arg].type->contains16BitFloat()) {
                requireFloat16Arithmetic(loc, constructorString.c_str(), "can't construct structure containing 16-bit type");
            }
            if (function[arg].type->contains16BitInt()) {
                requireInt16Arithmetic(loc, constructorString.c_str(), "can't construct structure containing 16-bit type");
            }
            if (function[arg].type->contains8BitInt()) {
                requireInt8Arithmetic(loc, constructorString.c_str(), "can't construct structure containing 8-bit type");
            }
        }
    }
    if (op == EOpConstructNonuniform)
        constType = false;

    switch (op) {
    case EOpConstructFloat16:
    case EOpConstructF16Vec2:
    case EOpConstructF16Vec3:
    case EOpConstructF16Vec4:
        if (type.isArray())
            requireFloat16Arithmetic(loc, constructorString.c_str(), "16-bit arrays not supported");
        if (type.isVector() && function.getParamCount() != 1)
            requireFloat16Arithmetic(loc, constructorString.c_str(), "16-bit vectors only take vector types");
        break;
    case EOpConstructUint16:
    case EOpConstructU16Vec2:
    case EOpConstructU16Vec3:
    case EOpConstructU16Vec4:
    case EOpConstructInt16:
    case EOpConstructI16Vec2:
    case EOpConstructI16Vec3:
    case EOpConstructI16Vec4:
        if (type.isArray())
            requireInt16Arithmetic(loc, constructorString.c_str(), "16-bit arrays not supported");
        if (type.isVector() && function.getParamCount() != 1)
            requireInt16Arithmetic(loc, constructorString.c_str(), "16-bit vectors only take vector types");
        break;
    case EOpConstructUint8:
    case EOpConstructU8Vec2:
    case EOpConstructU8Vec3:
    case EOpConstructU8Vec4:
    case EOpConstructInt8:
    case EOpConstructI8Vec2:
    case EOpConstructI8Vec3:
    case EOpConstructI8Vec4:
        if (type.isArray())
            requireInt8Arithmetic(loc, constructorString.c_str(), "8-bit arrays not supported");
        if (type.isVector() && function.getParamCount() != 1)
            requireInt8Arithmetic(loc, constructorString.c_str(), "8-bit vectors only take vector types");
        break;
    default:
        break;
    }

    // inherit constness from children
    if (constType) {
        bool makeSpecConst;
        // Finish pinning down spec-const semantics
        if (specConstType) {
            switch (op) {
            case EOpConstructInt8:
            case EOpConstructInt:
            case EOpConstructUint:
            case EOpConstructBool:
            case EOpConstructBVec2:
            case EOpConstructBVec3:
            case EOpConstructBVec4:
            case EOpConstructIVec2:
            case EOpConstructIVec3:
            case EOpConstructIVec4:
            case EOpConstructUVec2:
            case EOpConstructUVec3:
            case EOpConstructUVec4:
            case EOpConstructUint8:
            case EOpConstructInt16:
            case EOpConstructUint16:
            case EOpConstructInt64:
            case EOpConstructUint64:
            case EOpConstructI8Vec2:
            case EOpConstructI8Vec3:
            case EOpConstructI8Vec4:
            case EOpConstructU8Vec2:
            case EOpConstructU8Vec3:
            case EOpConstructU8Vec4:
            case EOpConstructI16Vec2:
            case EOpConstructI16Vec3:
            case EOpConstructI16Vec4:
            case EOpConstructU16Vec2:
            case EOpConstructU16Vec3:
            case EOpConstructU16Vec4:
            case EOpConstructI64Vec2:
            case EOpConstructI64Vec3:
            case EOpConstructI64Vec4:
            case EOpConstructU64Vec2:
            case EOpConstructU64Vec3:
            case EOpConstructU64Vec4:
                // This was the list of valid ones, if they aren't converting from float
                // and aren't making an array.
                makeSpecConst = ! floatArgument && ! type.isArray();
                break;

            case EOpConstructVec2:
            case EOpConstructVec3:
            case EOpConstructVec4:
                // This was the list of valid ones, if they aren't converting from int
                // and aren't making an array.
                makeSpecConst = ! intArgument && !type.isArray();
                break;

            default:
                // anything else wasn't white-listed in the spec as a conversion
                makeSpecConst = false;
                break;
            }
        } else
            makeSpecConst = false;

        if (makeSpecConst)
            type.getQualifier().makeSpecConstant();
        else if (specConstType)
            type.getQualifier().makeTemporary();
        else
            type.getQualifier().storage = EvqConst;
    }

    if (type.isArray()) {
        if (function.getParamCount() == 0) {
            error(loc, "array constructor must have at least one argument", constructorString.c_str(), "");
            return true;
        }

        if (type.isUnsizedArray()) {
            // auto adapt the constructor type to the number of arguments
            type.changeOuterArraySize(function.getParamCount());
        } else if (type.getOuterArraySize() != function.getParamCount()) {
            error(loc, "array constructor needs one argument per array element", constructorString.c_str(), "");
            return true;
        }

        if (type.isArrayOfArrays()) {
            // Types have to match, but we're still making the type.
            // Finish making the type, and the comparison is done later
            // when checking for conversion.
            TArraySizes& arraySizes = *type.getArraySizes();

            // At least the dimensionalities have to match.
            if (! function[0].type->isArray() ||
                    arraySizes.getNumDims() != function[0].type->getArraySizes()->getNumDims() + 1) {
                error(loc, "array constructor argument not correct type to construct array element", constructorString.c_str(), "");
                return true;
            }

            if (arraySizes.isInnerUnsized()) {
                // "Arrays of arrays ..., and the size for any dimension is optional"
                // That means we need to adopt (from the first argument) the other array sizes into the type.
                for (int d = 1; d < arraySizes.getNumDims(); ++d) {
                    if (arraySizes.getDimSize(d) == UnsizedArraySize) {
                        arraySizes.setDimSize(d, function[0].type->getArraySizes()->getDimSize(d - 1));
                    }
                }
            }
        }
    }

    if (arrayArg && op != EOpConstructStruct && ! type.isArrayOfArrays()) {
        error(loc, "constructing non-array constituent from array argument", constructorString.c_str(), "");
        return true;
    }

    if (matrixInMatrix && ! type.isArray()) {
        profileRequires(loc, ENoProfile, 120, nullptr, "constructing matrix from matrix");

        // "If a matrix argument is given to a matrix constructor,
        // it is a compile-time error to have any other arguments."
        if (function.getParamCount() != 1)
            error(loc, "matrix constructed from matrix can only have one argument", constructorString.c_str(), "");
        return false;
    }

    if (overFull) {
        error(loc, "too many arguments", constructorString.c_str(), "");
        return true;
    }

    if (op == EOpConstructStruct && ! type.isArray() && (int)type.getStruct()->size() != function.getParamCount()) {
        error(loc, "Number of constructor parameters does not match the number of structure fields", constructorString.c_str(), "");
        return true;
    }

    if ((op != EOpConstructStruct && size != 1 && size < type.computeNumComponents()) ||
        (op == EOpConstructStruct && size < type.computeNumComponents())) {
        error(loc, "not enough data provided for construction", constructorString.c_str(), "");
        return true;
    }

    if (type.isCoopMat() && function.getParamCount() != 1) {
        error(loc, "wrong number of arguments", constructorString.c_str(), "");
        return true;
    }
    if (type.isCoopMat() &&
        !(function[0].type->isScalar() || function[0].type->isCoopMat())) {
        error(loc, "Cooperative matrix constructor argument must be scalar or cooperative matrix", constructorString.c_str(), "");
        return true;
    }

    TIntermTyped* typed = node->getAsTyped();
    if (type.isCoopMat() && typed->getType().isCoopMat() &&
        !type.sameCoopMatShapeAndUse(typed->getType())) {
        error(loc, "Cooperative matrix type parameters mismatch", constructorString.c_str(), "");
        return true;
    }

    if (typed == nullptr) {
        error(loc, "constructor argument does not have a type", constructorString.c_str(), "");
        return true;
    }
    if (op != EOpConstructStruct && op != EOpConstructNonuniform && typed->getBasicType() == EbtSampler) {
        if (op == EOpConstructUVec2 && extensionTurnedOn(E_GL_ARB_bindless_texture)) {
            intermediate.setBindlessTextureMode(currentCaller, AstRefTypeFunc);
        }
        else {
            error(loc, "cannot convert a sampler", constructorString.c_str(), "");
            return true;
        }
    }
    if (op != EOpConstructStruct && typed->isAtomic()) {
        error(loc, "cannot convert an atomic_uint", constructorString.c_str(), "");
        return true;
    }
    if (typed->getBasicType() == EbtVoid) {
        error(loc, "cannot convert a void", constructorString.c_str(), "");
        return true;
    }

    return false;
}

// Verify all the correct semantics for constructing a combined texture/sampler.
// Return true if the semantics are incorrect.
bool TParseContext::constructorTextureSamplerError(const TSourceLoc& loc, const TFunction& function)
{
    TString constructorName = function.getType().getBasicTypeString();  // TODO: performance: should not be making copy; interface needs to change
    const char* token = constructorName.c_str();
    // verify the constructor for bindless texture, the input must be ivec2 or uvec2
    if (function.getParamCount() == 1) {
        TType* pType = function[0].type;
        TBasicType basicType = pType->getBasicType();
        bool isIntegerVec2 = ((basicType == EbtUint || basicType == EbtInt) && pType->getVectorSize() == 2);
        bool bindlessMode = extensionTurnedOn(E_GL_ARB_bindless_texture);
        if (isIntegerVec2 && bindlessMode) {
            if (pType->getSampler().isImage())
                intermediate.setBindlessImageMode(currentCaller, AstRefTypeFunc);
            else
                intermediate.setBindlessTextureMode(currentCaller, AstRefTypeFunc);
            return false;
        } else {
            if (!bindlessMode)
                error(loc, "sampler-constructor requires the extension GL_ARB_bindless_texture enabled", token, "");
            else
                error(loc, "sampler-constructor requires the input to be ivec2 or uvec2", token, "");
            return true;
        }
    }

    // exactly two arguments needed
    if (function.getParamCount() != 2) {
        error(loc, "sampler-constructor requires two arguments", token, "");
        return true;
    }

    // For now, not allowing arrayed constructors, the rest of this function
    // is set up to allow them, if this test is removed:
    if (function.getType().isArray()) {
        error(loc, "sampler-constructor cannot make an array of samplers", token, "");
        return true;
    }

    // first argument
    //  * the constructor's first argument must be a texture type
    //  * the dimensionality (1D, 2D, 3D, Cube, Rect, Buffer, MS, and Array)
    //    of the texture type must match that of the constructed sampler type
    //    (that is, the suffixes of the type of the first argument and the
    //    type of the constructor will be spelled the same way)
    if (function[0].type->getBasicType() != EbtSampler ||
        ! function[0].type->getSampler().isTexture() ||
        function[0].type->isArray()) {
        error(loc, "sampler-constructor first argument must be a scalar *texture* type", token, "");
        return true;
    }
    // simulate the first argument's impact on the result type, so it can be compared with the encapsulated operator!=()
    TSampler texture = function.getType().getSampler();
    texture.setCombined(false);
    texture.shadow = false;
    if (texture != function[0].type->getSampler()) {
        error(loc, "sampler-constructor first argument must be a *texture* type"
                   " matching the dimensionality and sampled type of the constructor", token, "");
        return true;
    }

    // second argument
    //   * the constructor's second argument must be a scalar of type
    //     *sampler* or *samplerShadow*
    if (  function[1].type->getBasicType() != EbtSampler ||
        ! function[1].type->getSampler().isPureSampler() ||
          function[1].type->isArray()) {
        error(loc, "sampler-constructor second argument must be a scalar sampler or samplerShadow", token, "");
        return true;
    }

    return false;
}

// Checks to see if a void variable has been declared and raise an error message for such a case
//
// returns true in case of an error
//
bool TParseContext::voidErrorCheck(const TSourceLoc& loc, const TString& identifier, const TBasicType basicType)
{
    if (basicType == EbtVoid) {
        error(loc, "illegal use of type 'void'", identifier.c_str(), "");
        return true;
    }

    return false;
}

// Checks to see if the node (for the expression) contains a scalar boolean expression or not
void TParseContext::boolCheck(const TSourceLoc& loc, const TIntermTyped* type)
{
    if (type->getBasicType() != EbtBool || type->isArray() || type->isMatrix() || type->isVector())
        error(loc, "boolean expression expected", "", "");
}

// This function checks to see if the node (for the expression) contains a scalar boolean expression or not
void TParseContext::boolCheck(const TSourceLoc& loc, const TPublicType& pType)
{
    if (pType.basicType != EbtBool || pType.arraySizes || pType.matrixCols > 1 || (pType.vectorSize > 1))
        error(loc, "boolean expression expected", "", "");
}

void TParseContext::samplerCheck(const TSourceLoc& loc, const TType& type, const TString& identifier, TIntermTyped* /*initializer*/)
{
    // Check that the appropriate extension is enabled if external sampler is used.
    // There are two extensions. The correct one must be used based on GLSL version.
    if (type.getBasicType() == EbtSampler && type.getSampler().isExternal()) {
        if (version < 300) {
            requireExtensions(loc, 1, &E_GL_OES_EGL_image_external, "samplerExternalOES");
        } else {
            requireExtensions(loc, 1, &E_GL_OES_EGL_image_external_essl3, "samplerExternalOES");
        }
    }
    if (type.getSampler().isYuv()) {
        requireExtensions(loc, 1, &E_GL_EXT_YUV_target, "__samplerExternal2DY2YEXT");
    }

    if (type.getQualifier().storage == EvqUniform)
        return;

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtSampler)) {
        // For bindless texture, sampler can be declared as an struct member
        if (extensionTurnedOn(E_GL_ARB_bindless_texture)) {
            if (type.getSampler().isImage())
                intermediate.setBindlessImageMode(currentCaller, AstRefTypeVar);
            else
                intermediate.setBindlessTextureMode(currentCaller, AstRefTypeVar);
        }
        else {
            error(loc, "non-uniform struct contains a sampler or image:", type.getBasicTypeString().c_str(), identifier.c_str());
        }
    }
    else if (type.getBasicType() == EbtSampler && type.getQualifier().storage != EvqUniform) {
        // For bindless texture, sampler can be declared as an input/output/block member
        if (extensionTurnedOn(E_GL_ARB_bindless_texture)) {
            if (type.getSampler().isImage())
                intermediate.setBindlessImageMode(currentCaller, AstRefTypeVar);
            else
                intermediate.setBindlessTextureMode(currentCaller, AstRefTypeVar);
        }
        else {
            // non-uniform sampler
            // not yet:  okay if it has an initializer
            // if (! initializer)
            if (type.getSampler().isAttachmentEXT() && type.getQualifier().storage != EvqTileImageEXT)
                 error(loc, "can only be used in tileImageEXT variables or function parameters:", type.getBasicTypeString().c_str(), identifier.c_str());
             else if (type.getQualifier().storage != EvqTileImageEXT)
                 error(loc, "sampler/image types can only be used in uniform variables or function parameters:", type.getBasicTypeString().c_str(), identifier.c_str());
        }
    }
}

void TParseContext::atomicUintCheck(const TSourceLoc& loc, const TType& type, const TString& identifier)
{
    if (type.getQualifier().storage == EvqUniform)
        return;

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtAtomicUint))
        error(loc, "non-uniform struct contains an atomic_uint:", type.getBasicTypeString().c_str(), identifier.c_str());
    else if (type.getBasicType() == EbtAtomicUint && type.getQualifier().storage != EvqUniform)
        error(loc, "atomic_uints can only be used in uniform variables or function parameters:", type.getBasicTypeString().c_str(), identifier.c_str());
}

void TParseContext::accStructCheck(const TSourceLoc& loc, const TType& type, const TString& identifier)
{
    if (type.getQualifier().storage == EvqUniform)
        return;

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtAccStruct))
        error(loc, "non-uniform struct contains an accelerationStructureNV:", type.getBasicTypeString().c_str(), identifier.c_str());
    else if (type.getBasicType() == EbtAccStruct && type.getQualifier().storage != EvqUniform)
        error(loc, "accelerationStructureNV can only be used in uniform variables or function parameters:",
            type.getBasicTypeString().c_str(), identifier.c_str());

}

void TParseContext::transparentOpaqueCheck(const TSourceLoc& loc, const TType& type, const TString& identifier)
{
    if (parsingBuiltins)
        return;

    if (type.getQualifier().storage != EvqUniform)
        return;

    if (type.containsNonOpaque()) {
        // Vulkan doesn't allow transparent uniforms outside of blocks
        if (spvVersion.vulkan > 0 && !spvVersion.vulkanRelaxed)
            vulkanRemoved(loc, "non-opaque uniforms outside a block");
        // OpenGL wants locations on these (unless they are getting automapped)
        if (spvVersion.openGl > 0 && !type.getQualifier().hasLocation() && !intermediate.getAutoMapLocations())
            error(loc, "non-opaque uniform variables need a layout(location=L)", identifier.c_str(), "");
    }
}

//
// Qualifier checks knowing the qualifier and that it is a member of a struct/block.
//
void TParseContext::memberQualifierCheck(glslang::TPublicType& publicType)
{
    globalQualifierFixCheck(publicType.loc, publicType.qualifier, true);
    checkNoShaderLayouts(publicType.loc, publicType.shaderQualifiers);
    if (publicType.qualifier.isNonUniform()) {
        error(publicType.loc, "not allowed on block or structure members", "nonuniformEXT", "");
        publicType.qualifier.nonUniform = false;
    }
}

//
// Check/fix just a full qualifier (no variables or types yet, but qualifier is complete) at global level.
//
void TParseContext::globalQualifierFixCheck(const TSourceLoc& loc, TQualifier& qualifier, bool isMemberCheck, const TPublicType* publicType)
{
    bool nonuniformOkay = false;

    // move from parameter/unknown qualifiers to pipeline in/out qualifiers
    switch (qualifier.storage) {
    case EvqIn:
        profileRequires(loc, ENoProfile, 130, nullptr, "in for stage inputs");
        profileRequires(loc, EEsProfile, 300, nullptr, "in for stage inputs");
        qualifier.storage = EvqVaryingIn;
        nonuniformOkay = true;
        break;
    case EvqOut:
        profileRequires(loc, ENoProfile, 130, nullptr, "out for stage outputs");
        profileRequires(loc, EEsProfile, 300, nullptr, "out for stage outputs");
        qualifier.storage = EvqVaryingOut;
        if (intermediate.isInvariantAll())
            qualifier.invariant = true;
        break;
    case EvqInOut:
        qualifier.storage = EvqVaryingIn;
        error(loc, "cannot use 'inout' at global scope", "", "");
        break;
    case EvqGlobal:
    case EvqTemporary:
        nonuniformOkay = true;
        break;
    case EvqUniform:
        // According to GLSL spec: The std430 qualifier is supported only for shader storage blocks; a shader using
        // the std430 qualifier on a uniform block will fail to compile.
        // Only check the global declaration: layout(std430) uniform;
        if (blockName == nullptr &&
            qualifier.layoutPacking == ElpStd430)
        {
            requireExtensions(loc, 1, &E_GL_EXT_scalar_block_layout, "default std430 layout for uniform");
        }

        if (publicType != nullptr && publicType->isImage() &&
            (qualifier.layoutFormat > ElfExtSizeGuard && qualifier.layoutFormat < ElfCount))
            qualifier.layoutFormat = mapLegacyLayoutFormat(qualifier.layoutFormat, publicType->sampler.getBasicType());

        break;
    default:
        break;
    }

    if (!nonuniformOkay && qualifier.isNonUniform())
        error(loc, "for non-parameter, can only apply to 'in' or no storage qualifier", "nonuniformEXT", "");

    if (qualifier.isSpirvByReference())
        error(loc, "can only apply to parameter", "spirv_by_reference", "");

    if (qualifier.isSpirvLiteral())
        error(loc, "can only apply to parameter", "spirv_literal", "");

    // Storage qualifier isn't ready for memberQualifierCheck, we should skip invariantCheck for it.
    if (!isMemberCheck || structNestingLevel > 0)
        invariantCheck(loc, qualifier);
}

//
// Check a full qualifier and type (no variable yet) at global level.
//
void TParseContext::globalQualifierTypeCheck(const TSourceLoc& loc, const TQualifier& qualifier, const TPublicType& publicType)
{
    if (! symbolTable.atGlobalLevel())
        return;

    if (!(publicType.userDef && publicType.userDef->isReference()) && !parsingBuiltins) {
        if (qualifier.isMemoryQualifierImageAndSSBOOnly() && ! publicType.isImage() && publicType.qualifier.storage != EvqBuffer) {
            error(loc, "memory qualifiers cannot be used on this type", "", "");
        } else if (qualifier.isMemory() && (publicType.basicType != EbtSampler) && !publicType.qualifier.isUniformOrBuffer()) {
            error(loc, "memory qualifiers cannot be used on this type", "", "");
        }
    }

    if (qualifier.storage == EvqBuffer &&
        publicType.basicType != EbtBlock &&
        !qualifier.hasBufferReference())
        error(loc, "buffers can be declared only as blocks", "buffer", "");

    if (qualifier.storage != EvqVaryingIn && publicType.basicType == EbtDouble &&
        extensionTurnedOn(E_GL_ARB_vertex_attrib_64bit) && language == EShLangVertex &&
        version < 400) {
        profileRequires(loc, ECoreProfile | ECompatibilityProfile, 410, E_GL_ARB_gpu_shader_fp64, "vertex-shader `double` type");
    }
    if (qualifier.storage != EvqVaryingIn && qualifier.storage != EvqVaryingOut)
        return;

    if (publicType.shaderQualifiers.hasBlendEquation())
        error(loc, "can only be applied to a standalone 'out'", "blend equation", "");

    // now, knowing it is a shader in/out, do all the in/out semantic checks

    if (publicType.basicType == EbtBool && !parsingBuiltins) {
        error(loc, "cannot be bool", GetStorageQualifierString(qualifier.storage), "");
        return;
    }

    if (isTypeInt(publicType.basicType) || publicType.basicType == EbtDouble) {
        profileRequires(loc, EEsProfile, 300, nullptr, "non-float shader input/output");
        profileRequires(loc, ~EEsProfile, 130, nullptr, "non-float shader input/output");
    }

    if (!qualifier.flat && !qualifier.isExplicitInterpolation() && !qualifier.isPervertexNV() && !qualifier.isPervertexEXT()) {
        if (isTypeInt(publicType.basicType) ||
            publicType.basicType == EbtDouble ||
            (publicType.userDef && (   publicType.userDef->containsBasicType(EbtInt)
                                    || publicType.userDef->containsBasicType(EbtUint)
                                    || publicType.userDef->contains16BitInt()
                                    || publicType.userDef->contains8BitInt()
                                    || publicType.userDef->contains64BitInt()
                                    || publicType.userDef->containsDouble()))) {
            if (qualifier.storage == EvqVaryingIn && language == EShLangFragment)
                error(loc, "must be qualified as flat", TType::getBasicString(publicType.basicType), GetStorageQualifierString(qualifier.storage));
            else if (qualifier.storage == EvqVaryingOut && language == EShLangVertex && version == 300)
                error(loc, "must be qualified as flat", TType::getBasicString(publicType.basicType), GetStorageQualifierString(qualifier.storage));
        }
    }

    if (qualifier.isPatch() && qualifier.isInterpolation())
        error(loc, "cannot use interpolation qualifiers with patch", "patch", "");

    if (qualifier.isTaskPayload() && publicType.basicType == EbtBlock)
        error(loc, "taskPayloadSharedEXT variables should not be declared as interface blocks", "taskPayloadSharedEXT", "");

    if (qualifier.isTaskMemory() && publicType.basicType != EbtBlock)
        error(loc, "taskNV variables can be declared only as blocks", "taskNV", "");

    if (qualifier.storage == EvqVaryingIn) {
        switch (language) {
        case EShLangVertex:
            if (publicType.basicType == EbtStruct) {
                error(loc, "cannot be a structure", GetStorageQualifierString(qualifier.storage), "");
                return;
            }
            if (publicType.arraySizes) {
                requireProfile(loc, ~EEsProfile, "vertex input arrays");
                profileRequires(loc, ENoProfile, 150, nullptr, "vertex input arrays");
            }
            if (publicType.basicType == EbtDouble)
                profileRequires(loc, ~EEsProfile, 410, E_GL_ARB_vertex_attrib_64bit, "vertex-shader `double` type input");
            if (qualifier.isAuxiliary() || qualifier.isInterpolation() || qualifier.isMemory() || qualifier.invariant)
                error(loc, "vertex input cannot be further qualified", "", "");
            break;
        case EShLangFragment:
            if (publicType.userDef) {
                profileRequires(loc, EEsProfile, 300, nullptr, "fragment-shader struct input");
                profileRequires(loc, ~EEsProfile, 150, nullptr, "fragment-shader struct input");
                if (publicType.userDef->containsStructure())
                    requireProfile(loc, ~EEsProfile, "fragment-shader struct input containing structure");
                if (publicType.userDef->containsArray())
                    requireProfile(loc, ~EEsProfile, "fragment-shader struct input containing an array");
            }
            break;
       case EShLangCompute:
            if (! symbolTable.atBuiltInLevel())
                error(loc, "global storage input qualifier cannot be used in a compute shader", "in", "");
            break;
       case EShLangTessControl:
            if (qualifier.patch)
                error(loc, "can only use on output in tessellation-control shader", "patch", "");
            break;
        default:
            break;
        }
    } else {
        // qualifier.storage == EvqVaryingOut
        switch (language) {
        case EShLangVertex:
            if (publicType.userDef) {
                profileRequires(loc, EEsProfile, 300, nullptr, "vertex-shader struct output");
                profileRequires(loc, ~EEsProfile, 150, nullptr, "vertex-shader struct output");
                if (publicType.userDef->containsStructure())
                    requireProfile(loc, ~EEsProfile, "vertex-shader struct output containing structure");
                if (publicType.userDef->containsArray())
                    requireProfile(loc, ~EEsProfile, "vertex-shader struct output containing an array");
            }

            break;
        case EShLangFragment:
            profileRequires(loc, EEsProfile, 300, nullptr, "fragment shader output");
            if (publicType.basicType == EbtStruct) {
                error(loc, "cannot be a structure", GetStorageQualifierString(qualifier.storage), "");
                return;
            }
            if (publicType.matrixRows > 0) {
                error(loc, "cannot be a matrix", GetStorageQualifierString(qualifier.storage), "");
                return;
            }
            if (qualifier.isAuxiliary())
                error(loc, "can't use auxiliary qualifier on a fragment output", "centroid/sample/patch", "");
            if (qualifier.isInterpolation())
                error(loc, "can't use interpolation qualifier on a fragment output", "flat/smooth/noperspective", "");
            if (publicType.basicType == EbtDouble || publicType.basicType == EbtInt64 || publicType.basicType == EbtUint64)
                error(loc, "cannot contain a double, int64, or uint64", GetStorageQualifierString(qualifier.storage), "");
        break;

        case EShLangCompute:
            error(loc, "global storage output qualifier cannot be used in a compute shader", "out", "");
            break;
        case EShLangTessEvaluation:
            if (qualifier.patch)
                error(loc, "can only use on input in tessellation-evaluation shader", "patch", "");
            break;
        default:
            break;
        }
    }
}

//
// Merge characteristics of the 'src' qualifier into the 'dst'.
// If there is duplication, issue error messages, unless 'force'
// is specified, which means to just override default settings.
//
// Also, when force is false, it will be assumed that 'src' follows
// 'dst', for the purpose of error checking order for versions
// that require specific orderings of qualifiers.
//
void TParseContext::mergeQualifiers(const TSourceLoc& loc, TQualifier& dst, const TQualifier& src, bool force)
{
    // Multiple auxiliary qualifiers (mostly done later by 'individual qualifiers')
    if (src.isAuxiliary() && dst.isAuxiliary())
        error(loc, "can only have one auxiliary qualifier (centroid, patch, and sample)", "", "");

    // Multiple interpolation qualifiers (mostly done later by 'individual qualifiers')
    if (src.isInterpolation() && dst.isInterpolation())
        error(loc, "can only have one interpolation qualifier (flat, smooth, noperspective, __explicitInterpAMD)", "", "");

    // Ordering
    if (! force && ((!isEsProfile() && version < 420) ||
                    (isEsProfile() && version < 310))
                && ! extensionTurnedOn(E_GL_ARB_shading_language_420pack)) {
        // non-function parameters
        if (src.isNoContraction() && (dst.invariant || dst.isInterpolation() || dst.isAuxiliary() || dst.storage != EvqTemporary || dst.precision != EpqNone))
            error(loc, "precise qualifier must appear first", "", "");
        if (src.invariant && (dst.isInterpolation() || dst.isAuxiliary() || dst.storage != EvqTemporary || dst.precision != EpqNone))
            error(loc, "invariant qualifier must appear before interpolation, storage, and precision qualifiers ", "", "");
        else if (src.isInterpolation() && (dst.isAuxiliary() || dst.storage != EvqTemporary || dst.precision != EpqNone))
            error(loc, "interpolation qualifiers must appear before storage and precision qualifiers", "", "");
        else if (src.isAuxiliary() && (dst.storage != EvqTemporary || dst.precision != EpqNone))
            error(loc, "Auxiliary qualifiers (centroid, patch, and sample) must appear before storage and precision qualifiers", "", "");
        else if (src.storage != EvqTemporary && (dst.precision != EpqNone))
            error(loc, "precision qualifier must appear as last qualifier", "", "");

        // function parameters
        if (src.isNoContraction() && (dst.storage == EvqConst || dst.storage == EvqIn || dst.storage == EvqOut))
            error(loc, "precise qualifier must appear first", "", "");
        if (src.storage == EvqConst && (dst.storage == EvqIn || dst.storage == EvqOut))
            error(loc, "in/out must appear before const", "", "");
    }

    // Storage qualification
    if (dst.storage == EvqTemporary || dst.storage == EvqGlobal)
        dst.storage = src.storage;
    else if ((dst.storage == EvqIn  && src.storage == EvqOut) ||
             (dst.storage == EvqOut && src.storage == EvqIn))
        dst.storage = EvqInOut;
    else if ((dst.storage == EvqIn    && src.storage == EvqConst) ||
             (dst.storage == EvqConst && src.storage == EvqIn))
        dst.storage = EvqConstReadOnly;
    else if (src.storage != EvqTemporary &&
             src.storage != EvqGlobal)
        error(loc, "too many storage qualifiers", GetStorageQualifierString(src.storage), "");

    // Precision qualifiers
    if (! force && src.precision != EpqNone && dst.precision != EpqNone)
        error(loc, "only one precision qualifier allowed", GetPrecisionQualifierString(src.precision), "");
    if (dst.precision == EpqNone || (force && src.precision != EpqNone))
        dst.precision = src.precision;

    if (!force && ((src.coherent && (dst.devicecoherent || dst.queuefamilycoherent || dst.workgroupcoherent || dst.subgroupcoherent || dst.shadercallcoherent)) ||
                   (src.devicecoherent && (dst.coherent || dst.queuefamilycoherent || dst.workgroupcoherent || dst.subgroupcoherent || dst.shadercallcoherent)) ||
                   (src.queuefamilycoherent && (dst.coherent || dst.devicecoherent || dst.workgroupcoherent || dst.subgroupcoherent || dst.shadercallcoherent)) ||
                   (src.workgroupcoherent && (dst.coherent || dst.devicecoherent || dst.queuefamilycoherent || dst.subgroupcoherent || dst.shadercallcoherent)) ||
                   (src.subgroupcoherent  && (dst.coherent || dst.devicecoherent || dst.queuefamilycoherent || dst.workgroupcoherent || dst.shadercallcoherent)) ||
                   (src.shadercallcoherent && (dst.coherent || dst.devicecoherent || dst.queuefamilycoherent || dst.workgroupcoherent || dst.subgroupcoherent)))) {
        error(loc, "only one coherent/devicecoherent/queuefamilycoherent/workgroupcoherent/subgroupcoherent/shadercallcoherent qualifier allowed",
            GetPrecisionQualifierString(src.precision), "");
    }

    // Layout qualifiers
    mergeObjectLayoutQualifiers(dst, src, false);

    // individual qualifiers
    bool repeated = false;
    #define MERGE_SINGLETON(field) repeated |= dst.field && src.field; dst.field |= src.field;
    MERGE_SINGLETON(invariant);
    MERGE_SINGLETON(centroid);
    MERGE_SINGLETON(smooth);
    MERGE_SINGLETON(flat);
    MERGE_SINGLETON(specConstant);
    MERGE_SINGLETON(noContraction);
    MERGE_SINGLETON(nopersp);
    MERGE_SINGLETON(explicitInterp);
    MERGE_SINGLETON(perPrimitiveNV);
    MERGE_SINGLETON(perViewNV);
    MERGE_SINGLETON(perTaskNV);
    MERGE_SINGLETON(patch);
    MERGE_SINGLETON(sample);
    MERGE_SINGLETON(coherent);
    MERGE_SINGLETON(devicecoherent);
    MERGE_SINGLETON(queuefamilycoherent);
    MERGE_SINGLETON(workgroupcoherent);
    MERGE_SINGLETON(subgroupcoherent);
    MERGE_SINGLETON(shadercallcoherent);
    MERGE_SINGLETON(nonprivate);
    MERGE_SINGLETON(volatil);
    MERGE_SINGLETON(restrict);
    MERGE_SINGLETON(readonly);
    MERGE_SINGLETON(writeonly);
    MERGE_SINGLETON(nonUniform);

    // SPIR-V storage class qualifier (GL_EXT_spirv_intrinsics)
    dst.spirvStorageClass = src.spirvStorageClass;

    // SPIR-V decorate qualifiers (GL_EXT_spirv_intrinsics)
    if (src.hasSprivDecorate()) {
        if (dst.hasSprivDecorate()) {
            const TSpirvDecorate& srcSpirvDecorate = src.getSpirvDecorate();
            TSpirvDecorate& dstSpirvDecorate = dst.getSpirvDecorate();
            for (auto& decorate : srcSpirvDecorate.decorates) {
                if (dstSpirvDecorate.decorates.find(decorate.first) != dstSpirvDecorate.decorates.end())
                    error(loc, "too many SPIR-V decorate qualifiers", "spirv_decorate", "(decoration=%u)", decorate.first);
                else
                    dstSpirvDecorate.decorates.insert(decorate);
            }

            for (auto& decorateId : srcSpirvDecorate.decorateIds) {
                if (dstSpirvDecorate.decorateIds.find(decorateId.first) != dstSpirvDecorate.decorateIds.end())
                    error(loc, "too many SPIR-V decorate qualifiers", "spirv_decorate_id", "(decoration=%u)", decorateId.first);
                else
                    dstSpirvDecorate.decorateIds.insert(decorateId);
            }

            for (auto& decorateString : srcSpirvDecorate.decorateStrings) {
                if (dstSpirvDecorate.decorates.find(decorateString.first) != dstSpirvDecorate.decorates.end())
                    error(loc, "too many SPIR-V decorate qualifiers", "spirv_decorate_string", "(decoration=%u)", decorateString.first);
                else
                    dstSpirvDecorate.decorateStrings.insert(decorateString);
            }
        } else {
            dst.spirvDecorate = src.spirvDecorate;
        }
    }

    if (repeated)
        error(loc, "replicated qualifiers", "", "");
}

void TParseContext::setDefaultPrecision(const TSourceLoc& loc, TPublicType& publicType, TPrecisionQualifier qualifier)
{
    TBasicType basicType = publicType.basicType;

    if (basicType == EbtSampler) {
        defaultSamplerPrecision[computeSamplerTypeIndex(publicType.sampler)] = qualifier;

        return;  // all is well
    }

    if (basicType == EbtInt || basicType == EbtFloat) {
        if (publicType.isScalar()) {
            defaultPrecision[basicType] = qualifier;
            if (basicType == EbtInt) {
                defaultPrecision[EbtUint] = qualifier;
                precisionManager.explicitIntDefaultSeen();
            } else
                precisionManager.explicitFloatDefaultSeen();

            return;  // all is well
        }
    }

    if (basicType == EbtAtomicUint) {
        if (qualifier != EpqHigh)
            error(loc, "can only apply highp to atomic_uint", "precision", "");

        return;
    }

    error(loc, "cannot apply precision statement to this type; use 'float', 'int' or a sampler type", TType::getBasicString(basicType), "");
}

// used to flatten the sampler type space into a single dimension
// correlates with the declaration of defaultSamplerPrecision[]
int TParseContext::computeSamplerTypeIndex(TSampler& sampler)
{
    int arrayIndex    = sampler.arrayed         ? 1 : 0;
    int shadowIndex   = sampler.shadow          ? 1 : 0;
    int externalIndex = sampler.isExternal()    ? 1 : 0;
    int imageIndex    = sampler.isImageClass()  ? 1 : 0;
    int msIndex       = sampler.isMultiSample() ? 1 : 0;

    int flattened = EsdNumDims * (EbtNumTypes * (2 * (2 * (2 * (2 * arrayIndex + msIndex) + imageIndex) + shadowIndex) +
                                                 externalIndex) + sampler.type) + sampler.dim;
    assert(flattened < maxSamplerIndex);

    return flattened;
}

TPrecisionQualifier TParseContext::getDefaultPrecision(TPublicType& publicType)
{
    if (publicType.basicType == EbtSampler)
        return defaultSamplerPrecision[computeSamplerTypeIndex(publicType.sampler)];
    else
        return defaultPrecision[publicType.basicType];
}

void TParseContext::precisionQualifierCheck(const TSourceLoc& loc, TBasicType baseType, TQualifier& qualifier, bool isCoopMat)
{
    // Built-in symbols are allowed some ambiguous precisions, to be pinned down
    // later by context.
    if (! obeyPrecisionQualifiers() || parsingBuiltins)
        return;

    if (baseType == EbtAtomicUint && qualifier.precision != EpqNone && qualifier.precision != EpqHigh)
        error(loc, "atomic counters can only be highp", "atomic_uint", "");

    if (isCoopMat)
        return;

    if (baseType == EbtFloat || baseType == EbtUint || baseType == EbtInt || baseType == EbtSampler || baseType == EbtAtomicUint) {
        if (qualifier.precision == EpqNone) {
            if (relaxedErrors())
                warn(loc, "type requires declaration of default precision qualifier", TType::getBasicString(baseType), "substituting 'mediump'");
            else
                error(loc, "type requires declaration of default precision qualifier", TType::getBasicString(baseType), "");
            qualifier.precision = EpqMedium;
            defaultPrecision[baseType] = EpqMedium;
        }
    } else if (qualifier.precision != EpqNone)
        error(loc, "type cannot have precision qualifier", TType::getBasicString(baseType), "");
}

void TParseContext::parameterTypeCheck(const TSourceLoc& loc, TStorageQualifier qualifier, const TType& type)
{
    if ((qualifier == EvqOut || qualifier == EvqInOut) && type.isOpaque() && !intermediate.getBindlessMode())
        error(loc, "samplers and atomic_uints cannot be output parameters", type.getBasicTypeString().c_str(), "");
    if (!parsingBuiltins && type.contains16BitFloat())
        requireFloat16Arithmetic(loc, type.getBasicTypeString().c_str(), "float16 types can only be in uniform block or buffer storage");
    if (!parsingBuiltins && type.contains16BitInt())
        requireInt16Arithmetic(loc, type.getBasicTypeString().c_str(), "(u)int16 types can only be in uniform block or buffer storage");
    if (!parsingBuiltins && type.contains8BitInt())
        requireInt8Arithmetic(loc, type.getBasicTypeString().c_str(), "(u)int8 types can only be in uniform block or buffer storage");
}

bool TParseContext::containsFieldWithBasicType(const TType& type, TBasicType basicType)
{
    if (type.getBasicType() == basicType)
        return true;

    if (type.getBasicType() == EbtStruct) {
        const TTypeList& structure = *type.getStruct();
        for (unsigned int i = 0; i < structure.size(); ++i) {
            if (containsFieldWithBasicType(*structure[i].type, basicType))
                return true;
        }
    }

    return false;
}

//
// Do size checking for an array type's size.
//
void TParseContext::arraySizeCheck(const TSourceLoc& loc, TIntermTyped* expr, TArraySize& sizePair,
                                   const char* sizeType, const bool allowZero)
{
    bool isConst = false;
    sizePair.node = nullptr;

    int size = 1;

    TIntermConstantUnion* constant = expr->getAsConstantUnion();
    if (constant) {
        // handle true (non-specialization) constant
        size = constant->getConstArray()[0].getIConst();
        isConst = true;
    } else {
        // see if it's a specialization constant instead
        if (expr->getQualifier().isSpecConstant()) {
            isConst = true;
            sizePair.node = expr;
            TIntermSymbol* symbol = expr->getAsSymbolNode();
            if (symbol && symbol->getConstArray().size() > 0)
                size = symbol->getConstArray()[0].getIConst();
        } else if (expr->getAsUnaryNode() && expr->getAsUnaryNode()->getOp() == glslang::EOpArrayLength &&
                   expr->getAsUnaryNode()->getOperand()->getType().isCoopMatNV()) {
            isConst = true;
            size = 1;
            sizePair.node = expr->getAsUnaryNode();
        }
    }

    sizePair.size = size;

    if (! isConst || (expr->getBasicType() != EbtInt && expr->getBasicType() != EbtUint)) {
        error(loc, sizeType, "", "must be a constant integer expression");
        return;
    }

    if (allowZero) {
        if (size < 0) {
            error(loc, sizeType, "", "must be a non-negative integer");
            return;
        }
    } else {
        if (size <= 0) {
            error(loc, sizeType, "", "must be a positive integer");
            return;
        }
    }
}

//
// See if this qualifier can be an array.
//
// Returns true if there is an error.
//
bool TParseContext::arrayQualifierError(const TSourceLoc& loc, const TQualifier& qualifier)
{
    if (qualifier.storage == EvqConst) {
        profileRequires(loc, ENoProfile, 120, E_GL_3DL_array_objects, "const array");
        profileRequires(loc, EEsProfile, 300, nullptr, "const array");
    }

    if (qualifier.storage == EvqVaryingIn && language == EShLangVertex) {
        requireProfile(loc, ~EEsProfile, "vertex input arrays");
        profileRequires(loc, ENoProfile, 150, nullptr, "vertex input arrays");
    }

    return false;
}

//
// See if this qualifier and type combination can be an array.
// Assumes arrayQualifierError() was also called to catch the type-invariant tests.
//
// Returns true if there is an error.
//
bool TParseContext::arrayError(const TSourceLoc& loc, const TType& type)
{
    if (type.getQualifier().storage == EvqVaryingOut && language == EShLangVertex) {
        if (type.isArrayOfArrays())
            requireProfile(loc, ~EEsProfile, "vertex-shader array-of-array output");
        else if (type.isStruct())
            requireProfile(loc, ~EEsProfile, "vertex-shader array-of-struct output");
    }
    if (type.getQualifier().storage == EvqVaryingIn && language == EShLangFragment) {
        if (type.isArrayOfArrays())
            requireProfile(loc, ~EEsProfile, "fragment-shader array-of-array input");
        else if (type.isStruct())
            requireProfile(loc, ~EEsProfile, "fragment-shader array-of-struct input");
    }
    if (type.getQualifier().storage == EvqVaryingOut && language == EShLangFragment) {
        if (type.isArrayOfArrays())
            requireProfile(loc, ~EEsProfile, "fragment-shader array-of-array output");
    }

    return false;
}

//
// Require array to be completely sized
//
void TParseContext::arraySizeRequiredCheck(const TSourceLoc& loc, const TArraySizes& arraySizes)
{
    if (!parsingBuiltins && arraySizes.hasUnsized())
        error(loc, "array size required", "", "");
}

void TParseContext::structArrayCheck(const TSourceLoc& /*loc*/, const TType& type)
{
    const TTypeList& structure = *type.getStruct();
    for (int m = 0; m < (int)structure.size(); ++m) {
        const TType& member = *structure[m].type;
        if (member.isArray())
            arraySizeRequiredCheck(structure[m].loc, *member.getArraySizes());
    }
}

void TParseContext::arraySizesCheck(const TSourceLoc& loc, const TQualifier& qualifier, TArraySizes* arraySizes,
    const TIntermTyped* initializer, bool lastMember)
{
    assert(arraySizes);

    // always allow special built-in ins/outs sized to topologies
    if (parsingBuiltins)
        return;

    // initializer must be a sized array, in which case
    // allow the initializer to set any unknown array sizes
    if (initializer != nullptr) {
        if (initializer->getType().isUnsizedArray())
            error(loc, "array initializer must be sized", "[]", "");
        return;
    }

    // No environment allows any non-outer-dimension to be implicitly sized
    if (arraySizes->isInnerUnsized()) {
        error(loc, "only outermost dimension of an array of arrays can be implicitly sized", "[]", "");
        arraySizes->clearInnerUnsized();
    }

    if (arraySizes->isInnerSpecialization() &&
        (qualifier.storage != EvqTemporary && qualifier.storage != EvqGlobal && qualifier.storage != EvqShared && qualifier.storage != EvqConst))
        error(loc, "only outermost dimension of an array of arrays can be a specialization constant", "[]", "");

    // desktop always allows outer-dimension-unsized variable arrays,
    if (!isEsProfile())
        return;

    // for ES, if size isn't coming from an initializer, it has to be explicitly declared now,
    // with very few exceptions

    // implicitly-sized io exceptions:
    switch (language) {
    case EShLangGeometry:
        if (qualifier.storage == EvqVaryingIn)
            if ((isEsProfile() && version >= 320) ||
                extensionsTurnedOn(Num_AEP_geometry_shader, AEP_geometry_shader))
                return;
        break;
    case EShLangTessControl:
        if ( qualifier.storage == EvqVaryingIn ||
            (qualifier.storage == EvqVaryingOut && ! qualifier.isPatch()))
            if ((isEsProfile() && version >= 320) ||
                extensionsTurnedOn(Num_AEP_tessellation_shader, AEP_tessellation_shader))
                return;
        break;
    case EShLangTessEvaluation:
        if ((qualifier.storage == EvqVaryingIn && ! qualifier.isPatch()) ||
             qualifier.storage == EvqVaryingOut)
            if ((isEsProfile() && version >= 320) ||
                extensionsTurnedOn(Num_AEP_tessellation_shader, AEP_tessellation_shader))
                return;
        break;
    case EShLangMesh:
        if (qualifier.storage == EvqVaryingOut)
            if ((isEsProfile() && version >= 320) ||
                extensionsTurnedOn(Num_AEP_mesh_shader, AEP_mesh_shader))
                return;
        break;
    default:
        break;
    }

    // last member of ssbo block exception:
    if (qualifier.storage == EvqBuffer && lastMember)
        return;

    arraySizeRequiredCheck(loc, *arraySizes);
}

void TParseContext::arrayOfArrayVersionCheck(const TSourceLoc& loc, const TArraySizes* sizes)
{
    if (sizes == nullptr || sizes->getNumDims() == 1)
        return;

    const char* feature = "arrays of arrays";

    requireProfile(loc, EEsProfile | ECoreProfile | ECompatibilityProfile, feature);
    profileRequires(loc, EEsProfile, 310, nullptr, feature);
    profileRequires(loc, ECoreProfile | ECompatibilityProfile, 430, nullptr, feature);
}

//
// Do all the semantic checking for declaring or redeclaring an array, with and
// without a size, and make the right changes to the symbol table.
//
void TParseContext::declareArray(const TSourceLoc& loc, const TString& identifier, const TType& type, TSymbol*& symbol)
{
    if (symbol == nullptr) {
        bool currentScope;
        symbol = symbolTable.find(identifier, nullptr, &currentScope);

        if (symbol && builtInName(identifier) && ! symbolTable.atBuiltInLevel()) {
            // bad shader (errors already reported) trying to redeclare a built-in name as an array
            symbol = nullptr;
            return;
        }
        if (symbol == nullptr || ! currentScope) {
            //
            // Successfully process a new definition.
            // (Redeclarations have to take place at the same scope; otherwise they are hiding declarations)
            //
            symbol = new TVariable(&identifier, type);
            symbolTable.insert(*symbol);
            if (symbolTable.atGlobalLevel())
                trackLinkage(*symbol);

            if (! symbolTable.atBuiltInLevel()) {
                if (isIoResizeArray(type)) {
                    ioArraySymbolResizeList.push_back(symbol);
                    checkIoArraysConsistency(loc, true);
                } else
                    fixIoArraySize(loc, symbol->getWritableType());
            }

            return;
        }
        if (symbol->getAsAnonMember()) {
            error(loc, "cannot redeclare a user-block member array", identifier.c_str(), "");
            symbol = nullptr;
            return;
        }
    }

    //
    // Process a redeclaration.
    //

    if (symbol == nullptr) {
        error(loc, "array variable name expected", identifier.c_str(), "");
        return;
    }

    // redeclareBuiltinVariable() should have already done the copyUp()
    TType& existingType = symbol->getWritableType();

    if (! existingType.isArray()) {
        error(loc, "redeclaring non-array as array", identifier.c_str(), "");
        return;
    }

    if (! existingType.sameElementType(type)) {
        error(loc, "redeclaration of array with a different element type", identifier.c_str(), "");
        return;
    }

    if (! existingType.sameInnerArrayness(type)) {
        error(loc, "redeclaration of array with a different array dimensions or sizes", identifier.c_str(), "");
        return;
    }

    if (existingType.isSizedArray()) {
        // be more leniant for input arrays to geometry shaders and tessellation control outputs, where the redeclaration is the same size
        if (! (isIoResizeArray(type) && existingType.getOuterArraySize() == type.getOuterArraySize()))
            error(loc, "redeclaration of array with size", identifier.c_str(), "");
        return;
    }

    arrayLimitCheck(loc, identifier, type.getOuterArraySize());

    existingType.updateArraySizes(type);

    if (isIoResizeArray(type))
        checkIoArraysConsistency(loc);
}

// Policy and error check for needing a runtime sized array.
void TParseContext::checkRuntimeSizable(const TSourceLoc& loc, const TIntermTyped& base)
{
    // runtime length implies runtime sizeable, so no problem
    if (isRuntimeLength(base))
        return;

    if (base.getType().getQualifier().builtIn == EbvSampleMask)
        return;

    // Check for last member of a bufferreference type, which is runtime sizeable
    // but doesn't support runtime length
    if (base.getType().getQualifier().storage == EvqBuffer) {
        const TIntermBinary* binary = base.getAsBinaryNode();
        if (binary != nullptr &&
            binary->getOp() == EOpIndexDirectStruct &&
            binary->getLeft()->isReference()) {

            const int index = binary->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();
            const int memberCount = (int)binary->getLeft()->getType().getReferentType()->getStruct()->size();
            if (index == memberCount - 1)
                return;
        }
    }

    // check for additional things allowed by GL_EXT_nonuniform_qualifier
    if (base.getBasicType() == EbtSampler || base.getBasicType() == EbtAccStruct || base.getBasicType() == EbtRayQuery ||
        base.getBasicType() == EbtHitObjectNV || (base.getBasicType() == EbtBlock && base.getType().getQualifier().isUniformOrBuffer()))
        requireExtensions(loc, 1, &E_GL_EXT_nonuniform_qualifier, "variable index");
    else
        error(loc, "", "[", "array must be redeclared with a size before being indexed with a variable");
}

// Policy decision for whether a run-time .length() is allowed.
bool TParseContext::isRuntimeLength(const TIntermTyped& base) const
{
    if (base.getType().getQualifier().storage == EvqBuffer) {
        // in a buffer block
        const TIntermBinary* binary = base.getAsBinaryNode();
        if (binary != nullptr && binary->getOp() == EOpIndexDirectStruct) {
            // is it the last member?
            const int index = binary->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();

            if (binary->getLeft()->isReference())
                return false;

            const int memberCount = (int)binary->getLeft()->getType().getStruct()->size();
            if (index == memberCount - 1)
                return true;
        }
    }

    return false;
}

// Check if mesh perviewNV attributes have a view dimension
// and resize it to gl_MaxMeshViewCountNV when implicitly sized.
void TParseContext::checkAndResizeMeshViewDim(const TSourceLoc& loc, TType& type, bool isBlockMember)
{
    // see if member is a per-view attribute
    if (!type.getQualifier().isPerView())
        return;

    if ((isBlockMember && type.isArray()) || (!isBlockMember && type.isArrayOfArrays())) {
        // since we don't have the maxMeshViewCountNV set during parsing builtins, we hardcode the value.
        int maxViewCount = parsingBuiltins ? 4 : resources.maxMeshViewCountNV;
        // For block members, outermost array dimension is the view dimension.
        // For non-block members, outermost array dimension is the vertex/primitive dimension
        // and 2nd outermost is the view dimension.
        int viewDim = isBlockMember ? 0 : 1;
        int viewDimSize = type.getArraySizes()->getDimSize(viewDim);

        if (viewDimSize != UnsizedArraySize && viewDimSize != maxViewCount)
            error(loc, "mesh view output array size must be gl_MaxMeshViewCountNV or implicitly sized", "[]", "");
        else if (viewDimSize == UnsizedArraySize)
            type.getArraySizes()->setDimSize(viewDim, maxViewCount);
    }
    else {
        error(loc, "requires a view array dimension", "perviewNV", "");
    }
}

// Returns true if the first argument to the #line directive is the line number for the next line.
//
// Desktop, pre-version 3.30:  "After processing this directive
// (including its new-line), the implementation will behave as if it is compiling at line number line+1 and
// source string number source-string-number."
//
// Desktop, version 3.30 and later, and ES:  "After processing this directive
// (including its new-line), the implementation will behave as if it is compiling at line number line and
// source string number source-string-number.
bool TParseContext::lineDirectiveShouldSetNextLine() const
{
    return isEsProfile() || version >= 330;
}

//
// Enforce non-initializer type/qualifier rules.
//
void TParseContext::nonInitConstCheck(const TSourceLoc& loc, TString& identifier, TType& type)
{
    //
    // Make the qualifier make sense, given that there is not an initializer.
    //
    if (type.getQualifier().storage == EvqConst ||
        type.getQualifier().storage == EvqConstReadOnly) {
        type.getQualifier().makeTemporary();
        error(loc, "variables with qualifier 'const' must be initialized", identifier.c_str(), "");
    }
}

//
// See if the identifier is a built-in symbol that can be redeclared, and if so,
// copy the symbol table's read-only built-in variable to the current
// global level, where it can be modified based on the passed in type.
//
// Returns nullptr if no redeclaration took place; meaning a normal declaration still
// needs to occur for it, not necessarily an error.
//
// Returns a redeclared and type-modified variable if a redeclarated occurred.
//
TSymbol* TParseContext::redeclareBuiltinVariable(const TSourceLoc& loc, const TString& identifier,
                                                 const TQualifier& qualifier, const TShaderQualifiers& publicType)
{
    if (! builtInName(identifier) || symbolTable.atBuiltInLevel() || ! symbolTable.atGlobalLevel())
        return nullptr;

    bool nonEsRedecls = (!isEsProfile() && (version >= 130 || identifier == "gl_TexCoord"));
    bool    esRedecls = (isEsProfile() &&
                         (version >= 320 || extensionsTurnedOn(Num_AEP_shader_io_blocks, AEP_shader_io_blocks)));
    if (! esRedecls && ! nonEsRedecls)
        return nullptr;

    // Special case when using GL_ARB_separate_shader_objects
    bool ssoPre150 = false;  // means the only reason this variable is redeclared is due to this combination
    if (!isEsProfile() && version <= 140 && extensionTurnedOn(E_GL_ARB_separate_shader_objects)) {
        if (identifier == "gl_Position"     ||
            identifier == "gl_PointSize"    ||
            identifier == "gl_ClipVertex"   ||
            identifier == "gl_FogFragCoord")
            ssoPre150 = true;
    }

    // Potentially redeclaring a built-in variable...

    if (ssoPre150 ||
        (identifier == "gl_FragDepth"           && ((nonEsRedecls && version >= 420) || esRedecls)) ||
        (identifier == "gl_FragCoord"           && ((nonEsRedecls && version >= 140) || esRedecls)) ||
         identifier == "gl_ClipDistance"                                                            ||
         identifier == "gl_CullDistance"                                                            ||
         identifier == "gl_ShadingRateEXT"                                                          ||
         identifier == "gl_PrimitiveShadingRateEXT"                                                 ||
         identifier == "gl_FrontColor"                                                              ||
         identifier == "gl_BackColor"                                                               ||
         identifier == "gl_FrontSecondaryColor"                                                     ||
         identifier == "gl_BackSecondaryColor"                                                      ||
         identifier == "gl_SecondaryColor"                                                          ||
        (identifier == "gl_Color"               && language == EShLangFragment)                     ||
        (identifier == "gl_FragStencilRefARB"   && (nonEsRedecls && version >= 140)
                                                && language == EShLangFragment)                     ||
         identifier == "gl_SampleMask"                                                              ||
         identifier == "gl_Layer"                                                                   ||
         identifier == "gl_PrimitiveIndicesNV"                                                      ||
         identifier == "gl_PrimitivePointIndicesEXT"                                                ||
         identifier == "gl_PrimitiveLineIndicesEXT"                                                 ||
         identifier == "gl_PrimitiveTriangleIndicesEXT"                                             ||
         identifier == "gl_TexCoord") {

        // Find the existing symbol, if any.
        bool builtIn;
        TSymbol* symbol = symbolTable.find(identifier, &builtIn);

        // If the symbol was not found, this must be a version/profile/stage
        // that doesn't have it.
        if (! symbol)
            return nullptr;

        // If it wasn't at a built-in level, then it's already been redeclared;
        // that is, this is a redeclaration of a redeclaration; reuse that initial
        // redeclaration.  Otherwise, make the new one.
        if (builtIn) {
            makeEditable(symbol);
            symbolTable.amendSymbolIdLevel(*symbol);
        }

        // Now, modify the type of the copy, as per the type of the current redeclaration.

        TQualifier& symbolQualifier = symbol->getWritableType().getQualifier();
        if (ssoPre150) {
            if (intermediate.inIoAccessed(identifier))
                error(loc, "cannot redeclare after use", identifier.c_str(), "");
            if (qualifier.hasLayout())
                error(loc, "cannot apply layout qualifier to", "redeclaration", symbol->getName().c_str());
            if (qualifier.isMemory() || qualifier.isAuxiliary() || (language == EShLangVertex   && qualifier.storage != EvqVaryingOut) ||
                                                                   (language == EShLangFragment && qualifier.storage != EvqVaryingIn))
                error(loc, "cannot change storage, memory, or auxiliary qualification of", "redeclaration", symbol->getName().c_str());
            if (! qualifier.smooth)
                error(loc, "cannot change interpolation qualification of", "redeclaration", symbol->getName().c_str());
        } else if (identifier == "gl_FrontColor"          ||
                   identifier == "gl_BackColor"           ||
                   identifier == "gl_FrontSecondaryColor" ||
                   identifier == "gl_BackSecondaryColor"  ||
                   identifier == "gl_SecondaryColor"      ||
                   identifier == "gl_Color") {
            symbolQualifier.flat = qualifier.flat;
            symbolQualifier.smooth = qualifier.smooth;
            symbolQualifier.nopersp = qualifier.nopersp;
            if (qualifier.hasLayout())
                error(loc, "cannot apply layout qualifier to", "redeclaration", symbol->getName().c_str());
            if (qualifier.isMemory() || qualifier.isAuxiliary() || symbol->getType().getQualifier().storage != qualifier.storage)
                error(loc, "cannot change storage, memory, or auxiliary qualification of", "redeclaration", symbol->getName().c_str());
        } else if (identifier == "gl_TexCoord"     ||
                   identifier == "gl_ClipDistance" ||
                   identifier == "gl_CullDistance") {
            if (qualifier.hasLayout() || qualifier.isMemory() || qualifier.isAuxiliary() ||
                qualifier.nopersp != symbolQualifier.nopersp || qualifier.flat != symbolQualifier.flat ||
                symbolQualifier.storage != qualifier.storage)
                error(loc, "cannot change qualification of", "redeclaration", symbol->getName().c_str());
        } else if (identifier == "gl_FragCoord") {
            if (!intermediate.getTexCoordRedeclared() && intermediate.inIoAccessed("gl_FragCoord"))
                error(loc, "cannot redeclare after use", "gl_FragCoord", "");
            if (qualifier.nopersp != symbolQualifier.nopersp || qualifier.flat != symbolQualifier.flat ||
                qualifier.isMemory() || qualifier.isAuxiliary())
                error(loc, "can only change layout qualification of", "redeclaration", symbol->getName().c_str());
            if (qualifier.storage != EvqVaryingIn)
                error(loc, "cannot change input storage qualification of", "redeclaration", symbol->getName().c_str());
            if (! builtIn && (publicType.pixelCenterInteger != intermediate.getPixelCenterInteger() ||
                              publicType.originUpperLeft != intermediate.getOriginUpperLeft()))
                error(loc, "cannot redeclare with different qualification:", "redeclaration", symbol->getName().c_str());


            intermediate.setTexCoordRedeclared();
            if (publicType.pixelCenterInteger)
                intermediate.setPixelCenterInteger();
            if (publicType.originUpperLeft)
                intermediate.setOriginUpperLeft();
        } else if (identifier == "gl_FragDepth") {
            if (qualifier.nopersp != symbolQualifier.nopersp || qualifier.flat != symbolQualifier.flat ||
                qualifier.isMemory() || qualifier.isAuxiliary())
                error(loc, "can only change layout qualification of", "redeclaration", symbol->getName().c_str());
            if (qualifier.storage != EvqVaryingOut)
                error(loc, "cannot change output storage qualification of", "redeclaration", symbol->getName().c_str());
            if (publicType.layoutDepth != EldNone) {
                if (intermediate.inIoAccessed("gl_FragDepth"))
                    error(loc, "cannot redeclare after use", "gl_FragDepth", "");
                if (! intermediate.setDepth(publicType.layoutDepth))
                    error(loc, "all redeclarations must use the same depth layout on", "redeclaration", symbol->getName().c_str());
            }
        } else if (identifier == "gl_FragStencilRefARB") {
            if (qualifier.nopersp != symbolQualifier.nopersp || qualifier.flat != symbolQualifier.flat ||
                qualifier.isMemory() || qualifier.isAuxiliary())
                error(loc, "can only change layout qualification of", "redeclaration", symbol->getName().c_str());
            if (qualifier.storage != EvqVaryingOut)
                error(loc, "cannot change output storage qualification of", "redeclaration", symbol->getName().c_str());
            if (publicType.layoutStencil != ElsNone) {
                if (intermediate.inIoAccessed("gl_FragStencilRefARB"))
                    error(loc, "cannot redeclare after use", "gl_FragStencilRefARB", "");
                if (!intermediate.setStencil(publicType.layoutStencil))
                    error(loc, "all redeclarations must use the same stencil layout on", "redeclaration",
                          symbol->getName().c_str());
            }
        }
        else if (
            identifier == "gl_PrimitiveIndicesNV") {
            if (qualifier.hasLayout())
                error(loc, "cannot apply layout qualifier to", "redeclaration", symbol->getName().c_str());
            if (qualifier.storage != EvqVaryingOut)
                error(loc, "cannot change output storage qualification of", "redeclaration", symbol->getName().c_str());
        }
        else if (identifier == "gl_SampleMask") {
            if (!publicType.layoutOverrideCoverage) {
                error(loc, "redeclaration only allowed for override_coverage layout", "redeclaration", symbol->getName().c_str());
            }
            intermediate.setLayoutOverrideCoverage();
        }
        else if (identifier == "gl_Layer") {
            if (!qualifier.layoutViewportRelative && qualifier.layoutSecondaryViewportRelativeOffset == -2048)
                error(loc, "redeclaration only allowed for viewport_relative or secondary_view_offset layout", "redeclaration", symbol->getName().c_str());
            symbolQualifier.layoutViewportRelative = qualifier.layoutViewportRelative;
            symbolQualifier.layoutSecondaryViewportRelativeOffset = qualifier.layoutSecondaryViewportRelativeOffset;
        }

        // TODO: semantics quality: separate smooth from nothing declared, then use IsInterpolation for several tests above

        return symbol;
    }

    return nullptr;
}

//
// Either redeclare the requested block, or give an error message why it can't be done.
//
// TODO: functionality: explicitly sizing members of redeclared blocks is not giving them an explicit size
void TParseContext::redeclareBuiltinBlock(const TSourceLoc& loc, TTypeList& newTypeList, const TString& blockName,
    const TString* instanceName, TArraySizes* arraySizes)
{
    const char* feature = "built-in block redeclaration";
    profileRequires(loc, EEsProfile, 320, Num_AEP_shader_io_blocks, AEP_shader_io_blocks, feature);
    profileRequires(loc, ~EEsProfile, 410, E_GL_ARB_separate_shader_objects, feature);

    if (blockName != "gl_PerVertex" && blockName != "gl_PerFragment" &&
        blockName != "gl_MeshPerVertexNV" && blockName != "gl_MeshPerPrimitiveNV" &&
        blockName != "gl_MeshPerVertexEXT" && blockName != "gl_MeshPerPrimitiveEXT") {
        error(loc, "cannot redeclare block: ", "block declaration", blockName.c_str());
        return;
    }

    // Redeclaring a built-in block...

    if (instanceName && ! builtInName(*instanceName)) {
        error(loc, "cannot redeclare a built-in block with a user name", instanceName->c_str(), "");
        return;
    }

    // Blocks with instance names are easy to find, lookup the instance name,
    // Anonymous blocks need to be found via a member.
    bool builtIn;
    TSymbol* block;
    if (instanceName)
        block = symbolTable.find(*instanceName, &builtIn);
    else
        block = symbolTable.find(newTypeList.front().type->getFieldName(), &builtIn);

    // If the block was not found, this must be a version/profile/stage
    // that doesn't have it, or the instance name is wrong.
    const char* errorName = instanceName ? instanceName->c_str() : newTypeList.front().type->getFieldName().c_str();
    if (! block) {
        error(loc, "no declaration found for redeclaration", errorName, "");
        return;
    }
    // Built-in blocks cannot be redeclared more than once, which if happened,
    // we'd be finding the already redeclared one here, rather than the built in.
    if (! builtIn) {
        error(loc, "can only redeclare a built-in block once, and before any use", blockName.c_str(), "");
        return;
    }

    // Copy the block to make a writable version, to insert into the block table after editing.
    block = symbolTable.copyUpDeferredInsert(block);

    if (block->getType().getBasicType() != EbtBlock) {
        error(loc, "cannot redeclare a non block as a block", errorName, "");
        return;
    }

    // Fix XFB stuff up, it applies to the order of the redeclaration, not
    // the order of the original members.
    if (currentBlockQualifier.storage == EvqVaryingOut && globalOutputDefaults.hasXfbBuffer()) {
        if (!currentBlockQualifier.hasXfbBuffer())
            currentBlockQualifier.layoutXfbBuffer = globalOutputDefaults.layoutXfbBuffer;
        if (!currentBlockQualifier.hasStream())
            currentBlockQualifier.layoutStream = globalOutputDefaults.layoutStream;
        fixXfbOffsets(currentBlockQualifier, newTypeList);
    }

    // Edit and error check the container against the redeclaration
    //  - remove unused members
    //  - ensure remaining qualifiers/types match

    TType& type = block->getWritableType();

    // if gl_PerVertex is redeclared for the purpose of passing through "gl_Position"
    // for passthrough purpose, the redeclared block should have the same qualifers as
    // the current one
    if (currentBlockQualifier.layoutPassthrough) {
        type.getQualifier().layoutPassthrough = currentBlockQualifier.layoutPassthrough;
        type.getQualifier().storage = currentBlockQualifier.storage;
        type.getQualifier().layoutStream = currentBlockQualifier.layoutStream;
        type.getQualifier().layoutXfbBuffer = currentBlockQualifier.layoutXfbBuffer;
    }

    TTypeList::iterator member = type.getWritableStruct()->begin();
    size_t numOriginalMembersFound = 0;
    while (member != type.getStruct()->end()) {
        // look for match
        bool found = false;
        TTypeList::const_iterator newMember;
        TSourceLoc memberLoc;
        memberLoc.init();
        for (newMember = newTypeList.begin(); newMember != newTypeList.end(); ++newMember) {
            if (member->type->getFieldName() == newMember->type->getFieldName()) {
                found = true;
                memberLoc = newMember->loc;
                break;
            }
        }

        if (found) {
            ++numOriginalMembersFound;
            // - ensure match between redeclared members' types
            // - check for things that can't be changed
            // - update things that can be changed
            TType& oldType = *member->type;
            const TType& newType = *newMember->type;
            if (! newType.sameElementType(oldType))
                error(memberLoc, "cannot redeclare block member with a different type", member->type->getFieldName().c_str(), "");
            if (oldType.isArray() != newType.isArray())
                error(memberLoc, "cannot change arrayness of redeclared block member", member->type->getFieldName().c_str(), "");
            else if (! oldType.getQualifier().isPerView() && ! oldType.sameArrayness(newType) && oldType.isSizedArray())
                error(memberLoc, "cannot change array size of redeclared block member", member->type->getFieldName().c_str(), "");
            else if (! oldType.getQualifier().isPerView() && newType.isArray())
                arrayLimitCheck(loc, member->type->getFieldName(), newType.getOuterArraySize());
            if (oldType.getQualifier().isPerView() && ! newType.getQualifier().isPerView())
                error(memberLoc, "missing perviewNV qualifier to redeclared block member", member->type->getFieldName().c_str(), "");
            else if (! oldType.getQualifier().isPerView() && newType.getQualifier().isPerView())
                error(memberLoc, "cannot add perviewNV qualifier to redeclared block member", member->type->getFieldName().c_str(), "");
            else if (newType.getQualifier().isPerView()) {
                if (oldType.getArraySizes()->getNumDims() != newType.getArraySizes()->getNumDims())
                    error(memberLoc, "cannot change arrayness of redeclared block member", member->type->getFieldName().c_str(), "");
                else if (! newType.isUnsizedArray() && newType.getOuterArraySize() != resources.maxMeshViewCountNV)
                    error(loc, "mesh view output array size must be gl_MaxMeshViewCountNV or implicitly sized", "[]", "");
                else if (newType.getArraySizes()->getNumDims() == 2) {
                    int innerDimSize = newType.getArraySizes()->getDimSize(1);
                    arrayLimitCheck(memberLoc, member->type->getFieldName(), innerDimSize);
                    oldType.getArraySizes()->setDimSize(1, innerDimSize);
                }
            }
            if (oldType.getQualifier().isPerPrimitive() && ! newType.getQualifier().isPerPrimitive())
                error(memberLoc, "missing perprimitiveNV qualifier to redeclared block member", member->type->getFieldName().c_str(), "");
            else if (! oldType.getQualifier().isPerPrimitive() && newType.getQualifier().isPerPrimitive())
                error(memberLoc, "cannot add perprimitiveNV qualifier to redeclared block member", member->type->getFieldName().c_str(), "");
            if (newType.getQualifier().isMemory())
                error(memberLoc, "cannot add memory qualifier to redeclared block member", member->type->getFieldName().c_str(), "");
            if (newType.getQualifier().hasNonXfbLayout())
                error(memberLoc, "cannot add non-XFB layout to redeclared block member", member->type->getFieldName().c_str(), "");
            if (newType.getQualifier().patch)
                error(memberLoc, "cannot add patch to redeclared block member", member->type->getFieldName().c_str(), "");
            if (newType.getQualifier().hasXfbBuffer() &&
                newType.getQualifier().layoutXfbBuffer != currentBlockQualifier.layoutXfbBuffer)
                error(memberLoc, "member cannot contradict block (or what block inherited from global)", "xfb_buffer", "");
            if (newType.getQualifier().hasStream() &&
                newType.getQualifier().layoutStream != currentBlockQualifier.layoutStream)
                error(memberLoc, "member cannot contradict block (or what block inherited from global)", "xfb_stream", "");
            oldType.getQualifier().centroid = newType.getQualifier().centroid;
            oldType.getQualifier().sample = newType.getQualifier().sample;
            oldType.getQualifier().invariant = newType.getQualifier().invariant;
            oldType.getQualifier().noContraction = newType.getQualifier().noContraction;
            oldType.getQualifier().smooth = newType.getQualifier().smooth;
            oldType.getQualifier().flat = newType.getQualifier().flat;
            oldType.getQualifier().nopersp = newType.getQualifier().nopersp;
            oldType.getQualifier().layoutXfbOffset = newType.getQualifier().layoutXfbOffset;
            oldType.getQualifier().layoutXfbBuffer = newType.getQualifier().layoutXfbBuffer;
            oldType.getQualifier().layoutXfbStride = newType.getQualifier().layoutXfbStride;
            if (oldType.getQualifier().layoutXfbOffset != TQualifier::layoutXfbBufferEnd) {
                // If any member has an xfb_offset, then the block's xfb_buffer inherents current xfb_buffer,
                // and for xfb processing, the member needs it as well, along with xfb_stride.
                type.getQualifier().layoutXfbBuffer = currentBlockQualifier.layoutXfbBuffer;
                oldType.getQualifier().layoutXfbBuffer = currentBlockQualifier.layoutXfbBuffer;
            }
            if (oldType.isUnsizedArray() && newType.isSizedArray())
                oldType.changeOuterArraySize(newType.getOuterArraySize());

            //  check and process the member's type, which will include managing xfb information
            layoutTypeCheck(loc, oldType);

            // go to next member
            ++member;
        } else {
            // For missing members of anonymous blocks that have been redeclared,
            // hide the original (shared) declaration.
            // Instance-named blocks can just have the member removed.
            if (instanceName)
                member = type.getWritableStruct()->erase(member);
            else {
                member->type->hideMember();
                ++member;
            }
        }
    }

    if (spvVersion.vulkan > 0) {
        // ...then streams apply to built-in blocks, instead of them being only on stream 0
        type.getQualifier().layoutStream = currentBlockQualifier.layoutStream;
    }

    if (numOriginalMembersFound < newTypeList.size())
        error(loc, "block redeclaration has extra members", blockName.c_str(), "");
    if (type.isArray() != (arraySizes != nullptr) ||
        (type.isArray() && arraySizes != nullptr && type.getArraySizes()->getNumDims() != arraySizes->getNumDims()))
        error(loc, "cannot change arrayness of redeclared block", blockName.c_str(), "");
    else if (type.isArray()) {
        // At this point, we know both are arrays and both have the same number of dimensions.

        // It is okay for a built-in block redeclaration to be unsized, and keep the size of the
        // original block declaration.
        if (!arraySizes->isSized() && type.isSizedArray())
            arraySizes->changeOuterSize(type.getOuterArraySize());

        // And, okay to be giving a size to the array, by the redeclaration
        if (!type.isSizedArray() && arraySizes->isSized())
            type.changeOuterArraySize(arraySizes->getOuterSize());

        // Now, they must match in all dimensions.
        if (type.isSizedArray() && *type.getArraySizes() != *arraySizes)
            error(loc, "cannot change array size of redeclared block", blockName.c_str(), "");
    }

    symbolTable.insert(*block);

    // Check for general layout qualifier errors
    layoutObjectCheck(loc, *block);

    // Tracking for implicit sizing of array
    if (isIoResizeArray(block->getType())) {
        ioArraySymbolResizeList.push_back(block);
        checkIoArraysConsistency(loc, true);
    } else if (block->getType().isArray())
        fixIoArraySize(loc, block->getWritableType());

    // Save it in the AST for linker use.
    trackLinkage(*block);
}

void TParseContext::paramCheckFixStorage(const TSourceLoc& loc, const TStorageQualifier& qualifier, TType& type)
{
    switch (qualifier) {
    case EvqConst:
    case EvqConstReadOnly:
        type.getQualifier().storage = EvqConstReadOnly;
        break;
    case EvqIn:
    case EvqOut:
    case EvqInOut:
    case EvqTileImageEXT:
        type.getQualifier().storage = qualifier;
        break;
    case EvqGlobal:
    case EvqTemporary:
        type.getQualifier().storage = EvqIn;
        break;
    default:
        type.getQualifier().storage = EvqIn;
        error(loc, "storage qualifier not allowed on function parameter", GetStorageQualifierString(qualifier), "");
        break;
    }
}

void TParseContext::paramCheckFix(const TSourceLoc& loc, const TQualifier& qualifier, TType& type)
{
    if (qualifier.isMemory()) {
        type.getQualifier().volatil   = qualifier.volatil;
        type.getQualifier().coherent  = qualifier.coherent;
        type.getQualifier().devicecoherent  = qualifier.devicecoherent ;
        type.getQualifier().queuefamilycoherent  = qualifier.queuefamilycoherent;
        type.getQualifier().workgroupcoherent  = qualifier.workgroupcoherent;
        type.getQualifier().subgroupcoherent  = qualifier.subgroupcoherent;
        type.getQualifier().shadercallcoherent = qualifier.shadercallcoherent;
        type.getQualifier().nonprivate = qualifier.nonprivate;
        type.getQualifier().readonly  = qualifier.readonly;
        type.getQualifier().writeonly = qualifier.writeonly;
        type.getQualifier().restrict  = qualifier.restrict;
    }

    if (qualifier.isAuxiliary() ||
        qualifier.isInterpolation())
        error(loc, "cannot use auxiliary or interpolation qualifiers on a function parameter", "", "");
    if (qualifier.hasLayout())
        error(loc, "cannot use layout qualifiers on a function parameter", "", "");
    if (qualifier.invariant)
        error(loc, "cannot use invariant qualifier on a function parameter", "", "");
    if (qualifier.isNoContraction()) {
        if (qualifier.isParamOutput())
            type.getQualifier().setNoContraction();
        else
            warn(loc, "qualifier has no effect on non-output parameters", "precise", "");
    }
    if (qualifier.isNonUniform())
        type.getQualifier().nonUniform = qualifier.nonUniform;
    if (qualifier.isSpirvByReference())
        type.getQualifier().setSpirvByReference();
    if (qualifier.isSpirvLiteral()) {
        if (type.getBasicType() == EbtFloat || type.getBasicType() == EbtInt || type.getBasicType() == EbtUint ||
            type.getBasicType() == EbtBool)
            type.getQualifier().setSpirvLiteral();
        else
            error(loc, "cannot use spirv_literal qualifier", type.getBasicTypeString().c_str(), "");
    }

    paramCheckFixStorage(loc, qualifier.storage, type);
}

void TParseContext::nestedBlockCheck(const TSourceLoc& loc)
{
    if (structNestingLevel > 0 || blockNestingLevel > 0)
        error(loc, "cannot nest a block definition inside a structure or block", "", "");
    ++blockNestingLevel;
}

void TParseContext::nestedStructCheck(const TSourceLoc& loc)
{
    if (structNestingLevel > 0 || blockNestingLevel > 0)
        error(loc, "cannot nest a structure definition inside a structure or block", "", "");
    ++structNestingLevel;
}

void TParseContext::arrayObjectCheck(const TSourceLoc& loc, const TType& type, const char* op)
{
    // Some versions don't allow comparing arrays or structures containing arrays
    if (type.containsArray()) {
        profileRequires(loc, ENoProfile, 120, E_GL_3DL_array_objects, op);
        profileRequires(loc, EEsProfile, 300, nullptr, op);
    }
}

void TParseContext::opaqueCheck(const TSourceLoc& loc, const TType& type, const char* op)
{
    if (containsFieldWithBasicType(type, EbtSampler) && !extensionTurnedOn(E_GL_ARB_bindless_texture))
        error(loc, "can't use with samplers or structs containing samplers", op, "");
}

void TParseContext::referenceCheck(const TSourceLoc& loc, const TType& type, const char* op)
{
    if (containsFieldWithBasicType(type, EbtReference))
        error(loc, "can't use with reference types", op, "");
}

void TParseContext::storage16BitAssignmentCheck(const TSourceLoc& loc, const TType& type, const char* op)
{
    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtFloat16))
        requireFloat16Arithmetic(loc, op, "can't use with structs containing float16");

    if (type.isArray() && type.getBasicType() == EbtFloat16)
        requireFloat16Arithmetic(loc, op, "can't use with arrays containing float16");

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtInt16))
        requireInt16Arithmetic(loc, op, "can't use with structs containing int16");

    if (type.isArray() && type.getBasicType() == EbtInt16)
        requireInt16Arithmetic(loc, op, "can't use with arrays containing int16");

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtUint16))
        requireInt16Arithmetic(loc, op, "can't use with structs containing uint16");

    if (type.isArray() && type.getBasicType() == EbtUint16)
        requireInt16Arithmetic(loc, op, "can't use with arrays containing uint16");

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtInt8))
        requireInt8Arithmetic(loc, op, "can't use with structs containing int8");

    if (type.isArray() && type.getBasicType() == EbtInt8)
        requireInt8Arithmetic(loc, op, "can't use with arrays containing int8");

    if (type.getBasicType() == EbtStruct && containsFieldWithBasicType(type, EbtUint8))
        requireInt8Arithmetic(loc, op, "can't use with structs containing uint8");

    if (type.isArray() && type.getBasicType() == EbtUint8)
        requireInt8Arithmetic(loc, op, "can't use with arrays containing uint8");
}

void TParseContext::specializationCheck(const TSourceLoc& loc, const TType& type, const char* op)
{
    if (type.containsSpecializationSize())
        error(loc, "can't use with types containing arrays sized with a specialization constant", op, "");
}

void TParseContext::structTypeCheck(const TSourceLoc& /*loc*/, TPublicType& publicType)
{
    const TTypeList& typeList = *publicType.userDef->getStruct();

    // fix and check for member storage qualifiers and types that don't belong within a structure
    for (unsigned int member = 0; member < typeList.size(); ++member) {
        TQualifier& memberQualifier = typeList[member].type->getQualifier();
        const TSourceLoc& memberLoc = typeList[member].loc;
        if (memberQualifier.isAuxiliary() ||
            memberQualifier.isInterpolation() ||
            (memberQualifier.storage != EvqTemporary && memberQualifier.storage != EvqGlobal))
            error(memberLoc, "cannot use storage or interpolation qualifiers on structure members", typeList[member].type->getFieldName().c_str(), "");
        if (memberQualifier.isMemory())
            error(memberLoc, "cannot use memory qualifiers on structure members", typeList[member].type->getFieldName().c_str(), "");
        if (memberQualifier.hasLayout()) {
            error(memberLoc, "cannot use layout qualifiers on structure members", typeList[member].type->getFieldName().c_str(), "");
            memberQualifier.clearLayout();
        }
        if (memberQualifier.invariant)
            error(memberLoc, "cannot use invariant qualifier on structure members", typeList[member].type->getFieldName().c_str(), "");
    }
}

//
// See if this loop satisfies the limitations for ES 2.0 (version 100) for loops in Appendex A:
//
// "The loop index has type int or float.
//
// "The for statement has the form:
//     for ( init-declaration ; condition ; expression )
//     init-declaration has the form: type-specifier identifier = constant-expression
//     condition has the form:  loop-index relational_operator constant-expression
//         where relational_operator is one of: > >= < <= == or !=
//     expression [sic] has one of the following forms:
//         loop-index++
//         loop-index--
//         loop-index += constant-expression
//         loop-index -= constant-expression
//
// The body is handled in an AST traversal.
//
void TParseContext::inductiveLoopCheck(const TSourceLoc& loc, TIntermNode* init, TIntermLoop* loop)
{
    // loop index init must exist and be a declaration, which shows up in the AST as an aggregate of size 1 of the declaration
    bool badInit = false;
    if (! init || ! init->getAsAggregate() || init->getAsAggregate()->getSequence().size() != 1)
        badInit = true;
    TIntermBinary* binaryInit = nullptr;
    if (! badInit) {
        // get the declaration assignment
        binaryInit = init->getAsAggregate()->getSequence()[0]->getAsBinaryNode();
        if (! binaryInit)
            badInit = true;
    }
    if (badInit) {
        error(loc, "inductive-loop init-declaration requires the form \"type-specifier loop-index = constant-expression\"", "limitations", "");
        return;
    }

    // loop index must be type int or float
    if (! binaryInit->getType().isScalar() || (binaryInit->getBasicType() != EbtInt && binaryInit->getBasicType() != EbtFloat)) {
        error(loc, "inductive loop requires a scalar 'int' or 'float' loop index", "limitations", "");
        return;
    }

    // init is the form "loop-index = constant"
    if (binaryInit->getOp() != EOpAssign || ! binaryInit->getLeft()->getAsSymbolNode() || ! binaryInit->getRight()->getAsConstantUnion()) {
        error(loc, "inductive-loop init-declaration requires the form \"type-specifier loop-index = constant-expression\"", "limitations", "");
        return;
    }

    // get the unique id of the loop index
    long long loopIndex = binaryInit->getLeft()->getAsSymbolNode()->getId();
    inductiveLoopIds.insert(loopIndex);

    // condition's form must be "loop-index relational-operator constant-expression"
    bool badCond = ! loop->getTest();
    if (! badCond) {
        TIntermBinary* binaryCond = loop->getTest()->getAsBinaryNode();
        badCond = ! binaryCond;
        if (! badCond) {
            switch (binaryCond->getOp()) {
            case EOpGreaterThan:
            case EOpGreaterThanEqual:
            case EOpLessThan:
            case EOpLessThanEqual:
            case EOpEqual:
            case EOpNotEqual:
                break;
            default:
                badCond = true;
            }
        }
        if (binaryCond && (! binaryCond->getLeft()->getAsSymbolNode() ||
                           binaryCond->getLeft()->getAsSymbolNode()->getId() != loopIndex ||
                           ! binaryCond->getRight()->getAsConstantUnion()))
            badCond = true;
    }
    if (badCond) {
        error(loc, "inductive-loop condition requires the form \"loop-index <comparison-op> constant-expression\"", "limitations", "");
        return;
    }

    // loop-index++
    // loop-index--
    // loop-index += constant-expression
    // loop-index -= constant-expression
    bool badTerminal = ! loop->getTerminal();
    if (! badTerminal) {
        TIntermUnary* unaryTerminal = loop->getTerminal()->getAsUnaryNode();
        TIntermBinary* binaryTerminal = loop->getTerminal()->getAsBinaryNode();
        if (unaryTerminal || binaryTerminal) {
            switch(loop->getTerminal()->getAsOperator()->getOp()) {
            case EOpPostDecrement:
            case EOpPostIncrement:
            case EOpAddAssign:
            case EOpSubAssign:
                break;
            default:
                badTerminal = true;
            }
        } else
            badTerminal = true;
        if (binaryTerminal && (! binaryTerminal->getLeft()->getAsSymbolNode() ||
                               binaryTerminal->getLeft()->getAsSymbolNode()->getId() != loopIndex ||
                               ! binaryTerminal->getRight()->getAsConstantUnion()))
            badTerminal = true;
        if (unaryTerminal && (! unaryTerminal->getOperand()->getAsSymbolNode() ||
                              unaryTerminal->getOperand()->getAsSymbolNode()->getId() != loopIndex))
            badTerminal = true;
    }
    if (badTerminal) {
        error(loc, "inductive-loop termination requires the form \"loop-index++, loop-index--, loop-index += constant-expression, or loop-index -= constant-expression\"", "limitations", "");
        return;
    }

    // the body
    inductiveLoopBodyCheck(loop->getBody(), loopIndex, symbolTable);
}

// Do limit checks for built-in arrays.
void TParseContext::arrayLimitCheck(const TSourceLoc& loc, const TString& identifier, int size)
{
    if (identifier.compare("gl_TexCoord") == 0)
        limitCheck(loc, size, "gl_MaxTextureCoords", "gl_TexCoord array size");
    else if (identifier.compare("gl_ClipDistance") == 0)
        limitCheck(loc, size, "gl_MaxClipDistances", "gl_ClipDistance array size");
    else if (identifier.compare("gl_CullDistance") == 0)
        limitCheck(loc, size, "gl_MaxCullDistances", "gl_CullDistance array size");
    else if (identifier.compare("gl_ClipDistancePerViewNV") == 0)
        limitCheck(loc, size, "gl_MaxClipDistances", "gl_ClipDistancePerViewNV array size");
    else if (identifier.compare("gl_CullDistancePerViewNV") == 0)
        limitCheck(loc, size, "gl_MaxCullDistances", "gl_CullDistancePerViewNV array size");
}

// See if the provided value is less than or equal to the symbol indicated by limit,
// which should be a constant in the symbol table.
void TParseContext::limitCheck(const TSourceLoc& loc, int value, const char* limit, const char* feature)
{
    TSymbol* symbol = symbolTable.find(limit);
    assert(symbol->getAsVariable());
    const TConstUnionArray& constArray = symbol->getAsVariable()->getConstArray();
    assert(! constArray.empty());
    if (value > constArray[0].getIConst())
        error(loc, "must be less than or equal to", feature, "%s (%d)", limit, constArray[0].getIConst());
}

//
// Do any additional error checking, etc., once we know the parsing is done.
//
void TParseContext::finish()
{
    TParseContextBase::finish();

    if (parsingBuiltins)
        return;

    // Check on array indexes for ES 2.0 (version 100) limitations.
    for (size_t i = 0; i < needsIndexLimitationChecking.size(); ++i)
        constantIndexExpressionCheck(needsIndexLimitationChecking[i]);

    // Check for stages that are enabled by extension.
    // Can't do this at the beginning, it is chicken and egg to add a stage by
    // extension.
    // Stage-specific features were correctly tested for already, this is just
    // about the stage itself.
    switch (language) {
    case EShLangGeometry:
        if (isEsProfile() && version == 310)
            requireExtensions(getCurrentLoc(), Num_AEP_geometry_shader, AEP_geometry_shader, "geometry shaders");
        break;
    case EShLangTessControl:
    case EShLangTessEvaluation:
        if (isEsProfile() && version == 310)
            requireExtensions(getCurrentLoc(), Num_AEP_tessellation_shader, AEP_tessellation_shader, "tessellation shaders");
        else if (!isEsProfile() && version < 400)
            requireExtensions(getCurrentLoc(), 1, &E_GL_ARB_tessellation_shader, "tessellation shaders");
        break;
    case EShLangCompute:
        if (!isEsProfile() && version < 430)
            requireExtensions(getCurrentLoc(), 1, &E_GL_ARB_compute_shader, "compute shaders");
        break;
    case EShLangTask:
        requireExtensions(getCurrentLoc(), Num_AEP_mesh_shader, AEP_mesh_shader, "task shaders");
        break;
    case EShLangMesh:
        requireExtensions(getCurrentLoc(), Num_AEP_mesh_shader, AEP_mesh_shader, "mesh shaders");
        break;
    default:
        break;
    }

    // Set default outputs for GL_NV_geometry_shader_passthrough
    if (language == EShLangGeometry && extensionTurnedOn(E_SPV_NV_geometry_shader_passthrough)) {
        if (intermediate.getOutputPrimitive() == ElgNone) {
            switch (intermediate.getInputPrimitive()) {
            case ElgPoints:      intermediate.setOutputPrimitive(ElgPoints);    break;
            case ElgLines:       intermediate.setOutputPrimitive(ElgLineStrip); break;
            case ElgTriangles:   intermediate.setOutputPrimitive(ElgTriangleStrip); break;
            default: break;
            }
        }
        if (intermediate.getVertices() == TQualifier::layoutNotSet) {
            switch (intermediate.getInputPrimitive()) {
            case ElgPoints:      intermediate.setVertices(1); break;
            case ElgLines:       intermediate.setVertices(2); break;
            case ElgTriangles:   intermediate.setVertices(3); break;
            default: break;
            }
        }
    }
}

//
// Layout qualifier stuff.
//

// Put the id's layout qualification into the public type, for qualifiers not having a number set.
// This is before we know any type information for error checking.
void TParseContext::setLayoutQualifier(const TSourceLoc& loc, TPublicType& publicType, TString& id)
{
    std::transform(id.begin(), id.end(), id.begin(), ::tolower);

    if (id == TQualifier::getLayoutMatrixString(ElmColumnMajor)) {
        publicType.qualifier.layoutMatrix = ElmColumnMajor;
        return;
    }
    if (id == TQualifier::getLayoutMatrixString(ElmRowMajor)) {
        publicType.qualifier.layoutMatrix = ElmRowMajor;
        return;
    }
    if (id == TQualifier::getLayoutPackingString(ElpPacked)) {
        if (spvVersion.spv != 0) {
            if (spvVersion.vulkanRelaxed)
                return; // silently ignore qualifier
            else
                spvRemoved(loc, "packed");
        }
        publicType.qualifier.layoutPacking = ElpPacked;
        return;
    }
    if (id == TQualifier::getLayoutPackingString(ElpShared)) {
        if (spvVersion.spv != 0) {
            if (spvVersion.vulkanRelaxed)
                return; // silently ignore qualifier
            else
                spvRemoved(loc, "shared");
        }
        publicType.qualifier.layoutPacking = ElpShared;
        return;
    }
    if (id == TQualifier::getLayoutPackingString(ElpStd140)) {
        publicType.qualifier.layoutPacking = ElpStd140;
        return;
    }
    if (id == TQualifier::getLayoutPackingString(ElpStd430)) {
        requireProfile(loc, EEsProfile | ECoreProfile | ECompatibilityProfile, "std430");
        profileRequires(loc, ECoreProfile | ECompatibilityProfile, 430, E_GL_ARB_shader_storage_buffer_object, "std430");
        profileRequires(loc, EEsProfile, 310, nullptr, "std430");
        publicType.qualifier.layoutPacking = ElpStd430;
        return;
    }
    if (id == TQualifier::getLayoutPackingString(ElpScalar)) {
        requireVulkan(loc, "scalar");
        requireExtensions(loc, 1, &E_GL_EXT_scalar_block_layout, "scalar block layout");
        publicType.qualifier.layoutPacking = ElpScalar;
        return;
    }
    // TODO: compile-time performance: may need to stop doing linear searches
    for (TLayoutFormat format = (TLayoutFormat)(ElfNone + 1); format < ElfCount; format = (TLayoutFormat)(format + 1)) {
        if (id == TQualifier::getLayoutFormatString(format)) {
            if ((format > ElfEsFloatGuard && format < ElfFloatGuard) ||
                (format > ElfEsIntGuard && format < ElfIntGuard) ||
                (format > ElfEsUintGuard && format < ElfCount))
                requireProfile(loc, ENoProfile | ECoreProfile | ECompatibilityProfile, "image load-store format");
            profileRequires(loc, ENoProfile | ECoreProfile | ECompatibilityProfile, 420, E_GL_ARB_shader_image_load_store, "image load store");
            profileRequires(loc, EEsProfile, 310, E_GL_ARB_shader_image_load_store, "image load store");
            publicType.qualifier.layoutFormat = format;
            return;
        }
    }
    if (id == "push_constant") {
        requireVulkan(loc, "push_constant");
        publicType.qualifier.layoutPushConstant = true;
        return;
    }
    if (id == "buffer_reference") {
        requireVulkan(loc, "buffer_reference");
        requireExtensions(loc, 1, &E_GL_EXT_buffer_reference, "buffer_reference");
        publicType.qualifier.layoutBufferReference = true;
        intermediate.setUseStorageBuffer();
        intermediate.setUsePhysicalStorageBuffer();
        return;
    }
    if (id == "bindless_sampler") {
        requireExtensions(loc, 1, &E_GL_ARB_bindless_texture, "bindless_sampler");
        publicType.qualifier.layoutBindlessSampler = true;
        intermediate.setBindlessTextureMode(currentCaller, AstRefTypeLayout);
        return;
    }
    if (id == "bindless_image") {
        requireExtensions(loc, 1, &E_GL_ARB_bindless_texture, "bindless_image");
        publicType.qualifier.layoutBindlessImage = true;
        intermediate.setBindlessImageMode(currentCaller, AstRefTypeLayout);
        return;
    }
    if (id == "bound_sampler") {
        requireExtensions(loc, 1, &E_GL_ARB_bindless_texture, "bound_sampler");
        publicType.qualifier.layoutBindlessSampler = false;
        return;
    }
    if (id == "bound_image") {
        requireExtensions(loc, 1, &E_GL_ARB_bindless_texture, "bound_image");
        publicType.qualifier.layoutBindlessImage = false;
        return;
    }
    if (language == EShLangGeometry || language == EShLangTessEvaluation || language == EShLangMesh) {
        if (id == TQualifier::getGeometryString(ElgTriangles)) {
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               