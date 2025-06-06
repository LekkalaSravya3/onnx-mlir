/*
 * SPDX-License-Identifier: Apache-2.0
 */

//===------------------ Transpose.cpp - ONNX Operations -------------------===//
//
// Copyright 2019-2022 The IBM Research Authors.
//
// =============================================================================
//
// This file provides definition of ONNX dialect Transpose operation.
//
//===----------------------------------------------------------------------===//

#include "src/Dialect/ONNX/ONNXOps/OpHelper.hpp"

using namespace mlir;
using namespace mlir::OpTrait::util;
using namespace onnx_mlir;

//===----------------------------------------------------------------------===//
// Support
//===----------------------------------------------------------------------===//

namespace onnx_mlir {
template <>
LogicalResult ONNXTransposeOpShapeHelper::computeShape() {
  // Basic information.
  ONNXTransposeOpAdaptor operandAdaptor(operands, op->getAttrDictionary());
  ONNXTransposeOp transposeOp = llvm::cast<ONNXTransposeOp>(op);

  Value data = operandAdaptor.getData();
  if (!hasShapeAndRank(data))
    return failure();
  int64_t rank = createIE->getShapedTypeRank(data);

  // Transposition which handles the default case of
  // reversing the shape of the tensor (similar to numpy.transpose).
  ArrayAttr permAttr = operandAdaptor.getPermAttr();
  if (!permAttr) {
    // Generate reverse order for default transpose operation.
    SmallVector<int64_t, 4> defaultVals;
    auto builder = Builder(op->getContext());
    for (int64_t i = rank - 1; i >= 0; --i)
      defaultVals.emplace_back(i);
    // Set default attribute.
    ArrayRef<int64_t> defaultRefs(defaultVals);
    transposeOp.setPermAttr(builder.getI64ArrayAttr(defaultRefs));
    permAttr = transposeOp.getPermAttr();
  }

  // Perform transposition according to perm attribute.
  DimsExpr transposedDims;
  for (int64_t i = 0; i < rank; ++i) {
    int64_t inputIndex = ArrayAttrIntVal(permAttr, i);
    if (inputIndex < 0)
      inputIndex += rank;
    assert(inputIndex >= 0 && inputIndex < rank &&
           "transpose permute attribute out of bound");
    transposedDims.emplace_back(createIE->getShapeAsDim(data, inputIndex));
  }

  setOutputDims(transposedDims);
  return success();
}

} // namespace onnx_mlir

//===----------------------------------------------------------------------===//
// Verify
//===----------------------------------------------------------------------===//

LogicalResult ONNXTransposeOp::verify() {

  Value data = getData();
  if (!hasShapeAndRank(data))
    return success();

  int64_t rank = mlir::cast<ShapedType>(data.getType()).getRank();
  auto permAttr = getPermAttr();

  // No permute pattern, it becomes an inverse, its good.
  if (!permAttr)
    return success();
  // Has a permute, make sure its good.
  for (int64_t i = 0; i < rank; ++i) {
    int64_t p = ArrayAttrIntVal(permAttr, i);
    if (p < 0)
      p += rank;
    if (p < 0)
      return emitOpError() << "Transpose op with too low a permute value (" << p
                           << " at index " << i << ")";
    if (p >= rank)
      return emitOpError() << "Transpose op with too high a permute value ("
                           << p << " at index " << i << ")";
  }
  return success();
}

//===----------------------------------------------------------------------===//
// Shape Inference
//===----------------------------------------------------------------------===//

LogicalResult ONNXTransposeOp::inferShapes(
    std::function<void(Region &)> doShapeInference) {
  // Cannot infer shape if no shape exists.
  if (!hasShapeAndRank(getData()))
    return success();

  Type elementType =
      mlir::cast<ShapedType>(getData().getType()).getElementType();
  ONNXTransposeOpShapeHelper shapeHelper(getOperation(), {});
  return shapeHelper.computeShapeAndUpdateType(elementType);
}

//===----------------------------------------------------------------------===//
// Template instantiation
//===----------------------------------------------------------------------===//

namespace onnx_mlir {
template struct ONNXNonSpecificOpShapeHelper<ONNXTransposeOp>;
} // namespace onnx_mlir
