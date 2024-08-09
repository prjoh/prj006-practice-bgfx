//
// Copyright (C) 2017-2018 Google, Inc.
// Copyright (C) 2017 LunarG, Inc.
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

#include "hlslParseHelper.h"
#include "hlslScanContext.h"
#include "hlslGrammar.h"
#include "hlslAttributes.h"

#include "../Include/Common.h"
#include "../MachineIndependent/Scan.h"
#include "../MachineIndependent/preprocessor/PpContext.h"

#include "../OSDependent/osinclude.h"

#include <algorithm>
#include <functional>
#include <cctype>
#include <array>
#include <set>

namespace glslang {

HlslParseContext::HlslParseContext(TSymbolTable& symbolTable, TIntermediate& interm, bool parsingBuiltins,
                                   int version, EProfile profile, const SpvVersion& spvVersion, EShLanguage language,
                                   TInfoSink& infoSink,
                                   const TString sourceEntryPointName,
                                   bool forwardCompatible, EShMessages messages) :
    TParseContextBase(symbolTable, interm, parsingBuiltins, version, profile, spvVersion, language, infoSink,
                      forwardCompatible, messages, &sourceEntryPointName),
    annotationNestingLevel(0),
    inputPatch(nullptr),
    nextInLocation(0), nextOutLocation(0),
    entryPointFunction(nullptr),
    entryPointFunctionBody(nullptr),
    gsStreamOutput(nullptr),
    clipDistanceOutput(nullptr),
    cullDistanceOutput(nullptr),
    clipDistanceInput(nullptr),
    cullDistanceInput(nullptr),
    parsingEntrypointParameters(false)
{
    globalUniformDefaults.clear();
    globalUniformDefaults.layoutMatrix = ElmRowMajor;
    globalUniformDefaults.layoutPacking = ElpStd140;

    globalBufferDefaults.clear();
    globalBufferDefaults.layoutMatrix = ElmRowMajor;
    globalBufferDefaults.layoutPacking = ElpStd430;

    globalInputDefaults.clear();
    globalOutputDefaults.clear();

    clipSemanticNSizeIn.fill(0);
    cullSemanticNSizeIn.fill(0);
    clipSemanticNSizeOut.fill(0);
    cullSemanticNSizeOut.fill(0);

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
}

HlslParseContext::~HlslParseContext()
{
}

void HlslParseContext::initializeExtensionBehavior()
{
    TParseContextBase::initializeExtensionBehavior();

    // HLSL allows #line by default.
    extensionBehavior[E_GL_GOOGLE_cpp_style_line_directive] = EBhEnable;
}

void HlslParseContext::setLimits(const TBuiltInResource& r)
{
    resources = r;
    intermediate.setLimits(resources);
}

//
// Parse an array of strings using the parser in HlslRules.
//
// Returns true for successful acceptance of the shader, false if any errors.
//
bool HlslParseContext::parseShaderStrings(TPpContext& ppContext, TInputScanner& input, bool versionWillBeError)
{
    currentScanner = &input;
    ppContext.setInput(input, versionWillBeError);

    HlslScanContext scanContext(*this, ppContext);
    HlslGrammar grammar(scanContext, *this);
    if (!grammar.parse()) {
        // Print a message formated such that if you click on the message it will take you right to
        // the line through most UIs.
        const glslang::TSourceLoc& sourceLoc = input.getSourceLoc();
        infoSink.info << sourceLoc.getFilenameStr() << "(" << sourceLoc.line << "): error at column " << sourceLoc.column
                      << ", HLSL parsing failed.\n";
        ++numErrors;
        return false;
    }

    finish();

    return numErrors == 0;
}

//
// Return true if this l-value node should be converted in some manner.
// For instance: turning a load aggregate into a store in an l-value.
//
bool HlslParseContext::shouldConvertLValue(const TIntermNode* node) const
{
    if (node == nullptr || node->getAsTyped() == nullptr)
        return false;

    const TIntermAggregate* lhsAsAggregate = node->getAsAggregate();
    const TIntermBinary* lhsAsBinary = node->getAsBinaryNode();

    // If it's a swizzled/indexed aggregate, look at the left node instead.
    if (lhsAsBinary != nullptr &&
        (lhsAsBinary->getOp() == EOpVectorSwizzle || lhsAsBinary->getOp() == EOpIndexDirect))
        lhsAsAggregate = lhsAsBinary->getLeft()->getAsAggregate();
    if (lhsAsAggregate != nullptr && lhsAsAggregate->getOp() == EOpImageLoad)
        return true;

    return false;
}

void HlslParseContext::growGlobalUniformBlock(const TSourceLoc& loc, TType& memberType, const TString& memberName,
                                              TTypeList* newTypeList)
{
    newTypeList = nullptr;
    correctUniform(memberType.getQualifier());
    if (memberType.isStruct()) {
        auto it = ioTypeMap.find(memberType.getStruct());
        if (it != ioTypeMap.end() && it->second.uniform)
            newTypeList = it->second.uniform;
    }
    TParseContextBase::growGlobalUniformBlock(loc, memberType, memberName, newTypeList);
}

//
// Return a TLayoutFormat corresponding to the given texture type.
//
TLayoutFormat HlslParseContext::getLayoutFromTxType(const TSourceLoc& loc, const TType& txType)
{
    if (txType.isStruct()) {
        // TODO: implement.
        error(loc, "unimplemented: structure type in image or buffer", "", "");
        return ElfNone;
    }

    const int components = txType.getVectorSize();
    const TBasicType txBasicType = txType.getBasicType();

    const auto selectFormat = [this,&components](TLayoutFormat v1, TLayoutFormat v2, TLayoutFormat v4) -> TLayoutFormat {
        if (intermediate.getNoStorageFormat())
            return ElfNone;

        return components == 1 ? v1 :
               components == 2 ? v2 : v4;
    };

    switch (txBasicType) {
    case EbtFloat: return selectFormat(ElfR32f,  ElfRg32f,  ElfRgba32f);
    case EbtInt:   return selectFormat(ElfR32i,  ElfRg32i,  ElfRgba32i);
    case EbtUint:  return selectFormat(ElfR32ui, ElfRg32ui, ElfRgba32ui);
    default:
        error(loc, "unknown basic type in image format", "", "");
        return ElfNone;
    }
}

//
// Both test and if necessary, spit out an error, to see if the node is really
// an l-value that can be operated on this way.
//
// Returns true if there was an error.
//
bool HlslParseContext::lValueErrorCheck(const TSourceLoc& loc, const char* op, TIntermTyped* node)
{
    if (shouldConvertLValue(node)) {
        // if we're writing to a texture, it must be an RW form.

        TIntermAggregate* lhsAsAggregate = node->getAsAggregate();
        TIntermTyped* object = lhsAsAggregate->getSequence()[0]->getAsTyped();

        if (!object->getType().getSampler().isImage()) {
            error(loc, "operator[] on a non-RW texture must be an r-value", "", "");
            return true;
        }
    }

    // We tolerate samplers as l-values, even though they are nominally
    // illegal, because we expect a later optimization to eliminate them.
    if (node->getType().getBasicType() == EbtSampler) {
        intermediate.setNeedsLegalization();
        return false;
    }

    // Let the base class check errors
    return TParseContextBase::lValueErrorCheck(loc, op, node);
}

//
// This function handles l-value conversions and verifications.  It uses, but is not synonymous
// with lValueErrorCheck.  That function accepts an l-value directly, while this one must be
// given the surrounding tree - e.g, with an assignment, so we can convert the assign into a
// series of other image operations.
//
// Most things are passed through unmodified, except for error checking.
//
TIntermTyped* HlslParseContext::handleLvalue(const TSourceLoc& loc, const char* op, TIntermTyped*& node)
{
    if (node == nullptr)
        return nullptr;

    TIntermBinary* nodeAsBinary = node->getAsBinaryNode();
    TIntermUnary* nodeAsUnary = node->getAsUnaryNode();
    TIntermAggregate* sequence = nullptr;

    TIntermTyped* lhs = nodeAsUnary  ? nodeAsUnary->getOperand() :
                        nodeAsBinary ? nodeAsBinary->getLeft() :
                        nullptr;

    // Early bail out if there is no conversion to apply
    if (!shouldConvertLValue(lhs)) {
        if (lhs != nullptr)
            if (lValueErrorCheck(loc, op, lhs))
                return nullptr;
        return node;
    }

    // *** If we get here, we're going to apply some conversion to an l-value.

    // Helper to create a load.
    const auto makeLoad = [&](TIntermSymbol* rhsTmp, TIntermTyped* object, TIntermTyped* coord, const TType& derefType) {
        TIntermAggregate* loadOp = new TIntermAggregate(EOpImageLoad);
        loadOp->setLoc(loc);
        loadOp->getSequence().push_back(object);
        loadOp->getSequence().push_back(intermediate.addSymbol(*coord->getAsSymbolNode()));
        loadOp->setType(derefType);

        sequence = intermediate.growAggregate(sequence,
                                              intermediate.addAssign(EOpAssign, rhsTmp, loadOp, loc),
                                              loc);
    };

    // Helper to create a store.
    const auto makeStore = [&](TIntermTyped* object, TIntermTyped* coord, TIntermSymbol* rhsTmp) {
        TIntermAggregate* storeOp = new TIntermAggregate(EOpImageStore);
        storeOp->getSequence().push_back(object);
        storeOp->getSequence().push_back(coord);
        storeOp->getSequence().push_back(intermediate.addSymbol(*rhsTmp));
        storeOp->setLoc(loc);
        storeOp->setType(TType(EbtVoid));

        sequence = intermediate.growAggregate(sequence, storeOp);
    };

    // Helper to create an assign.
    const auto makeBinary = [&](TOperator op, TIntermTyped* lhs, TIntermTyped* rhs) {
        sequence = intermediate.growAggregate(sequence,
                                              intermediate.addBinaryNode(op, lhs, rhs, loc, lhs->getType()),
                                              loc);
    };

    // Helper to complete sequence by adding trailing variable, so we evaluate to the right value.
    const auto finishSequence = [&](TIntermSymbol* rhsTmp, const TType& derefType) -> TIntermAggregate* {
        // Add a trailing use of the temp, so the sequence returns the proper value.
        sequence = intermediate.growAggregate(sequence, intermediate.addSymbol(*rhsTmp));
        sequence->setOperator(EOpSequence);
        sequence->setLoc(loc);
        sequence->setType(derefType);

        return sequence;
    };

    // Helper to add unary op
    const auto makeUnary = [&](TOperator op, TIntermSymbol* rhsTmp) {
        sequence = intermediate.growAggregate(sequence,
                                              intermediate.addUnaryNode(op, intermediate.addSymbol(*rhsTmp), loc,
                                                                        rhsTmp->getType()),
                                              loc);
    };

    // Return true if swizzle or index writes all components of the given variable.
    const auto writesAllComponents = [&](TIntermSymbol* var, TIntermBinary* swizzle) -> bool {
        if (swizzle == nullptr)  // not a swizzle or index
            return true;

        // Track which components are being set.
        std::array<bool, 4> compIsSet;
        compIsSet.fill(false);

        const TIntermConstantUnion* asConst     = swizzle->getRight()->getAsConstantUnion();
        const TIntermAggregate*     asAggregate = swizzle->getRight()->getAsAggregate();

        // This could be either a direct index, or a swizzle.
        if (asConst) {
            compIsSet[asConst->getConstArray()[0].getIConst()] = true;
        } else if (asAggregate) {
            const TIntermSequence& seq = asAggregate->getSequence();
            for (int comp=0; comp<int(seq.size()); ++comp)
                compIsSet[seq[comp]->getAsConstantUnion()->getConstArray()[0].getIConst()] = true;
        } else {
            assert(0);
        }

        // Return true if all components are being set by the index or swizzle
        return std::all_of(compIsSet.begin(), compIsSet.begin() + var->getType().getVectorSize(),
                           [](bool isSet) { return isSet; } );
    };

    // Create swizzle matching input swizzle
    const auto addSwizzle = [&](TIntermSymbol* var, TIntermBinary* swizzle) -> TIntermTyped* {
        if (swizzle)
            return intermediate.addBinaryNode(swizzle->getOp(), var, swizzle->getRight(), loc, swizzle->getType());
        else
            return var;
    };

    TIntermBinary*    lhsAsBinary    = lhs->getAsBinaryNode();
    TIntermAggregate* lhsAsAggregate = lhs->getAsAggregate();
    bool lhsIsSwizzle = false;

    // If it's a swizzled L-value, remember the swizzle, and use the LHS.
    if (lhsAsBinary != nullptr && (lhsAsBinary->getOp() == EOpVectorSwizzle || lhsAsBinary->getOp() == EOpIndexDirect)) {
        lhsAsAggregate = lhsAsBinary->getLeft()->getAsAggregate();
        lhsIsSwizzle = true;
    }

    TIntermTyped* object = lhsAsAggregate->getSequence()[0]->getAsTyped();
    TIntermTyped* coord  = lhsAsAggregate->getSequence()[1]->getAsTyped();

    const TSampler& texSampler = object->getType().getSampler();

    TType objDerefType;
    getTextureReturnType(texSampler, objDerefType);

    if (nodeAsBinary) {
        TIntermTyped* rhs = nodeAsBinary->getRight();
        const TOperator assignOp = nodeAsBinary->getOp();

        bool isModifyOp = false;

        switch (assignOp) {
        case EOpAddAssign:
        case EOpSubAssign:
        case EOpMulAssign:
        case EOpVectorTimesMatrixAssign:
        case EOpVectorTimesScalarAssign:
        case EOpMatrixTimesScalarAssign:
        case EOpMatrixTimesMatrixAssign:
        case EOpDivAssign:
        case EOpModAssign:
        case EOpAndAssign:
        case EOpInclusiveOrAssign:
        case EOpExclusiveOrAssign:
        case EOpLeftShiftAssign:
        case EOpRightShiftAssign:
            isModifyOp = true;
            // fall through...
        case EOpAssign:
            {
                // Since this is an lvalue, we'll convert an image load to a sequence like this
                // (to still provide the value):
                //   OpSequence
                //      OpImageStore(object, lhs, rhs)
                //      rhs
                // But if it's not a simple symbol RHS (say, a fn call), we don't want to duplicate the RHS,
                // so we'll convert instead to this:
                //   OpSequence
                //      rhsTmp = rhs
                //      OpImageStore(object, coord, rhsTmp)
                //      rhsTmp
                // If this is a read-modify-write op, like +=, we issue:
                //   OpSequence
                //      coordtmp = load's param1
                //      rhsTmp = OpImageLoad(object, coordTmp)
                //      rhsTmp op= rhs
                //      OpImageStore(object, coordTmp, rhsTmp)
                //      rhsTmp
                //
                // If the lvalue is swizzled, we apply that when writing the temp variable, like so:
                //    ...
                //    rhsTmp.some_swizzle = ...
                // For partial writes, an error is generated.

                TIntermSymbol* rhsTmp = rhs->getAsSymbolNode();
                TIntermTyped* coordTmp = coord;

                if (rhsTmp == nullptr || isModifyOp || lhsIsSwizzle) {
                    rhsTmp = makeInternalVariableNode(loc, "storeTemp", objDerefType);

                    // Partial updates not yet supported
                    if (!writesAllComponents(rhsTmp, lhsAsBinary)) {
                        error(loc, "unimplemented: partial image updates", "", "");
                    }

                    // Assign storeTemp = rhs
                    if (isModifyOp) {
                        // We have to make a temp var for the coordinate, to avoid evaluating it twice.
                        coordTmp = makeInternalVariableNode(loc, "coordTemp", coord->getType());
                        makeBinary(EOpAssign, coordTmp, coord); // coordtmp = load[param1]
                        makeLoad(rhsTmp, object, coordTmp, objDerefType); // rhsTmp = OpImageLoad(object, coordTmp)
                    }

                    // rhsTmp op= rhs.
                    makeBinary(assignOp, addSwizzle(intermediate.addSymbol(*rhsTmp), lhsAsBinary), rhs);
                }

                makeStore(object, coordTmp, rhsTmp);         // add a store
                return finishSequence(rhsTmp, objDerefType); // return rhsTmp from sequence
            }

        default:
            break;
        }
    }

    if (nodeAsUnary) {
        const TOperator assignOp = nodeAsUnary->getOp();

        switch (assignOp) {
        case EOpPreIncrement:
        case EOpPreDecrement:
            {
                // We turn this into:
                //   OpSequence
                //      coordtmp = load's param1
                //      rhsTmp = OpImageLoad(object, coordTmp)
                //      rhsTmp op
                //      OpImageStore(object, coordTmp, rhsTmp)
                //      rhsTmp

                TIntermSymbol* rhsTmp = makeInternalVariableNode(loc, "storeTemp", objDerefType);
                TIntermTyped* coordTmp = makeInternalVariableNode(loc, "coordTemp", coord->getType());

                makeBinary(EOpAssign, coordTmp, coord);           // coordtmp = load[param1]
                makeLoad(rhsTmp, object, coordTmp, objDerefType); // rhsTmp = OpImageLoad(object, coordTmp)
                makeUnary(assignOp, rhsTmp);                      // op rhsTmp
                makeStore(object, coordTmp, rhsTmp);              // OpImageStore(object, coordTmp, rhsTmp)
                return finishSequence(rhsTmp, objDerefType);      // return rhsTmp from sequence
            }

        case EOpPostIncrement:
        case EOpPostDecrement:
            {
                // We turn this into:
                //   OpSequence
                //      coordtmp = load's param1
                //      rhsTmp1 = OpImageLoad(object, coordTmp)
                //      rhsTmp2 = rhsTmp1
                //      rhsTmp2 op
                //      OpImageStore(object, coordTmp, rhsTmp2)
                //      rhsTmp1 (pre-op value)
                TIntermSymbol* rhsTmp1 = makeInternalVariableNode(loc, "storeTempPre",  objDerefType);
                TIntermSymbol* rhsTmp2 = makeInternalVariableNode(loc, "storeTempPost", objDerefType);
                TIntermTyped* coordTmp = makeInternalVariableNode(loc, "coordTemp", coord->getType());

                makeBinary(EOpAssign, coordTmp, coord);            // coordtmp = load[param1]
                makeLoad(rhsTmp1, object, coordTmp, objDerefType); // rhsTmp1 = OpImageLoad(object, coordTmp)
                makeBinary(EOpAssign, rhsTmp2, rhsTmp1);           // rhsTmp2 = rhsTmp1
                makeUnary(assignOp, rhsTmp2);                      // rhsTmp op
                makeStore(object, coordTmp, rhsTmp2);              // OpImageStore(object, coordTmp, rhsTmp2)
                return finishSequence(rhsTmp1, objDerefType);      // return rhsTmp from sequence
            }

        default:
            break;
        }
    }

    if (lhs)
        if (lValueErrorCheck(loc, op, lhs))
            return nullptr;

    return node;
}

void HlslParseContext::handlePragma(const TSourceLoc& loc, const TVector<TString>& tokens)
{
    if (pragmaCallback)
        pragmaCallback(loc.line, tokens);

    if (tokens.size() == 0)
        return;

    // These pragmas are case insensitive in HLSL, so we'll compare in lower case.
    TVector<TString> lowerTokens = tokens;

    for (auto it = lowerTokens.begin(); it != lowerTokens.end(); ++it)
        std::transform(it->begin(), it->end(), it->begin(), ::tolower);

    // Handle pack_matrix
    if (tokens.size() == 4 && lowerTokens[0] == "pack_matrix" && tokens[1] == "(" && tokens[3] == ")") {
        // Note that HLSL semantic order is Mrc, not Mcr like SPIR-V, so we reverse the sense.
        // Row major becomes column major and vice versa.

        if (lowerTokens[2] == "row_major") {
            globalUniformDefaults.layoutMatrix = globalBufferDefaults.layoutMatrix = ElmColumnMajor;
        } else if (lowerTokens[2] == "column_major") {
            globalUniformDefaults.layoutMatrix = globalBufferDefaults.layoutMatrix = ElmRowMajor;
        } else {
            // unknown majorness strings are treated as (HLSL column major)==(SPIR-V row major)
            warn(loc, "unknown pack_matrix pragma value", tokens[2].c_str(), "");
            globalUniformDefaults.layoutMatrix = globalBufferDefaults.layoutMatrix = ElmRowMajor;
        }
        return;
    }

    // Handle once
    if (lowerTokens[0] == "once") {
        warn(loc, "not implemented", "#pragma once", "");
        return;
    }
}

//
// Look at a '.' matrix selector string and change it into components
// for a matrix. There are two types:
//
//   _21    second row, first column (one based)
//   _m21   third row, second column (zero based)
//
// Returns true if there is no error.
//
bool HlslParseContext::parseMatrixSwizzleSelector(const TSourceLoc& loc, const TString& fields, int cols, int rows,
                                                  TSwizzleSelectors<TMatrixSelector>& components)
{
    int startPos[MaxSwizzleSelectors];
    int numComps = 0;
    TString compString = fields;

    // Find where each component starts,
    // recording the first character position after the '_'.
    for (size_t c = 0; c < compString.size(); ++c) {
        if (compString[c] == '_') {
            if (numComps >= MaxSwizzleSelectors) {
                error(loc, "matrix component swizzle has too many components", compString.c_str(), "");
                return false;
            }
            if (c > compString.size() - 3 ||
                    ((compString[c+1] == 'm' || compString[c+1] == 'M') && c > compString.size() - 4)) {
                error(loc, "matrix component swizzle missing", compString.c_str(), "");
                return false;
            }
            startPos[numComps++] = (int)c + 1;
        }
    }

    // Process each component
    for (int i = 0; i < numComps; ++i) {
        int pos = startPos[i];
        int bias = -1;
        if (compString[pos] == 'm' || compString[pos] == 'M') {
            bias = 0;
            ++pos;
        }
        TMatrixSelector comp;
        comp.coord1 = compString[pos+0] - '0' + bias;
        comp.coord2 = compString[pos+1] - '0' + bias;
        if (comp.coord1 < 0 || comp.coord1 >= cols) {
            error(loc, "matrix row component out of range", compString.c_str(), "");
            return false;
        }
        if (comp.coord2 < 0 || comp.coord2 >= rows) {
            error(loc, "matrix column component out of range", compString.c_str(), "");
            return false;
        }
        components.push_back(comp);
    }

    return true;
}

// If the 'comps' express a column of a matrix,
// return the column.  Column means the first coords all match.
//
// Otherwise, return -1.
//
int HlslParseContext::getMatrixComponentsColumn(int rows, const TSwizzleSelectors<TMatrixSelector>& selector)
{
    int col = -1;

    // right number of comps?
    if (selector.size() != rows)
        return -1;

    // all comps in the same column?
    // rows in order?
    col = selector[0].coord1;
    for (int i = 0; i < rows; ++i) {
        if (col != selector[i].coord1)
            return -1;
        if (i != selector[i].coord2)
            return -1;
    }

    return col;
}

//
// Handle seeing a variable identifier in the grammar.
//
TIntermTyped* HlslParseContext::handleVariable(const TSourceLoc& loc, const TString* string)
{
    int thisDepth;
    TSymbol* symbol = symbolTable.find(*string, thisDepth);
    if (symbol && symbol->getAsVariable() && symbol->getAsVariable()->isUserType()) {
        error(loc, "expected symbol, not user-defined type", string->c_str(), "");
        return nullptr;
    }

    const TVariable* variable = nullptr;
    const TAnonMember* anon = symbol ? symbol->getAsAnonMember() : nullptr;
    TIntermTyped* node = nullptr;
    if (anon) {
        // It was a member of an anonymous container, which could be a 'this' structure.

        // Create a subtree for its dereference.
        if (thisDepth > 0) {
            variable = getImplicitThis(thisDepth);
            if (variable == nullptr)
                error(loc, "cannot access member variables (static member function?)", "this", "");
        }
        if (variable == nullptr)
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
            if ((variable->getType().getBasicType() == EbtBlock ||
                variable->getType().getBasicType() == EbtStruct) && variable->getType().getStruct() == nullptr) {
                error(loc, "cannot be used (maybe an instance name is needed)", string->c_str(), "");
                variable = nullptr;
            }
        } else {
            if (symbol)
                error(loc, "variable name expected", string->c_str(), "");
        }

        // Recovery, if it wasn't found or was not a variable.
        if (variable == nullptr) {
            error(loc, "unknown variable", string->c_str(), "");
            variable = new TVariable(string, TType(EbtVoid));
        }

        if (variable->getType().getQualifier().isFrontEndConstant())
            node = intermediate.addConstantUnion(variable->getConstArray(), variable->getType(), loc);
        else
            node = intermediate.addSymbol(*variable, loc);
    }

    if (variable->getType().getQualifier().isIo())
        intermediate.addIoAccessed(*string);

    return node;
}

//
// Handle operator[] on any objects it applies to.  Currently:
//    Textures
//    Buffers
//
TIntermTyped* HlslParseContext::handleBracketOperator(const TSourceLoc& loc, TIntermTyped* base, TIntermTyped* index)
{
    // handle r-value operator[] on textures and images.  l-values will be processed later.
    if (base->getType().getBasicType() == EbtSampler && !base->isArray()) {
        const TSampler& sampler = base->getType().getSampler();
        if (sampler.isImage() || sampler.isTexture()) {
            if (! mipsOperatorMipArg.empty() && mipsOperatorMipArg.back().mipLevel == nullptr) {
                // The first operator[] to a .mips[] sequence is the mip level.  We'll remember it.
                mipsOperatorMipArg.back().mipLevel = index;
                return base;  // next [] index is to the same base.
            } else {
                TIntermAggregate* load = new TIntermAggregate(sampler.isImage() ? EOpImageLoad : EOpTextureFetch);

                TType sampReturnType;
                getTextureReturnType(sampler, sampReturnType);

                load->setType(sampReturnType);
                load->setLoc(loc);
                load->getSequence().push_back(base);
                load->getSequence().push_back(index);

                // Textures need a MIP.  If we saw one go by, use it.  Otherwise, use zero.
                if (sampler.isTexture()) {
                    if (! mipsOperatorMipArg.empty()) {
                        load->getSequence().push_back(mipsOperatorMipArg.back().mipLevel);
                        mipsOperatorMipArg.pop_back();
                    } else {
                        load->getSequence().push_back(intermediate.addConstantUnion(0, loc, true));
                    }
                }

                return load;
            }
        }
    }

    // Handle operator[] on structured buffers: this indexes into the array element of the buffer.
    // indexStructBufferContent returns nullptr if it isn't a structuredbuffer (SSBO).
    TIntermTyped* sbArray = indexStructBufferContent(loc, base);
    if (sbArray != nullptr) {
        // Now we'll apply the [] index to that array
        const TOperator idxOp = (index->getQualifier().storage == EvqConst) ? EOpIndexDirect : EOpIndexIndirect;

        TIntermTyped* element = intermediate.addIndex(idxOp, sbArray, index, loc);
        const TType derefType(sbArray->getType(), 0);
        element->setType(derefType);
        return element;
    }

    return nullptr;
}

//
// Cast index value to a uint if it isn't already (for operator[], load indexes, etc)
TIntermTyped* HlslParseContext::makeIntegerIndex(TIntermTyped* index)
{
    const TBasicType indexBasicType = index->getType().getBasicType();
    const int vecSize = index->getType().getVectorSize();

    // We can use int types directly as the index
    if (indexBasicType == EbtInt || indexBasicType == EbtUint ||
        indexBasicType == EbtInt64 || indexBasicType == EbtUint64)
        return index;

    // Cast index to unsigned integer if it isn't one.
    return intermediate.addConversion(EOpConstructUint, TType(EbtUint, EvqTemporary, vecSize), index);
}

//
// Handle seeing a base[index] dereference in the grammar.
//
TIntermTyped* HlslParseContext::handleBracketDereference(const TSourceLoc& loc, TIntermTyped* base, TIntermTyped* index)
{
    index = makeIntegerIndex(index);

    if (index == nullptr) {
        error(loc, " unknown index type ", "", "");
        return nullptr;
    }

    TIntermTyped* result = handleBracketOperator(loc, base, index);

    if (result != nullptr)
        return result;  // it was handled as an operator[]

    bool flattened = false;
    int indexValue = 0;
    if (index->getQualifier().isFrontEndConstant())
        indexValue = index->getAsConstantUnion()->getConstArray()[0].getIConst();

    variableCheck(base);
    if (! base->isArray() && ! base->isMatrix() && ! base->isVector()) {
        if (base->getAsSymbolNode())
            error(loc, " left of '[' is not of type array, matrix, or vector ",
                  base->getAsSymbolNode()->getName().c_str(), "");
        else
            error(loc, " left of '[' is not of type array, matrix, or vector ", "expression", "");
    } else if (base->getType().getQualifier().isFrontEndConstant() &&
               index->getQualifier().isFrontEndConstant()) {
        // both base and index are front-end constants
        checkIndex(loc, base->getType(), indexValue);
        return intermediate.foldDereference(base, indexValue, loc);
    } else {
        // at least one of base and index is variable...

        if (index->getQualifier().isFrontEndConstant())
            checkIndex(loc, base->getType(), indexValue);

        if (base->getType().isScalarOrVec1())
            result = base;
        else if (base->getAsSymbolNode() && wasFlattened(base)) {
            if (index->getQualifier().storage != EvqConst)
                error(loc, "Invalid variable index to flattened array", base->getAsSymbolNode()->getName().c_str(), "");

            result = flattenAccess(base, indexValue);
            flattened = (result != base);
        } else {
            if (index->getQualifier().isFrontEndConstant()) {
                if (base->getType().isUnsizedArray())
                    base->getWritableType().updateImplicitArraySize(indexValue + 1);
                else
                    checkIndex(loc, base->getType(), indexValue);
                result = intermediate.addIndex(EOpIndexDirect, base, index, loc);
            } else
                result = intermediate.addIndex(EOpIndexIndirect, base, index, loc);
        }
    }

    if (result == nullptr) {
        // Insert dummy error-recovery result
        result = intermediate.addConstantUnion(0.0, EbtFloat, loc);
    } else {
        // If the array reference was flattened, it has the correct type.  E.g, if it was
        // a uniform array, it was flattened INTO a set of scalar uniforms, not scalar temps.
        // In that case, we preserve the qualifiers.
        if (!flattened) {
            // Insert valid dereferenced result
            TType newType(base->getType(), 0);  // dereferenced type
            if (base->getType().getQualifier().storage == EvqConst && index->getQualifier().storage == EvqConst)
                newType.getQualifier().storage = EvqConst;
            else
                newType.getQualifier().storage = EvqTemporary;
            result->setType(newType);
        }
    }

    return result;
}

// Handle seeing a binary node with a math operation.
TIntermTyped* HlslParseContext::handleBinaryMath(const TSourceLoc& loc, const char* str, TOperator op,
                                                 TIntermTyped* left, TIntermTyped* right)
{
    TIntermTyped* result = intermediate.addBinaryMath(op, left, right, loc);
    if (result == nullptr)
        binaryOpError(loc, str, left->getCompleteString(), right->getCompleteString());

    return result;
}

// Handle seeing a unary node with a math operation.
TIntermTyped* HlslParseContext::handleUnaryMath(const TSourceLoc& loc, const char* str, TOperator op,
                                                TIntermTyped* childNode)
{
    TIntermTyped* result = intermediate.addUnaryMath(op, childNode, loc);

    if (result)
        return result;
    else
        unaryOpError(loc, str, childNode->getCompleteString());

    return childNode;
}
//
// Return true if the name is a struct buffer method
//
bool HlslParseContext::isStructBufferMethod(const TString& name) const
{
    return
        name == "GetDimensions"              ||
        name == "Load"                       ||
        name == "Load2"                      ||
        name == "Load3"                      ||
        name == "Load4"                      ||
        name == "Store"                      ||
        name == "Store2"                     ||
        name == "Store3"                     ||
        name == "Store4"                     ||
        name == "InterlockedAdd"             ||
        name == "InterlockedAnd"             ||
        name == "InterlockedCompareExchange" ||
        name == "InterlockedCompareStore"    ||
        name == "InterlockedExchange"        ||
        name == "InterlockedMax"             ||
        name == "InterlockedMin"             ||
        name == "InterlockedOr"              ||
        name == "InterlockedXor"             ||
        name == "IncrementCounter"           ||
        name == "DecrementCounter"           ||
        name == "Append"                     ||
        name == "Consume";
}

//
// Handle seeing a base.field dereference in the grammar, where 'field' is a
// swizzle or member variable.
//
TIntermTyped* HlslParseContext::handleDotDereference(const TSourceLoc& loc, TIntermTyped* base, const TString& field)
{
    variableCheck(base);

    if (base->isArray()) {
        error(loc, "cannot apply to an array:", ".", field.c_str());
        return base;
    }

    TIntermTyped* result = base;

    if (base->getType().getBasicType() == EbtSampler) {
        // Handle .mips[mipid][pos] operation on textures
        const TSampler& sampler = base->getType().getSampler();
        if (sampler.isTexture() && field == "mips") {
            // Push a null to signify that we expect a mip level under operator[] next.
            mipsOperatorMipArg.push_back(tMipsOperatorData(loc, nullptr));
            // Keep 'result' pointing to 'base', since we expect an operator[] to go by next.
        } else {
            if (field == "mips")
                error(loc, "unexpected texture type for .mips[][] operator:",
                      base->getType().getCompleteString().c_str(), "");
            else
                error(loc, "unexpected operator on texture type:", field.c_str(),
                      base->getType().getCompleteString().c_str());
        }
    } else if (base->isVector() || base->isScalar()) {
        TSwizzleSelectors<TVectorSelector> selectors;
        parseSwizzleSelector(loc, field, base->getVectorSize(), selectors);

        if (base->isScalar()) {
            if (selectors.size() == 1)
                return result;
            else {
                TType type(base->getBasicType(), EvqTemporary, selectors.size());
                return addConstructor(loc, base, type);
            }
        }
        if (base->getVectorSize() == 1) {
            TType scalarType(base->getBasicType(), EvqTemporary, 1);
            if (selectors.size() == 1)
                return addConstructor(loc, base, scalarType);
            else {
                TType vectorType(base->getBasicType(), EvqTemporary, selectors.size());
                return addConstructor(loc, addConstructor(loc, base, scalarType), vectorType);
            }
        }

        if (base->getType().getQualifier().isFrontEndConstant())
            result = intermediate.foldSwizzle(base, selectors, loc);
        else {
            if (selectors.size() == 1) {
                TIntermTyped* index = intermediate.addConstantUnion(selectors[0], loc);
                result = intermediate.addIndex(EOpIndexDirect, base, index, loc);
                result->setType(TType(base->getBasicType(), EvqTemporary));
            } else {
                TIntermTyped* index = intermediate.addSwizzle(selectors, loc);
                result = intermediate.addIndex(EOpVectorSwizzle, base, index, loc);
                result->setType(TType(base->getBasicType(), EvqTemporary, base->getType().getQualifier().precision,
                                selectors.size()));
            }
        }
    } else if (base->isMatrix()) {
        TSwizzleSelectors<TMatrixSelector> selectors;
        if (! parseMatrixSwizzleSelector(loc, field, base->getMatrixCols(), base->getMatrixRows(), selectors))
            return result;

        if (selectors.size() == 1) {
            // Representable by m[c][r]
            if (base->getType().getQualifier().isFrontEndConstant()) {
                result = intermediate.foldDereference(base, selectors[0].coord1, loc);
                result = intermediate.foldDereference(result, selectors[0].coord2, loc);
            } else {
                result = intermediate.addIndex(EOpIndexDirect, base,
                                               intermediate.addConstantUnion(selectors[0].coord1, loc),
                                               loc);
                TType dereferencedCol(base->getType(), 0);
                result->setType(dereferencedCol);
                result = intermediate.addIndex(EOpIndexDirect, result,
                                               intermediate.addConstantUnion(selectors[0].coord2, loc),
                                               loc);
                TType dereferenced(dereferencedCol, 0);
                result->setType(dereferenced);
            }
        } else {
            int column = getMatrixComponentsColumn(base->getMatrixRows(), selectors);
            if (column >= 0) {
                // Representable by m[c]
                if (base->getType().getQualifier().isFrontEndConstant())
                    result = intermediate.foldDereference(base, column, loc);
                else {
                    result = intermediate.addIndex(EOpIndexDirect, base, intermediate.addConstantUnion(column, loc),
                                                   loc);
                    TType dereferenced(base->getType(), 0);
                    result->setType(dereferenced);
                }
            } else {
                // general case, not a column, not a single component
                TIntermTyped* index = intermediate.addSwizzle(selectors, loc);
                result = intermediate.addIndex(EOpMatrixSwizzle, base, index, loc);
                result->setType(TType(base->getBasicType(), EvqTemporary, base->getType().getQualifier().precision,
                                      selectors.size()));
           }
        }
    } else if (base->getBasicType() == EbtStruct || base->getBasicType() == EbtBlock) {
        const TTypeList* fields = base->getType().getStruct();
        bool fieldFound = false;
        int member;
        for (member = 0; member < (int)fields->size(); ++member) {
            if ((*fields)[member].type->getFieldName() == field) {
                fieldFound = true;
                break;
            }
        }
        if (fieldFound) {
            if (base->getAsSymbolNode() && wasFlattened(base)) {
                result = flattenAccess(base, member);
            } else {
                if (base->getType().getQualifier().storage == EvqConst)
                    result = intermediate.foldDereference(base, member, loc);
                else {
                    TIntermTyped* index = intermediate.addConstantUnion(member, loc);
                    result = intermediate.addIndex(EOpIndexDirectStruct, base, index, loc);
                    result->setType(*(*fields)[member].type);
                }
            }
        } else
            error(loc, "no such field in structure", field.c_str(), "");
    } else
        error(loc, "does not apply to this type:", field.c_str(), base->getType().getCompleteString().c_str());

    return result;
}

//
// Return true if the field should be treated as a built-in method.
// Return false otherwise.
//
bool HlslParseContext::isBuiltInMethod(const TSourceLoc&, TIntermTyped* base, const TString& field)
{
    if (base == nullptr)
        return false;

    variableCheck(base);

    if (base->getType().getBasicType() == EbtSampler) {
        return true;
    } else if (isStructBufferType(base->getType()) && isStructBufferMethod(field)) {
        return true;
    } else if (field == "Append" ||
               field == "RestartStrip") {
        // We cannot check the type here: it may be sanitized if we're not compiling a geometry shader, but
        // the code is around in the shader source.
        return true;
    } else
        return false;
}

// Independently establish a built-in that is a member of a structure.
// 'arraySizes' are what's desired for the independent built-in, whatever
// the higher-level source/expression of them was.
void HlslParseContext::splitBuiltIn(const TString& baseName, const TType& memberType, const TArraySizes* arraySizes,
                                    const TQualifier& outerQualifier)
{
    // Because of arrays of structs, we might be asked more than once,
    // but the arraySizes passed in should have captured the whole thing
    // the first time.
    // However, clip/cull rely on multiple updates.
    if (!isClipOrCullDistance(memberType))
        if (splitBuiltIns.find(tInterstageIoData(memberType.getQualifier().builtIn, outerQualifier.storage)) !=
            splitBuiltIns.end())
            return;

    TVariable* ioVar = makeInternalVariable(baseName + "." + memberType.getFieldName(), memberType);

    if (arraySizes != nullptr && !memberType.isArray())
        ioVar->getWritableType().copyArraySizes(*arraySizes);

    splitBuiltIns[tInterstageIoData(memberType.getQualifier().builtIn, outerQualifier.storage)] = ioVar;
    if (!isClipOrCullDistance(ioVar->getType()))
        trackLinkage(*ioVar);

    // Merge qualifier from the user structure
    mergeQualifiers(ioVar->getWritableType().getQualifier(), outerQualifier);

    // Fix the builtin type if needed (e.g, some types require fixed array sizes, no matter how the
    // shader declared them).  This is done after mergeQualifiers(), in case fixBuiltInIoType looks
    // at the qualifier to determine e.g, in or out qualifications.
    fixBuiltInIoType(ioVar->getWritableType());

    // But, not location, we're losing that
    ioVar->getWritableType().getQualifier().layoutLocation = TQualifier::layoutLocationEnd;
}

// Split a type into
//   1. a struct of non-I/O members
//   2. a collection of independent I/O variables
void HlslParseContext::split(const TVariable& variable)
{
    // Create a new variable:
    const TType& clonedType = *variable.getType().clone();
    const TType& splitType = split(clonedType, variable.getName(), clonedType.getQualifier());
    splitNonIoVars[variable.getUniqueId()] = makeInternalVariable(variable.getName(), splitType);
}

// Recursive implementation of split().
// Returns reference to the modified type.
const TType& HlslParseContext::split(const TType& type, const TString& name, const TQualifier& outerQualifier)
{
    if (type.isStruct()) {
        TTypeList* userStructure = type.getWritableStruct();
        for (auto ioType = userStructure->begin(); ioType != userStructure->end(); ) {
            if (ioType->type->isBuiltIn()) {
                // move out the built-in
                splitBuiltIn(name, *ioType->type, type.getArraySizes(), outerQualifier);
                ioType = userStructure->erase(ioType);
            } else {
                split(*ioType->type, name + "." + ioType->type->getFieldName(), outerQualifier);
                ++ioType;
            }
        }
    }

    return type;
}

// Is this an aggregate that should be flattened?
// Can be applied to intermediate levels of type in a hierarchy.
// Some things like flattening uniform arrays are only about the top level
// of the aggregate, triggered on 'topLevel'.
bool HlslParseContext::shouldFlatten(const TType& type, TStorageQualifier qualifier, bool topLevel) const
{
    switch (qualifier) {
    case EvqVaryingIn:
    case EvqVaryingOut:
        return type.isStruct() || type.isArray();
    case EvqUniform:
        return (type.isArray() && intermediate.getFlattenUniformArrays() && topLevel) ||
               (type.isStruct() && type.containsOpaque());
    default:
        return false;
    };
}

// Top level variable flattening: construct data
void HlslParseContext::flatten(const TVariable& variable, bool linkage, bool arrayed)
{
    const TType& type = variable.getType();

    // If it's a standalone built-in, there is nothing to flatten
    if (type.isBuiltIn() && !type.isStruct())
        return;


    auto entry = flattenMap.insert(std::make_pair(variable.getUniqueId(),
                                                  TFlattenData(type.getQualifier().layoutBinding,
                                                               type.getQualifier().layoutLocation)));

    if (type.isStruct() && type.getStruct()->size()==0)
        return;
    // if flattening arrayed io struct, array each member of dereferenced type
    if (arrayed) {
        const TType dereferencedType(type, 0);
        flatten(variable, dereferencedType, entry.first->second, variable.getName(), linkage,
                type.getQualifier(), type.getArraySizes());
    } else {
        flatten(variable, type, entry.first->second, variable.getName(), linkage,
                type.getQualifier(), nullptr);
    }
}

// Recursively flatten the given variable at the provided type, building the flattenData as we go.
//
// This is mutually recursive with flattenStruct and flattenArray.
// We are going to flatten an arbitrarily nested composite structure into a linear sequence of
// members, and later on, we want to turn a path through the tree structure into a final
// location in this linear sequence.
//
// If the tree was N-ary, that can be directly calculated.  However, we are dealing with
// arbitrary numbers - perhaps a struct of 7 members containing an array of 3.  Thus, we must
// build a data structure to allow the sequence of bracket and dot operators on arrays and
// structs to arrive at the proper member.
//
// To avoid storing a tree with pointers, we are going to flatten the tree into a vector of integers.
// The leaves are the indexes into the flattened member array.
// Each level will have the next location for the Nth item stored sequentially, so for instance:
//
// struct { float2 a[2]; int b; float4 c[3] };
//
// This will produce the following flattened tree:
// Pos: 0  1   2    3  4    5  6   7     8   9  10   11  12 13
//     (3, 7,  8,   5, 6,   0, 1,  2,   11, 12, 13,   3,  4, 5}
//
// Given a reference to mystruct.c[1], the access chain is (2,1), so we traverse:
//   (0+2) = 8  -->  (8+1) = 12 -->   12 = 4
//
// so the 4th flattened member in traversal order is ours.
//
int HlslParseContext::flatten(const TVariable& variable, const TType& type,
                              TFlattenData& flattenData, TString name, bool linkage,
                              const TQualifier& outerQualifier,
                              const TArraySizes* builtInArraySizes)
{
    // If something is an arrayed struct, the array flattener will recursively call flatten()
    // to then flatten the struct, so this is an "if else": we don't do both.
    if (type.isArray())
        return flattenArray(variable, type, flattenData, name, linkage, outerQualifier);
    else if (type.isStruct())
        return flattenStruct(variable, type, flattenData, name, linkage, outerQualifier, builtInArraySizes);
    else {
        assert(0); // should never happen
        return -1;
    }
}

// Add a single flattened member to the flattened data being tracked for the composite
// Returns true for the final flattening level.
int HlslParseContext::addFlattenedMember(const TVariable& variable, const TType& type, TFlattenData& flattenData,
                                         const TString& memberName, bool linkage,
                                         const TQualifier& outerQualifier,
                                         const TArraySizes* builtInArraySizes)
{
    if (!shouldFlatten(type, outerQualifier.storage, false)) {
        // This is as far as we flatten.  Insert the variable.
        TVariable* memberVariable = makeInternalVariable(memberName, type);
        mergeQualifiers(memberVariable->getWritableType().getQualifier(), variable.getType().getQualifier());

        if (flattenData.nextBinding != TQualifier::layoutBindingEnd)
            memberVariable->getWritableType().getQualifier().layoutBinding = flattenData.nextBinding++;

        if (memberVariable->getType().isBuiltIn()) {
            // inherited locations are nonsensical for built-ins (TODO: what if semantic had a number)
            memberVariable->getWritableType().getQualifier().layoutLocation = TQualifier::layoutLocationEnd;
        } else {
            // inherited locations must be auto bumped, not replicated
            if (flattenData.nextLocation != TQualifier::layoutLocationEnd) {
                memberVariable->getWritableType().getQualifier().layoutLocation = flattenData.nextLocation;
                flattenData.nextLocation += intermediate.computeTypeLocationSize(memberVariable->getType(), language);
                nextOutLocation = std::max(nextOutLocation, flattenData.nextLocation);
            }
        }

        // Only propagate arraysizes here for arrayed io
        if (variable.getType().getQualifier().isArrayedIo(language) && builtInArraySizes != nullptr)
            memberVariable->getWritableType().copyArraySizes(*builtInArraySizes);

        flattenData.offsets.push_back(static_cast<int>(flattenData.members.size()));
        flattenData.members.push_back(memberVariable);

        if (linkage)
            trackLinkage(*memberVariable);

        return static_cast<int>(flattenData.offsets.size()) - 1; // location of the member reference
    } else {
        // Further recursion required
        return flatten(variable, type, flattenData, memberName, linkage, outerQualifier, builtInArraySizes);
    }
}

// Figure out the mapping between an aggregate's top members and an
// equivalent set of individual variables.
//
// Assumes shouldFlatten() or equivalent was called first.
int HlslParseContext::flattenStruct(const TVariable& variable, const TType& type,
                                    TFlattenData& flattenData, TString name, bool linkage,
                                    const TQualifier& outerQualifier,
                                    const TArraySizes* builtInArraySizes)
{
    assert(type.isStruct());

    auto members = *type.getStruct();

    // Reserve space for this tree level.
    int start = static_cast<int>(flattenData.offsets.size());
    int pos = start;
    flattenData.offsets.resize(int(pos + members.size()), -1);

    for (int member = 0; member < (int)members.size(); ++member) {
        TType& dereferencedType = *members[member].type;
        if (dereferencedType.isBuiltIn())
            splitBuiltIn(variable.getName(), dereferencedType, builtInArraySizes, outerQualifier);
        else {
            const int mpos = addFlattenedMember(variable, dereferencedType, flattenData,
                                                name + "." + dereferencedType.getFieldName(),
                                                linkage, outerQualifier,
                                                builtInArraySizes == nullptr && dereferencedType.isArray()
                                                                       ? dereferencedType.getArraySizes()
                                                                       : builtInArraySizes);
            flattenData.offsets[pos++] = mpos;
        }
    }

    return start;
}

// Figure out mapping between an array's members and an
// equivalent set of individual variables.
//
// Assumes shouldFlatten() or equivalent was called first.
int HlslParseContext::flattenArray(const TVariable& variable, const TType& type,
                                   TFlattenData& flattenData, TString name, bool linkage,
                                   const TQualifier& outerQualifier)
{
    assert(type.isSizedArray());

    const int size = type.getOuterArraySize();
    const TType dereferencedType(type, 0);

    if (name.empty())
        name = variable.getName();

    // Reserve space for this tree level.
    int start = static_cast<int>(flattenData.offsets.size());
    int pos   = start;
    flattenData.offsets.resize(int(pos + size), -1);

    for (int element=0; element < size; ++element) {
        char elementNumBuf[20];  // sufficient for MAXINT
        snprintf(elementNumBuf, sizeof(elementNumBuf)-1, "[%d]", element);
        const int mpos = addFlattenedMember(variable, dereferencedType, flattenData,
                                            name + elementNumBuf, linkage, outerQualifier,
                                            type.getArraySizes());

        flattenData.offsets[pos++] = mpos;
    }

    return start;
}

// Return true if we have flattened this node.
bool HlslParseContext::wasFlattened(const TIntermTyped* node) const
{
    return node != nullptr && node->getAsSymbolNode() != nullptr &&
           wasFlattened(node->getAsSymbolNode()->getId());
}

// Return true if we have split this structure
bool HlslParseContext::wasSplit(const TIntermTyped* node) const
{
    return node != nullptr && node->getAsSymbolNode() != nullptr &&
           wasSplit(node->getAsSymbolNode()->getId());
}

// Turn an access into an aggregate that was flattened to instead be
// an access to the individual variable the member was flattened to.
// Assumes wasFlattened() or equivalent was called first.
TIntermTyped* HlslParseContext::flattenAccess(TIntermTyped* base, int member)
{
    const TType dereferencedType(base->getType(), member);  // dereferenced type
    const TIntermSymbol& symbolNode = *base->getAsSymbolNode();
    TIntermTyped* flattened = flattenAccess(symbolNode.getId(), member, base->getQualifier().storage,
                                            dereferencedType, symbolNode.getFlattenSubset());

    return flattened ? flattened : base;
}
TIntermTyped* HlslParseContext::flattenAccess(long long uniqueId, int member, TStorageQualifier outerStorage,
    const TType& dereferencedType, int subset)
{
    const auto flattenData = flattenMap.find(uniqueId);

    if (flattenData == flattenMap.end())
        return nullptr;

    // Calculate new cumulative offset from the packed tree
    int newSubset = flattenData->second.offsets[subset >= 0 ? subset + member : member];

    TIntermSymbol* subsetSymbol;
    if (!shouldFlatten(dereferencedType, outerStorage, false)) {
        // Finished flattening: create symbol for variable
        member = flattenData->second.offsets[newSubset];
        const TVariable* memberVariable = flattenData->second.members[member];
        subsetSymbol = intermediate.addSymbol(*memberVariable);
        subsetSymbol->setFlattenSubset(-1);
    } else {

        // If this is not the final flattening, accumulate the position and return
        // an object of the partially dereferenced type.
        subsetSymbol = new TIntermSymbol(uniqueId, "flattenShadow", dereferencedType);
        subsetSymbol->setFlattenSubset(newSubset);
    }

    return subsetSymbol;
}

// For finding where the first leaf is in a subtree of a multi-level aggregate
// that is just getting a subset assigned. Follows the same logic as flattenAccess,
// but logically going down the "left-most" tree branch each step of the way.
//
// Returns the offset into the first leaf of the subset.
int HlslParseContext::findSubtreeOffset(const TIntermNode& node) const
{
    const TIntermSymbol* sym = node.getAsSymbolNode();
    if (sym == nullptr)
        return 0;
    if (!sym->isArray() && !sym->isStruct())
        return 0;
    int subset = sym->getFlattenSubset();
    if (subset == -1)
        return 0;

    // Getting this far means a partial aggregate is identified by the flatten subset.
    // Find the first leaf of the subset.

    const auto flattenData = flattenMap.find(sym->getId());
    if (flattenData == flattenMap.end())
        return 0;

    return findSubtreeOffset(sym->getType(), subset, flattenData->second.offsets);

    do {
        subset = flattenData->second.offsets[subset];
    } while (true);
}
// Recursively do the desent
int HlslParseContext::findSubtreeOffset(const TType& type, int subset, const TVector<int>& offsets) const
{
    if (!type.isArray() && !type.isStruct())
        return offsets[subset];
    TType derefType(type, 0);
    return findSubtreeOffset(derefType, offsets[subset], offsets);
};

// Find and return the split IO TVariable for id, or nullptr if none.
TVariable* HlslParseContext::getSplitNonIoVar(long long id) const
{
    const auto splitNonIoVar = splitNonIoVars.find(id);
    if (splitNonIoVar == splitNonIoVars.end())
        return nullptr;

    return splitNonIoVar->second;
}

// Pass through to base class after remembering built-in mappings.
void HlslParseContext::trackLinkage(TSymbol& symbol)
{
    TBuiltInVariable biType = symbol.getType().getQualifier().builtIn;

    if (biType != EbvNone)
        builtInTessLinkageSymbols[biType] = symbol.clone();

    TParseContextBase::trackLinkage(symbol);
}


// Returns true if the built-in is a clip or cull distance variable.
bool HlslParseContext::isClipOrCullDistance(TBuiltInVariable builtIn)
{
    return builtIn == EbvClipDistance || builtIn == EbvCullDistance;
}

// Some types require fixed array sizes in SPIR-V, but can be scalars or
// arrays of sizes SPIR-V doesn't allow.  For example, tessellation factors.
// This creates the right size.  A conversion is performed when the internal
// type is copied to or from the external type.  This corrects the externally
// facing input or output type to abide downstream semantics.
void HlslParseContext::fixBuiltInIoType(TType& type)
{
    int requiredArraySize = 0;
    int requiredVectorSize = 0;

    switch (type.getQualifier().builtIn) {
    case EbvTessLevelOuter: requiredArraySize = 4; break;
    case EbvTessLevelInner: requiredArraySize = 2; break;

    case EbvSampleMask:
        {
            // Promote scalar to array of size 1.  Leave existing arrays alone.
            if (!type.isArray())
                requiredArraySize = 1;
            break;
        }

    case EbvWorkGroupId:        requiredVectorSize = 3; break;
    case EbvGlobalInvocationId: requiredVectorSize = 3; break;
    case EbvLocalInvocationId:  requiredVectorSize = 3; break;
    case EbvTessCoord:          requiredVectorSize = 3; break;

    default:
        if (isClipOrCullDistance(type)) {
            const int loc = type.getQualifier().layoutLocation;

            if (type.getQualifier().builtIn == EbvClipDistance) {
                if (type.getQualifier().storage == EvqVaryingIn)
                    clipSemanticNSizeIn[loc] = type.getVectorSize();
                else
                    clipSemanticNSizeOut[loc] = type.getVectorSize();
            } else {
                if (type.getQualifier().storage == EvqVaryingIn)
                    cullSemanticNSizeIn[loc] = type.getVectorSize();
                else
                    cullSemanticNSizeOut[loc] = type.getVectorSize();
            }
        }

        return;
    }

    // Alter or set vector size as needed.
    if (requiredVectorSize > 0) {
        TType newType(type.getBasicType(), type.getQualifier().storage, requiredVectorSize);
        newType.getQualifier() = type.getQualifier();

        type.shallowCopy(newType);
    }

    // Alter or set array size as needed.
    if (requiredArraySize > 0) {
        if (!type.isArray() || type.getOuterArraySize() != requiredArraySize) {
            TArraySizes* arraySizes = new TArraySizes;
            arraySizes->addInnerSize(requiredArraySize);
            type.transferArraySizes(arraySizes);
        }
    }
}

// Variables that correspond to the user-interface in and out of a stage
// (not the built-in interface) are
//  - assigned locations
//  - registered as a linkage node (part of the stage's external interface).
// Assumes it is called in the order in which locations should be assigned.
void HlslParseContext::assignToInterface(TVariable& variable)
{
    const auto assignLocation = [&](TVariable& variable) {
        TType& type = variable.getWritableType();
        if (!type.isStruct() || type.getStruct()->size() > 0) {
            TQualifier& qualifier = type.getQualifier();
            if (qualifier.storage == EvqVaryingIn || qualifier.storage == EvqVaryingOut) {
                if (qualifier.builtIn == EbvNone && !qualifier.hasLocation()) {
                    // Strip off the outer array dimension for those having an extra one.
                    int size;
                    if (type.isArray() && qualifier.isArrayedIo(language)) {
                        TType elementType(type, 0);
                        size = intermediate.computeTypeLocationSize(elementType, language);
                    } else
                        size = intermediate.computeTypeLocationSize(type, language);

                    if (qualifier.storage == EvqVaryingIn) {
                        variable.getWritableType().getQualifier().layoutLocation = nextInLocation;
                        nextInLocation += size;
                    } else {
                        variable.getWritableType().getQualifier().layoutLocation = nextOutLocation;
                        nextOutLocation += size;
                    }
                }
                trackLinkage(variable);
            }
        }
    };

    if (wasFlattened(variable.getUniqueId())) {
        auto& memberList = flattenMap[variable.getUniqueId()].members;
        for (auto member = memberList.begin(); member != memberList.end(); ++member)
            assignLocation(**member);
    } else if (wasSplit(variable.getUniqueId())) {
        TVariable* splitIoVar = getSplitNonIoVar(variable.getUniqueId());
        assignLocation(*splitIoVar);
    } else {
        assignLocation(variable);
    }
}

//
// Handle seeing a function declarator in the grammar.  This is the precursor
// to recognizing a function prototype or function definition.
//
void HlslParseContext::handleFunctionDeclarator(const TSourceLoc& loc, TFunction& function, bool prototype)
{
    //
    // Multiple declarations of the same function name are allowed.
    //
    // If this is a definition, the definition production code will check for redefinitions
    // (we don't know at this point if it's a definition or not).
    //
    bool builtIn;
    TSymbol* symbol = symbolTable.find(function.getMangledName(), &builtIn);
    const TFunction* prevDec = symbol ? symbol->getAsFunction() : nullptr;

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
}

// For struct buffers with counters, we must pass the counter buffer as hidden parameter.
// This adds the hidden parameter to the parameter list in 'paramNodes' if needed.
// Otherwise, it's a no-op
void HlslParseContext::addStructBufferHiddenCounterParam(const TSourceLoc& loc, TParameter& param,
                                                         TIntermAggregate*& paramNodes)
{
    if (! hasStructBuffCounter(*param.type))
        return;

    const TString counterBlockName(intermediate.addCounterBufferName(*param.name));

    TType counterType;
    counterBufferType(loc, counterType);
    TVariable *variable = makeInternalVariable(counterBlockName, counterType);

    if (! symbolTable.insert(*variable))
        error(loc, "redefinition", variable->getName().c_str(), "");

    paramNodes = intermediate.growAggregate(paramNodes,
                                            intermediate.addSymbol(*variable, loc),
                                            loc);
}

//
// Handle seeing the function prototype in front of a function definition in the grammar.
// The body is handled after this function returns.
//
// Returns an aggregate of parameter-symbol nodes.
//
TIntermAggregate* HlslParseContext::handleFunctionDefinition(const TSourceLoc& loc, TFunction& function,
                                                             const TAttributes& attributes,
                                                             TIntermNode*& entryPointTree)
{
    currentCaller = function.getMangledName();
    TSymbol* symbol = symbolTable.find(function.getMangledName());
    TFunction* prevDec = symbol ? symbol->getAsFunction() : nullptr;

    if (prevDec == nullptr)
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

    // Entry points need different I/O and other handling, transform it so the
    // rest of this function doesn't care.
    entryPointTree = transformEntryPoint(loc, function, attributes);

    //
    // New symbol table scope for body of function plus its arguments
    //
    pushScope();

    //
    // Insert parameters into the symbol table.
    // If the parameter has no name, it's not an error, just don't insert it
    // (could be used for unused args).
    //
    // Also, accumulate the list of parameters into the AST, so lower level code
    // knows where to find parameters.
    //
    TIntermAggregate* paramNodes = new TIntermAggregate;
    for (int i = 0; i < function.getParamCount(); i++) {
        TParameter& param = function[i];
        if (param.name != nullptr) {
            TVariable *variable = new TVariable(param.name, *param.type);

            if (i == 0 && function.hasImplicitThis()) {
                // Anonymous 'this' members are already in a symbol-table level,
                // and we need to know what function parameter to map them to.
                symbolTable.makeInternalVariable(*variable);
                pushImplicitThis(variable);
            }

            // Insert the parameters with name in the symbol table.
            if (! symbolTable.insert(*variable))
                error(loc, "redefinition", variable->getName().c_str(), "");

            // Add parameters to the AST list.
            if (shouldFlatten(variable->getType(), variable->getType().getQualifier().storage, true)) {
                // Expand the AST parameter nodes (but not the name mangling or symbol table view)
                // for structures that need to be flattened.
                flatten(*variable, false);
                const TTypeList* structure = variable->getType().getStruct();
                for (int mem = 0; mem < (int)structure->size(); ++mem) {
                    paramNodes = intermediate.growAggregate(paramNodes,
                                                            flattenAccess(variable->getUniqueId(), mem,
                                                                          variable->getType().getQualifier().storage,
                                                                          *(*structure)[mem].type),
                                                            loc);
                }
            } else {
                // Add the parameter to the AST
                paramNodes = intermediate.growAggregate(paramNodes,
                                                        intermediate.addSymbol(*variable, loc),
                                                        loc);
            }

            // Add hidden AST parameter for struct buffer counters, if needed.
            addStructBufferHiddenCounterParam(loc, param, paramNodes);
        } else
            paramNodes = intermediate.growAggregate(paramNodes, intermediate.addSymbol(*param.type, loc), loc);
    }
    if (function.hasIllegalImplicitThis())
        pushImplicitThis(nullptr);

    intermediate.setAggregateOperator(paramNodes, EOpParameters, TType(EbtVoid), loc);
    loopNestingLevel = 0;
    controlFlowNestingLevel = 0;
    postEntryPointReturn = false;

    return paramNodes;
}

// Handle all [attrib] attribute for the shader entry point
void HlslParseContext::handleEntryPointAttributes(const TSourceLoc& loc, const TAttributes& attributes)
{
    for (auto it = attributes.begin(); it != attributes.end(); ++it) {
        switch (it->name) {
        case EatNumThreads:
        {
            const TIntermSequence& sequence = it->args->getSequence();
            for (int lid = 0; lid < int(sequence.size()); ++lid)
                intermediate.setLocalSize(lid, sequence[lid]->getAsConstantUnion()->getConstArray()[0].getIConst());
            break;
        }
        case EatInstance: 
        {
            int invocations;

            if (!it->getInt(invocations)) {
                error(loc, "invalid instance", "", "");
            } else {
                if (!intermediate.setInvocations(invocations))
                    error(loc, "cannot change previously set instance attribute", "", "");
            }
            break;
        }
        case EatMaxVertexCount:
        {
            int maxVertexCount;

            if (! it->getInt(maxVertexCount)) {
                error(loc, "invalid maxvertexcount", "", "");
            } else {
                if (! intermediate.setVertices(maxVertexCount))
                    error(loc, "cannot change previously set maxvertexcount attribute", "", "");
            }
            break;
        }
        case EatPatchConstantFunc:
        {
            TString pcfName;
            if (! it->getString(pcfName, 0, false)) {
                error(loc, "invalid patch constant function", "", "");
            } else {
                patchConstantFunctionName = pcfName;
            }
            break;
        }
        case EatDomain:
        {
            // Handle [domain("...")]
            TString domainStr;
            if (! it->getString(domainStr)) {
                error(loc, "invalid domain", "", "");
            } else {
                TLayoutGeometry domain = ElgNone;

                if (domainStr == "tri") {
                    domain = ElgTriangles;
                } else if (domainStr == "quad") {
                    domain = ElgQuads;
                } else if (domainStr == "isoline") {
                    domain = ElgIsolines;
                } else {
                    error(loc, "unsupported domain type", domainStr.c_str(), "");
                }

                if (language == EShLangTessEvaluation) {
                    if (! intermediate.setInputPrimitive(domain))
                        error(loc, "cannot change previously set domain", TQualifier::getGeometryString(domain), "");
                } else {
                    if (! intermediate.setOutputPrimitive(domain))
                        error(loc, "cannot change previously set domain", TQualifier::getGeometryString(domain), "");
                }
            }
            break;
        }
        case EatOutputTopology:
        {
            // Handle [outputtopology("...")]
            TString topologyStr;
            if (! it->getString(topologyStr)) {
                error(loc, "invalid outputtopology", "", "");
            } else {
                TVertexOrder vertexOrder = EvoNone;
                TLayoutGeometry primitive = ElgNone;

                if (topologyStr == "point") {
                    intermediate.setPointMode();
                } else if (topologyStr == "line") {
                    primitive = ElgIsolines;
                } else if (topologyStr == "triangle_cw") {
                    vertexOrder = EvoCw;
                    primitive = ElgTriangles;
                } else if (topologyStr == "triangle_ccw") {
                    vertexOrder = EvoCcw;
                    primitive = ElgTriangles;
                } else {
                    error(loc, "unsupported outputtopology type", topologyStr.c_str(), "");
                }

                if (vertexOrder != EvoNone) {
                    if (! intermediate.setVertexOrder(vertexOrder)) {
                        error(loc, "cannot change previously set outputtopology",
                              TQualifier::getVertexOrderString(vertexOrder), "");
                    }
                }
                if (primitive != ElgNone)
                    intermediate.setOutputPrimitive(primitive);
            }
            break;
        }
        case EatPartitioning:
        {
            // Handle [partitioning("...")]
            TString partitionStr;
            if (! it->getString(partitionStr)) {
                error(loc, "invalid partitioning", "", "");
            } else {
                TVertexSpacing partitioning = EvsNone;

                if (partitionStr == "integer") {
                    partitioning = EvsEqual;
                } else if (partitionStr == "fractional_even") {
                    partitioning = EvsFractionalEven;
                } else if (partitionStr == "fractional_odd") {
                    partitioning = EvsFractionalOdd;
                    //} else if (partition == "pow2") { // TODO: currently nothing to map this to.
                } else {
                    error(loc, "unsupported partitioning type", partitionStr.c_str(), "");
                }

                if (! intermediate.setVertexSpacing(partitioning))
                    error(loc, "cannot change previously set partitioning",
                          TQualifier::getVertexSpacingString(partitioning), "");
            }
            break;
        }
        case EatOutputControlPoints:
        {
            // Handle [outputcontrolpoints("...")]
            int ctrlPoints;
            if (! it->getInt(ctrlPoints)) {
                error(loc, "invalid outputcontrolpoints", "", "");
            } else {
                if (! intermediate.setVertices(ctrlPoints)) {
                    error(loc, "cannot change previously set outputcontrolpoints attribute", "", "");
                }
            }
            break;
        }
        case EatEarlyDepthStencil:
            intermediate.setEarlyFragmentTests();
            break;
        case EatBuiltIn:
        case EatLocation:
            // tolerate these because of dual use of entrypoint and type attributes
            break;
        default:
            warn(loc, "attribute does not apply to entry point", "", "");
            break;
        }
    }
}

// Update the given type with any type-like attribute information in the
// attributes.
void HlslParseContext::transferTypeAttributes(const TSourceLoc& loc, const TAttributes& attributes, TType& type,
    bool allowEntry)
{
    if (attributes.size() == 0)
        return;

    int value;
    TString builtInString;
    for (auto it = attributes.begin(); it != attributes.end(); ++it) {
        switch (it->name) {
        case EatLocation:
            // location
            if (it->getInt(value))
                type.getQualifier().layoutLocation = value;
            else
                error(loc, "needs a literal integer", "location", "");
            break;
        case EatBinding:
            // binding
            if (it->getInt(value)) {
                type.getQualifier().layoutBinding = value;
                type.getQualifier().layoutSet = 0;
            } else
                error(loc, "needs a literal integer", "binding", "");
            // set
            if (it->getInt(value, 1))
                type.getQualifier().layoutSet = value;
            break;
        case EatGlobalBinding:
            // global cbuffer binding
            if (it->getInt(value))
                globalUniformBinding = value;
            else
                error(loc, "needs a literal integer", "global binding", "");
            // global cbuffer set
            if (it->getInt(value, 1))
                globalUniformSet = value;
            break;
        case EatInputAttachment:
            // input attachment
            if (it->getInt(value))
                type.getQualifier().layoutAttachment = value;
            else
                error(loc, "needs a literal integer", "input attachment", "");
            break;
        case EatBuiltIn:
            // PointSize built-in
            if (it->getString(builtInString, 0, false)) {
                if (builtInString == "PointSize")
                    type.getQualifier().builtIn = EbvPointSize;
            }
            break;
        case EatPushConstant:
            // push_constant
            type.getQualifier().layoutPushConstant = true;
            break;
        case EatConstantId:
            // specialization constant
            if (type.getQualifier().storage != EvqConst) {
                error(loc, "needs a const type", "constant_id", "");
                break;
            }
            if (it->getInt(value)) {
                TSourceLoc loc;
                loc.init();
                setSpecConstantId(loc, type.getQualifier(), value);
            }
            break;

        // image formats
        case EatFormatRgba32f:      type.getQualifier().layoutFormat = ElfRgba32f;      break;
        case EatFormatRgba16f:      type.getQualifier().layoutFormat = ElfRgba16f;      break;
        case EatFormatR32f:         type.getQualifier().layoutFormat = ElfR32f;         break;
        case EatFormatRgba8:        type.getQualifier().layoutFormat = ElfRgba8;        break;
        case EatFormatRgba8Snorm:   type.getQualifier().layoutFormat = ElfRgba8Snorm;   break;
        case EatFormatRg32f:        type.getQualifier().layoutFormat = ElfRg32f;        break;
        case EatFormatRg16f:        type.getQualifier().layoutFormat = ElfRg16f;        break;
        case EatFormatR11fG11fB10f: type.getQualifier().layoutFormat = ElfR11fG11fB10f; break;
        case EatFormatR16f:         type.getQualifier().layoutFormat = ElfR16f;         break;
        case EatFormatRgba16:       type.getQualifier().layoutFormat = ElfRgba16;       break;
        case EatFormatRgb10A2:      type.getQualifier().layoutFormat = ElfRgb10A2;      break;
        case EatFormatRg16:         type.getQualifier().layoutFormat = ElfRg16;         break;
        case EatFormatRg8:          type.getQualifier().layoutFormat = ElfRg8;          break;
        case EatFormatR16:          type.getQualifier().layoutFormat = ElfR16;          break;
        case EatFormatR8:           type.getQualifier().layoutFormat = ElfR8;           break;
        case EatFormatRgba16Snorm:  type.getQualifier().layoutFormat = ElfRgba16Snorm;  break;
        case EatFormatRg16Snorm:    type.getQualifier().layoutFormat = ElfRg16Snorm;    break;
        case EatFormatRg8Snorm:     type.getQualifier().layoutFormat = ElfRg8Snorm;     break;
        case EatFormatR16Snorm:     type.getQualifier().layoutFormat = ElfR16Snorm;     break;
        case EatFormatR8Snorm:      type.getQualifier().layoutFormat = ElfR8Snorm;      break;
        case EatFormatRgba32i:      type.getQualifier().layoutFormat = ElfRgba32i;      break;
        case EatFormatRgba16i:      type.getQualifier().layoutFormat = ElfRgba16i;      break;
        case EatFormatRgba8i:       type.getQualifier().layoutFormat = ElfRgba8i;       break;
        case EatFormatR32i:         type.getQualifier().layoutFormat = ElfR32i;         break;
        case EatFormatRg32i:        type.getQualifier().layoutFormat = ElfRg32i;        break;
        case EatFormatRg16i:        type.getQualifier().layoutFormat = ElfRg16i;        break;
        case EatFormatRg8i:         type.getQualifier().layoutFormat = ElfRg8i;         break;
        case EatFormatR16i:         type.getQualifier().layoutFormat = ElfR16i;         break;
        case EatFormatR8i:          type.getQualifier().layoutFormat = ElfR8i;          break;
        case EatFormatRgba32ui:     type.getQualifier().layoutFormat = ElfRgba32ui;     break;
        case EatFormatRgba16ui:     type.getQualifier().layoutFormat = ElfRgba16ui;     break;
        case EatFormatRgba8ui:      type.getQualifier().layoutFormat = ElfRgba8ui;      break;
        case EatFormatR32ui:        type.getQualifier().layoutFormat = ElfR32ui;        break;
        case EatFormatRgb10a2ui:    type.getQualifier().layoutFormat = ElfRgb10a2ui;    break;
        case EatFormatRg32ui:       type.getQualifier().layoutFormat = ElfRg32ui;       break;
        case EatFormatRg16ui:       type.getQualifier().layoutFormat = ElfRg16ui;       break;
        case EatFormatRg8ui:        type.getQualifier().layoutFormat = ElfRg8ui;        break;
        case EatFormatR16ui:        type.getQualifier().layoutFormat = ElfR16ui;        break;
        case EatFormatR8ui:         type.getQualifier().layoutFormat = ElfR8ui;         break;
        case EatFormatUnknown:      type.getQualifier().layoutFormat = ElfNone;         break;

        case EatNonWritable:  type.getQualifier().readonly = true;   break;
        case EatNonReadable:  type.getQualifier().writeonly = true;  break;

        default:
            if (! allowEntry)
                warn(loc, "attribute does not apply to a type", "", "");
            break;
        }
    }
}

//
// Do all special handling for the entry point, including wrapping
// the shader's entry point with the official entry point that will call it.
//
// The following:
//
//    retType shaderEntryPoint(args...) // shader declared entry point
//    { body }
//
// Becomes
//
//    out retType ret;
//    in iargs<that are input>...;
//    out oargs<that are output> ...;
//
//    void shaderEntryPoint()    // synthesized, but official, entry point
//    {
//        args<that are input> = iargs...;
//        ret = @shaderEntryPoint(args...);
//        oargs = args<that are output>...;
//    }
//    retType @shaderEntryPoint(args...)
//    { body }
//
// The symbol table will still map the original entry point name to the
// the modified function and its new name:
//
//    symbol table:  shaderEntryPoint  ->   @shaderEntryPoint
//
// Returns nullptr if no entry-point tree was built, otherwise, returns
// a subtree that creates the entry point.
//
TIntermNode* HlslParseContext::transformEntryPoint(const TSourceLoc& loc, TFunction& userFunction,
                                                   const TAttributes& attributes)
{
    // Return true if this is a tessellation patch constant function input to a domain shader.
    const auto isDsPcfInput = [this](const TType& type) {
        return language == EShLangTessEvaluation &&
        type.contains([](const TType* t) {
                return t->getQualifier().builtIn == EbvTessLevelOuter ||
                       t->getQualifier().builtIn == EbvTessLevelInner;
            });
    };

    // if we aren't in the entry point, fix the IO as such and exit
    if (! isEntrypointName(userFunction.getName())) {
        remapNonEntryPointIO(userFunction);
        return nullptr;
    }

    entryPointFunction = &userFunction; // needed in finish()

    // Handle entry point attributes
    handleEntryPointAttributes(loc, attributes);

    // entry point logic...

    // Move parameters and return value to shader in/out
    TVariable* entryPointOutput; // gets created in remapEntryPointIO
    TVector<TVariable*> inputs;
    TVector<TVariable*> outputs;
    remapEntryPointIO(userFunction, entryPointOutput, inputs, outputs);

    // Further this return/in/out transform by flattening, splitting, and assigning locations
    const auto makeVariableInOut = [&](TVariable& variable) {
        if (variable.getType().isStruct()) {
            bool arrayed = variable.getType().getQualifier().isArrayedIo(language);
            flatten(variable, false /* don't track linkage here, it will be tracked in assignToInterface() */, arrayed);
        }
        // TODO: flatten arrays too
        // TODO: flatten everything in I/O
        // TODO: replace all split with flatten, make all paths can create flattened I/O, then split code can be removed

        // For clip and cull distance, multiple output variables potentially get merged
        // into one in assignClipCullDistance.  That code in assignClipCullDistance
        // handles the interface logic, so we avoid it here in that case.
        if (!isClipOrCullDistance(variable.getType()))
            assignToInterface(variable);
    };
    if (entryPointOutput != nullptr)
        makeVariableInOut(*entryPointOutput);
    for (auto it = inputs.begin(); it != inputs.end(); ++it)
        if (!isDsPcfInput((*it)->getType()))  // wait until the end for PCF input (see comment below)
            makeVariableInOut(*(*it));
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        makeVariableInOut(*(*it));

    // In the domain shader, PCF input must be at the end of the linkage.  That's because in the
    // hull shader there is no ordering: the output comes from the separate PCF, which does not
    // participate in the argument list.  That is always put at the end of the HS linkage, so the
    // input side of the DS must match.  The argument may be in any position in the DS argument list
    // however, so this ensures the linkage is built in the correct order regardless of argument order.
    if (language == EShLangTessEvaluation) {
        for (auto it = inputs.begin(); it != inputs.end(); ++it)
            if (isDsPcfInput((*it)->getType()))
                makeVariableInOut(*(*it));
    }

    // Add uniform parameters to the $Global uniform block.
    TVector<TVariable*> opaque_uniforms;
    for (int i = 0; i < userFunction.getParamCount(); i++) {
        TType& paramType = *userFunction[i].type;
        TString& paramName = *userFunction[i].name;
        if (paramType.getQualifier().storage == EvqUniform) {
            if (!paramType.containsOpaque()) {
                // Add it to the global uniform block.
                growGlobalUniformBlock(loc, paramType, paramName);
            } else {
                // Declare it as a separate variable.
                TVariable *var = makeInternalVariable(paramName.c_str(), paramType);
                opaque_uniforms.push_back(var);
            }
        }
    }

    // Synthesize the call

    pushScope(); // matches the one in handleFunctionBody()

    // new signature
    TType voidType(EbtVoid);
    TFunction synthEntryPoint(&userFunction.getName(), voidType);
    TIntermAggregate* synthParams = new TIntermAggregate();
    intermediate.setAggregateOperator(synthParams, EOpParameters, voidType, loc);
    intermediate.setEntryPointMangledName(synthEntryPoint.getMangledName().c_str());
    intermediate.incrementEntryPointCount();
    TFunction callee(&userFunction.getName(), voidType); // call based on old name, which is still in the symbol table

    // change original name
    userFunction.addPrefix("@");                         // change the name in the function, but not in the symbol table

    // Copy inputs (shader-in -> calling arg), while building up the call node
    TVector<TVariable*> argVars;
    TIntermAggregate* synthBody = new TIntermAggregate();
    auto inputIt = inputs.begin();
    auto opaqueUniformIt = opaque_uniforms.begin();
    TIntermTyped* callingArgs = nullptr;

    for (int i = 0; i < userFunction.getParamCount(); i++) {
        TParameter& param = userFunction[i];
        argVars.push_back(makeInternalVariable(*param.name, *param.type));
        argVars.back()->getWritableType().getQualifier().makeTemporary();

        // Track the input patch, which is the only non-builtin supported by hull shader PCF.
        if (param.getDeclaredBuiltIn() == EbvInputPatch)
            inputPatch = argVars.back();

        TIntermSymbol* arg = intermediate.addSymbol(*argVars.back());
        handleFunctionArgument(&callee, callingArgs, arg);
        if (param.type->getQualifier().isParamInput()) {
            TIntermTyped* input = intermediate.addSymbol(**inputIt);
            if (input->getType().getQualifier().builtIn == EbvFragCoord && intermediate.getDxPositionW()) {
                // Replace FragCoord W with reciprocal
                auto pos_xyz = handleDotDereference(loc, input, "xyz");
                auto pos_w   = handleDotDereference(loc, input, "w");
                auto one     = intermediate.addConstantUnion(1.0, EbtFloat, loc);
                auto recip_w = intermediate.addBinaryMath(EOpDiv, one, pos_w, loc);
                TIntermAggregate* dst = new TIntermAggregate(EOpConstructVec4);
                dst->getSequence().push_back(pos_xyz);
                dst->getSequence().push_back(recip_w);
                dst->setType(TType(EbtFloat, EvqTemporary, 4));
                dst->setLoc(loc);
                input = dst;
            }
            intermediate.growAggregate(synthBody, handleAssign(loc, EOpAssign, arg, input));
            inputIt++;
        }
        if (param.type->getQualifier().storage == EvqUniform) {
            if (!param.type->containsOpaque()) {
                // Look it up in the $Global uniform block.
                intermediate.growAggregate(synthBody, handleAssign(loc, EOpAssign, arg,
                                                                   handleVariable(loc, param.name)));
            } else {
                intermediate.growAggregate(synthBody, handleAssign(loc, EOpAssign, arg,
                                                                   intermediate.addSymbol(**opaqueUniformIt)));
                ++opaqueUniformIt;
            }
        }
    }

    // Call
    currentCaller = synthEntryPoint.getMangledName();
    TIntermTyped* callReturn = handleFunctionCall(loc, &callee, callingArgs);
    currentCaller = userFunction.getMangledName();

    // Return value
    if (entryPointOutput) {
        TIntermTyped* returnAssign;

        // For hull shaders, the wrapped entry point return value is written to
        // an array element as indexed by invocation ID, which we might have to make up.
        // This is required to match SPIR-V semantics.
        if (language == EShLangTessControl) {
            TIntermSymbol* invocationIdSym = findTessLinkageSymbol(EbvInvocationId);

            // If there is no user declared invocation ID, we must make one.
            if (invocationIdSym == nullptr) {
                TType invocationIdType(EbtUint, EvqIn, 1);
                TString* invocationIdName = NewPoolTString("InvocationId");
                invocationIdType.getQualifier().builtIn = EbvInvocationId;

                TVariable* variable = makeInternalVariable(*invocationIdName, invocationIdType);

                globalQualifierFix(loc, variable->getWritableType().getQualifier());
                trackLinkage(*variable);

                invocationIdSym = intermediate.addSymbol(*variable);
            }

            TIntermTyped* element = intermediate.addIndex(EOpIndexIndirect, intermediate.addSymbol(*entryPointOutput),
                                                          invocationIdSym, loc);

            // Set the type of the array element being dereferenced
            const TType derefElementType(entryPointOutput->getType(), 0);
            element->setType(derefElementType);

            returnAssign = handleAssign(loc, EOpAssign, element, callReturn);
        } else {
            returnAssign = handleAssign(loc, EOpAssign, intermediate.addSymbol(*entryPointOutput), callReturn);
        }
        intermediate.growAggregate(synthBody, returnAssign);
    } else
        intermediate.growAggregate(synthBody, callReturn);

    // Output copies
    auto outputIt = outputs.begin();
    for (int i = 0; i < userFunction.getParamCount(); i++) {
        TParameter& param = userFunction[i];

        // GS outputs are via emit, so we do not copy them here.
        if (param.type->getQualifier().isParamOutput()) {
            if (param.getDeclaredBuiltIn() == EbvGsOutputStream) {
                // GS output stream does not assign outputs here: it's the Append() method
                // which writes to the output, probably multiple times separated by Emit.
                // We merely remember the output to use, here.
                gsStreamOutput = *outputIt;
            } else {
                intermediate.growAggregate(synthBody, handleAssign(loc, EOpAssign,
                                                                   intermediate.addSymbol(**outputIt),
                                                                   intermediate.addSymbol(*argVars[i])));
            }

            outputIt++;
        }
    }

    // Put the pieces together to form a full function subtree
    // for the synthesized entry point.
    synthBody->setOperator(EOpSequence);
    TIntermNode* synthFunctionDef = synthParams;
    handleFunctionBody(loc, synthEntryPoint, synthBody, synthFunctionDef);

    entryPointFunctionBody = synthBody;

    return synthFunctionDef;
}

void HlslParseContext::handleFunctionBody(const TSourceLoc& loc, TFunction& function, TIntermNode* functionBody,
                                          TIntermNode*& node)
{
    node = intermediate.growAggregate(node, functionBody);
    intermediate.setAggregateOperator(node, EOpFunction, function.getType(), loc);
    node->getAsAggregate()->setName(function.getMangledName().c_str());

    popScope();
    if (function.hasImplicitThis())
        popImplicitThis();

    if (function.getType().getBasicType() != EbtVoid && ! functionReturnsValue)
        error(loc, "function does not return a value:", "", function.getName().c_str());
}

// AST I/O is done through shader globals declared in the 'in' or 'out'
// storage class.  An HLSL entry point has a return value, input parameters
// and output parameters.  These need to get remapped to the AST I/O.
void HlslParseContext::remapEntryPointIO(TFunction& function, TVariable*& returnValue,
    TVector<TVariable*>& inputs, TVector<TVariable*>& outputs)
{
    // We might have in input structure type with no decorations that caused it
    // to look like an input type, yet it has (e.g.) interpolation types that
    // must be modified that turn it into an input type.
    // Hence, a missing ioTypeMap for 'input' might need to be synthesized.
    const auto synthesizeEditedInput = [this](TType& type) {
        // True if a type needs to be 'flat'
        const auto needsFlat = [](const TType& type) {
            return type.containsBasicType(EbtInt) ||
                    type.containsBasicType(EbtUint) ||
                    type.containsBasicType(EbtInt64) ||
                    type.containsBasicType(EbtUint64) ||
                    type.containsBasicType(EbtBool) ||
                    type.containsBasicType(EbtDouble);
        };

        if (language == EShLangFragment && needsFlat(type)) {
            if (type.isStruct()) {
                TTypeList* finalList = nullptr;
                auto it = ioTypeMap.find(type.getStruct());
                if (it == ioTypeMap.end() || it->second.input == nullptr) {
                    // Getting here means we have no input struct, but we need one.
                    auto list = new TTypeList;
                    for (auto member = type.getStruct()->begin(); member != type.getStruct()->end(); ++member) {
                        TType* newType = new TType;
                        newType->shallowCopy(*member->type);
                        TTypeLoc typeLoc = { newType, member->loc };
                        list->push_back(typeLoc);
                    }
                    // install the new input type
                    if (it == ioTypeMap.end()) {
                        tIoKinds newLists = { list, nullptr, nullptr };
                        ioTypeMap[type.getStruct()] = newLists;
                    } else
                        it->second.input = list;
                    finalList = list;
                } else
                    finalList = it->second.input;
                // edit for 'flat'
                for (auto member = finalList->begin(); member != finalList->end(); ++member) {
                    if (needsFlat(*member->type)) {
                        member->type->getQualifier().clearInterpolation();
                        member->type->getQualifier().flat = true;
                    }
                }
            } else {
                type.getQualifier().clearInterpolation();
                type.getQualifier().flat = true;
            }
        }
    };

    // Do the actual work to make a type be a shader input or output variable,
    // and clear the original to be non-IO (for use as a normal function parameter/return).
    const auto makeIoVariable = [this](const char* name, TType& type, TStorageQualifier storage) -> TVariable* {
        TVariable* ioVariable = makeInternalVariable(name, type);
        clearUniformInputOutput(type.getQualifier());
        if (type.isStruct()) {
            auto newLists = ioTypeMap.find(ioVariable->getType().getStruct());
            if (newLists != ioTypeMap.end()) {
                if (storage == EvqVaryingIn && newLists->second.input)
                    ioVariable->getWritableType().setStruct(newLists->second.input);
                else if (storage == EvqVaryingOut && newLists->second.output)
                    ioVariable->getWritableType().setStruct(newLists->second.output);
            }
        }
        if (storage == EvqVaryingIn) {
            correctInput(ioVariable->getWritableType().getQualifier());
            if (language == EShLangTessEvaluation)
                if (!ioVariable->getType().isArray())
                    ioVariable->getWritableType().getQualifier().patch = true;
        } else {
            correctOutput(ioVariable->getWritableType().getQualifier());
        }
        ioVariable->getWritableType().getQualifier().storage = storage;

        fixBuiltInIoType(ioVariable->getWritableType());

        return ioVariable;
    };

    // return value is actually a shader-scoped output (out)
    if (function.getType().getBasicType() == EbtVoid) {
        returnValue = nullptr;
    } else {
        if (language == EShLangTessControl) {
            // tessellation evaluation in HLSL writes a per-ctrl-pt value, but it needs to be an
            // array in SPIR-V semantics.  We'll write to it indexed by invocation ID.

            returnValue = makeIoVariable("@entryPointOutput", function.getWritableType(), EvqVaryingOut);

            TType outputType;
            outputType.shallowCopy(function.getType());

            // vertices has necessarily already been set when handling entry point attributes.
            TArraySizes* arraySizes = new TArraySizes;
            arraySizes->addInnerSize(intermediate.getVertices());
            outputType.transferArraySizes(arraySizes);

            clearUniformInputOutput(function.getWritableType().getQualifier());
            returnValue = makeIoVariable("@entryPointOutput", outputType, EvqVaryingOut);
        } else {
            returnValue = makeIoVariable("@entryPointOutput", function.getWritableType(), EvqVaryingOut);
        }
    }

    // parameters are actually shader-scoped inputs and outputs (in or out)
    for (int i = 0; i < function.getParamCount(); i++) {
        TType& paramType = *function[i].type;
        if (paramType.getQualifier().isParamInput()) {
            synthesizeEditedInput(paramType);
            TVariable* argAsGlobal = makeIoVariable(function[i].name->c_str(), paramType, EvqVaryingIn);
            inputs.push_back(argAsGlobal);
        }
        if (paramType.getQualifier().isParamOutput()) {
            TVariable* argAsGlobal = makeIoVariable(function[i].name->c_str(), paramType, EvqVaryingOut);
            outputs.push_back(argAsGlobal);
        }
    }
}

// An HLSL function that looks like an entry point, but is not,
// declares entry point IO built-ins, but these have to be undone.
void HlslParseContext::remapNonEntryPointIO(TFunction& function)
{
    // return value
    if (function.getType().getBasicType() != EbtVoid)
        clearUniformInputOutput(function.getWritableType().getQualifier());

    // parameters.
    // References to structuredbuffer types are left unmodified
    for (int i = 0; i < function.getParamCount(); i++)
        if (!isReference(*function[i].type))
            clearUniformInputOutput(function[i].type->getQualifier());
}

TIntermNode* HlslParseContext::handleDeclare(const TSourceLoc& loc, TIntermTyped* var)
{
    return intermediate.addUnaryNode(EOpDeclare, var, loc, TType(EbtVoid));
}

// Handle function returns, including type conversions to the function return type
// if necessary.
TIntermNode* HlslParseContext::handleReturnValue(const TSourceLoc& loc, TIntermTyped* value)
{
    functionReturnsValue = true;

    if (currentFunctionType->getBasicType() == EbtVoid) {
        error(loc, "void function cannot return a value", "return", "");
        return intermediate.addBranch(EOpReturn, loc);
    } else if (*currentFunctionType != value->getType()) {
        value = intermediate.addConversion(EOpReturn, *currentFunctionType, value);
        if (value && *currentFunctionType != value->getType())
            value = intermediate.addUniShapeConversion(EOpReturn, *currentFunctionType, value);
        if (value == nullptr || *currentFunctionType != value->getType()) {
            error(loc, "type does not match, or is not convertible to, the function's return type", "return", "");
            return value;
        }
    }

    return intermediate.addBranch(EOpReturn, value, loc);
}

void HlslParseContext::handleFunctionArgument(TFunction* function,
                                              TIntermTyped*& arguments, TIntermTyped* newArg)
{
    TParameter param = { nullptr, new TType, nullptr };
    param.type->shallowCopy(newArg->getType());

    function->addParameter(param);
    if (arguments)
        arguments = intermediate.growAggregate(arguments, newArg);
    else
        arguments = newArg;
}

// FragCoord may require special loading: we can optionally reciprocate W.
TIntermTyped* HlslParseContext::assignFromFragCoord(const TSourceLoc& loc, TOperator op,
                                                    TIntermTyped* left, TIntermTyped* right)
{
    // If we are not asked for reciprocal W, use a plain old assign.
    if (!intermediate.getDxPositionW())
        return intermediate.addAssign(op, left, right, loc);

    // If we get here, we should reciprocate W.
    TIntermAggregate* assignList = nullptr;

    // If this is a complex rvalue, we don't want to dereference it many times.  Create a temporary.
    TVariable* rhsTempVar = nullptr;
    rhsTempVar = makeInternalVariable("@fragcoord", right->getType());
    rhsTempVar->getWritableType().getQualifier().makeTemporary();

    {
        TIntermTyped* rhsTempSym = intermediate.addSymbol(*rhsTempVar, loc);
        assignList = intermediate.growAggregate(assignList,
            intermediate.addAssign(EOpAssign, rhsTempSym, right, loc), loc);
    }

    // tmp.w = 1.0 / tmp.w
    {
        const int W = 3;

        TIntermTyped* tempSymL = intermediate.addSymbol(*rhsTempVar, loc);
        TIntermTyped* tempSymR = intermediate.addSymbol(*rhsTempVar, loc);
        TIntermTyped* index = intermediate.addConstantUnion(W, loc);

        TIntermTyped* lhsElement = intermediate.addIndex(EOpIndexDirect, tempSymL, index, loc);
        TIntermTyped* rhsElement = intermediate.addIndex(EOpIndexDirect, tempSymR, index, loc);

        const TType derefType(right->getType(), 0);

        lhsElement->setType(derefType);
        rhsElement->setType(derefType);

        auto one     = intermediate.addConstantUnion(1.0, EbtFloat, loc);
        auto recip_w = intermediate.addBinaryMath(EOpDiv, one, rhsElement, loc);

        assignList = intermediate.growAggregate(assignList, intermediate.addAssign(EOpAssign, lhsElement, recip_w, loc));
    }

    // Assign the rhs temp (now with W reciprocal) to the final output
    {
        TIntermTyped* rhsTempSym = intermediate.addSymbol(*rhsTempVar, loc);
        assignList = intermediate.growAggregate(assignList, intermediate.addAssign(op, left, rhsTempSym, loc));
    }

    assert(assignList != nullptr);
    assignList->setOperator(EOpSequence);

    return assignList;
}

// Position may require special handling: we can optionally invert Y.
// See: https://github.com/KhronosGroup/glslang/issues/1173
//      https://github.com/KhronosGroup/glslang/issues/494
TIntermTyped* HlslParseContext::assignPosition(const TSourceLoc& loc, TOperator op,
                                               TIntermTyped* left, TIntermTyped* right)
{
    // If we are not asked for Y inversion, use a plain old assign.
    if (!intermediate.getInvertY())
        return intermediate.addAssign(op, left, right, loc);

    // If we get here, we should invert Y.
    TIntermAggregate* assignList = nullptr;

    // If this is a complex rvalue, we don't want to dereference it many times.  Create a temporary.
    TVariable* rhsTempVar = nullptr;
    rhsTempVar = makeInternalVariable("@position", right->getType());
    rhsTempVar->getWritableType().getQualifier().makeTemporary();

    {
        TIntermTyped* rhsTempSym = intermediate.addSymbol(*rhsTempVar, loc);
        assignList = intermediate.growAggregate(assignList,
                                                intermediate.addAssign(EOpAssign, rhsTempSym, right, loc), loc);
    }

    // pos.y = -pos.y
    {
        const int Y = 1;

        TIntermTyped* tempSymL = intermediate.addSymbol(*rhsTempVar, loc);
        TIntermTyped* tempSymR = intermediate.addSymbol(*rhsTempVar, loc);
        TIntermTyped* index = intermediate.addConstantUnion(Y, loc);

        TIntermTyped* lhsElement = intermediate.addIndex(EOpIndexDirect, tempSymL, index, loc);
        TIntermTyped* rhsElement = intermediate.addIndex(EOpIndexDirect, tempSymR, index, loc);

        const TType derefType(right->getType(), 0);

        lhsElement->setType(derefType);
        rhsElement->setType(derefType);

        TIntermTyped* yNeg = intermediate.addUnaryMath(EOpNegative, rhsElement, loc);

        assignList = intermediate.growAggregate(assignList, intermediate.addAssign(EOpAssign, lhsElement, yNeg, loc));
    }

    // Assign the rhs temp (now with Y inversion) to the final output
    {
        TIntermTyped* rhsTempSym = intermediate.addSymbol(*rhsTempVar, loc);
        assignList = intermediate.growAggregate(assignList, intermediate.addAssign(op, left, rhsTempSym, loc));
    }

    assert(assignList != nullptr);
    assignList->setOperator(EOpSequence);

    return assignList;
}

// Clip and cull distance require special handling due to a semantic mismatch.  In HLSL,
// these can be float scalar, float vector, or arrays of float scalar or float vector.
// In SPIR-V, they are arrays of scalar floats in all cases.  We must copy individual components
// (e.g, both x and y components of a float2) out into the destination float array.
//
// The values are assigned to sequential members of the output array.  The inner dimension
// is vector components.  The outer dimension is array elements.
TIntermAggregate* HlslParseContext::assignClipCullDistance(const TSourceLoc& loc, TOperator op, int semanticId,
                                                           TIntermTyped* left, TIntermTyped* right)
{
    switch (language) {
    case EShLangFragment:
    case EShLangVertex:
    case EShLangGeometry:
        break;
    default:
        error(loc, "unimplemented: clip/cull not currently implemented for this stage", "", "");
        return nullptr;
    }

    TVariable** clipCullVar = nullptr;

    // Figure out if we are assigning to, or from, clip or cull distance.
    const bool isOutput = isClipOrCullDistance(left->getType());

    // This is the rvalue or lvalue holding the clip or cull distance.
    TIntermTyped* clipCullNode = isOutput ? left : right;
    // This is the value going into or out of the clip or cull distance.
    TIntermTyped* internalNode = isOutput ? right : left;

    const TBuiltInVariable builtInType = clipCullNode->getQualifier().builtIn;

    decltype(clipSemanticNSizeIn)* semanticNSize = nullptr;

    // Refer to either the clip or the cull distance, depending on semantic.
    switch (builtInType) {
    case EbvClipDistance:
        clipCullVar = isOutput ? &clipDistanceOutput : &clipDistanceInput;
        semanticNSize = isOutput ? &clipSemanticNSizeOut : &clipSemanticNSizeIn;
        break;
    case EbvCullDistance:
        clipCullVar = isOutput ? &cullDistanceOutput : &cullDistanceInput;
        semanticNSize = isOutput ? &cullSemanticNSizeOut : &cullSemanticNSizeIn;
        break;

    // called invalidly: we expected a clip or a cull distance.
    // static compile time problem: should not happen.
    default: assert(0); return nullptr;
    }

    // This is the offset in the destination array of a given semantic's data
    std::array<int, maxClipCullRegs> semanticOffset;

    // Calculate offset of variable of semantic N in destination array
    int arrayLoc = 0;
    int vecItems = 0;

    for (int x = 0; x < maxClipCullRegs; ++x) {
        // See if we overflowed the vec4 packing
        if ((vecItems + (*semanticNSize)[x]) > 4) {
            arrayLoc = (arrayLoc + 3) & (~0x3); // round up to next multiple of 4
            vecItems = 0;
        }

        semanticOffset[x] = arrayLoc;
        vecItems += (*semanticNSize)[x];
        arrayLoc += (*semanticNSize)[x];
    }


    // It can have up to 2 array dimensions (in the case of geometry shader inputs)
    const TArraySizes* const internalArraySizes = internalNode->getType().getArraySizes();
    const int internalArrayDims = internalNode->getType().isArray() ? internalArraySizes->getNumDims() : 0;
    // vector sizes:
    const int internalVectorSize = internalNode->getType().getVectorSize();
    // array sizes, or 1 if it's not an array:
    const int internalInnerArraySize = (internalArrayDims > 0 ? internalArraySizes->getDimSize(internalArrayDims-1) : 1);
    const int internalOuterArraySize = (internalArrayDims > 1 ? internalArraySizes->getDimSize(0) : 1);

    // The created type may be an array of arrays, e.g, for geometry shader inputs.
    const bool isImplicitlyArrayed = (language == EShLangGeometry && !isOutput);

    // If we haven't created the output already, create it now.
    if (*clipCullVar == nullptr) {
        // ClipDistance and CullDistance are handled specially in the entry point input/output copy
        // algorithm, because they may need to be unpacked from components of vectors (or a scalar)
        // into a float array, or vice versa.  Here, we make the array the right size and type,
        // which depends on the incoming data, which has several potential dimensions:
        //    * Semantic ID
        //    * vector size
        //    * array size
        // Of those, semantic ID and array size cannot appear simultaneously.
        //
        // Also to note: for implicitly arrayed forms (e.g, geometry shader inputs), we need to create two
        // array dimensions.  The shader's declaration may have one or two array dimensions.  One is always
        // the geometry's dimension.

        const bool useInnerSize = internalArrayDims > 1 || !isImplicitlyArrayed;

        const int requiredInnerArraySize = arrayLoc * (useInnerSize ? internalInnerArraySize : 1);
        const int requiredOuterArraySize = (internalArrayDims > 0) ? internalArraySizes->getDimSize(0) : 1;

        TType clipCullType(EbtFloat, clipCullNode->getType().getQualifier().storage, 1);
        clipCullType.getQualifier() = clipCullNode->getType().getQualifier();

        // Create required array dimension
        TArraySizes* arraySizes = new TArraySizes;
        if (isImplicitlyArrayed)
            arraySizes->addInnerSize(requiredOuterArraySize);
        arraySizes->addInnerSize(requiredInnerArraySize);
        clipCullType.transferArraySizes(arraySizes);

        // Obtain symbol name: we'll use that for the symbol we introduce.
        TIntermSymbol* sym = clipCullNode->getAsSymbolNode();
        assert(sym != nullptr);

        // We are moving the semantic ID from the layout location, so it is no longer needed or
        // desired there.
        clipCullType.getQualifier().layoutLocation = TQualifier::layoutLocationEnd;

        // Create variable and track its linkage
        *clipCullVar = makeInternalVariable(sym->getName().c_str(), clipCullType);

        trackLinkage(**clipCullVar);
    }

    // Create symbol for the clip or cull variable.
    TIntermSymbol* clipCullSym = intermediate.addSymbol(**clipCullVar);

    // vector sizes:
    const int clipCullVectorSize = clipCullSym->getType().getVectorSize();

    // array sizes, or 1 if it's not an array:
    const TArraySizes* const clipCullArraySizes = clipCullSym->getType().getArraySizes();
    const int clipCullOuterArraySize = isImplicitlyArrayed ? clipCullArraySizes->getDimSize(0) : 1;
    const int clipCullInnerArraySize = clipCullArraySizes->getDimSize(isImplicitlyArrayed ? 1 : 0);

    // clipCullSym has got to be an array of scalar floats, per SPIR-V semantics.
    // fixBuiltInIoType() should have handled that upstream.
    assert(clipCullSym->getType().isArray());
    assert(clipCullSym->getType().getVectorSize() == 1);
    assert(clipCullSym->getType().getBasicType() == EbtFloat);

    // We may be creating multiple sub-assignments.  This is an aggregate to hold them.
    // TODO: it would be possible to be clever sometimes and avoid the sequence node if not needed.
    TIntermAggregate* assignList = nullptr;

    // Holds individual component assignments as we make them.
    TIntermTyped* clipCullAssign = nullptr;

    // If the types are homomorphic, use a simple assign.  No need to mess about with
    // individual components.
    if (clipCullSym->getType().isArray() == internalNode->getType().isArray() &&
        clipCullInnerArraySize == internalInnerArraySize &&
        clipCullOuterArraySize == internalOuterArraySize &&
        clipCullVectorSize == internalVectorSize) {

        if (isOutput)
            clipCullAssign = intermediate.addAssign(op, clipCullSym, internalNode, loc);
        else
            clipCullAssign = intermediate.addAssign(op, internalNode, clipCullSym, loc);

        assignList = intermediate.growAggregate(assignList, clipCullAssign);
        assignList->setOperator(EOpSequence);

        return assignList;
    }

    // We are going to copy each component of the internal (per array element if indicated) to sequential
    // array elements of the clipCullSym.  This tracks the lhs element we're writing to as we go along.
    // We may be starting in the middle - e.g, for a non-zero semantic ID calculated above.
    int clipCullInnerArrayPos = semanticOffset[semanticId];
    int clipCullOuterArrayPos = 0;

    // Lambda to add an index to a node, set the type of the result, and return the new node.
    const auto addIndex = [this, &loc](TIntermTyped* node, int pos) -> TIntermTyped* {
        const TType derefType(node->getType(), 0);
        node = intermediate.addIndex(EOpIndexDirect, node, intermediate.addConstantUnion(pos, loc), loc);
        node->setType(derefType);
        return node;
    };

    // Loop through every component of every element of the internal, and copy to or from the matching external.
    for (int internalOuterArrayPos = 0; internalOuterArrayPos < internalOuterArraySize; ++internalOuterArrayPos) {
        for (int internalInnerArrayPos = 0; internalInnerArrayPos < internalInnerArraySize; ++internalInnerArrayPos) {
            for (int internalComponent = 0; internalComponent < internalVectorSize; ++internalComponent) {
                // clip/cull array member to read from / write to:
                TIntermTyped* clipCullMember = clipCullSym;

                // If implicitly arrayed, there is an outer array dimension involved
                if (isImplicitlyArrayed)
                    clipCullMember = addIndex(clipCullMember, clipCullOuterArrayPos);

                // Index into proper array position for clip cull member
                clipCullMember = addIndex(clipCullMember, clipCullInnerArrayPos++);

                // if needed, start over with next outer array slice.
                if (isImplicitlyArrayed && clipCullInnerArrayPos >= clipCullInnerArraySize) {
                    clipCullInnerArrayPos = semanticOffset[semanticId];
                    ++clipCullOuterArrayPos;
                }

                // internal member to read from / write to:
                TIntermTyped* internalMember = internalNode;

                // If internal node has outer array dimension, index appropriately.
                if (internalArrayDims > 1)
                    internalMember = addIndex(internalMember, internalOuterArrayPos);

                // If internal node has inner array dimension, index appropriately.
                if (internalArrayDims > 0)
                    internalMember = addIndex(internalMember, internalInnerArrayPos);

                // If internal node is a vector, extract the component of interest.
                if (internalNode->getType().isVector())
                    internalMember = addIndex(internalMember, internalComponent);

                // Create an assignment: output from internal to clip cull, or input from clip cull to internal.
                if (isOutput)
                    clipCullAssign = intermediate.addAssign(op, clipCullMember, internalMember, loc);
                else
                    clipCullAssign = intermediate.addAssign(op, internalMember, clipCullMember, loc);

                // Track assignment in the sequence.
                assignList = intermediate.growAggregate(assignList, clipCullAssign);
            }
        }
    }

    assert(assignList != nullptr);
    assignList->setOperator(EOpSequence);

    return assignList;
}

// Some simple source assignments need to be flattened to a sequence
// of AST assignments. Catch these and flatten, otherwise, pass through
// to intermediate.addAssign().
//
// Also, assignment to matrix swizzles requires multiple component assignments,
// intercept those as well.
TIntermTyped* HlslParseContext::handleAssign(const TSourceLoc& loc, TOperator op, TIntermTyped* left,
                                             TIntermTyped* right)
{
    if (left == nullptr || right == nullptr)
        return nullptr;

    // writing to opaques will require fixing transforms
    if (left->getType().containsOpaque())
        intermediate.setNeedsLegalization();

    if (left->getAsOperator() && left->getAsOperator()->getOp() == EOpMatrixSwizzle)
        return handleAssignToMatrixSwizzle(loc, op, left, right);

    // Return true if the given node is an index operation into a split variable.
    const auto indexesSplit = [this](const TIntermTyped* node) -> bool {
        const TIntermBinary* binaryNode = node->getAsBinaryNode();

        if (binaryNode == nullptr)
            return false;

        return (binaryNode->getOp() == EOpIndexDirect || binaryNode->getOp() == EOpIndexIndirect) &&
               wasSplit(binaryNode->getLeft());
    };

    // Return symbol if node is symbol or index ref
    const auto getSymbol = [](const TIntermTyped* node) -> const TIntermSymbol* {
        const TIntermSymbol* symbolNode = node->getAsSymbolNode();
        if (symbolNode != nullptr)
            return symbolNode;

        const TIntermBinary* binaryNode = node->getAsBinaryNode();
        if (binaryNode != nullptr && (binaryNode->getOp() == EOpIndexDirect || binaryNode->getOp() == EOpIndexIndirect))
            return binaryNode->getLeft()->getAsSymbolNode();

        return nullptr;
    };

    // Return true if this stage assigns clip position with potentially inverted Y
    const auto assignsClipPos = [this](const TIntermTyped* node) -> bool {
        return node->getType().getQualifier().builtIn == EbvPosition &&
               (language == EShLangVertex || language == EShLangGeometry || language == EShLangTessEvaluation);
    };

    const TIntermSymbol* leftSymbol = getSymbol(left);
    const TIntermSymbol* rightSymbol = getSymbol(right);

    const bool isSplitLeft    = wasSplit(left) || indexesSplit(left);
    const bool isSplitRight   = wasSplit(right) || indexesSplit(right);

    const bool isFlattenLeft  = wasFlattened(leftSymbol);
    const bool isFlattenRight = wasFlattened(rightSymbol);

    // OK to do a single assign if neither side is split or flattened.  Otherwise,
    // fall through to a member-wise copy.
    if (!isFlattenLeft && !isFlattenRight && !isSplitLeft && !isSplitRight) {
        // Clip and cull distance requires more processing.  See comment above assignClipCullDistance.
        if (isClipOrCullDistance(left->getType()) || isClipOrCullDistance(right->getType())) {
            const bool isOutput = isClipOrCullDistance(left->getType());

            const int semanticId = (isOutput ? left : right)->getType().getQualifier().layoutLocation;
            return assignClipCullDistance(loc, op, semanticId, left, right);
        } else if (assignsClipPos(left)) {
            // Position can require special handling: see comment above assignPosition
            return assignPosition(loc, op, left, right);
        } else if (left->getQualifier().builtIn == EbvSampleMask) {
            // Certain builtins are required to be arrayed outputs in SPIR-V, but may internally be scalars
            // in the shader.  Copy the scalar RHS into the LHS array element zero, if that happens.
            if (left->isArray() && !right->isArray()) {
                const TType derefType(left->getType(), 0);
                left = intermediate.addIndex(EOpIndexDirect, left, intermediate.addConstantUnion(0, loc), loc);
                left->setType(derefType);
                // Fall through to add assign.
            }
        }

        return intermediate.addAssign(op, left, right, loc);
    }

    TIntermAggregate* assignList = nullptr;
    const TVector<TVariable*>* leftVariables = nullptr;
    const TVector<TVariable*>* rightVariables = nullptr;

    // A temporary to store the right node's value, so we don't keep indirecting into it
    // if it's not a simple symbol.
    TVariable* rhsTempVar = nullptr;

    // If the RHS is a simple symbol node, we'll copy it for each member.
    TIntermSymbol* cloneSymNode = nullptr;

    int memberCount = 0;

    // Track how many items there are to copy.
    if (left->getType().isStruct())
        memberCount = (int)left->getType().getStruct()->size();
    if (left->getType().isArray())
        memberCount = left->getType().getCumulativeArraySize();

    if (isFlattenLeft)
        leftVariables = &flattenMap.find(leftSymbol->getId())->second.members;

    if (isFlattenRight) {
        rightVariables = &flattenMap.find(rightSymbol->getId())->second.members;
    } else {
        // The RHS is not flattened.  There are several cases:
        // 1. 1 item to copy:  Use the RHS directly.
        // 2. >1 item, simple symbol RHS: we'll create a new TIntermSymbol node for each, but no assign to temp.
        // 3. >1 item, complex RHS: assign it to a new temp variable, and create a TIntermSymbol for each member.

        if (memberCount <= 1) {
            // case 1: we'll use the symbol directly below.  Nothing to do.
        } else {
            if (right->getAsSymbolNode() != nullptr) {
                // case 2: we'll copy the symbol per iteration below.
                cloneSymNode = right->getAsSymbolNode();
            } else {
                // case 3: assign to a temp, and indirect into that.
                rhsTempVar = makeInternalVariable("flattenTemp", right->getType());
                rhsTempVar->getWritableType().getQualifier().makeTemporary();
                TIntermTyped* noFlattenRHS = intermediate.addSymbol(*rhsTempVar, loc);

                // Add this to the aggregate being built.
                assignList = intermediate.growAggregate(assignList,
                                                        intermediate.addAssign(op, noFlattenRHS, right, loc), loc);
            }
        }
    }

    // When dealing with split arrayed structures of built-ins, the arrayness is moved to the extracted built-in
    // variables, which is awkward when copying between split and unsplit structures.  This variable tracks
    // array indirections so they can be percolated from outer structs to inner variables.
    std::vector <int> arrayElement;

    TStorageQualifier leftStorage = left->getType().getQualifier().storage;
    TStorageQualifier rightStorage = right->getType().getQualifier().storage;

    int leftOffsetStart = findSubtreeOffset(*left);
    int rightOffsetStart = findSubtreeOffset(*right);
    int leftOffset = leftOffsetStart;
    int rightOffset = rightOffsetStart;

    const auto getMember = [&](bool isLeft, const TType& type, int member, TIntermTyped* splitNode, int splitMember,
                               bool flattened)
                           -> TIntermTyped * {
        const bool split     = isLeft ? isSplitLeft   : isSplitRight;

        TIntermTyped* subTree;
        const TType derefType(type, member);
        const TVariable* builtInVar = nullptr;
        if ((flattened || split) && derefType.isBuiltIn()) {
            auto splitPair = splitBuiltIns.find(HlslParseContext::tInterstageIoData(
                                                   derefType.getQualifier().builtIn,
                                                   isLeft ? leftStorage : rightStorage));
            if (splitPair != splitBuiltIns.end())
                builtInVar = splitPair->second;
        }
        if (builtInVar != nullptr) {
            // copy from interstage IO built-in if needed
            subTree = intermediate.addSymbol(*builtInVar);

            if (subTree->getType().isArray()) {
                // Arrayness of builtIn symbols isn't handled by the normal recursion:
                // it's been extracted and moved to the built-in.
                if (!arrayElement.empty()) {
                    const TType splitDerefType(subTree->getType(), arrayElement.back());
                    subTree = intermediate.addIndex(EOpIndexDirect, subTree,
                                                    intermediate.addConstantUnion(arrayElement.back(), loc), loc);
                    subTree->setType(splitDerefType);
                } else if (splitNode->getAsOperator() != nullptr && (splitNode->getAsOperator()->getOp() == EOpIndexIndirect)) {
                    // This might also be a stage with arrayed outputs, in which case there's an index
                    // operation we should transfer to the output builtin.

                    const TType splitDerefType(subTree->getType(), 0);
                    subTree = intermediate.addIndex(splitNode->getAsOperator()->getOp(), subTree,
                                                    splitNode->getAsBinaryNode()->getRight(), loc);
                    subTree->setType(splitDerefType);
                }
            }
        } else if (flattened && !shouldFlatten(derefType, isLeft ? leftStorage : rightStorage, false)) {
            if (isLeft) {
                // offset will cycle through variables for arrayed io
                if (leftOffset >= static_cast<int>(leftVariables->size()))
                    leftOffset = leftOffsetStart;
                subTree = intermediate.addSymbol(*(*leftVariables)[leftOffset++]);
            } else {
                // offset will cycle through variables for arrayed io
                if (rightOffset >= static_cast<int>(rightVariables->size()))
                    rightOffset = rightOffsetStart;
                subTree = intermediate.addSymbol(*(*rightVariables)[rightOffset++]);
            }

            // arrayed io
            if (subTree->getType().isArray()) {
                if (!arrayElement.empty()) {
                    const TType derefType(subTree->getType(), arrayElement.front());
                    subTree = intermediate.addIndex(EOpIndexDirect, subTree,
                                                    intermediate.addConstantUnion(arrayElement.front(), loc), loc);
                    subTree->setType(derefType);
                } else {
                    // There's an index operation we should transfer to the output builtin.
                    assert(splitNode->getAsOperator() != nullptr &&
                           splitNode->getAsOperator()->getOp() == EOpIndexIndirect);
                    const TType splitDerefType(subTree->getType(), 0);
                    subTree = intermediate.addIndex(splitNode->getAsOperator()->getOp(), subTree,
                                                    splitNode->getAsBinaryNode()->getRight(), loc);
                    subTree->setType(splitDerefType);
                }
            }
        } else {
            // Index operator if it's an aggregate, else EOpNull
            const TOperator accessOp = type.isArray()  ? EOpIndexDirect
                                     : type.isStruct() ? EOpIndexDirectStruct
                                     : EOpNull;
            if (accessOp == EOpNull) {
                subTree = splitNode;
            } else {
                subTree = intermediate.addIndex(accessOp, splitNode, intermediate.addConstantUnion(splitMember, loc),
                                                loc);
                const TType splitDerefType(splitNode->getType(), splitMember);
                subTree->setType(splitDerefType);
            }
        }

        return subTree;
    };

    // Use the proper RHS node: a new symbol from a TVariable, copy
    // of an TIntermSymbol node, or sometimes the right node directly.
    right = rhsTempVar != nullptr   ? intermediate.addSymbol(*rhsTempVar, loc) :
            cloneSymNode != nullptr ? intermediate.addSymbol(*cloneSymNode) :
            right;

    // Cannot use auto here, because this is recursive, and auto can't work out the type without seeing the
    // whole thing.  So, we'll resort to an explicit type via std::function.
    const std::function<void(TIntermTyped* left, TIntermTyped* right, TIntermTyped* splitLeft, TIntermTyped* splitRight,
                             bool topLevel)>
    traverse = [&](TIntermTyped* left, TIntermTyped* right, TIntermTyped* splitLeft, TIntermTyped* splitRight,
                   bool topLevel) -> void {
        // If we get here, we are assigning to or from a whole array or struct that must be
        // flattened, so have to do member-by-member assignment:

        bool shouldFlattenSubsetLeft = isFlattenLeft && shouldFlatten(left->getType(), leftStorage, topLevel);
        bool shouldFlattenSubsetRight = isFlattenRight && shouldFlatten(right->getType(), rightStorage, topLevel);

        if ((left->getType().isArray() || right->getType().isArray()) &&
              (shouldFlattenSubsetLeft  || isSplitLeft ||
               shouldFlattenSubsetRight || isSplitRight)) {
            const int elementsL = left->getType().isArray()  ? left->getType().getOuterArraySize()  : 1;
            const int elementsR = right->getType().isArray() ? right->getType().getOuterArraySize() : 1;

            // The arrays might not be the same size,
            // e.g., if the size has been forced for EbvTessLevelInner/Outer.
            const int elementsToCopy = std::min(elementsL, elementsR);

            // array case
            for (int element = 0; element < elementsToCopy; ++element) {
                arrayElement.push_back(element);

                // Add a new AST symbol node if we have a temp variable holding a complex RHS.
                TIntermTyped* subLeft  = getMember(true,  left->getType(),  element, left, element,
                                                   shouldFlattenSubsetLeft);
                TIntermTyped* subRight = getMember(false, right->getType(), element, right, element,
                                                   shouldFlattenSubsetRight);

                TIntermTyped* subSplitLeft =  isSplitLeft  ? getMember(true,  left->getType(),  element, splitLeft,
                                                                       element, shouldFlattenSubsetLeft)
                                                           : subLeft;
                TIntermTyped* subSplitRight = isSplitRight ? getMember(false, right->getType(), element, splitRight,
                                                                       element, shouldFlattenSubsetRight)
                                                           : subRight;

                traverse(subLeft, subRight, subSplitLeft, subSplitRight, false);

                arrayElement.pop_back();
            }
        } else if (left->getType().isStruct() && (shouldFlattenSubsetLeft  || isSplitLeft ||
                                                  shouldFlattenSubsetRight || isSplitRight)) {
            // struct case
            const auto& membersL = *left->getType().getStruct();
            const auto& membersR = *right->getType().getStruct();

            // These track the members in the split structures corresponding to the same in the unsplit structures,
            // which we traverse in parallel.
            int memberL = 0;
            int memberR = 0;

            // Handle empty structure assignment
            if (int(membersL.size()) == 0 && int(membersR.size()) == 0)
                assignList = intermediate.growAggregate(assignList, intermediate.addAssign(op, left, right, loc), loc);

            for (int member = 0; member < int(membersL.size()); ++member) {
                const TType& typeL = *membersL[member].type;
                const TType& typeR = *membersR[member].type;

                TIntermTyped* subLeft  = getMember(true,  left->getType(), member, left, member,
                                                   shouldFlattenSubsetLeft);
                TIntermTyped* subRight = getMember(false, right->getType(), member, right, member,
                                                   shouldFlattenSubsetRight);

                // If there is no splitting, use the same values to avoid inefficiency.
                TIntermTyped* subSplitLeft =  isSplitLeft  ? getMember(true,  left->getType(),  member, splitLeft,
                                                                       memberL, shouldFlattenSubsetLeft)
                                                           : subLeft;
                TIntermTyped* subSplitRight = isSplitRight ? getMember(false, right->getType(), member, splitRight,
                                                                       memberR, shouldFlattenSubsetRight)
                                                           : subRight;

                if (isClipOrCullDistance(subSplitLeft->getType()) || isClipOrCullDistance(subSplitRight->getType())) {
                    // Clip and cull distance built-in assignment is complex in its own right, and is handled in
                    // a separate function dedicated to that task.  See comment above assignClipCullDistance;

                    const bool isOutput = isClipOrCullDistance(subSplitLeft->getType());

                    // Since all clip/cull semantics boil down to the same built-in type, we need to get the
                    // semantic ID from the dereferenced type's layout location, to avoid an N-1 mapping.
                    const TType derefType((isOutput ? left : right)->getType(), member);
                    const int semanticId = derefType.getQualifier().layoutLocation;

                    TIntermAggregate* clipCullAssign = assignClipCullDistance(loc, op, semanticId,
                                                                              subSplitLeft, subSplitRight);

                    assignList = intermediate.growAggregate(assignList, clipCullAssign, loc);
                } else if (subSplitRight->getType().getQualifier().builtIn == EbvFragCoord) {
                    // FragCoord can require special handling: see comment above assignFromFragCoord
                    TIntermTyped* fragCoordAssign = assignFromFragCoord(loc, op, subSplitLeft, subSplitRight);
                    assignList = intermediate.growAggregate(assignList, fragCoordAssign, loc);
                } else if (assignsClipPos(subSplitLeft)) {
                    // Position can require special handling: see comment above assignPosition
                    TIntermTyped* positionAssign = assignPosition(loc, op, subSplitLeft, subSplitRight);
                    assignList = intermediate.growAggregate(assignList, positionAssign, loc);
                } else if (!shouldFlattenSubsetLeft && !shouldFlattenSubsetRight &&
                           !typeL.containsBuiltIn() && !typeR.containsBuiltIn()) {
                    // If this is the final flattening (no nested types below to flatten)
                    // we'll copy the member, else recurse into the type hierarchy.
                    // However, if splitting the struct, that means we can copy a whole
                    // subtree here IFF it does not itself contain any interstage built-in
                    // IO variables, so we only have to recurse into it if there's something
                    // for splitting to do.  That can save a lot of AST verbosity for
                    // a bunch of memberwise copies.

                    assignList = intermediate.growAggregate(assignList,
                                                            intermediate.addAssign(op, subSplitLeft, subSplitRight, loc),
                                                            loc);
                } else {
                    traverse(subLeft, subRight, subSplitLeft, subSplitRight, false);
                }

                memberL += (typeL.isBuiltIn() ? 0 : 1);
                memberR += (typeR.isBuiltIn() ? 0 : 1);
            }
        } else {
            // Member copy
            assignList = intermediate.growAggregate(assignList, intermediate.addAssign(op, left, right, loc), loc);
        }

    };

    TIntermTyped* splitLeft  = left;
    TIntermTyped* splitRight = right;

    // If either left or right was a split structure, we must read or write it, but still have to
    // parallel-recurse through the unsplit structure to identify the built-in IO vars.
    // The left can be either a symbol, or an index into a symbol (e.g, array reference)
    if (isSplitLeft) {
        if (indexesSplit(left)) {
            // Index case: Refer to the indexed symbol, if the left is an index operator.
            const TIntermSymbol* symNode = left->getAsBinaryNode()->getLeft()->getAsSymbolNode();

            TIntermTyped* splitLeftNonIo = intermediate.addSymbol(*getSplitNonIoVar(symNode->getId()), loc);

            splitLeft = intermediate.addIndex(left->getAsBinaryNode()->getOp(), splitLeftNonIo,
                                              left->getAsBinaryNode()->getRight(), loc);

            const TType derefType(splitLeftNonIo->getType(), 0);
            splitLeft->setType(derefType);
        } else {
            // Symbol case: otherwise, if not indexed, we have the symbol directly.
            const TIntermSymbol* symNode = left->getAsSymbolNode();
            splitLeft = intermediate.addSymbol(*getSplitNonIoVar(symNode->getId()), loc);
        }
    }

    if (isSplitRight)
        splitRight = intermediate.addSymbol(*getSplitNonIoVar(right->getAsSymbolNode()->getId()), loc);

    // This makes the whole assignment, recursing through subtypes as needed.
    traverse(left, right, splitLeft, splitRight, true);

    assert(assignList != nullptr);
    assignList->setOperator(EOpSequence);

    return assignList;
}

// An assignment to matrix swizzle must be decomposed into individual assignments.
// These must be selected component-wise from the RHS and stored component-wise
// into the LHS.
TIntermTyped* HlslParseContext::handleAssignToMatrixSwizzle(const TSourceLoc& loc, TOperator op, TIntermTyped* left,
                                                            TIntermTyped* right)
{
    assert(left->getAsOperator() && left->getAsOperator()->getOp() == EOpMatrixSwizzle);

    if (op != EOpAssign)
        error(loc, "only simple assignment to non-simple matrix swizzle is supported", "assign", "");

    // isolate the matrix and swizzle nodes
    TIntermTyped* matrix = left->getAsBinaryNode()->getLeft()->getAsTyped();
    const TIntermSequence& swizzle = left->getAsBinaryNode()->getRight()->getAsAggregate()->getSequence();

    // if the RHS isn't already a simple vector, let's store into one
    TIntermSymbol* vector = right->getAsSymbolNode();
    TIntermTyped* vectorAssign = nullptr;
    if (vector == nullptr) {
        // create a new intermediate vector variable to assign to
        TType vectorType(matrix->getBasicType(), EvqTemporary, matrix->getQualifier().precision, (int)swizzle.size()/2);
        vector = intermediate.addSymbol(*makeInternalVariable("intermVec", vectorType), loc);

        // assign the right to the new vector
        vectorAssign = handleAssign(loc, op, vector, right);
    }

    // Assign the vector components to the matrix components.
    // Store this as a sequence, so a single aggregate node represents this
    // entire operation.
    TIntermAggregate* result = intermediate.makeAggregate(vectorAssign);
    TType columnType(matrix->getType(), 0);
    TType componentType(columnType, 0);
    TType indexType(EbtInt);
    for (int i = 0; i < (int)swizzle.size(); i += 2) {
        // the right component, single index into the RHS vector
        TIntermTyped* rightComp = intermediate.addIndex(EOpIndexDirect, vector,
                                    intermediate.addConstantUnion(i/2, loc), loc);

        // the left component, double index into the LHS matrix
        TIntermTyped* leftComp = intermediate.addIndex(EOpIndexDirect, matrix,
                                    intermediate.addConstantUnion(swizzle[i]->getAsConstantUnion()->getConstArray(),
                                                                  indexType, loc),
                                    loc);
        leftComp->setType(columnType);
        leftComp = intermediate.addIndex(EOpIndexDirect, leftComp,
                                    intermediate.addConstantUnion(swizzle[i+1]->getAsConstantUnion()->getConstArray(),
                                                                  indexType, loc),
                                    loc);
        leftComp->setType(componentType);

        // Add the assignment to the aggregate
        result = intermediate.growAggregate(result, intermediate.addAssign(op, leftComp, rightComp, loc));
    }

    result->setOp(EOpSequence);

    return result;
}

//
// HLSL atomic operations have slightly different arguments than
// GLSL/AST/SPIRV.  The semantics are converted below in decomposeIntrinsic.
// This provides the post-decomposition equivalent opcode.
//
TOperator HlslParseContext::mapAtomicOp(const TSourceLoc& loc, TOperator op, bool isImage)
{
    switch (op) {
    case EOpInterlockedAdd:             return isImage ? EOpImageAtomicAdd      : EOpAtomicAdd;
    case EOpInterlockedAnd:             return isImage ? EOpImageAtomicAnd      : EOpAtomicAnd;
    case EOpInterlockedCompareExchange: return isImage ? EOpImageAtomicCompSwap : EOpAtomicCompSwap;
    case EOpInterlockedMax:             return isImage ? EOpImageAtomicMax      : EOpAtomicMax;
    case EOpInterlockedMin:             return isImage ? EOpImageAtomicMin      : EOpAtomicMin;
    case EOpInterlockedOr:              return isImage ? EOpImageAtomicOr       : EOpAtomicOr;
    case EOpInterlockedXor:             return isImage ? EOpImageAtomicXor      : EOpAtomicXor;
    case EOpInterlockedExchange:        return isImage ? EOpImageAtomicExchange : EOpAtomicExchange;
    case EOpInterlockedCompareStore:  // TODO: ...
    default:
        error(loc, "unknown atomic operation", "unknown op", "");
        return EOpNull;
    }
}

//
// Create a combined sampler/texture from separate sampler and texture.
//
TIntermAggregate* HlslParseContext::handleSamplerTextureCombine(const TSourceLoc& loc, TIntermTyped* argTex,
                                                                TIntermTyped* argSampler)
{
    TIntermAggregate* txcombine = new TIntermAggregate(EOpConstructTextureSampler);

    txcombine->getSequence().push_back(argTex);
    txcombine->getSequence().push_back(argSampler);

    TSampler samplerType = argTex->getType().getSampler();
    samplerType.combined = true;

    // TODO:
    // This block exists until the spec no longer requires shadow modes on texture objects.
    // It can be deleted after that, along with the shadowTextureVariant member.
    {
        const bool shadowMode = argSampler->getType().getSampler().shadow;

        TIntermSymbol* texSymbol = argTex->getAsSymbolNode();

        if (texSymbol == nullptr)
            texSymbol = argTex->getAsBinaryNode()->getLeft()->getAsSymbolNode();

        if (texSymbol == nullptr) {
            error(loc, "unable to find texture symbol", "", "");
            return nullptr;
        }

        // This forces the texture's shadow state to be the sampler's
        // shadow state.  This depends on downstream optimization to
        // DCE one variant in [shadow, nonshadow] if both are present,
        // or the SPIR-V module would be invalid.
        long long newId = texSymbol->getId();

        // Check to see if this texture has been given a shadow mode already.
        // If so, look up the one we already have.
        const auto textureShadowEntry = textureShadowVariant.find(texSymbol->getId());

        if (textureShadowEntry != textureShadowVariant.end())
            newId = textureShadowEntry->second->get(shadowMode);
        else
            textureShadowVariant[texSymbol->getId()] = NewPoolObject(tShadowTextureSymbols(), 1);

        // Sometimes we have to create another symbol (if this texture has been seen before,
        // and we haven't created the form for this shadow mode).
        if (newId == -1) {
            TType texType;
            texType.shallowCopy(argTex->getType());
            texType.getSampler().shadow = shadowMode;  // set appropriate shadow mode.
            globalQualifierFix(loc, texType.getQualifier());

            TVariable* newTexture = makeInternalVariable(texSymbol->getName(), texType);

            trackLinkage(*newTexture);

            newId = newTexture->getUniqueId();
        }

        assert(newId != -1);

        if (textureShadowVariant.find(newId) == textureShadowVariant.end())
            textureShadowVariant[newId] = textureShadowVariant[texSymbol->getId()];

        textureShadowVariant[newId]->set(shadowMode, newId);

        // Remember this shadow mode in the texture and the merged type.
        argTex->getWritableType().getSampler().shadow = shadowMode;
        samplerType.shadow = shadowMode;

        texSymbol->switchId(newId);
    }

    txcombine->setType(TType(samplerType, EvqTemporary));
    txcombine->setLoc(loc);

    return txcombine;
}

// Return true if this a buffer type that has an associated counter buffer.
bool HlslParseContext::hasStructBuffCounter(const TType& type) const
{
    switch (type.getQualifier().declaredBuiltIn) {
    case EbvAppendConsume:       // fall through...
    case EbvRWStructuredBuffer:  // ...
        return true;
    default:
        return false; // the other structuredbuffer types do not have a counter.
    }
}

void HlslParseContext::counterBufferType(const TSourceLoc& loc, TType& type)
{
    // Counter type
    TType* counterType = new TType(EbtUint, EvqBuffer);
    counterType->setFieldName(intermediate.implicitCounterName);

    TTypeList* blockStruct = new TTypeList;
    TTypeLoc  member = { counterType, loc };
    blockStruct->push_back(member);

    TType blockType(blockStruct, "", counterType->getQualifier());
    blockType.getQualifier().storage = EvqBuffer;

    type.shallowCopy(blockType);
    shareStructBufferType(type);
}

// declare counter for a structured buffer type
void HlslParseContext::declareStructBufferCounter(const TSourceLoc& loc, const TType& bufferType, const TString& name)
{
    // Bail out if not a struct buffer
    if (! isStructBufferType(bufferType))
        return;

    if (! hasStructBuffCounter(bufferType))
        return;

    TType blockType;
    counterBufferType(loc, blockType);

    TString* blockName = NewPoolTString(intermediate.addCounterBufferName(name).c_str());

    // Counter buffer is not yet in use
    structBufferCounter[*blockName] = false;

    shareStructBufferType(blockType);
    declareBlock(loc, blockType, blockName);
}

// return the counter that goes with a given structuredbuffer
TIntermTyped* HlslParseContext::getStructBufferCounter(const TSourceLoc& loc, TIntermTyped* buffer)
{
    // Bail out if not a struct buffer
    if (buffer == nullptr || ! isStructBufferType(buffer->getType()))
        return nullptr;

    const TString counterBlockName(intermediate.addCounterBufferName(buffer->getAsSymbolNode()->getName()));

    // Mark the counter as being used
    structBufferCounter[counterBlockName] = true;

    TIntermTyped* counterVar = handleVariable(loc, &counterBlockName);  // find the block structure
    TIntermTyped* index = intermediate.addConstantUnion(0, loc); // index to counter inside block struct

    TIntermTyped* counterMember = intermediate.addIndex(EOpIndexDirectStruct, counterVar, index, loc);
    counterMember->setType(TType(EbtUint));
    return counterMember;
}

//
// Decompose structure buffer methods into AST
//
void HlslParseContext::decomposeStructBufferMethods(const TSourceLoc& loc, TIntermTyped*& node, TIntermNode* arguments)
{
    if (node == nullptr || node->getAsOperator() == nullptr || arguments == nullptr)
        return;

    const TOperator op  = node->getAsOperator()->getOp();
    TIntermAggregate* argAggregate = arguments->getAsAggregate();

    // Buffer is the object upon which method is called, so always arg 0
    TIntermTyped* bufferObj = nullptr;

    // The parameters can be an aggregate, or just a the object as a symbol if there are no fn params.
    if (argAggregate) {
        if (argAggregate->getSequence().empty())
            return;
        if (argAggregate->getSequence()[0])
            bufferObj = argAggregate->getSequence()[0]->getAsTyped();
    } else {
        bufferObj = arguments->getAsSymbolNode();
    }

    if (bufferObj == nullptr || bufferObj->getAsSymbolNode() == nullptr)
        return;

    // Some methods require a hidden internal counter, obtained via getStructBufferCounter().
    // This lambda adds something to it and returns the old value.
    const auto incDecCounter = [&](int incval) -> TIntermTyped* {
        TIntermTyped* incrementValue = intermediate.addConstantUnion(static_cast<unsigned int>(incval), loc, true);
        TIntermTyped* counter = getStructBufferCounter(loc, bufferObj); // obtain the counter member

        if (counter == nullptr)
            return nullptr;

        TIntermAggregate* counterIncrement = new TIntermAggregate(EOpAtomicAdd);
        counterIncrement->setType(TType(EbtUint, EvqTemporary));
        counterIncrement->setLoc(loc);
        counterIncrement->getSequence().push_back(counter);
        counterIncrement->getSequence().push_back(incrementValue);

        return counterIncrement;
    };

    // Index to obtain the runtime sized array out of the buffer.
    TIntermTyped* argArray = indexStructBufferContent(loc, bufferObj);
    if (argArray == nullptr)
        return;  // It might not be a struct buffer method.

    switch (op) {
    case EOpMethodLoad:
        {
            TIntermTyped* argIndex = makeIntegerIndex(argAggregate->getSequence()[1]->getAsTyped());  // index

            const TType& bufferType = bufferObj->getType();

            const TBuiltInVariable builtInType = bufferType.getQualifier().declaredBuiltIn;

            // Byte address buffers index in bytes (only multiples of 4 permitted... not so much a byte address
            // buffer then, but that's what it calls itself.
            const bool isByteAddressBuffer = (builtInType == EbvByteAddressBuffer   ||
                                              builtInType == EbvRWByteAddressBuffer);


            if (isByteAddressBuffer)
                argIndex = intermediate.addBinaryNode(EOpRightShift, argIndex,
                                                      intermediate.addConstantUnion(2, loc, true),
                                                      loc, TType(EbtInt));

            // Index into the array to find the item being loaded.
            const TOperator idxOp = (argIndex->getQualifier().storage == EvqConst) ? EOpIndexDirect : EOpIndexIndirect;

            node = intermediate.addIndex(idxOp, argArray, argIndex, loc);

            const TType derefType(argArray->getType(), 0);
            node->setType(derefType);
        }

        break;

    case EOpMethodLoad2:
    case EOpMethodLoad3:
    case EOpMethodLoad4:
        {
            TIntermTyped* argIndex = makeIntegerIndex(argAggregate->getSequence()[1]->getAsTyped());  // index

            TOperator constructOp = EOpNull;
            int size = 0;

            switch (op) {
            case EOpMethodLoad2: size = 2; constructOp = EOpConstructVec2; break;
            case EOpMethodLoad3: size = 3; constructOp = EOpConstructVec3; break;
            case EOpMethodLoad4: size = 4; constructOp = EOpConstructVec4; break;
            default: assert(0);
            }

            TIntermTyped* body = nullptr;

            // First, we'll store the address in a variable to avoid multiple shifts
            // (we must convert the byte address to an item address)
            TIntermTyped* byteAddrIdx = intermediate.addBinaryNode(EOpRightShift, argIndex,
                                                                   intermediate.addConstantUnion(2, loc, true),
                                                                   loc, TType(EbtInt));

            TVariable* byteAddrSym = makeInternalVariable("byteAddrTemp", TType(EbtInt, EvqTemporary));
            TIntermTyped* byteAddrIdxVar = intermediate.addSymbol(*byteAddrSym, loc);

            body = intermediate.growAggregate(body, intermediate.addAssign(EOpAssign, byteAddrIdxVar, byteAddrIdx, loc));

            TIntermTyped* vec = nullptr;

            // These are only valid on (rw)byteaddressbuffers, so we can always perform the >>2
            // address conversion.
            for (int idx=0; idx<size; ++idx) {
                TIntermTyped* offsetIdx = byteAddrIdxVar;

                // add index offset
                if (idx != 0)
                    offsetIdx = intermediate.addBinaryNode(EOpAdd, offsetIdx,
                                                           intermediate.addConstantUnion(idx, loc, true),
                                                           loc, TType(EbtInt));

                const TOperator idxOp = (offsetIdx->getQualifier().storage == EvqConst) ? EOpIndexDirect
                                                                                        : EOpIndexIndirect;

                TIntermTyped* indexVal = intermediate.addIndex(idxOp, argArray, offsetIdx, loc);

                TType derefType(argArray->getType(), 0);
                derefType.getQualifier().makeTemporary();
                indexVal->setType(derefType);

                vec = intermediate.growAggregate(vec, indexVal);
            }

            vec->setType(TType(argArray->getBasicType(), EvqTemporary, size));
            vec->getAsAggregate()->setOperator(constructOp);

            body = intermediate.growAggregate(body, vec);
            body->setType(vec->getType());
            body->getAsAggregate()->setOperator(EOpSequence);

            node = body;
        }

        break;

    case EOpMethodStore:
    case EOpMethodStore2:
    case EOpMethodStore3:
    case EOpMethodStore4:
        {
            TIntermTyped* argIndex = makeIntegerIndex(argAggregate->getSequence()[1]->getAsTyped());  // index
            TIntermTyped* argValue = argAggregate->getSequence()[2]->getAsTyped();  // value

            // Index into the array to find the item being loaded.
            // Byte address buffers index in bytes (only multiples of 4 permitted... not so much a byte address
            // buffer then, but that's what it calls itself).

            int size = 0;

            switch (op) {
            case EOpMethodStore:  size = 1; break;
            case EOpMethodStore2: size = 2; break;
            case EOpMethodStore3: size = 3; break;
            case EOpMethodStore4: size = 4; break;
            default: assert(0);
            }

            TIntermAggregate* body = nullptr;

            // First, we'll store the address in a variable to avoid multiple shifts
            // (we must convert the byte address to an item address)
            TIntermTyped* byteAddrIdx = intermediate.addBinaryNode(EOpRightShift, argIndex,
                                                                   intermediate.addConstantUnion(2, loc, true), loc, TType(EbtInt));

            TVariable* byteAddrSym = makeInternalVariable("byteAddrTemp", TType(EbtInt, EvqTemporary));
            TIntermTyped* byteAddrIdxVar = intermediate.addSymbol(*byteAddrSym, loc);

            body = intermediate.growAggregate(body, intermediate.addAssign(EOpAssign, byteAddrIdxVar, byteAddrIdx, loc));

            for (int idx=0; idx<size; ++idx) {
                TIntermTyped* offsetIdx = byteAddrIdxVar;
                TIntermTyped* idxConst = intermediate.addConstantUnion(idx, loc, true);

                // add index offset
                if (idx != 0)
                    offsetIdx = intermediate.addBinaryNode(EOpAdd, offsetIdx, idxConst, loc, TType(EbtInt));

                const TOperator idxOp = (offsetIdx->getQualifier().storage == EvqConst) ? EOpIndexDirect
                                                                                        : EOpIndexIndirect;

                TIntermTyped* lValue = intermediate.addIndex(idxOp, argArray, offsetIdx, loc);
                const TType derefType(argArray->getType(), 0);
                lValue->setType(derefType);

                TIntermTyped* rValue;
                if (size == 1) {
                    rValue = argValue;
                } else {
                    rValue = intermediate.addIndex(EOpIndexDirect, argValue, idxConst, loc);
                    const TType indexType(argValue->getType(), 0);
                    rValue->setType(indexType);
                }

                TIntermTyped* assign = intermediate.addAssign(EOpAssign, lValue, rValue, loc);

                body = intermediate.growAggregate(body, assign);
            }

            body->setOperator(EOpSequence);
            node = body;
        }

        break;

    case EOpMethodGetDimensions:
        {
            const int numArgs = (int)argAggregate->getSequence().size();
            TIntermTyped* argNumItems = argAggregate->getSequence()[1]->getAsTyped();  // out num items
            TIntermTyped* argStride   = numArgs > 2 ? argAggregate->getSequence()[2]->getAsTyped() : nullptr;  // out stride

            TIntermAggregate* body = nullptr;

            // Length output:
            if (argArray->getType().isSizedArray()) {
                const int length = argArray->getType().getOuterArraySize();
                TIntermTyped* assign = intermediate.addAssign(EOpAssign, argNumItems,
                                                              intermediate.addConstantUnion(length, loc, true), loc);
                body = intermediate.growAggregate(body, assign, loc);
            } else {
                TIntermTyped* lengthCall = intermediate.addBuiltInFunctionCall(loc, EOpArrayLength, true, argArray,
                                                                               argNumItems->getType());
                TIntermTyped* assign = intermediate.addAssign(EOpAssign, argNumItems, lengthCall, loc);
                body = intermediate.growAggregate(body, assign, loc);
            }

            // Stride output:
            if (argStride != nullptr) {
                int size;
                int stride;
                intermediate.getMemberAlignment(argArray->getType(), size, stride, argArray->getType().getQualifier().layoutPacking,
                                                argArray->getType().getQualifier().layoutMatrix == ElmRowMajor);

                TIntermTyped* assign = intermediate.addAssign(EOpAssign, argStride,
                                                              intermediate.addConstantUnion(stride, loc, true), loc);

                body = intermediate.growAggregate(body, assign);
            }

            body->setOperator(EOpSequence);
            node = body;
        }

        break;

    case EOpInterlockedAdd:
    case EOpInterlockedAnd:
    case EOpInterlockedExchange:
    case EOpInterlockedMax:
    case EOpInterlockedMin:
    case EOpInterlockedOr:
    case EOpInterlockedXor:
    case EOpInterlockedCompareExchange:
    case EOpInterlockedCompareStore:
        {
            // We'll replace the first argument with the block dereference, and let
            // downstream decomposition handle the rest.

            TIntermSequence& sequence = argAggregate->getSequence();

            TIntermTyped* argIndex     = makeIntegerIndex(sequence[1]->getAsTyped());  // index
            argIndex = intermediate.addBinaryNode(EOpRightShift, argIndex, intermediate.addConstantUnion(2, loc, true),
                                                  loc, TType(EbtInt));

            const TOperator idxOp = (argIndex->getQualifier().storage == EvqConst) ? EOpIndexDirect : EOpIndexIndirect;
            TIntermTyped* element = intermediate.addIndex(idxOp, argArray, argIndex, loc);

            const TType derefType(argArray->getType(), 0);
            element->setType(derefType);

            // Replace the numeric byte offset parameter with array reference.
            sequence[1] = element;
            sequence.erase(sequence.begin(), sequence.begin()+1);
        }
        break;

    case EOpMethodIncrementCounter:
        {
            node = incDecCounter(1);
            break;
        }

    case EOpMethodDecrementCounter:
        {
            TIntermTyped* preIncValue = incDecCounter(-1); // result is original value
            node = intermediate.addBinaryNode(EOpAdd, preIncValue, intermediate.addConstantUnion(-1, loc, true), loc,
                                              preIncValue->getType());
            break;
        }

    case EOpMethodAppend:
        {
            TIntermTyped* oldCounter = incDecCounter(1);

            TIntermTyped* lValue = intermediate.addIndex(EOpIndexIndirect, argArray, oldCounter, loc);
            TIntermTyped* rValue = argAggregate->getSequence()[1]->getAsTyped();

            const TType derefType(argArray->getType(), 0);
            lValue->setType(derefType);

            node = intermediate.addAssign(EOpAssign, lValue, rValue, loc);

            break;
        }

    case EOpMethodConsume:
        {
            TIntermTyped* oldCounter = incDecCounter(-1);

            TIntermTyped* newCounter = intermediate.addBinaryNode(EOpAdd, oldCounter,
                                                                  intermediate.addConstantUnion(-1, loc, true), loc,
                                                                  oldCounter->getType());

            node = intermediate.addIndex(EOpIndexIndirect, argArray, newCounter, loc);

            const TType derefType(argArray->getType(), 0);
            node->setType(derefType);

            break;
        }

    default:
        break; // most pass through unchanged
    }
}

// Create array of standard sample positions for given sample count.
// TODO: remove when a real method to query sample pos exists in SPIR-V.
TIntermConstantUnion* HlslParseContext::getSamplePosArray(int count)
{
    struct tSamplePos { float x, y; };

    static const tSamplePos pos1[] = {
        { 0.0/16.0,  0.0/16.0 },
    };

    // standard sample positions for 2, 4, 8, and 16 samples.
    static const tSamplePos pos2[] = {
        { 4.0/16.0,  4.0/16.0 }, {-4.0/16.0, -4.0/16.0 },
    };

    static const tSamplePos pos4[] = {
        {-2.0/16.0, -6.0/16.0 }, { 6.0/16.0, -2.0/16.0 }, {-6.0/16.0,  2.0/16.0 }, { 2.0/16.0,  6.0/16.0 },
    };

    static const tSamplePos pos8[] = {
        { 1.0/16.0, -3.0/16.0 }, {-1.0/16.0,  3.0/16.0 }, { 5.0/16.0,  1.0/16.0 }, {-3.0/16.0, -5.0/16.0 },
        {-5.0/16.0,  5.0/16.0 }, {-7.0/16.0, -1.0/16.0 }, { 3.0/16.0,  7.0/16.0 }, { 7.0/16.0, -7.0/16.0 },
    };

    static const tSamplePos pos16[] = {
        { 1.0/16.0,  1.0/16.0 }, {-1.0/16.0, -3.0/16.0 }, {-3.0/16.0,  2.0/16.0 }, { 4.0/16.0, -1.0/16.0 },
        {-5.0/16.0, -2.0/16.0 }, { 2.0/16.0,  5.0/16.0 }, { 5.0/16.0,  3.0/16.0 }, { 3.0/16.0, -5.0/16.0 },
        {-2.0/16.0,  6.0/16.0 }, { 0.0/16.0, -7.0/16.0 }, {-4.0/16.0, -6.0/16.0 }, {-6.0/16.0,  4.0/16.0 },
        {-8.0/16.0,  0.0/16.0 }, { 7.0/16.0, -4.0/16.0 }, { 6.0/16.0,  7.0/16.0 }, {-7.0/16.0, -8.0/16.0 },
    };

    const tSamplePos* sampleLoc = nullptr;
    int numSamples = count;

    switch (count) {
    case 2:  sampleLoc = pos2;  break;
    case 4:  sampleLoc = pos4;  break;
    case 8:  sampleLoc = pos8;  break;
    case 16: sampleLoc = pos16; break;
    default:
        sampleLoc = pos1;
        numSamples = 1;
    }

    TConstUnionArray* values = new TConstUnionArray(numSamples*2);

    for (int pos=0; pos<count; ++pos) {
        TConstUnion x, y;
        x.setDConst(sampleLoc[pos].x);
        y.setDConst(sampleLoc[pos].y);

        (*values)[pos*2+0] = x;
        (*values)[pos*2+1] = y;
    }

    TType retType(EbtFloat, EvqConst, 2);

    if (numSamples != 1) {
        TArraySizes* arraySizes = new TArraySizes;
        arraySizes->addInnerSize(numSamples);
        retType.transferArraySizes(arraySizes);
    }

    return new TIntermConstantUnion(*values, retType);
}

//
// Decompose DX9 and DX10 sample intrinsics & object methods into AST
//
void HlslParseContext::decomposeSampleMethods(const TSourceLoc& loc, TIntermTyped*& node, TIntermNode* arguments)
{
    if (node == nullptr || !node->getAsOperator())
        return;

    // Sampler return must always be a vec4, but we can construct a shorter vector or a structure from it.
    const auto convertReturn = [&loc, &node, this](TIntermTyped* result, const TSampler& sampler) -> TIntermTyped* {
        result->setType(TType(node->getType().getBasicType(), EvqTemporary, node->getVectorSize()));

        TIntermTyped* convertedResult = nullptr;

        TType retType;
        getTextureReturnType(sampler, retType);

        if (retType.isStruct()) {
            // For type convenience, conversionAggregate points to the convertedResult (we know it's an aggregate here)
            TIntermAggregate* conversionAggregate = new TIntermAggregate;
            convertedResult = conversionAggregate;

            // Convert vector output to return structure.  We will need a temp symbol to copy the results to.
            TVariable* structVar = makeInternalVariable("@sampleStructTemp", retType);

            // We also need a temp symbol to hold the result of the texture.  We don't want to re-fetch the
            // sample each time we'll index into the result, so we'll copy to this, and index into the copy.
            TVariable* sampleShadow = makeInternalVariable("@sampleResultShadow", result->getType());

            // Initial copy from texture to our sample result shadow.
            TIntermTyped* shadowCopy = intermediate.addAssign(EOpAssign, intermediate.addSymbol(*sampleShadow, loc),
                                                              result, loc);

            conversionAggregate->getSequence().push_back(shadowCopy);

            unsigned vec4Pos = 0;

            for (unsigned m = 0; m < unsigned(retType.getStruct()->size()); ++m) {
                const TType memberType(retType, m); // dereferenced type of the member we're about to assign.

                // Check for bad struct members.  This should have been caught upstream.  Complain, because
                // wwe don't know what to do with it.  This algorithm could be generalized to handle
                // other things, e.g, sub-structures, but HLSL doesn't allow them.
                if (!memberType.isVector() && !memberType.isScalar()) {
                    error(loc, "expected: scalar or vector type in texture structure", "", "");
                    return nullptr;
                }

                // Index into the struct variable to find the member to assign.
                TIntermTyped* structMember = intermediate.addIndex(EOpIndexDirectStruct,
                                                                   intermediate.addSymbol(*structVar, loc),
                                                                   intermediate.addConstantUnion(m, loc), loc);

                structMember->setType(memberType);

                // Assign each component of (possible) vector in struct member.
                for (int component = 0; component < memberType.getVectorSize(); ++component) {
                    TIntermTyped* vec4Member = intermediate.addIndex(EOpIndexDirect,
                                                                     intermediate.addSymbol(*sampleShadow, loc),
                                                                     intermediate.addConstantUnion(vec4Pos++, loc), loc);
                    vec4Member->setType(TType(memberType.getBasicType(), EvqTemporary, 1));

                    TIntermTyped* memberAssign = nullptr;

                    if (memberType.isVector()) {
                        // Vector member: we need to create an access chain to the vector component.

                        TIntermTyped* structVecComponent = intermediate.addIndex(EOpIndexDirect, structMember,
                                                                                 intermediate.addConstantUnion(component, loc), loc);

                        memberAssign = intermediate.addAssign(EOpAssign, structVecComponent, vec4Member, loc);
                    } else {
                        // Scalar member: we can assign to it directly.
                        memberAssign = intermediate.addAssign(EOpAssign, structMember, vec4Member, loc);
                    }


                    conversionAggregate->getSequence().push_back(memberAssign);
                }
            }

            // Add completed variable so the expression results in the whole struct value we just built.
            conversionAggregate->getSequence().push_back(intermediate.addSymbol(*structVar, loc));

            // Make it a sequence.
            intermediate.setAggregateOperator(conversionAggregate, EOpSequence, retType, loc);
        } else {
            // vector clamp the output if template vector type is smaller than sample result.
            if (retType.getVectorSize() < node->getVectorSize()) {
                // Too many components.  Construct shorter vector from it.
                const TOperator op = intermediate.mapTypeToConstructorOp(retType);

                convertedResult = constructBuiltIn(retType, op, result, loc, false);
            } else {
                // Enough components.  Use directly.
                convertedResult = result;
            }
        }

        convertedResult->setLoc(loc);
        return convertedResult;
    };

    const TOperator op  = node->getAsOperator()->getOp();
    const TIntermAggregate* argAggregate = arguments ? arguments->getAsAggregate() : nullptr;

    // Bail out if not a sampler method.
    // Note though this is odd to do before checking the op, because the op
    // could be something that takes the arguments, and the function in question
    // takes the result of the op.  So, this is not the final word.
    if (arguments != nullptr) {
        if (argAggregate == nullptr) {
            if (arguments->getAsTyped()->getBasicType() != EbtSampler)
                return;
        } else {
            if (argAggregate->getSequence().size() == 0 ||
                argAggregate->getSequence()[0] == nullptr ||
                argAggregate->getSequence()[0]->getAsTyped()->getBasicType() != EbtSampler)
                return;
        }
    }

    switch (op) {
    // **** DX9 intrinsics: ****
    case EOpTexture:
        {
            // Texture with ddx & ddy is really gradient form in HLSL
            if (argAggregate->getSequence().size() == 4)
                node->getAsAggregate()->setOperator(EOpTextureGrad);

            break;
        }
    case EOpTextureLod: //is almost EOpTextureBias (only args & operations are different)
        {
            TIntermTyped *argSamp = argAggregate->getSequence()[0]->getAsTyped();   // sampler
            TIntermTyped *argCoord = argAggregate->getSequence()[1]->getAsTyped();  // coord

            assert(argCoord->getVectorSize() == 4);
            TIntermTyped *w = intermediate.addConstantUnion(3, loc, true);
            TIntermTyped *argLod = intermediate.addIndex(EOpIndexDirect, argCoord, w, loc);

            TOperator constructOp = EOpNull;
            const TSampler &sampler = argSamp->getType().getSampler();
            int coordSize = 0;

            switch (sampler.dim)
            {
            case Esd1D:   constructOp = EOpConstructFloat; coordSize = 1; break; // 1D
            case Esd2D:   constructOp = EOpConstructVec2;  coordSize = 2; break; // 2D
            case Esd3D:   constructOp = EOpConstructVec3;  coordSize = 3; break; // 3D
            case EsdCube: constructOp = EOpConstructVec3;  coordSize = 3; break; // also 3D
            default:
                error(loc, "unhandled DX9 texture LoD dimension", "", "");
                break;
            }

            TIntermAggregate *constructCoord = new TIntermAggregate(constructOp);
            constructCoord->getSequence().push_back(argCoord);
            constructCoord->setLoc(loc);
            constructCoord->setType(TType(argCoord->getBasicType(), EvqTemporary, coordSize));

            TIntermAggregate *tex = new TIntermAggregate(EOpTextureLod);
            tex->getSequence().push_back(argSamp);        // sampler
            tex->getSequence().push_back(constructCoord); // coordinate
            tex->getSequence().push_back(argLod);         // lod

            node = convertReturn(tex, sampler);

            break;
        }

    case EOpTextureBias:
        {
            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();  // sampler
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();  // coord

            // HLSL puts bias in W component of coordinate.  We extract it and add it to
            // the argument list, instead
            TIntermTyped* w = intermediate.addConstantUnion(3, loc, true);
            TIntermTyped* bias = intermediate.addIndex(EOpIndexDirect, arg1, w, loc);

            TOperator constructOp = EOpNull;
            const TSampler& sampler = arg0->getType().getSampler();

            switch (sampler.dim) {
            case Esd1D:   constructOp = EOpConstructFloat; break; // 1D
            case Esd2D:   constructOp = EOpConstructVec2;  break; // 2D
            case Esd3D:   constructOp = EOpConstructVec3;  break; // 3D
            case EsdCube: constructOp = EOpConstructVec3;  break; // also 3D
            default:
                error(loc, "unhandled DX9 texture bias dimension", "", "");
                break;
            }

            TIntermAggregate* constructCoord = new TIntermAggregate(constructOp);
            constructCoord->getSequence().push_back(arg1);
            constructCoord->setLoc(loc);

            // The input vector should never be less than 2, since there's always a bias.
            // The max is for safety, and should be a no-op.
            constructCoord->setType(TType(arg1->getBasicType(), EvqTemporary, std::max(arg1->getVectorSize() - 1, 0)));

            TIntermAggregate* tex = new TIntermAggregate(EOpTexture);
            tex->getSequence().push_back(arg0);           // sampler
            tex->getSequence().push_back(constructCoord); // coordinate
            tex->getSequence().push_back(bias);           // bias

            node = convertReturn(tex, sampler);

            break;
        }

    // **** DX10 methods: ****
    case EOpMethodSample:     // fall through
    case EOpMethodSampleBias: // ...
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSamp   = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[2]->getAsTyped();
            TIntermTyped* argBias   = nullptr;
            TIntermTyped* argOffset = nullptr;
            const TSampler& sampler = argTex->getType().getSampler();

            int nextArg = 3;

            if (op == EOpMethodSampleBias)  // SampleBias has a bias arg
                argBias = argAggregate->getSequence()[nextArg++]->getAsTyped();

            TOperator textureOp = EOpTexture;

            if ((int)argAggregate->getSequence().size() == (nextArg+1)) { // last parameter is offset form
                textureOp = EOpTextureOffset;
                argOffset = argAggregate->getSequence()[nextArg++]->getAsTyped();
            }

            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);

            TIntermAggregate* txsample = new TIntermAggregate(textureOp);
            txsample->getSequence().push_back(txcombine);
            txsample->getSequence().push_back(argCoord);

            if (argOffset != nullptr)
                txsample->getSequence().push_back(argOffset);

            if (argBias != nullptr)
              txsample->getSequence().push_back(argBias);

            node = convertReturn(txsample, sampler);

            break;
        }

    case EOpMethodSampleGrad: // ...
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSamp   = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[2]->getAsTyped();
            TIntermTyped* argDDX    = argAggregate->getSequence()[3]->getAsTyped();
            TIntermTyped* argDDY    = argAggregate->getSequence()[4]->getAsTyped();
            TIntermTyped* argOffset = nullptr;
            const TSampler& sampler = argTex->getType().getSampler();

            TOperator textureOp = EOpTextureGrad;

            if (argAggregate->getSequence().size() == 6) { // last parameter is offset form
                textureOp = EOpTextureGradOffset;
                argOffset = argAggregate->getSequence()[5]->getAsTyped();
            }

            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);

            TIntermAggregate* txsample = new TIntermAggregate(textureOp);
            txsample->getSequence().push_back(txcombine);
            txsample->getSequence().push_back(argCoord);
            txsample->getSequence().push_back(argDDX);
            txsample->getSequence().push_back(argDDY);

            if (argOffset != nullptr)
                txsample->getSequence().push_back(argOffset);

            node = convertReturn(txsample, sampler);

            break;
        }

    case EOpMethodGetDimensions:
        {
            // AST returns a vector of results, which we break apart component-wise into
            // separate values to assign to the HLSL method's outputs, ala:
            //  tx . GetDimensions(width, height);
            //      float2 sizeQueryTemp = EOpTextureQuerySize
            //      width = sizeQueryTemp.X;
            //      height = sizeQueryTemp.Y;

            TIntermTyped* argTex = argAggregate->getSequence()[0]->getAsTyped();
            const TType& texType = argTex->getType();

            assert(texType.getBasicType() == EbtSampler);

            const TSampler& sampler = texType.getSampler();
            const TSamplerDim dim = sampler.dim;
            const bool isImage = sampler.isImage();
            const bool isMs = sampler.isMultiSample();
            const int numArgs = (int)argAggregate->getSequence().size();

            int numDims = 0;

            switch (dim) {
            case Esd1D:     numDims = 1; break; // W
            case Esd2D:     numDims = 2; break; // W, H
            case Esd3D:     numDims = 3; break; // W, H, D
            case EsdCube:   numDims = 2; break; // W, H (cube)
            case EsdBuffer: numDims = 1; break; // W (buffers)
            case EsdRect:   numDims = 2; break; // W, H (rect)
            default:
                error(loc, "unhandled DX10 MethodGet dimension", "", "");
                break;
            }

            // Arrayed adds another dimension for the number of array elements
            if (sampler.isArrayed())
                ++numDims;

            // Establish whether the method itself is querying mip levels.  This can be false even
            // if the underlying query requires a MIP level, due to the available HLSL method overloads.
            const bool mipQuery = (numArgs > (numDims + 1 + (isMs ? 1 : 0)));

            // Establish whether we must use the LOD form of query (even if the method did not supply a mip level to query).
            // True if:
            //   1. 1D/2D/3D/Cube AND multisample==0 AND NOT image (those can be sent to the non-LOD query)
            // or,
            //   2. There is a LOD (because the non-LOD query cannot be used in that case, per spec)
            const bool mipRequired =
                ((dim == Esd1D || dim == Esd2D || dim == Esd3D || dim == EsdCube) && !isMs && !isImage) || // 1...
                mipQuery; // 2...

            // AST assumes integer return.  Will be converted to float if required.
            TIntermAggregate* sizeQuery = new TIntermAggregate(isImage ? EOpImageQuerySize : EOpTextureQuerySize);
            sizeQuery->getSequence().push_back(argTex);

            // If we're building an LOD query, add the LOD.
            if (mipRequired) {
                // If the base HLSL query had no MIP level given, use level 0.
                TIntermTyped* queryLod = mipQuery ? argAggregate->getSequence()[1]->getAsTyped() :
                    intermediate.addConstantUnion(0, loc, true);
                sizeQuery->getSequence().push_back(queryLod);
            }

            sizeQuery->setType(TType(EbtUint, EvqTemporary, numDims));
            sizeQuery->setLoc(loc);

            // Return value from size query
            TVariable* tempArg = makeInternalVariable("sizeQueryTemp", sizeQuery->getType());
            tempArg->getWritableType().getQualifier().makeTemporary();
            TIntermTyped* sizeQueryAssign = intermediate.addAssign(EOpAssign,
                                                                   intermediate.addSymbol(*tempArg, loc),
                                                                   sizeQuery, loc);

            // Compound statement for assigning outputs
            TIntermAggregate* compoundStatement = intermediate.makeAggregate(sizeQueryAssign, loc);
            // Index of first output parameter
            const int outParamBase = mipQuery ? 2 : 1;

            for (int compNum = 0; compNum < numDims; ++compNum) {
                TIntermTyped* indexedOut = nullptr;
                TIntermSymbol* sizeQueryReturn = intermediate.addSymbol(*tempArg, loc);

                if (numDims > 1) {
                    TIntermTyped* component = intermediate.addConstantUnion(compNum, loc, true);
                    indexedOut = intermediate.addIndex(EOpIndexDirect, sizeQueryReturn, component, loc);
                    indexedOut->setType(TType(EbtUint, EvqTemporary, 1));
                    indexedOut->setLoc(loc);
                } else {
                    indexedOut = sizeQueryReturn;
                }

                TIntermTyped* outParam = argAggregate->getSequence()[outParamBase + compNum]->getAsTyped();
                TIntermTyped* compAssign = intermediate.addAssign(EOpAssign, outParam, indexedOut, loc);

                compoundStatement = intermediate.growAggregate(compoundStatement, compAssign);
            }

            // handle mip level parameter
            if (mipQuery) {
                TIntermTyped* outParam = argAggregate->getSequence()[outParamBase + numDims]->getAsTyped();

                TIntermAggregate* levelsQuery = new TIntermAggregate(EOpTextureQueryLevels);
                levelsQuery->getSequence().push_back(argTex);
                levelsQuery->setType(TType(EbtUint, EvqTemporary, 1));
                levelsQuery->setLoc(loc);

                TIntermTyped* compAssign = intermediate.addAssign(EOpAssign, outParam, levelsQuery, loc);
                compoundStatement = intermediate.growAggregate(compoundStatement, compAssign);
            }

            // 2DMS formats query # samples, which needs a different query op
            if (sampler.isMultiSample()) {
                TIntermTyped* outParam = argAggregate->getSequence()[outParamBase + numDims]->getAsTyped();

                TIntermAggregate* samplesQuery = new TIntermAggregate(EOpImageQuerySamples);
                samplesQuery->getSequence().push_back(argTex);
                samplesQuery->setType(TType(EbtUint, EvqTemporary, 1));
                samplesQuery->setLoc(loc);

                TIntermTyped* compAssign = intermediate.addAssign(EOpAssign, outParam, samplesQuery, loc);
                compoundStatement = intermediate.growAggregate(compoundStatement, compAssign);
            }

            compoundStatement->setOperator(EOpSequence);
            compoundStatement->setLoc(loc);
            compoundStatement->setType(TType(EbtVoid));

            node = compoundStatement;

            break;
        }

    case EOpMethodSampleCmp:  // fall through...
    case EOpMethodSampleCmpLevelZero:
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSamp   = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[2]->getAsTyped();
            TIntermTyped* argCmpVal = argAggregate->getSequence()[3]->getAsTyped();
            TIntermTyped* argOffset = nullptr;

            // Sampler argument should be a sampler.
            if (argSamp->getType().getBasicType() != EbtSampler) {
                error(loc, "expected: sampler type", "", "");
                return;
            }

            // Sampler should be a SamplerComparisonState
            if (! argSamp->getType().getSampler().isShadow()) {
                error(loc, "expected: SamplerComparisonState", "", "");
                return;
            }

            // optional offset value
            if (argAggregate->getSequence().size() > 4)
                argOffset = argAggregate->getSequence()[4]->getAsTyped();

            const int coordDimWithCmpVal = argCoord->getType().getVectorSize() + 1; // +1 for cmp

            // AST wants comparison value as one of the texture coordinates
            TOperator constructOp = EOpNull;
            switch (coordDimWithCmpVal) {
            // 1D can't happen: there's always at least 1 coordinate dimension + 1 cmp val
            case 2: constructOp = EOpConstructVec2;  break;
            case 3: constructOp = EOpConstructVec3;  break;
            case 4: constructOp = EOpConstructVec4;  break;
            case 5: constructOp = EOpConstructVec4;  break; // cubeArrayShadow, cmp value is separate arg.
            default:
                error(loc, "unhandled DX10 MethodSample dimension", "", "");
                break;
            }

            TIntermAggregate* coordWithCmp = new TIntermAggregate(constructOp);
            coordWithCmp->getSequence().push_back(argCoord);
            if (coordDimWithCmpVal != 5) // cube array shadow is special.
                coordWithCmp->getSequence().push_back(argCmpVal);
            coordWithCmp->setLoc(loc);
            coordWithCmp->setType(TType(argCoord->getBasicType(), EvqTemporary, std::min(coordDimWithCmpVal, 4)));

            TOperator textureOp = (op == EOpMethodSampleCmpLevelZero ? EOpTextureLod : EOpTexture);
            if (argOffset != nullptr)
                textureOp = (op == EOpMethodSampleCmpLevelZero ? EOpTextureLodOffset : EOpTextureOffset);

            // Create combined sampler & texture op
            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);
            TIntermAggregate* txsample = new TIntermAggregate(textureOp);
            txsample->getSequence().push_back(txcombine);
            txsample->getSequence().push_back(coordWithCmp);

            if (coordDimWithCmpVal == 5) // cube array shadow is special: cmp val follows coord.
                txsample->getSequence().push_back(argCmpVal);

            // the LevelZero form uses 0 as an explicit LOD
            if (op == EOpMethodSampleCmpLevelZero)
                txsample->getSequence().push_back(intermediate.addConstantUnion(0.0, EbtFloat, loc, true));

            // Add offset if present
            if (argOffset != nullptr)
                txsample->getSequence().push_back(argOffset);

            txsample->setType(node->getType());
            txsample->setLoc(loc);
            node = txsample;

            break;
        }

    case EOpMethodLoad:
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argOffset = nullptr;
            TIntermTyped* lodComponent = nullptr;
            TIntermTyped* coordSwizzle = nullptr;

            const TSampler& sampler = argTex->getType().getSampler();
            const bool isMS = sampler.isMultiSample();
            const bool isBuffer = sampler.dim == EsdBuffer;
            const bool isImage = sampler.isImage();
            const TBasicType coordBaseType = argCoord->getType().getBasicType();

            // Last component of coordinate is the mip level, for non-MS.  we separate them here:
            if (isMS || isBuffer || isImage) {
                // MS, Buffer, and Image have no LOD
                coordSwizzle = argCoord;
            } else {
                // Extract coordinate
                int swizzleSize = argCoord->getType().getVectorSize() - (isMS ? 0 : 1);
                TSwizzleSelectors<TVectorSelector> coordFields;
                for (int i = 0; i < swizzleSize; ++i)
                    coordFields.push_back(i);
                TIntermTyped* coordIdx = intermediate.addSwizzle(coordFields, loc);
                coordSwizzle = intermediate.addIndex(EOpVectorSwizzle, argCoord, coordIdx, loc);
                coordSwizzle->setType(TType(coordBaseType, EvqTemporary, coordFields.size()));

                // Extract LOD
                TIntermTyped* lodIdx = intermediate.addConstantUnion(coordFields.size(), loc, true);
                lodComponent = intermediate.addIndex(EOpIndexDirect, argCoord, lodIdx, loc);
                lodComponent->setType(TType(coordBaseType, EvqTemporary, 1));
            }

            const int numArgs    = (int)argAggregate->getSequence().size();
            const bool hasOffset = ((!isMS && numArgs == 3) || (isMS && numArgs == 4));

            // Create texel fetch
            const TOperator fetchOp = (isImage   ? EOpImageLoad :
                                       hasOffset ? EOpTextureFetchOffset :
                                       EOpTextureFetch);
            TIntermAggregate* txfetch = new TIntermAggregate(fetchOp);

            // Build up the fetch
            txfetch->getSequence().push_back(argTex);
            txfetch->getSequence().push_back(coordSwizzle);

            if (isMS) {
                // add 2DMS sample index
                TIntermTyped* argSampleIdx  = argAggregate->getSequence()[2]->getAsTyped();
                txfetch->getSequence().push_back(argSampleIdx);
            } else if (isBuffer) {
                // Nothing else to do for buffers.
            } else if (isImage) {
                // Nothing else to do for images.
            } else {
                // 2DMS and buffer have no LOD, but everything else does.
                txfetch->getSequence().push_back(lodComponent);
            }

            // Obtain offset arg, if there is one.
            if (hasOffset) {
                const int offsetPos  = (isMS ? 3 : 2);
                argOffset = argAggregate->getSequence()[offsetPos]->getAsTyped();
                txfetch->getSequence().push_back(argOffset);
            }

            node = convertReturn(txfetch, sampler);

            break;
        }

    case EOpMethodSampleLevel:
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSamp   = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[2]->getAsTyped();
            TIntermTyped* argLod    = argAggregate->getSequence()[3]->getAsTyped();
            TIntermTyped* argOffset = nullptr;
            const TSampler& sampler = argTex->getType().getSampler();

            const int  numArgs = (int)argAggregate->getSequence().size();

            if (numArgs == 5) // offset, if present
                argOffset = argAggregate->getSequence()[4]->getAsTyped();

            const TOperator textureOp = (argOffset == nullptr ? EOpTextureLod : EOpTextureLodOffset);
            TIntermAggregate* txsample = new TIntermAggregate(textureOp);

            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);

            txsample->getSequence().push_back(txcombine);
            txsample->getSequence().push_back(argCoord);
            txsample->getSequence().push_back(argLod);

            if (argOffset != nullptr)
                txsample->getSequence().push_back(argOffset);

            node = convertReturn(txsample, sampler);

            break;
        }

    case EOpMethodGather:
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSamp   = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[2]->getAsTyped();
            TIntermTyped* argOffset = nullptr;

            // Offset is optional
            if (argAggregate->getSequence().size() > 3)
                argOffset = argAggregate->getSequence()[3]->getAsTyped();

            const TOperator textureOp = (argOffset == nullptr ? EOpTextureGather : EOpTextureGatherOffset);
            TIntermAggregate* txgather = new TIntermAggregate(textureOp);

            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);

            txgather->getSequence().push_back(txcombine);
            txgather->getSequence().push_back(argCoord);
            // Offset if not given is implicitly channel 0 (red)

            if (argOffset != nullptr)
                txgather->getSequence().push_back(argOffset);

            txgather->setType(node->getType());
            txgather->setLoc(loc);
            node = txgather;

            break;
        }

    case EOpMethodGatherRed:      // fall through...
    case EOpMethodGatherGreen:    // ...
    case EOpMethodGatherBlue:     // ...
    case EOpMethodGatherAlpha:    // ...
    case EOpMethodGatherCmpRed:   // ...
    case EOpMethodGatherCmpGreen: // ...
    case EOpMethodGatherCmpBlue:  // ...
    case EOpMethodGatherCmpAlpha: // ...
        {
            int channel = 0;    // the channel we are gathering
            int cmpValues = 0;  // 1 if there is a compare value (handier than a bool below)

            switch (op) {
            case EOpMethodGatherCmpRed:   cmpValues = 1;  // fall through
            case EOpMethodGatherRed:      channel = 0; break;
            case EOpMethodGatherCmpGreen: cmpValues = 1;  // fall through
            case EOpMethodGatherGreen:    channel = 1; break;
            case EOpMethodGatherCmpBlue:  cmpValues = 1;  // fall through
            case EOpMethodGatherBlue:     channel = 2; break;
            case EOpMethodGatherCmpAlpha: cmpValues = 1;  // fall through
            case EOpMethodGatherAlpha:    channel = 3; break;
            default:                      assert(0);   break;
            }

            // For now, we have nothing to map the component-wise comparison forms
            // to, because neither GLSL nor SPIR-V has such an opcode.  Issue an
            // unimplemented error instead.  Most of the machinery is here if that
            // should ever become available.  However, red can be passed through
            // to OpImageDrefGather.  G/B/A cannot, because that opcode does not
            // accept a component.
            if (cmpValues != 0 && op != EOpMethodGatherCmpRed) {
                error(loc, "unimplemented: component-level gather compare", "", "");
                return;
            }

            int arg = 0;

            TIntermTyped* argTex        = argAggregate->getSequence()[arg++]->getAsTyped();
            TIntermTyped* argSamp       = argAggregate->getSequence()[arg++]->getAsTyped();
            TIntermTyped* argCoord      = argAggregate->getSequence()[arg++]->getAsTyped();
            TIntermTyped* argOffset     = nullptr;
            TIntermTyped* argOffsets[4] = { nullptr, nullptr, nullptr, nullptr };
            // TIntermTyped* argStatus     = nullptr; // TODO: residency
            TIntermTyped* argCmp        = nullptr;

            const TSamplerDim dim = argTex->getType().getSampler().dim;

            const int  argSize = (int)argAggregate->getSequence().size();
            bool hasStatus     = (argSize == (5+cmpValues) || argSize == (8+cmpValues));
            bool hasOffset1    = false;
            bool hasOffset4    = false;

            // Sampler argument should be a sampler.
            if (argSamp->getType().getBasicType() != EbtSampler) {
                error(loc, "expected: sampler type", "", "");
                return;
            }

            // Cmp forms require SamplerComparisonState
            if (cmpValues > 0 && ! argSamp->getType().getSampler().isShadow()) {
                error(loc, "expected: SamplerComparisonState", "", "");
                return;
            }

            // Only 2D forms can have offsets.  Discover if we have 0, 1 or 4 offsets.
            if (dim == Esd2D) {
                hasOffset1 = (argSize == (4+cmpValues) || argSize == (5+cmpValues));
                hasOffset4 = (argSize == (7+cmpValues) || argSize == (8+cmpValues));
            }

            assert(!(hasOffset1 && hasOffset4));

            TOperator textureOp = EOpTextureGather;

            // Compare forms have compare value
            if (cmpValues != 0)
                argCmp = argOffset = argAggregate->getSequence()[arg++]->getAsTyped();

            // Some forms have single offset
            if (hasOffset1) {
                textureOp = EOpTextureGatherOffset;   // single offset form
                argOffset = argAggregate->getSequence()[arg++]->getAsTyped();
            }

            // Some forms have 4 gather offsets
            if (hasOffset4) {
                textureOp = EOpTextureGatherOffsets;  // note plural, for 4 offset form
                for (int offsetNum = 0; offsetNum < 4; ++offsetNum)
                    argOffsets[offsetNum] = argAggregate->getSequence()[arg++]->getAsTyped();
            }

            // Residency status
            if (hasStatus) {
                // argStatus = argAggregate->getSequence()[arg++]->getAsTyped();
                error(loc, "unimplemented: residency status", "", "");
                return;
            }

            TIntermAggregate* txgather = new TIntermAggregate(textureOp);
            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);

            TIntermTyped* argChannel = intermediate.addConstantUnion(channel, loc, true);

            txgather->getSequence().push_back(txcombine);
            txgather->getSequence().push_back(argCoord);

            // AST wants an array of 4 offsets, where HLSL has separate args.  Here
            // we construct an array from the separate args.
            if (hasOffset4) {
                TType arrayType(EbtInt, EvqTemporary, 2);
                TArraySizes* arraySizes = new TArraySizes;
                arraySizes->addInnerSize(4);
                arrayType.transferArraySizes(arraySizes);

                TIntermAggregate* initList = new TIntermAggregate(EOpNull);

                for (int offsetNum = 0; offsetNum < 4; ++offsetNum)
                    initList->getSequence().push_back(argOffsets[offsetNum]);

                argOffset = addConstructor(loc, initList, arrayType);
            }

            // Add comparison value if we have one
            if (argCmp != nullptr)
                txgather->getSequence().push_back(argCmp);

            // Add offset (either 1, or an array of 4) if we have one
            if (argOffset != nullptr)
                txgather->getSequence().push_back(argOffset);

            // Add channel value if the sampler is not shadow
            if (! argSamp->getType().getSampler().isShadow())
                txgather->getSequence().push_back(argChannel);

            txgather->setType(node->getType());
            txgather->setLoc(loc);
            node = txgather;

            break;
        }

    case EOpMethodCalculateLevelOfDetail:
    case EOpMethodCalculateLevelOfDetailUnclamped:
        {
            TIntermTyped* argTex    = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSamp   = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* argCoord  = argAggregate->getSequence()[2]->getAsTyped();

            TIntermAggregate* txquerylod = new TIntermAggregate(EOpTextureQueryLod);

            TIntermAggregate* txcombine = handleSamplerTextureCombine(loc, argTex, argSamp);
            txquerylod->getSequence().push_back(txcombine);
            txquerylod->getSequence().push_back(argCoord);

            TIntermTyped* lodComponent = intermediate.addConstantUnion(
                op == EOpMethodCalculateLevelOfDetail ? 0 : 1,
                loc, true);
            TIntermTyped* lodComponentIdx = intermediate.addIndex(EOpIndexDirect, txquerylod, lodComponent, loc);
            lodComponentIdx->setType(TType(EbtFloat, EvqTemporary, 1));
            node = lodComponentIdx;

            break;
        }

    case EOpMethodGetSamplePosition:
        {
            // TODO: this entire decomposition exists because there is not yet a way to query
            // the sample position directly through SPIR-V.  Instead, we return fixed sample
            // positions for common cases.  *** If the sample positions are set differently,
            // this will be wrong. ***

            TIntermTyped* argTex     = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* argSampIdx = argAggregate->getSequence()[1]->getAsTyped();

            TIntermAggregate* samplesQuery = new TIntermAggregate(EOpImageQuerySamples);
            samplesQuery->getSequence().push_back(argTex);
            samplesQuery->setType(TType(EbtUint, EvqTemporary, 1));
            samplesQuery->setLoc(loc);

            TIntermAggregate* compoundStatement = nullptr;

            TVariable* outSampleCount = makeInternalVariable("@sampleCount", TType(EbtUint));
            outSampleCount->getWritableType().getQualifier().makeTemporary();
            TIntermTyped* compAssign = intermediate.addAssign(EOpAssign, intermediate.addSymbol(*outSampleCount, loc),
                                                              samplesQuery, loc);
            compoundStatement = intermediate.growAggregate(compoundStatement, compAssign);

            TIntermTyped* idxtest[4];

            // Create tests against 2, 4, 8, and 16 sample values
            int count = 0;
            for (int val = 2; val <= 16; val *= 2)
                idxtest[count++] =
                    intermediate.addBinaryNode(EOpEqual,
                                               intermediate.addSymbol(*outSampleCount, loc),
                                               intermediate.addConstantUnion(val, loc),
                                               loc, TType(EbtBool));

            const TOperator idxOp = (argSampIdx->getQualifier().storage == EvqConst) ? EOpIndexDirect : EOpIndexIndirect;

            // Create index ops into position arrays given sample index.
            // TODO: should it be clamped?
            TIntermTyped* index[4];
            count = 0;
            for (int val = 2; val <= 16; val *= 2) {
                index[count] = intermediate.addIndex(idxOp, getSamplePosArray(val), argSampIdx, loc);
                index[count++]->setType(TType(EbtFloat, EvqTemporary, 2));
            }

            // Create expression as:
            // (sampleCount == 2)  ? pos2[idx] :
            // (sampleCount == 4)  ? pos4[idx] :
            // (sampleCount == 8)  ? pos8[idx] :
            // (sampleCount == 16) ? pos16[idx] : float2(0,0);
            TIntermTyped* test =
                intermediate.addSelection(idxtest[0], index[0],
                    intermediate.addSelection(idxtest[1], index[1],
                        intermediate.addSelection(idxtest[2], index[2],
                            intermediate.addSelection(idxtest[3], index[3],
                                                      getSamplePosArray(1), loc), loc), loc), loc);

            compoundStatement = intermediate.growAggregate(compoundStatement, test);
            compoundStatement->setOperator(EOpSequence);
            compoundStatement->setLoc(loc);
            compoundStatement->setType(TType(EbtFloat, EvqTemporary, 2));

            node = compoundStatement;

            break;
        }

    case EOpSubpassLoad:
        {
            const TIntermTyped* argSubpass =
                argAggregate ? argAggregate->getSequence()[0]->getAsTyped() :
                arguments->getAsTyped();

            const TSampler& sampler = argSubpass->getType().getSampler();

            // subpass load: the multisample form is overloaded.  Here, we convert that to
            // the EOpSubpassLoadMS opcode.
            if (argAggregate != nullptr && argAggregate->getSequence().size() > 1)
                node->getAsOperator()->setOp(EOpSubpassLoadMS);

            node = convertReturn(node, sampler);

            break;
        }


    default:
        break; // most pass through unchanged
    }
}

//
// Decompose geometry shader methods
//
void HlslParseContext::decomposeGeometryMethods(const TSourceLoc& loc, TIntermTyped*& node, TIntermNode* arguments)
{
    if (node == nullptr || !node->getAsOperator())
        return;

    const TOperator op  = node->getAsOperator()->getOp();
    const TIntermAggregate* argAggregate = arguments ? arguments->getAsAggregate() : nullptr;

    switch (op) {
    case EOpMethodAppend:
        if (argAggregate) {
            // Don't emit these for non-GS stage, since we won't have the gsStreamOutput symbol.
            if (language != EShLangGeometry) {
                node = nullptr;
                return;
            }

            TIntermAggregate* sequence = nullptr;
            TIntermAggregate* emit = new TIntermAggregate(EOpEmitVertex);

            emit->setLoc(loc);
            emit->setType(TType(EbtVoid));

            TIntermTyped* data = argAggregate->getSequence()[1]->getAsTyped();

            // This will be patched in finalization during finalizeAppendMethods()
            sequence = intermediate.growAggregate(sequence, data, loc);
            sequence = intermediate.growAggregate(sequence, emit);

            sequence->setOperator(EOpSequence);
            sequence->setLoc(loc);
            sequence->setType(TType(EbtVoid));

            gsAppends.push_back({sequence, loc});

            node = sequence;
        }
        break;

    case EOpMethodRestartStrip:
        {
            // Don't emit these for non-GS stage, since we won't have the gsStreamOutput symbol.
            if (language != EShLangGeometry) {
                node = nullptr;
                return;
            }

            TIntermAggregate* cut = new TIntermAggregate(EOpEndPrimitive);
            cut->setLoc(loc);
            cut->setType(TType(EbtVoid));
            node = cut;
        }
        break;

    default:
        break; // most pass through unchanged
    }
}

//
// Optionally decompose intrinsics to AST opcodes.
//
void HlslParseContext::decomposeIntrinsic(const TSourceLoc& loc, TIntermTyped*& node, TIntermNode* arguments)
{
    // Helper to find image data for image atomics:
    // OpImageLoad(image[idx])
    // We take the image load apart and add its params to the atomic op aggregate node
    const auto imageAtomicParams = [this, &loc, &node](TIntermAggregate* atomic, TIntermTyped* load) {
        TIntermAggregate* loadOp = load->getAsAggregate();
        if (loadOp == nullptr) {
            error(loc, "unknown image type in atomic operation", "", "");
            node = nullptr;
            return;
        }

        atomic->getSequence().push_back(loadOp->getSequence()[0]);
        atomic->getSequence().push_back(loadOp->getSequence()[1]);
    };

    // Return true if this is an imageLoad, which we will change to an image atomic.
    const auto isImageParam = [](TIntermTyped* image) -> bool {
        TIntermAggregate* imageAggregate = image->getAsAggregate();
        return imageAggregate != nullptr && imageAggregate->getOp() == EOpImageLoad;
    };

    const auto lookupBuiltinVariable = [&](const char* name, TBuiltInVariable builtin, TType& type) -> TIntermTyped* {
        TSymbol* symbol = symbolTable.find(name);
        if (nullptr == symbol) {
            type.getQualifier().builtIn = builtin;

            TVariable* variable = new TVariable(NewPoolTString(name), type);

            symbolTable.insert(*variable);

            symbol = symbolTable.find(name);
            assert(symbol && "Inserted symbol could not be found!");
        }

        return intermediate.addSymbol(*(symbol->getAsVariable()), loc);
    };

    // HLSL intrinsics can be pass through to native AST opcodes, or decomposed here to existing AST
    // opcodes for compatibility with existing software stacks.
    static const bool decomposeHlslIntrinsics = true;

    if (!decomposeHlslIntrinsics || !node || !node->getAsOperator())
        return;

    const TIntermAggregate* argAggregate = arguments ? arguments->getAsAggregate() : nullptr;
    TIntermUnary* fnUnary = node->getAsUnaryNode();
    const TOperator op  = node->getAsOperator()->getOp();

    switch (op) {
    case EOpGenMul:
        {
            // mul(a,b) -> MatrixTimesMatrix, MatrixTimesVector, MatrixTimesScalar, VectorTimesScalar, Dot, Mul
            // Since we are treating HLSL rows like GLSL columns (the first matrix indirection),
            // we must reverse the operand order here.  Hence, arg0 gets sequence[1], etc.
            TIntermTyped* arg0 = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* arg1 = argAggregate->getSequence()[0]->getAsTyped();

            if (arg0->isVector() && arg1->isVector()) {  // vec * vec
                node->getAsAggregate()->setOperator(EOpDot);
            } else {
                node = handleBinaryMath(loc, "mul", EOpMul, arg0, arg1);
            }

            break;
        }

    case EOpRcp:
        {
            // rcp(a) -> 1 / a
            TIntermTyped* arg0 = fnUnary->getOperand();
            TBasicType   type0 = arg0->getBasicType();
            TIntermTyped* one  = intermediate.addConstantUnion(1, type0, loc, true);
            node  = handleBinaryMath(loc, "rcp", EOpDiv, one, arg0);

            break;
        }

    case EOpAny: // fall through
    case EOpAll:
        {
            TIntermTyped* typedArg = arguments->getAsTyped();

            // HLSL allows float/etc types here, and the SPIR-V opcode requires a bool.
            // We'll convert here.  Note that for efficiency, we could add a smarter
            // decomposition for some type cases, e.g, maybe by decomposing a dot product.
            if (typedArg->getType().getBasicType() != EbtBool) {
                const TType boolType(EbtBool, EvqTemporary,
                                     typedArg->getVectorSize(),
                                     typedArg->getMatrixCols(),
                                     typedArg->getMatrixRows(),
                                     typedArg->isVector());

                typedArg = intermediate.addConversion(EOpConstructBool, boolType, typedArg);
                node->getAsUnaryNode()->setOperand(typedArg);
            }

            break;
        }

    case EOpSaturate:
        {
            // saturate(a) -> clamp(a,0,1)
            TIntermTyped* arg0 = fnUnary->getOperand();
            TBasicType   type0 = arg0->getBasicType();
            TIntermAggregate* clamp = new TIntermAggregate(EOpClamp);

            clamp->getSequence().push_back(arg0);
            clamp->getSequence().push_back(intermediate.addConstantUnion(0, type0, loc, true));
            clamp->getSequence().push_back(intermediate.addConstantUnion(1, type0, loc, true));
            clamp->setLoc(loc);
            clamp->setType(node->getType());
            clamp->getWritableType().getQualifier().makeTemporary();
            node = clamp;

            break;
        }

    case EOpSinCos:
        {
            // sincos(a,b,c) -> b = sin(a), c = cos(a)
            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* arg2 = argAggregate->getSequence()[2]->getAsTyped();

            TIntermTyped* sinStatement = handleUnaryMath(loc, "sin", EOpSin, arg0);
            TIntermTyped* cosStatement = handleUnaryMath(loc, "cos", EOpCos, arg0);
            TIntermTyped* sinAssign    = intermediate.addAssign(EOpAssign, arg1, sinStatement, loc);
            TIntermTyped* cosAssign    = intermediate.addAssign(EOpAssign, arg2, cosStatement, loc);

            TIntermAggregate* compoundStatement = intermediate.makeAggregate(sinAssign, loc);
            compoundStatement = intermediate.growAggregate(compoundStatement, cosAssign);
            compoundStatement->setOperator(EOpSequence);
            compoundStatement->setLoc(loc);
            compoundStatement->setType(TType(EbtVoid));

            node = compoundStatement;

            break;
        }

    case EOpClip:
        {
            // clip(a) -> if (any(a<0)) discard;
            TIntermTyped*  arg0 = fnUnary->getOperand();
            TBasicType     type0 = arg0->getBasicType();
            TIntermTyped*  compareNode = nullptr;

            // For non-scalars: per experiment with FXC compiler, discard if any component < 0.
            if (!arg0->isScalar()) {
                // component-wise compare: a < 0
                TIntermAggregate* less = new TIntermAggregate(EOpLessThan);
                less->getSequence().push_back(arg0);
                less->setLoc(loc);

                // make vec or mat of bool matching dimensions of input
                less->setType(TType(EbtBool, EvqTemporary,
                                    arg0->getType().getVectorSize(),
                                    arg0->getType().getMatrixCols(),
                                    arg0->getType().getMatrixRows(),
                                    arg0->getType().isVector()));

                // calculate # of components for comparison const
                const int constComponentCount =
                    std::max(arg0->getType().getVectorSize(), 1) *
                    std::max(arg0->getType().getMatrixCols(), 1) *
                    std::max(arg0->getType().getMatrixRows(), 1);

                TConstUnion zero;
                if (arg0->getType().isIntegerDomain())
                    zero.setDConst(0);
                else
                    zero.setDConst(0.0);
                TConstUnionArray zeros(constComponentCount, zero);

                less->getSequence().push_back(intermediate.addConstantUnion(zeros, arg0->getType(), loc, true));

                compareNode = intermediate.addBuiltInFunctionCall(loc, EOpAny, true, less, TType(EbtBool));
            } else {
                TIntermTyped* zero;
                if (arg0->getType().isIntegerDomain())
                    zero = intermediate.addConstantUnion(0, loc, true);
                else
                    zero = intermediate.addConstantUnion(0.0, type0, loc, true);
                compareNode = handleBinaryMath(loc, "clip", EOpLessThan, arg0, zero);
            }

            TIntermBranch* killNode = intermediate.addBranch(EOpKill, loc);

            node = new TIntermSelection(compareNode, killNode, nullptr);
            node->setLoc(loc);

            break;
        }

    case EOpLog10:
        {
            // log10(a) -> log2(a) * 0.301029995663981  (== 1/log2(10))
            TIntermTyped* arg0 = fnUnary->getOperand();
            TIntermTyped* log2 = handleUnaryMath(loc, "log2", EOpLog2, arg0);
            TIntermTyped* base = intermediate.addConstantUnion(0.301029995663981f, EbtFloat, loc, true);

            node  = handleBinaryMath(loc, "mul", EOpMul, log2, base);

            break;
        }

    case EOpDst:
        {
            // dest.x = 1;
            // dest.y = src0.y * src1.y;
            // dest.z = src0.z;
            // dest.w = src1.w;

            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();

            TIntermTyped* y = intermediate.addConstantUnion(1, loc, true);
            TIntermTyped* z = intermediate.addConstantUnion(2, loc, true);
            TIntermTyped* w = intermediate.addConstantUnion(3, loc, true);

            TIntermTyped* src0y = intermediate.addIndex(EOpIndexDirect, arg0, y, loc);
            TIntermTyped* src1y = intermediate.addIndex(EOpIndexDirect, arg1, y, loc);
            TIntermTyped* src0z = intermediate.addIndex(EOpIndexDirect, arg0, z, loc);
            TIntermTyped* src1w = intermediate.addIndex(EOpIndexDirect, arg1, w, loc);

            TIntermAggregate* dst = new TIntermAggregate(EOpConstructVec4);

            dst->getSequence().push_back(intermediate.addConstantUnion(1.0, EbtFloat, loc, true));
            dst->getSequence().push_back(handleBinaryMath(loc, "mul", EOpMul, src0y, src1y));
            dst->getSequence().push_back(src0z);
            dst->getSequence().push_back(src1w);
            dst->setType(TType(EbtFloat, EvqTemporary, 4));
            dst->setLoc(loc);
            node = dst;

            break;
        }

    case EOpInterlockedAdd: // optional last argument (if present) is assigned from return value
    case EOpInterlockedMin: // ...
    case EOpInterlockedMax: // ...
    case EOpInterlockedAnd: // ...
    case EOpInterlockedOr:  // ...
    case EOpInterlockedXor: // ...
    case EOpInterlockedExchange: // always has output arg
        {
            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();  // dest
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();  // value
            TIntermTyped* arg2 = nullptr;

            if (argAggregate->getSequence().size() > 2)
                arg2 = argAggregate->getSequence()[2]->getAsTyped();

            const bool isImage = isImageParam(arg0);
            const TOperator atomicOp = mapAtomicOp(loc, op, isImage);
            TIntermAggregate* atomic = new TIntermAggregate(atomicOp);
            atomic->setType(arg0->getType());
            atomic->getWritableType().getQualifier().makeTemporary();
            atomic->setLoc(loc);

            if (isImage) {
                // orig_value = imageAtomicOp(image, loc, data)
                imageAtomicParams(atomic, arg0);
                atomic->getSequence().push_back(arg1);

                if (argAggregate->getSequence().size() > 2) {
                    node = intermediate.addAssign(EOpAssign, arg2, atomic, loc);
                } else {
                    node = atomic; // no assignment needed, as there was no out var.
                }
            } else {
                // Normal memory variable:
                // arg0 = mem, arg1 = data, arg2(optional,out) = orig_value
                if (argAggregate->getSequence().size() > 2) {
                    // optional output param is present.  return value goes to arg2.
                    atomic->getSequence().push_back(arg0);
                    atomic->getSequence().push_back(arg1);

                    node = intermediate.addAssign(EOpAssign, arg2, atomic, loc);
                } else {
                    // Set the matching operator.  Since output is absent, this is all we need to do.
                    node->getAsAggregate()->setOperator(atomicOp);
                    node->setType(atomic->getType());
                }
            }

            break;
        }

    case EOpInterlockedCompareExchange:
        {
            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();  // dest
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();  // cmp
            TIntermTyped* arg2 = argAggregate->getSequence()[2]->getAsTyped();  // value
            TIntermTyped* arg3 = argAggregate->getSequence()[3]->getAsTyped();  // orig

            const bool isImage = isImageParam(arg0);
            TIntermAggregate* atomic = new TIntermAggregate(mapAtomicOp(loc, op, isImage));
            atomic->setLoc(loc);
            atomic->setType(arg2->getType());
            atomic->getWritableType().getQualifier().makeTemporary();

            if (isImage) {
                imageAtomicParams(atomic, arg0);
            } else {
                atomic->getSequence().push_back(arg0);
            }

            atomic->getSequence().push_back(arg1);
            atomic->getSequence().push_back(arg2);
            node = intermediate.addAssign(EOpAssign, arg3, atomic, loc);

            break;
        }

    case EOpEvaluateAttributeSnapped:
        {
            // SPIR-V InterpolateAtOffset uses float vec2 offset in pixels
            // HLSL uses int2 offset on a 16x16 grid in [-8..7] on x & y:
            //   iU = (iU<<28)>>28
            //   fU = ((float)iU)/16
            // Targets might handle this natively, in which case they can disable
            // decompositions.

            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();  // value
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();  // offset

            TIntermTyped* i28 = intermediate.addConstantUnion(28, loc, true);
            TIntermTyped* iU = handleBinaryMath(loc, ">>", EOpRightShift,
                                                handleBinaryMath(loc, "<<", EOpLeftShift, arg1, i28),
                                                i28);

            TIntermTyped* recip16 = intermediate.addConstantUnion((1.0/16.0), EbtFloat, loc, true);
            TIntermTyped* floatOffset = handleBinaryMath(loc, "mul", EOpMul,
                                                         intermediate.addConversion(EOpConstructFloat,
                                                                                    TType(EbtFloat, EvqTemporary, 2), iU),
                                                         recip16);

            TIntermAggregate* interp = new TIntermAggregate(EOpInterpolateAtOffset);
            interp->getSequence().push_back(arg0);
            interp->getSequence().push_back(floatOffset);
            interp->setLoc(loc);
            interp->setType(arg0->getType());
            interp->getWritableType().getQualifier().makeTemporary();

            node = interp;

            break;
        }

    case EOpLit:
        {
            TIntermTyped* n_dot_l = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* n_dot_h = argAggregate->getSequence()[1]->getAsTyped();
            TIntermTyped* m = argAggregate->getSequence()[2]->getAsTyped();

            TIntermAggregate* dst = new TIntermAggregate(EOpConstructVec4);

            // Ambient
            dst->getSequence().push_back(intermediate.addConstantUnion(1.0, EbtFloat, loc, true));

            // Diffuse:
            TIntermTyped* zero = intermediate.addConstantUnion(0.0, EbtFloat, loc, true);
            TIntermAggregate* diffuse = new TIntermAggregate(EOpMax);
            diffuse->getSequence().push_back(n_dot_l);
            diffuse->getSequence().push_back(zero);
            diffuse->setLoc(loc);
            diffuse->setType(TType(EbtFloat));
            dst->getSequence().push_back(diffuse);

            // Specular:
            TIntermAggregate* min_ndot = new TIntermAggregate(EOpMin);
            min_ndot->getSequence().push_back(n_dot_l);
            min_ndot->getSequence().push_back(n_dot_h);
            min_ndot->setLoc(loc);
            min_ndot->setType(TType(EbtFloat));

            TIntermTyped* compare = handleBinaryMath(loc, "<", EOpLessThan, min_ndot, zero);
            TIntermTyped* n_dot_h_m = handleBinaryMath(loc, "mul", EOpMul, n_dot_h, m);  // n_dot_h * m

            dst->getSequence().push_back(intermediate.addSelection(compare, zero, n_dot_h_m, loc));

            // One:
            dst->getSequence().push_back(intermediate.addConstantUnion(1.0, EbtFloat, loc, true));

            dst->setLoc(loc);
            dst->setType(TType(EbtFloat, EvqTemporary, 4));
            node = dst;
            break;
        }

    case EOpAsDouble:
        {
            // asdouble accepts two 32 bit ints.  we can use EOpUint64BitsToDouble, but must
            // first construct a uint64.
            TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();
            TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();

            if (arg0->getType().isVector()) { // TODO: ...
                error(loc, "double2 conversion not implemented", "asdouble", "");
                break;
            }

            TIntermAggregate* uint64 = new TIntermAggregate(EOpConstructUVec2);

            uint64->getSequence().push_back(arg0);
            uint64->getSequence().push_back(arg1);
            uint64->setType(TType(EbtUint, EvqTemporary, 2));  // convert 2 uints to a uint2
            uint64->setLoc(loc);

            // bitcast uint2 to a double
            TIntermTyped* convert = new TIntermUnary(EOpUint64BitsToDouble);
            convert->getAsUnaryNode()->setOperand(uint64);
            convert->setLoc(loc);
            convert->setType(TType(EbtDouble, EvqTemporary));
            node = convert;

            break;
        }

    case EOpF16tof32:
        {
            // input uvecN with low 16 bits of each component holding a float16.  convert to float32.
            TIntermTyped* argValue = node->getAsUnaryNode()->getOperand();
            TIntermTyped* zero = intermediate.addConstantUnion(0, loc, true);
            const int vecSize = argValue->getType().getVectorSize();

            TOperator constructOp = EOpNull;
            switch (vecSize) {
            case 1: constructOp = EOpNull;          break; // direct use, no construct needed
            case 2: constructOp = EOpConstructVec2; break;
            case 3: constructOp = EOpConstructVec3; break;
            case 4: constructOp = EOpConstructVec4; break;
            default: assert(0); break;
            }

            // For scalar case, we don't need to construct another type.
            TIntermAggregate* result = (vecSize > 1) ? new TIntermAggregate(constructOp) : nullptr;

            if (result) {
                result->setType(TType(EbtFloat, EvqTemporary, vecSize));
                result->setLoc(loc);
            }

            for (int idx = 0; idx < vecSize; ++idx) {
                TIntermTyped* idxConst = intermediate.addConstantUnion(idx, loc, true);
                TIntermTyped* component = argValue->getType().isVector() ?
                    intermediate.addIndex(EOpIndexDirect, argValue, idxConst, loc) : argValue;

                if (component != argValue)
                    component->setType(TType(argValue->getBasicType(), EvqTemporary));

                TIntermTyped* unpackOp  = new TIntermUnary(EOpUnpackHalf2x16);
                unpackOp->setType(TType(EbtFloat, EvqTemporary, 2));
                unpackOp->getAsUnaryNode()->setOperand(component);
                unpackOp->setLoc(loc);

                TIntermTyped* lowOrder  = intermediate.addIndex(EOpIndexDirect, unpackOp, zero, loc);

                if (result != nullptr) {
                    result->getSequence().push_back(lowOrder);
                    node = result;
                } else {
                    node = lowOrder;
                }
            }

            break;
        }

    case EOpF32tof16:
        {
            // input floatN converted to 16 bit float in low order bits of each component of uintN
            TIntermTyped* argValue = node->getAsUnaryNode()->getOperand();

            TIntermTyped* zero = intermediate.addConstantUnion(0.0, EbtFloat, loc, true);
            const int vecSize = argValue->getType().getVectorSize();

            TOperator constructOp = EOpNull;
            switch (vecSize) {
            case 1: constructOp = EOpNull;           break; // direct use, no construct needed
            case 2: constructOp = EOpConstructUVec2; break;
            case 3: constructOp = EOpConstructUVec3; break;
            case 4: constructOp = EOpConstructUVec4; break;
            default: assert(0); break;
            }

            // For scalar case, we don't need to construct another type.
            TIntermAggregate* result = (vecSize > 1) ? new TIntermAggregate(constructOp) : nullptr;

            if (result) {
                result->setType(TType(EbtUint, EvqTemporary, vecSize));
                result->setLoc(loc);
            }

            for (int idx = 0; idx < vecSize; ++idx) {
                TIntermTyped* idxConst = intermediate.addConstantUnion(idx, loc, true);
                TIntermTyped* component = argValue->getType().isVector() ?
                    intermediate.addIndex(EOpIndexDirect, argValue, idxConst, loc) : argValue;

                if (component != argValue)
                    component->setType(TType(argValue->getBasicType(), EvqTemporary));

                TIntermAggregate* vec2ComponentAndZero = new TIntermAggregate(EOpConstructVec2);
                vec2ComponentAndZero->getSequence().push_back(component);
                vec2ComponentAndZero->getSequence().push_back(zero);
                vec2ComponentAndZero->setType(TType(EbtFloat, EvqTemporary, 2));
                vec2ComponentAndZero->setLoc(loc);

                TIntermTyped* packOp = new TIntermUnary(EOpPackHalf2x16);
                packOp->getAsUnaryNode()->setOperand(vec2ComponentAndZero);
                packOp->setLoc(loc);
                packOp->setType(TType(EbtUint, EvqTemporary));

                if (result != nullptr) {
                    result->getSequence().push_back(packOp);
                    node = result;
                } else {
                    node = packOp;
                }
            }

            break;
        }

    case EOpD3DCOLORtoUBYTE4:
        {
            // ivec4 ( x.zyxw * 255.001953 );
            TIntermTyped* arg0 = node->getAsUnaryNode()->getOperand();
            TSwizzleSelectors<TVectorSelector> selectors;
            selectors.push_back(2);
            selectors.push_back(1);
            selectors.push_back(0);
            selectors.push_back(3);
            TIntermTyped* swizzleIdx = intermediate.addSwizzle(selectors, loc);
            TIntermTyped* swizzled = intermediate.addIndex(EOpVectorSwizzle, arg0, swizzleIdx, loc);
            swizzled->setType(arg0->getType());
            swizzled->getWritableType().getQualifier().makeTemporary();

            TIntermTyped* conversion = intermediate.addConstantUnion(255.001953f, EbtFloat, loc, true);
            TIntermTyped* rangeConverted = handleBinaryMath(loc, "mul", EOpMul, conversion, swizzled);
            rangeConverted->setType(arg0->getType());
            rangeConverted->getWritableType().getQualifier().makeTemporary();

            node = intermediate.addConversion(EOpConstructInt, TType(EbtInt, EvqTemporary, 4), rangeConverted);
            node->setLoc(loc);
            node->setType(TType(EbtInt, EvqTemporary, 4));
            break;
        }

    case EOpIsFinite:
        {
            // Since OPIsFinite in SPIR-V is only supported with the Kernel capability, we translate
            // it to !isnan && !isinf

            TIntermTyped* arg0 = node->getAsUnaryNode()->getOperand();

            // We'll make a temporary in case the RHS is cmoplex
            TVariable* tempArg = makeInternalVariable("@finitetmp", arg0->getType());
            tempArg->getWritableType().getQualifier().makeTemporary();

            TIntermTyped* tmpArgAssign = intermediate.addAssign(EOpAssign,
                                                                intermediate.addSymbol(*tempArg, loc),
                                                                arg0, loc);

            TIntermAggregate* compoundStatement = intermediate.makeAggregate(tmpArgAssign, loc);

            const TType boolType(EbtBool, EvqTemporary, arg0->getVectorSize(), arg0->getMatrixCols(),
                                 arg0->getMatrixRows());

            TIntermTyped* isnan = handleUnaryMath(loc, "isnan", EOpIsNan, intermediate.addSymbol(*tempArg, loc));
            isnan->setType(boolType);

            TIntermTyped* notnan = handleUnaryMath(loc, "!", EOpLogicalNot, isnan);
            notnan->setType(boolType);

            TIntermTyped* isinf = handleUnaryMath(loc, "isinf", EOpIsInf, intermediate.addSymbol(*tempArg, loc));
            isinf->setType(boolType);

            TIntermTyped* notinf = handleUnaryMath(loc, "!", EOpLogicalNot, isinf);
            notinf->setType(boolType);

            TIntermTyped* andNode = handleBinaryMath(loc, "and", EOpLogicalAnd, notnan, notinf);
            andNode->setType(boolType);

            compoundStatement = intermediate.growAggregate(compoundStatement, andNode);
            compoundStatement->setOperator(EOpSequence);
            compoundStatement->setLoc(loc);
            compoundStatement->setType(boolType);

            node = compoundStatement;

            break;
        }
    case EOpWaveGetLaneCount:
        {
            // Mapped to gl_SubgroupSize builtin (We preprend @ to the symbol
            // so that it inhabits the symbol table, but has a user-invalid name
            // in-case some source HLSL defined the symbol also).
            TType type(EbtUint, EvqVaryingIn);
            node = lookupBuiltinVariable("@gl_SubgroupSize", EbvSubgroupSize2, type);
            break;
        }
    case EOpWaveGetLaneIndex:
        {
            // Mapped to gl_SubgroupInvocationID builtin (We preprend @ to the
            // symbol so that it inhabits the symbol table, but has a
            // user-invalid name in-case some source HLSL defined the symbol
            // also).
            TType type(EbtUint, EvqVaryingIn);
            node = lookupBuiltinVariable("@gl_SubgroupInvocationID", EbvSubgroupInvocation2, type);
            break;
        }
    case EOpWaveActiveCountBits:
        {
            // Mapped to subgroupBallotBitCount(subgroupBallot()) builtin

            // uvec4 type.
            TType uvec4Type(EbtUint, EvqTemporary, 4);

            // Get the uvec4 return from subgroupBallot().
            TIntermTyped* res = intermediate.addBuiltInFunctionCall(loc,
                EOpSubgroupBallot, true, arguments, uvec4Type);

            // uint type.
            TType uintType(EbtUint, EvqTemporary);

            node = intermediate.addBuiltInFunctionCall(loc,
                EOpSubgroupBallotBitCount, true, res, uintType);

            break;
        }
    case EOpWavePrefixCountBits:
        {
            // Mapped to subgroupBallotExclusiveBitCount(subgroupBallot())
            // builtin

            // uvec4 type.
            TType uvec4Type(EbtUint, EvqTemporary, 4);

            // Get the uvec4 return from subgroupBallot().
            TIntermTyped* res = intermediate.addBuiltInFunctionCall(loc,
                EOpSubgroupBallot, true, arguments, uvec4Type);

            // uint type.
            TType uintType(EbtUint, EvqTemporary);

            node = intermediate.addBuiltInFunctionCall(loc,
                EOpSubgroupBallotExclusiveBitCount, true, res, uintType);

            break;
        }

    default:
        break; // most pass through unchanged
    }
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
TIntermTyped* HlslParseContext::handleFunctionCall(const TSourceLoc& loc, TFunction* function, TIntermTyped* arguments)
{
    TIntermTyped* result = nullptr;

    TOperator op = function->getBuiltInOp();
    if (op != EOpNull) {
        //
        // Then this should be a constructor.
        // Don't go through the symbol table for constructors.
        // Their parameters will be verified algorithmically.
        //
        TType type(EbtVoid);  // use this to get the type back
        if (! constructorError(loc, arguments, *function, op, type)) {
            //
            // It's a constructor, of type 'type'.
            //
            result = handleConstructor(loc, arguments, type);
            if (result == nullptr) {
                error(loc, "cannot construct with these arguments", type.getCompleteString().c_str(), "");
                return nullptr;
            }
        }
    } else {
        //
        // Find it in the symbol table.
        //
        const TFunction* fnCandidate = nullptr;
        bool builtIn = false;
        int thisDepth = 0;

        // For mat mul, the situation is unusual: we have to compare vector sizes to mat row or col sizes,
        // and clamp the opposite arg.  Since that's complex, we farm it off to a separate method.
        // It doesn't naturally fall out of processing an argument at a time in isolation.
        if (function->getName() == "mul")
            addGenMulArgumentConversion(loc, *function, arguments);

        TIntermAggregate* aggregate = arguments ? arguments->getAsAggregate() : nullptr;

        // TODO: this needs improvement: there's no way at present to look up a signature in
        // the symbol table for an arbitrary type.  This is a temporary hack until that ability exists.
        // It will have false positives, since it doesn't check arg counts or types.
        if (arguments) {
            // Check if first argument is struct buffer type.  It may be an aggregate or a symbol, so we
            // look for either case.

            TIntermTyped* arg0 = nullptr;

            if (aggregate && aggregate->getSequence().size() > 0 && aggregate->getSequence()[0])
                arg0 = aggregate->getSequence()[0]->getAsTyped();
            else if (arguments->getAsSymbolNode())
                arg0 = arguments->getAsSymbolNode();

            if (arg0 != nullptr && isStructBufferType(arg0->getType())) {
                static const int methodPrefixSize = sizeof(BUILTIN_PREFIX)-1;

                if (function->getName().length() > methodPrefixSize &&
                    isStructBufferMethod(function->getName().substr(methodPrefixSize))) {
                    const TString mangle = function->getName() + "(";
                    TSymbol* symbol = symbolTable.find(mangle, &builtIn);

                    if (symbol)
                        fnCandidate = symbol->getAsFunction();
                }
            }
        }

        if (fnCandidate == nullptr)
            fnCandidate = findFunction(loc, *function, builtIn, thisDepth, arguments);

        if (fnCandidate) {
            // This is a declared function that might map to
            //  - a built-in operator,
            //  - a built-in function not mapped to an operator, or
            //  - a user function.

            // turn an implicit member-function resolution into an explicit call
            TString callerName;
            if (thisDepth == 0)
                callerName = fnCandidate->getMangledName();
            else {
                // get the explicit (full) name of the function
                callerName = currentTypePrefix[currentTypePrefix.size() - thisDepth];
                callerName += fnCandidate->getMangledName();
                // insert the implicit calling argument
                pushFrontArguments(intermediate.addSymbol(*getImplicitThis(thisDepth)), arguments);
            }

            // Convert 'in' arguments, so that types match.
            // However, skip those that need expansion, that is covered next.
            if (arguments)
                addInputArgumentConversions(*fnCandidate, arguments);

            // Expand arguments.  Some arguments must physically expand to a different set
            // than what the shader declared and passes.
            if (arguments && !builtIn)
                expandArguments(loc, *fnCandidate, arguments);

            // Expansion may have changed the form of arguments
            aggregate = arguments ? arguments->getAsAggregate() : nullptr;

            op = fnCandidate->getBuiltInOp();
            if (builtIn && op != EOpNull) {
                // SM 4.0 and above guarantees roundEven semantics for round()
                if (!hlslDX9Compatible() && op == EOpRound)
                    op = EOpRoundEven;

                // A function call mapped to a built-in operation.
                result = intermediate.addBuiltInFunctionCall(loc, op, fnCandidate->getParamCount() == 1, arguments,
                                                             fnCandidate->getType());
                if (result == nullptr)  {
                    error(arguments->getLoc(), " wrong operand type", "Internal Error",
                        "built in unary operator function.  Type: %s",
                        static_cast<TIntermTyped*>(arguments)->getCompleteString().c_str());
                } else if (result->getAsOperator()) {
                    builtInOpCheck(loc, *fnCandidate, *result->getAsOperator());
                }
            } else {
                // This is a function call not mapped to built-in operator.
                // It could still be a built-in function, but only if PureOperatorBuiltins == false.
                result = intermediate.setAggregateOperator(arguments, EOpFunctionCall, fnCandidate->getType(), loc);
                TIntermAggregate* call = result->getAsAggregate();
                call->setName(callerName);

                // this is how we know whether the given function is a built-in function or a user-defined function
                // if builtIn == false, it's a userDefined -> could be an overloaded built-in function also
                // if builtIn == true, it's definitely a built-in function with EOpNull
                if (! builtIn) {
                    call->setUserDefined();
                    intermediate.addToCallGraph(infoSink, currentCaller, callerName);
                }
            }

            // for decompositions, since we want to operate on the function node, not the aggregate holding
            // output conversions.
            const TIntermTyped* fnNode = result;

            decomposeStructBufferMethods(loc, result, arguments); // HLSL->AST struct buffer method decompositions
            decomposeIntrinsic(loc, result, arguments);           // HLSL->AST intrinsic decompositions
            decomposeSampleMethods(loc, result, arguments);       // HLSL->AST sample method decompositions
            decomposeGeometryMethods(loc, result, arguments);     // HLSL->AST geometry method decompositions

            // Create the qualifier list, carried in the AST for the call.
            // Because some arguments expand to multiple arguments, the qualifier list will
            // be longer than the formal parameter list.
            if (result == fnNode && result->getAsAggregate()) {
                TQualifierList& qualifierList = result->getAsAggregate()->getQualifierList();
                for (int i = 0; i < fnCandidate->getParamCount(); ++i) {
                    TStorageQualifier qual = (*fnCandidate)[i].type->getQualifier().storage;
                    if (hasStructBuffCounter(*(*fnCandidate)[i].type)) {
                        // add buffer and counter buffer argument qualifier
                        qualifierList.push_back(qual);
                        qualifierList.push_back(qual);
                    } else if (shouldFlatten(*(*fnCandidate)[i].type, (*fnCandidate)[i].type->getQualifier().storage,
                                             true)) {
                        // add structure member expansion
                        for (int memb = 0; memb < (int)(*fnCandidate)[i].type->getStruct()->size(); ++memb)
                            qualifierList.push_back(qual);
                    } else {
                        // Normal 1:1 case
                        qualifierList.push_back(qual);
                    }
                }
            }

            // Convert 'out' arguments.  If it was a constant folded built-in, it won't be an aggregate anymore.
            // Built-ins with a single argument aren't called with an aggregate, but they also don't have an output.
            // Also, build the qualifier list for user function calls, which are always called with an aggregate.
            // We don't do this is if there has been a decomposition, which will have added its own conversions
            // for output parameters.
            if (result == fnNode && result->getAsAggregate())
                result = addOutputArgumentConversions(*fnCandidate, *result->getAsOperator());
        }
    }

    // generic error recovery
    // TODO: simplification: localize all the error recoveries that look like this, and taking type into account to
    //       reduce cascades
    if (result == nullptr)
        result = intermediate.addConstantUnion(0.0, EbtFloat, loc);

    return result;
}

// An initial argument list is difficult: it can be null, or a single node,
// or an aggregate if more than one argument.  Add one to the front, maintaining
// this lack of uniformity.
void HlslParseContext::pushFrontArguments(TIntermTyped* front, TIntermTyped*& arguments)
{
    if (arguments == nullptr)
        arguments = front;
    else if (arguments->getAsAggregate() != nullptr)
        arguments->getAsAggregate()->getSequence().insert(arguments->getAsAggregate()->getSequence().begin(), front);
    else
        arguments = intermediate.growAggregate(front, arguments);
}

//
// HLSL allows mismatched dimensions on vec*mat, mat*vec, vec*vec, and mat*mat.  This is a
// situation not well suited to resolution in intrinsic selection, but we can do so here, since we
// can look at both arguments insert explicit shape changes if required.
//
void HlslParseContext::addGenMulArgumentConversion(const TSourceLoc& loc, TFunction& call, TIntermTyped*& args)
{
    TIntermAggregate* argAggregate = args ? args->getAsAggregate() : nullptr;

    if (argAggregate == nullptr || argAggregate->getSequence().size() != 2) {
        // It really ought to have two arguments.
        error(loc, "expected: mul arguments", "", "");
        return;
    }

    TIntermTyped* arg0 = argAggregate->getSequence()[0]->getAsTyped();
    TIntermTyped* arg1 = argAggregate->getSequence()[1]->getAsTyped();

    if (arg0->isVector() && arg1->isVector()) {
        // For:
        //    vec * vec: it's handled during intrinsic selection, so while we could do it here,
        //               we can also ignore it, which is easier.
    } else if (arg0->isVector() && arg1->isMatrix()) {
        // vec * mat: we clamp the vec if the mat col is smaller, else clamp the mat col.
        if (arg0->getVectorSize() < arg1->getMatrixCols()) {
            // vec is smaller, so truncate larger mat dimension
            const TType truncType(arg1->getBasicType(), arg1->getQualifier().storage, arg1->getQualifier().precision,
                                  0, arg0->getVectorSize(), arg1->getMatrixRows());
            arg1 = addConstructor(loc, arg1, truncType);
        } else if (arg0->getVectorSize() > arg1->getMatrixCols()) {
            // vec is larger, so truncate vec to mat size
            const TType truncType(arg0->getBasicType(), arg0->getQualifier().storage, arg0->getQualifier().precision,
                                  arg1->getMatrixCols());
            arg0 = addConstructor(loc, arg0, truncType);
        }
    } else if (arg0->isMatrix() && arg1->isVector()) {
        // mat * vec: we clamp the vec if the mat col is smaller, else clamp the mat col.
        if (arg1->getVectorSize() < arg0->getMatrixRows()) {
            // vec is smaller, so truncate larger mat dimension
            const TType truncType(arg0->getBasicType(), arg0->getQualifier().storage, arg0->getQualifier().precision,
                                  0, arg0->getMatrixCols(), arg1->getVectorSize());
            arg0 = addConstructor(loc, arg0, truncType);
        } else if (arg1->getVectorSize() > arg0->getMatrixRows()) {
            // vec is larger, so truncate vec to mat size
            const TType truncType(arg1->getBasicType(), arg1->getQualifier().storage, arg1->getQualifier().precision,
                                  arg0->getMatrixRows());
            arg1 = addConstructor(loc, arg1, truncType);
        }
    } else if (arg0->isMatrix() && arg1->isMatrix()) {
        // mat * mat: we clamp the smaller inner dimension to match the other matrix size.
        // Remember, HLSL Mrc = GLSL/SPIRV Mcr.
        if (arg0->getMatrixRows() > arg1->getMatrixCols()) {
            const TType truncType(arg0->getBasicType(), arg0->getQualifier().storage, arg0->getQualifier().precision,
                                  0, arg0->getMatrixCols(), arg1->getMatrixCols());
            arg0 = addConstructor(loc, arg0, truncType);
        } else if (arg0->getMatrixRows() < arg1->getMatrixCols()) {
            const TType truncType(arg1->getBasicType(), arg1->getQualifier().storage, arg1->getQualifier().precision,
                                  0, arg0->getMatrixRows(), arg1->getMatrixRows());
            arg1 = addConstructor(loc, arg1, truncType);
        }
    } else {
        // It's something with scalars: we'll just leave it alone.  Function selection will handle it
        // downstream.
    }

    // Warn if we altered one of the arguments
    if (arg0 != argAggregate->getSequence()[0] || arg1 != argAggregate->getSequence()[1])
        warn(loc, "mul() matrix size mismatch", "", "");

    // Put arguments back.  (They might be unchanged, in which case this is harmless).
    argAggregate->getSequence()[0] = arg0;
    argAggregate->getSequence()[1] = arg1;

    call[0].type = &arg0->getWritableType();
    call[1].type = &arg1->getWritableType();
}

//
// Add any needed implicit conversions for function-call arguments to input parameters.
//
void HlslParseContext::addInputArgumentConversions(const TFunction& function, TIntermTyped*& arguments)
{
    TIntermAggregate* aggregate = arguments->getAsAggregate();

    // Replace a single argument with a single argument.
    const auto setArg = [&](int paramNum, TIntermTyped* arg) {
        if (function.getParamCount() == 1)
            arguments = arg;
        else {
            if (aggregate == nullptr)
                arguments = arg;
            else
                aggregate->getSequence()[paramNum] = arg;
        }
    };

    // Process each argument's conversion
    for (int param = 0; param < function.getParamCount(); ++param) {
        if (! function[param].type->getQualifier().isParamInput())
            continue;

        // At this early point there is a slight ambiguity between whether an aggregate 'arguments'
        // is the single argument itself or its children are the arguments.  Only one argument
        // means take 'arguments' itself as the one argument.
        TIntermTyped* arg = function.getParamCount() == 1
                                   ? arguments->getAsTyped()
                                   : (aggregate ?
                                        aggregate->getSequence()[param]->getAsTyped() :
                                        arguments->getAsTyped());
        if (*function[param].type != arg->getType()) {
            // In-qualified arguments just need an extra node added above the argument to
            // convert to the correct type.
            TIntermTyped* convArg = intermediate.addConversion(EOpFunctionCall, *function[param].type, arg);
            if (convArg != nullptr)
                convArg = intermediate.addUniShapeConversion(EOpFunctionCall, *function[param].type, convArg);
            if (convArg != nullptr)
                setArg(param, convArg);
            else
                error(arg->getLoc(), "cannot convert input argument, argument", "", "%d", param);
        } else {
            if (wasFlattened(arg)) {
                // If both formal and calling arg are to be flattened, leave that to argument
                // expansion, not conversion.
                if (!shouldFlatten(*function[param].type, function[param].type->getQualifier().storage, true)) {
                    // Will make a two-level subtree.
                    // The deepest will copy member-by-member to build the structure to pass.
                    // The level above that will be a two-operand EOpComma sequence that follows the copy by the
                    // object itself.
                    TVariable* internalAggregate = makeInternalVariable("aggShadow", *function[param].type);
                    internalAggregate->getWritableType().getQualifier().makeTemporary();
                    TIntermSymbol* internalSymbolNode = new TIntermSymbol(internalAggregate->getUniqueId(),
                                                                          internalAggregate->getName(),
                                                                          internalAggregate->getType());
                    internalSymbolNode->setLoc(arg->getLoc());
                    // This makes the deepest level, the member-wise copy
                    TIntermAggregate* assignAgg = handleAssign(arg->getLoc(), EOpAssign,
                                                               internalSymbolNode, arg)->getAsAggregate();

                    // Now, pair that with the resulting aggregate.
                    assignAgg = intermediate.growAggregate(assignAgg, internalSymbolNode, arg->getLoc());
                    assignAgg->setOperator(EOpComma);
                    assignAgg->setType(internalAggregate->getType());
                    setArg(param, assignAgg);
                }
            }
        }
    }
}

//
// Add any needed implicit expansion of calling arguments from what the shader listed to what's
// internally needed for the AST (given the constraints downstream).
//
void HlslParseContext::expandArguments(const TSourceLoc& loc, const TFunction& function, TIntermTyped*& arguments)
{
    TIntermAggregate* aggregate = arguments->getAsAggregate();
    int functionParamNumberOffset = 0;

    // Replace a single argument with a single argument.
    const auto setArg = [&](int paramNum, TIntermTyped* arg) {
        if (function.getParamCount() + functionParamNumberOffset == 1)
            arguments = arg;
        else {
            if (aggregate == nullptr)
                arguments = arg;
            else
                aggregate->getSequence()[paramNum] = arg;
        }
    };

    // Replace a single argument with a list of arguments
    const auto setArgList = [&](int paramNum, const TVector<TIntermTyped*>& args) {
        if (args.size() == 1)
            setArg(paramNum, args.front());
        else if (args.size() > 1) {
            if (function.getParamCount() + functionParamNumberOffset == 1) {
                arguments = intermediate.makeAggregate(args.front());
                std::for_each(args.begin() + 1, args.end(),
                    [&](TIntermTyped* arg) {
                        arguments = intermediate.growAggregate(arguments, arg);
                    });
            } else {
                auto it = aggregate->getSequence().erase(aggregate->getSequence().begin() + paramNum);
                aggregate->getSequence().insert(it, args.begin(), args.end());
            }
            functionParamNumberOf                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              