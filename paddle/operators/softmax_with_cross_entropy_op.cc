/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include "paddle/operators/softmax_with_cross_entropy_op.h"

namespace paddle {
namespace operators {

class SoftmaxWithCrossEntropyOpMaker
    : public framework::OpProtoAndCheckerMaker {
 public:
  SoftmaxWithCrossEntropyOpMaker(framework::OpProto* proto,
                                 framework::OpAttrChecker* op_checker)
      : OpProtoAndCheckerMaker(proto, op_checker) {
    AddInput("Logits",
             "(Tensor, default: Tensor<float>), The unscaled log probabilities "
             "which is a 2-D tensor with shape [N x K]. N is the batch_size, "
             "and K is the class number.")
        .NotInGradient();
    AddInput(
        "Label",
        "(Tensor, default: Tensor<int>), The ground truth which is a 2-D "
        "tensor. "
        "If softLable is set to 0, Label is a Tensor<int> with shape [N x 1]. "
        "If softLable is set to 1, Label is a Tensor<float/double> "
        "with shape [N x K].");
    AddOutput(
        "Softmax",
        "(Tensor, default: Tensor<float>), A 2-D tensor with shape [N x K]. "
        "The outputs value of softmax activation by given the input batch, "
        "which will be used in backward calculation.")
        .AsIntermediate();
    AddOutput("Loss",
              "(Tensor, default: Tensor<float>), A 2-D tensor. The cross "
              "entropy loss with shape [N x 1].");
    AddAttr<bool>(
        "softLabel",
        "(bool, default: false), A flag to indicate whether to interpretate "
        "the given labels as soft labels.")
        .SetDefault(false);
    AddComment(R"DOC(
Cross entropy loss with softmax are used as the output layer extensively. This
operator computes the softmax normalized values for each row of the input
tensor, after which cross-entropy loss is then computed. This provides a more
numerically stable gradient.

Because this operators performs a softmax on logits internally, it expects
unscaled logits. Please do not call this op with the output of softmax operator,
which will produce incorrect results.

This operators expects mutually exclusive hard labels, each sample in a batch
is in exactly one class with probabilities 1. Each sample in the batch with one
and only one label.

Equation:

1) hard label (one-hot label)

Loss_j = -\text{Logit}_{Label_j} + \log\left(\sum_{i=0}^{K}\exp(\text{Logit}_i)\right), j = 1, ..., K

2) soft label (a distribution over all classes)

Loss_j = -\sum_{i=0}^{K}\text{Label}_i\left(\text{Logit}_i-\log\left(\sum_{i=0}^{K}\exp(\text{Logit}_i)\right)\right), j = 1,...,K

)DOC");
  }
};

class SoftmaxWithCrossEntropyOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  void InferShape(framework::InferShapeContextBase* ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("Logits"),
                   "Input(Logits) should be not null.");
    PADDLE_ENFORCE(ctx->HasInput("Label"), "Input(Label) should be not null.");

    PADDLE_ENFORCE(ctx->HasOutput("Softmax"),
                   "Output(Softmax) should be not null.");
    PADDLE_ENFORCE(ctx->HasOutput("Loss"), "Output(Loss) should be not null.");

    auto logits_dims = ctx->GetInputDim("Logits");
    auto labels_dims = ctx->GetInputDim("Label");
    PADDLE_ENFORCE_EQ(
        logits_dims.size(), 2UL,
        "The input of softmax_with_cross_entropy should be a 2-D tensor.");
    PADDLE_ENFORCE_EQ(labels_dims.size(), 2UL,
                      "The labels should be a 2-D tensor.");

    if (ctx->Attrs().Get<bool>("softLabel")) {
      PADDLE_ENFORCE_EQ(logits_dims[1], labels_dims[1],
                        "If Attr(softLabel) == true, the 2nd dimension of "
                        "Input(X) and Input(Label) should be equal.");
    } else {
      PADDLE_ENFORCE_EQ(labels_dims[1], 1UL,
                        "If Attr(softLabel) == false, the 2nd dimension of "
                        "Input(Label) should be 1.");
    }

    ctx->SetOutputDim("Softmax", logits_dims);
    ctx->SetOutputDim("Loss", {logits_dims[0], 1});

    ctx->ShareLoD("Logits", /*->*/ "Softmax");
    ctx->ShareLoD("Logits", /*->*/ "Loss");
  }
};

class SoftmaxWithCrossEntropyOpGrad : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  void InferShape(framework::InferShapeContextBase* ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput(framework::GradVarName("Loss")),
                   "Input(Loss@Grad) should not be null.");
    PADDLE_ENFORCE(ctx->HasInput("Softmax"),
                   "Input(Softmax) should be not null.");
    PADDLE_ENFORCE(ctx->HasInput("Label"), "Input(Label) should be not null.");
    PADDLE_ENFORCE(ctx->HasOutput(framework::GradVarName("Logits")),
                   "Output(Logits@Grad) should be not null.");

    auto softmax_dims = ctx->GetInputDim("Softmax");
    auto labels_dims = ctx->GetInputDim("Label");
    PADDLE_ENFORCE_EQ(labels_dims.size(), 2UL,
                      "The labels should be a 2-D tensor.");

    if (ctx->Attrs().Get<bool>("softLabel")) {
      PADDLE_ENFORCE_EQ(softmax_dims[1], labels_dims[1],
                        "When Attr(softLabel) == true, the 2nd dimension of "
                        "Input(X) and Input(Label) should be equal.");
    } else {
      PADDLE_ENFORCE_EQ(labels_dims[1], 1UL,
                        "When Attr(softLabel) == false, the 2nd dimension of "
                        "Input(Label) should be 1.");
    }

    ctx->SetOutputDim(framework::GradVarName("Logits"),
                      ctx->GetInputDim("Softmax"));
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;

REGISTER_OP(softmax_with_cross_entropy, ops::SoftmaxWithCrossEntropyOp,
            ops::SoftmaxWithCrossEntropyOpMaker,
            softmax_with_cross_entropy_grad,
            ops::SoftmaxWithCrossEntropyOpGrad);
REGISTER_OP_CPU_KERNEL(softmax_with_cross_entropy,
                       ops::SoftmaxWithCrossEntropyKernel<float>);
REGISTER_OP_CPU_KERNEL(softmax_with_cross_entropy_grad,
                       ops::SoftmaxWithCrossEntropyGradKernel<float>);
