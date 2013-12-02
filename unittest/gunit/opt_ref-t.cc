/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"

#include "fake_table.h"
#include "mock_field_long.h"
#include "sql_optimizer.cc"


// Unit tests of the ref optimizer.
namespace opt_ref_unittest {

using my_testing::Server_initializer;

bool scrap_bool; // Needed by Key_field CTOR.

/*
  Class for easy creation of an array of Key_field's. Must be the same
  size as Key_field.
*/
class Fake_key_field: public Key_field {

public:
  Fake_key_field() : Key_field(NULL, NULL, 0, 0, false, false, &scrap_bool, 0)
  {}
};


/*
  Class that tests the ref optimizer. The class creates the fake table
  definitions:

  t1(a int, b int, key(a, b))
  t2(a int, b int)
*/
class OptRefTest : public ::testing::Test
{
public:

  OptRefTest() :
    field_t1_a("field1", true),
    field_t1_b("field2", true),
    field_t2_a("field3", true),
    field_t2_b("field4", true),
    t1(&field_t1_a, &field_t1_b),
    t2(&field_t2_a, &field_t2_b),
    t1_key_fields(&t1_key_field_arr[0])
  {
    index_over_t1ab_id= t1.create_index(&field_t1_a, &field_t1_b);
    indexes.set_bit(index_over_t1ab_id);

    t1.reginfo.join_tab= &t1_join_tab;
    t1.pos_in_table_list= &t1_table_list;

    t1_table_list.embedding= NULL;
    t1_table_list.derived_keys_ready= true;
  }

  virtual void SetUp()
  {
    // We do some pointer arithmetic on these
    compile_time_assert(sizeof(Fake_key_field) == sizeof(Key_field));
    initializer.SetUp();

    item_zero= new Item_int(0);
    item_one= new Item_int(1);

    item_field_t1_a= new Item_field(&field_t1_a);
    item_field_t1_b= new Item_field(&field_t1_b);

    item_field_t2_a= new Item_field(&field_t2_a);
    item_field_t2_b= new Item_field(&field_t2_b);
  }

  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Bitmap<64> indexes;

  Mock_field_long field_t1_a, field_t1_b;
  Mock_field_long field_t2_a, field_t2_b;

  Fake_TABLE t1;
  Fake_TABLE t2;
  TABLE_LIST t1_table_list;
  TABLE_LIST t2_table_list;

  JOIN_TAB t1_join_tab;
  JOIN_TAB t2_join_tab;
  Fake_key_field t1_key_field_arr[10];
  Key_field *t1_key_fields;

  Item_int *item_zero;
  Item_int *item_one;

  Item_field *item_field_t1_a, *item_field_t1_b;
  Item_field *item_field_t2_a, *item_field_t2_b;

  int index_over_t1ab_id;

  void call_add_key_fields(Item *cond)
  {
    uint and_level= 0;
    add_key_fields(NULL /* join */, &t1_key_fields, &and_level, cond, ~0ULL,
                   NULL);
  }

private:

  Server_initializer initializer;
};


Item_row *make_item_row(Item *a, Item *b)
{
  /*
    The Item_row CTOR doesn't store the reference to the list, hence
    it can live on the stack.
  */
  List<Item> items;
  items.push_front(b);
  items.push_front(a);
  return new Item_row(items);
}

TEST_F(OptRefTest, addKeyFieldsFromInOneRow)
{
  /*
    We simulate the where condition (a, b) IN ((0, 0)). Note that this
    can't happen in practice since the parser is hacked to parse such
    an expression in to (a, b) = (0, 0), which gets rewritten into a =
    0 AND b = 0 before the ref optimizer runs.
   */
  List<Item> all_args;
  all_args.push_front(make_item_row(item_zero, item_zero));
  all_args.push_front(make_item_row(item_field_t1_a, item_field_t1_b));
  Item_func_in *cond= new Item_func_in(all_args);

  call_add_key_fields(cond);

  // We expect the key_fields pointer not to be incremented.
  EXPECT_EQ(0, t1_key_fields - static_cast<Key_field*>(&t1_key_field_arr[0]));
  EXPECT_EQ(indexes, t1_join_tab.const_keys)
    << "SARGable index not present in const_keys";
  EXPECT_EQ(indexes, t1_join_tab.keys);
  EXPECT_EQ(0U, t1_key_field_arr[0].level);
  EXPECT_EQ(0U, t1_key_field_arr[1].level);
}

TEST_F(OptRefTest, addKeyFieldsFromInTwoRows)
{
  // We simulate the where condition (col_a, col_b) IN ((0, 0), (1, 1))
  List<Item> all_args;
  all_args.push_front(make_item_row(item_one, item_one));
  all_args.push_front(make_item_row(item_zero, item_zero));
  all_args.push_front(make_item_row(item_field_t1_a, item_field_t1_b));
  Item_func_in *cond= new Item_func_in(all_args);

  call_add_key_fields(cond);

  // We expect the key_fields pointer not to be incremented.
  EXPECT_EQ(0, t1_key_fields - static_cast<Key_field*>(&t1_key_field_arr[0]));
  EXPECT_EQ(indexes, t1_join_tab.const_keys)
    << "SARGable index not present in const_keys";
  EXPECT_EQ(indexes, t1_join_tab.keys);
}

TEST_F(OptRefTest, addKeyFieldsFromInOneRowWithCols)
{
  // We simulate the where condition (t1.a, t1.b) IN ((t2.a, t2.b))
  List<Item> all_args;
  all_args.push_front(make_item_row(item_field_t2_a, item_field_t2_b));
  all_args.push_front(make_item_row(item_field_t1_a, item_field_t1_b));
  Item_func_in *cond= new Item_func_in(all_args);

  call_add_key_fields(cond);

  // We expect the key_fields pointer not to be incremented.
  EXPECT_EQ(0, t1_key_fields - static_cast<Key_field*>(&t1_key_field_arr[0]));
  EXPECT_EQ(Bitmap<64>(0), t1_join_tab.const_keys);
  EXPECT_EQ(indexes, t1_join_tab.keys);

  EXPECT_EQ(t2.map, t1_join_tab.key_dependent);
}

TEST_F(OptRefTest, addKeyFieldsFromEq)
{
  // We simulate the where condition a = 0 AND b = 0
  Item_func_eq *eq1= new Item_func_eq(item_field_t1_a, item_zero);
  Item_func_eq *eq2= new Item_func_eq(item_field_t1_b, item_zero);
  Item_cond_and *cond= new Item_cond_and(eq1, eq2);

  call_add_key_fields(cond);

  /*
    We expect 2 Key_field's to be written. Actually they're always
    written, but we expect the pointer to be incremented.
  */
  EXPECT_EQ(2, t1_key_fields - static_cast<Key_field*>(&t1_key_field_arr[0]));
  EXPECT_EQ(indexes, t1_join_tab.const_keys)
    << "SARGable index not present in const_keys";
  EXPECT_EQ(indexes, t1_join_tab.keys);

  EXPECT_EQ(0U, t1_join_tab.key_dependent);

  EXPECT_EQ(0U, t1_key_field_arr[0].level);
  EXPECT_EQ(0U, t1_key_field_arr[1].level);
}

}
