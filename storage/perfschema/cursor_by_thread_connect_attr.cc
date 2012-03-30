/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "cursor_by_thread_connect_attr.h"

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("PROCESS_ID") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ATTR_NAME") },
    { C_STRING_WITH_LEN("varchar(32)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ATTR_VALUE") },
    { C_STRING_WITH_LEN("varchar(1024)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ORDINAL_POSITION") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  }
};

/** symbolic names for field offsets, keep in sync with field_types */
enum field_offsets {
  FO_PROCESS_ID,
  FO_ATTR_NAME,
  FO_ATTR_VALUE,
  FO_ORDINAL_POSITION
};

TABLE_FIELD_DEF
cursor_by_thread_connect_attr::m_field_def=
{ 4, field_types };

cursor_by_thread_connect_attr::cursor_by_thread_connect_attr(
  const PFS_engine_table_share *share) :
  PFS_engine_table(share, &m_pos), m_row_exists(false)
{}

/**
  Take a length encoded string

  @arg ptr  inout       the input string array
  @arg dest             where to store the result
  @arg dest_size        max size of @c dest
  @arg copied_len       the actual length of the data copied
  @arg start_ptr        pointer to the start of input
  @arg input_length     the length of the incoming data
  @arg copy_data        copy the data or just skip the input
  @arg from_cs          character set in which @c ptr is encoded
  @arg nchars_max       maximum number of characters to read
  @return status
    @retval true    parsing failed
    @retval false   parsing succeeded
*/
static bool parse_length_encoded_string(const char **ptr,
                                        char *dest, uint dest_size,
                                        uint *copied_len,
                                        const char *start_ptr, uint input_length,
                                        bool copy_data,
                                        const CHARSET_INFO *from_cs,
                                        uint nchars_max)
{
  ulong copy_length, data_length;
  const char *well_formed_error_pos= NULL, *cannot_convert_error_pos= NULL,
        *from_end_pos= NULL;

  copy_length= data_length= net_field_length((uchar **) ptr);

  /* we don't tolerate NULL as a length */
  if (data_length == NULL_LENGTH)
    return true;

  if (*ptr - start_ptr + data_length > input_length)
    return true;

  copy_length= well_formed_copy_nchars(&my_charset_utf8_bin, dest, dest_size,
                                       from_cs, *ptr, data_length, nchars_max,
                                       &well_formed_error_pos,
                                       &cannot_convert_error_pos,
                                       &from_end_pos);
  *copied_len= copy_length;
  (*ptr)+= data_length;

  return false;
}


/**
  Take the nth attribute name/value pair

  Parse the attributes blob form the beginning, skipping the attributes
  whose number is lower than the one we seek.
  When we reach the attribute at an index we're looking for the values
  are copied to the output parameters.
  If parsing fails or no more attributes are found the function stops
  and returns an error code.

  @arg connect_attrs            pointer to the connect attributes blob
  @arg connect_attrs_length     length of @c connect_attrs
  @arg connect_attrs_cs         character set used to encode @c connect_attrs
  @arg ordinal                  index of the attribute we need
  @arg attr_name [out]          buffer to receive the attribute name
  @arg max_attr_name            max size of @c attr_name in bytes
  @arg attr_name_length [out]   number of bytes written in @attr_name
  @arg attr_value [out]         buffer to receive the attribute name
  @arg max_attr_value           max size of @c attr_value in bytes
  @arg attr_value_length [out]  number of bytes written in @attr_value
  @return status
    @retval true    requested attribute pair is found and copied
    @retval false   error. Either because of parsing or too few attributes.
*/
bool read_nth_attr(const char *connect_attrs, uint connect_attrs_length,
                          const CHARSET_INFO *connect_attrs_cs,
                          uint ordinal,
                          char *attr_name, uint max_attr_name,
                          uint *attr_name_length,
                          char *attr_value, uint max_attr_value,
                          uint *attr_value_length)
{
  uint idx;
  const char *ptr;

  for (ptr= connect_attrs, idx= 0;
       (ptr - connect_attrs) < connect_attrs_length && idx <= ordinal;
      idx++)
  {
    uint copy_length;
    /* do the copying only if we absolutely have to */
    bool fill_in_attr_name= idx == ordinal;
    bool fill_in_attr_value= idx == ordinal;

    /* read the key */
    if (parse_length_encoded_string(&ptr,
                                    attr_name, max_attr_name, &copy_length,
                                    connect_attrs,
				    connect_attrs_length,
				    fill_in_attr_name,
                                    connect_attrs_cs, 32) ||
	!copy_length
	)
      return false;
    if (idx == ordinal)
      *attr_name_length= copy_length;


    /* read the value */
    if (parse_length_encoded_string(&ptr,
                                    attr_value, max_attr_value, &copy_length,
                                    connect_attrs,
				    connect_attrs_length,
				    fill_in_attr_value,
                                    connect_attrs_cs, 1024))
      return false;

    if (idx == ordinal)
      *attr_value_length= copy_length;

    if (idx == ordinal)
      return true;
  }

  return false;
}


int cursor_by_thread_connect_attr::rnd_next(void)
{
  PFS_thread *thread;
  PFS_thread *current_thread= PFS_thread::get_current_thread();

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_thread();
       m_pos.next_thread())
  {
    thread= &thread_array[m_pos.m_index_1];

    if (thread->m_lock.is_populated() && thread_fits(thread, current_thread))
    {
      make_row(thread, m_pos.m_index_2);
      if (m_row_exists)
      {
        m_next_pos.set_after(&m_pos);
	return 0;
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}


int cursor_by_thread_connect_attr::rnd_pos(const void *pos)
{
  PFS_thread *thread;
  PFS_thread *current_thread= PFS_thread::get_current_thread();

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);

  thread= &thread_array[m_pos.m_index_1];
  if (!thread->m_lock.is_populated() ||
      !thread_fits(thread, current_thread))
    return HA_ERR_RECORD_DELETED;

  make_row(thread, m_pos.m_index_2);
  if (m_row_exists)
    return 0;

  return HA_ERR_RECORD_DELETED;
}


void cursor_by_thread_connect_attr::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}


void cursor_by_thread_connect_attr::make_row(PFS_thread *pfs, uint ordinal)
{
  pfs_lock lock;
  PFS_thread_class *safe_class;

  m_row_exists= false;

  /* Protect this reader against thread termination */
  pfs->m_lock.begin_optimistic_lock(&lock);
  safe_class= sanitize_thread_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  /* populate the row */
  if (read_nth_attr(pfs->m_connect_attrs, pfs->m_connect_attrs_length,
                    pfs->m_connect_attrs_cs,
                    ordinal,
                    m_row.m_attr_name, (uint) sizeof(m_row.m_attr_name),
                    &m_row.m_attr_name_length,
                    m_row.m_attr_value, (uint) sizeof(m_row.m_attr_value),
                    &m_row.m_attr_value_length))
  {
    /* we don't expect internal threads to have connection attributes */
    DBUG_ASSERT(pfs->m_thread_id != 0);

    m_row.m_ordinal_position= ordinal;
    m_row.m_process_id= pfs->m_thread_id;
  }
  else
    return;


  if (pfs->m_lock.end_optimistic_lock(& lock))
    m_row_exists= true;
}


int cursor_by_thread_connect_attr::read_row_values(TABLE *table,
                                   unsigned char *buf,
                                   Field **fields,
                                   bool read_all)
{
  Field *f;

  if (unlikely(!m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case FO_PROCESS_ID:
        if (m_row.m_process_id != 0)
          set_field_ulong(f, m_row.m_process_id);
        else
          f->set_null();
        break;
      case FO_ATTR_NAME:
        set_field_varchar_utf8(f, m_row.m_attr_name,
			       m_row.m_attr_name_length);
        break;
      case FO_ATTR_VALUE:
        if (m_row.m_attr_value_length)
          set_field_varchar_utf8(f, m_row.m_attr_value,
                                 m_row.m_attr_value_length);
        else
          f->set_null();
        break;
      case FO_ORDINAL_POSITION:
	set_field_ulong(f, m_row.m_ordinal_position);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
