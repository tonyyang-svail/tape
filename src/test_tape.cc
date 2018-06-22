// Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>

#include "gtest/gtest.h"
#include "src/function.h"

using paddle::tape::VariableHandle;
using paddle::tape::Variable;
using paddle::tape::Linear;
using paddle::tape::Convolution2D;
using paddle::tape::SGD;
using paddle::tape::Fill;
using paddle::tape::mean;
using paddle::tape::softmax;
using paddle::tape::cross_entropy;
using paddle::tape::reset_global_tape;
using paddle::tape::get_global_tape;
using paddle::tape::CreateRecordioFileReader;
using paddle::tape::ReadNext;

bool is_file_exist(const std::string& fileName) {
  std::ifstream infile(fileName);
  return infile.good();
}

TEST(Tape, TestMnist) {
  std::string filename = "/tmp/mnist.recordio";
  PADDLE_ENFORCE(
      is_file_exist(filename),
      "file doesn't exist; have you run data/create_mnist_recordio.py");
  VariableHandle reader = CreateRecordioFileReader(
      "/tmp/mnist.recordio", {32, 1, 28, 28, 32, 1}, {4, 2}, {0, 0});

  Linear linear1(784, 200, "relu");
  Linear linear2(200, 200, "relu");
  Linear linear3(200, 10, "relu");
  SGD sgd(0.001);

  int print_step = 100;
  float avg_loss = 0.0;

  for (int i = 0; i < 1000000; ++i) {
    reset_global_tape();
    auto data_label = ReadNext(reader);
    auto data = data_label[0];
    auto label = data_label[1];

    auto predict = softmax(linear3(linear2(linear1(data))));
    auto loss = mean(cross_entropy(predict, label));
    if (i % print_step == 0) {
      avg_loss +=
          loss->Value().Get<paddle::framework::LoDTensor>().data<float>()[0];
      LOG(INFO) << avg_loss;
      avg_loss = 0;
    }

    get_global_tape().Backward(loss);

    for (auto w : linear1.Params()) {
      sgd.Update(w);
    }
    for (auto w : linear2.Params()) {
      sgd.Update(w);
    }
    for (auto w : linear3.Params()) {
      sgd.Update(w);
    }
  }
}

TEST(Tape, TestConv) {
  Convolution2D conv1(3, 16, 3, "relu");
  Convolution2D conv2(16, 1, 3, "relu");

  SGD sgd(0.001);

  std::string initializer = "uniform_random";
  paddle::framework::AttributeMap attrs;
  attrs["min"] = -1.0f;
  attrs["max"] = 1.0f;
  attrs["dtype"] = paddle::framework::proto::VarType::Type::VarType_Type_FP32;
  attrs["seed"] = 123;
  attrs["shape"] = std::vector<int>{32, 3, 8, 8};
  Fill filler(initializer, attrs);

  for (int i = 0; i < 2; ++i) {
    reset_global_tape();

    VariableHandle input(new Variable("input"));
    filler(input);

    auto loss = mean(conv2(conv1(input)));

    get_global_tape().Backward(loss);

    for (auto w : conv1.Params()) {
      sgd.Update(w);
    }
    for (auto w : conv2.Params()) {
      sgd.Update(w);
    }
  }
}

TEST(Tape, TestMLP) {
  Linear linear1(3, 3, "relu");
  Linear linear2(3, 3, "relu");

  SGD sgd(0.001);

  std::string initializer = "uniform_random";
  paddle::framework::AttributeMap attrs;
  attrs["min"] = -1.0f;
  attrs["max"] = 1.0f;
  attrs["dtype"] = paddle::framework::proto::VarType::Type::VarType_Type_FP32;
  attrs["seed"] = 123;
  attrs["shape"] = std::vector<int>{3, 3};
  Fill filler(initializer, attrs);

  for (int i = 0; i < 2; ++i) {
    reset_global_tape();

    VariableHandle input(new Variable("input"));
    filler(input);

    auto loss = mean(linear2(linear1(input)));
    LOG(INFO) << loss->Value();

    get_global_tape().Backward(loss);

    for (auto w : linear1.Params()) {
      sgd.Update(w);
    }
    for (auto w : linear2.Params()) {
      sgd.Update(w);
    }
  }
}

int main(int argc, char** argv) {
  std::vector<paddle::platform::Place> places;
  places.emplace_back(paddle::platform::CPUPlace());
  paddle::platform::DeviceContextPool::Init(places);

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
