/*
 * Copyright (c) 2015, 2021, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef UPDATE_STATEMENT_BUILDER_H_
#define UPDATE_STATEMENT_BUILDER_H_

#include "statement_builder.h"

namespace xpl
{

class Update_statement_builder: public Crud_statement_builder
{
public:
  typedef ::Mysqlx::Crud::Update Update;

  explicit Update_statement_builder(const Expression_generator &gen)
      : Crud_statement_builder(gen) {}

  void build(const Update &msg) const;

protected:
  typedef ::Mysqlx::Crud::UpdateOperation Operation_item;
  typedef ::google::protobuf::RepeatedPtrField<Operation_item> Operation_list;
  typedef Operation_list::const_iterator Operation_iterator;

  void add_operation(const Operation_list &operation, const bool is_relational) const;
  void add_table_operation(const Operation_list &operation) const;
  void add_table_operation_items(Operation_iterator begin, Operation_iterator end) const;
  void add_document_operation(const Operation_list &operation) const;
  void add_document_operation_item(const Operation_item &item, int &opeartion_id) const;
  void add_member(const Operation_item &item) const;
  void add_value(const Operation_item &item) const;
  void add_member_with_value(const Operation_item &item) const;
  void add_field_with_value(const Operation_item &item) const;
};

} // namespace xpl

#endif // UPDATE_STATEMENT_BUILDER_H_
