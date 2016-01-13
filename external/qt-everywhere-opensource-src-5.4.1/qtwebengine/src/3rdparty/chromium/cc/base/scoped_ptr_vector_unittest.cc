// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "cc/base/scoped_ptr_vector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {
namespace {

class Data {
 public:
  static scoped_ptr<Data> Create(int i) { return make_scoped_ptr(new Data(i)); }
  int data() const { return data_; }
 private:
  explicit Data(int i) : data_(i) {}
  int data_;
};

class IsOddPredicate {
 public:
  bool operator()(const Data* data) { return (data->data() % 2) == 1; }
};

TEST(ScopedPtrVectorTest, PushBack) {
  ScopedPtrVector<Data> v;

  // Insert 5 things into the vector.
  v.push_back(Data::Create(1));
  v.push_back(Data::Create(2));
  v.push_back(Data::Create(3));
  v.push_back(Data::Create(4));
  v.push_back(Data::Create(5));

  EXPECT_EQ(5u, v.size());
  EXPECT_EQ(1, v[0]->data());
  EXPECT_EQ(2, v[1]->data());
  EXPECT_EQ(3, v[2]->data());
  EXPECT_EQ(4, v[3]->data());
  EXPECT_EQ(5, v[4]->data());
}

TEST(ScopedPtrVectorTest, InsertAndTake) {
  // Insert 3 things into each vector.
  ScopedPtrVector<Data> v;
  v.push_back(Data::Create(1));
  v.push_back(Data::Create(2));
  v.push_back(Data::Create(6));

  ScopedPtrVector<Data> v2;
  v2.push_back(Data::Create(3));
  v2.push_back(Data::Create(4));
  v2.push_back(Data::Create(5));

  ScopedPtrVector<Data>::iterator it = v.begin();
  ++it;
  ++it;
  EXPECT_EQ(6, (*it)->data());

  v.insert_and_take(it, v2);

  EXPECT_EQ(6u, v.size());
  EXPECT_EQ(1, v[0]->data());
  EXPECT_EQ(2, v[1]->data());
  EXPECT_EQ(3, v[2]->data());
  EXPECT_EQ(4, v[3]->data());
  EXPECT_EQ(5, v[4]->data());
  EXPECT_EQ(6, v[5]->data());

  EXPECT_EQ(3u, v2.size());
  EXPECT_EQ(NULL, v2[0]);
  EXPECT_EQ(NULL, v2[1]);
  EXPECT_EQ(NULL, v2[2]);
}

TEST(ScopedPtrVectorTest, Partition) {
  ScopedPtrVector<Data> v;
  v.push_back(Data::Create(1));
  v.push_back(Data::Create(2));
  v.push_back(Data::Create(3));
  v.push_back(Data::Create(4));
  v.push_back(Data::Create(5));

  ScopedPtrVector<Data>::iterator it = v.partition(IsOddPredicate());
  std::set<int> odd_numbers;
  for (ScopedPtrVector<Data>::iterator second_it = v.begin();
       second_it != it;
       ++second_it) {
    EXPECT_EQ(1, (*second_it)->data() % 2);
    odd_numbers.insert((*second_it)->data());
  }
  EXPECT_EQ(3u, odd_numbers.size());

  std::set<int> even_numbers;
  for (; it != v.end(); ++it) {
    EXPECT_EQ(0, (*it)->data() % 2);
    even_numbers.insert((*it)->data());
  }
  EXPECT_EQ(2u, even_numbers.size());
}

}  // namespace
}  // namespace cc
