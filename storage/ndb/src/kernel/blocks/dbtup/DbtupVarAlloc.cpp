/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUP_C
#define DBTUP_VAR_ALLOC_CPP
#include "Dbtup.hpp"

void Dbtup::init_list_sizes(void)
{
  c_min_list_size[0]= 200;
  c_max_list_size[0]= 499;

  c_min_list_size[1]= 500;
  c_max_list_size[1]= 999;

  c_min_list_size[2]= 1000;
  c_max_list_size[2]= 4079;

  c_min_list_size[3]= 4080;
  c_max_list_size[3]= 8159;

  c_min_list_size[4]= 0;
  c_max_list_size[4]= 199;
}

/*
  Allocator for variable sized segments
  Part of the external interface for variable sized segments

  This method is used to allocate and free variable sized tuples and
  parts of tuples. This part can be used to implement variable sized
  attributes without wasting memory. It can be used to support small
  BLOB's attached to the record. It can also be used to support adding
  and dropping attributes without the need to copy the entire table.

  SYNOPSIS
    fragPtr         A pointer to the fragment description
    tabPtr          A pointer to the table description
    alloc_size       Size of the allocated record
    signal           The signal object to be used if a signal needs to
                     be sent
  RETURN VALUES
    Returns true if allocation was successful otherwise false

    page_offset      Page offset of allocated record
    page_index       Page index of allocated record
    page_ptr         The i and p value of the page where the record was
                     allocated
*/
Uint32* Dbtup::alloc_var_rec(Fragrecord* fragPtr,
			     Tablerec* tabPtr,
			     Uint32 alloc_size,
			     Local_key* key,
			     Uint32 * out_frag_page_id)
{
  /**
   * TODO alloc fix+var part
   */
  const Uint32 XXX = Tuple_header::HeaderSize + Var_part_ref::SZ32;
  tabPtr->m_offsets[MM].m_fix_header_size += XXX;
  Uint32 *ptr = alloc_fix_rec(fragPtr, tabPtr, key, out_frag_page_id);
  tabPtr->m_offsets[MM].m_fix_header_size -= XXX;
  if (unlikely(ptr == 0))
  {
    return 0;
  }
  
  Local_key varref;
  if (likely(alloc_var_part(fragPtr, tabPtr, alloc_size, &varref) != 0))
  {
    Tuple_header* tuple = (Tuple_header*)ptr;
    Var_part_ref* dst = (Var_part_ref*)tuple->get_var_part_ptr(tabPtr);
    dst->assign(&varref);
    return ptr;
  }
  
  PagePtr pagePtr;
  c_page_pool.getPtr(pagePtr, key->m_page_no);
  free_fix_rec(fragPtr, tabPtr, key, (Fix_page*)pagePtr.p);
  return 0;
}

Uint32*
Dbtup::alloc_var_part(Fragrecord* fragPtr,
		      Tablerec* tabPtr,
		      Uint32 alloc_size,
		      Local_key* key)
{
  PagePtr pagePtr;
  pagePtr.i= get_alloc_page(fragPtr, (alloc_size + 1));
  if (pagePtr.i == RNIL) { 
    jam();
    if ((pagePtr.i= get_empty_var_page(fragPtr)) == RNIL) {
      jam();
      return 0;
    }
    c_page_pool.getPtr(pagePtr);
    ((Var_page*)pagePtr.p)->init();
    pagePtr.p->list_index = MAX_FREE_LIST - 1;
    LocalDLList<Page> list(c_page_pool, 
			   fragPtr->free_var_page_array[MAX_FREE_LIST-1]);
    list.add(pagePtr);
    /*
     * Tup scan and index build check ZEMPTY_MM to skip un-init()ed
     * page.  Change state here.  For varsize it means "page in use".
     */
    pagePtr.p->page_state = ZTH_MM_FREE;
  } else {
    c_page_pool.getPtr(pagePtr);
    jam();
  }
  Uint32 idx= ((Var_page*)pagePtr.p)
    ->alloc_record(alloc_size, (Var_page*)ctemp_page, Var_page::CHAIN);
  
  key->m_page_no = pagePtr.i;
  key->m_page_idx = idx;
  
  update_free_page_list(fragPtr, pagePtr);  
  return ((Var_page*)pagePtr.p)->get_ptr(idx);
}

/*
  Deallocator for variable sized segments
  Part of the external interface for variable sized segments

  SYNOPSIS
    fragPtr         A pointer to the fragment description
    tabPtr          A pointer to the table description
    signal           The signal object to be used if a signal needs to
                     be sent
    page_ptr         A reference to the page of the variable sized
                     segment
    free_page_index  Page index on page of variable sized segment
                     which is freed
  RETURN VALUES
    Returns true if deallocation was successful otherwise false
*/
void Dbtup::free_var_rec(Fragrecord* fragPtr,
			 Tablerec* tabPtr,
			 Local_key* key,
			 Ptr<Page> pagePtr)
{
  /**
   * TODO free fix + var part
   */
  Uint32 *ptr = ((Fix_page*)pagePtr.p)->get_ptr(key->m_page_idx, 0);
  Tuple_header* tuple = (Tuple_header*)ptr;

  Local_key ref;
  Var_part_ref * varref = (Var_part_ref*)tuple->get_var_part_ptr(tabPtr);
  varref->copyout(&ref);

  free_fix_rec(fragPtr, tabPtr, key, (Fix_page*)pagePtr.p);

  c_page_pool.getPtr(pagePtr, ref.m_page_no);
  ((Var_page*)pagePtr.p)->free_record(ref.m_page_idx, Var_page::CHAIN);
  
  ndbassert(pagePtr.p->free_space <= Var_page::DATA_WORDS);
  if (pagePtr.p->free_space == Var_page::DATA_WORDS - 1)
  {
    jam();
    /*
      This code could be used when we release pages.
      remove_free_page(signal,fragPtr,page_header,page_header->list_index);
      return_empty_page(fragPtr, page_header);
    */
    update_free_page_list(fragPtr, pagePtr);
  } else {
    jam();
    update_free_page_list(fragPtr, pagePtr);
  }
  return;
}

Uint32 *
Dbtup::realloc_var_part(Fragrecord* fragPtr, Tablerec* tabPtr, PagePtr pagePtr,
			Var_part_ref* refptr, Uint32 oldsz, Uint32 newsz)
{
  Uint32 add = newsz - oldsz;
  Uint32 *new_var_ptr;
  Var_page* pageP = (Var_page*)pagePtr.p;
  Local_key oldref;
  refptr->copyout(&oldref);
  
  if (pageP->free_space >= add)
  {
    jam();
    new_var_ptr= pageP->get_ptr(oldref.m_page_idx);
    if(!pageP->is_space_behind_entry(oldref.m_page_idx, add))
    {
      if(0) printf("extra reorg");
      jam();
      /**
       * In this case we need to reorganise the page to fit. To ensure we
       * don't complicate matters we make a little trick here where we
       * fool the reorg_page to avoid copying the entry at hand and copy
       * that separately at the end. This means we need to copy it out of
       * the page before reorg_page to save the entry contents.
       */
      Uint32* copyBuffer= cinBuffer;
      memcpy(copyBuffer, new_var_ptr, 4*oldsz);
      pageP->set_entry_len(oldref.m_page_idx, 0);
      pageP->free_space += oldsz;
      pageP->reorg((Var_page*)ctemp_page);
      new_var_ptr= pageP->get_free_space_ptr();
      memcpy(new_var_ptr, copyBuffer, 4*oldsz);
      pageP->set_entry_offset(oldref.m_page_idx, pageP->insert_pos);
      add += oldsz;
    }
    pageP->grow_entry(oldref.m_page_idx, add);
    update_free_page_list(fragPtr, pagePtr);
  }
  else
  {
    Local_key newref;
    Uint32 *src = pageP->get_ptr(oldref.m_page_idx);
    new_var_ptr = alloc_var_part(fragPtr, tabPtr, newsz, &newref);
    if (unlikely(new_var_ptr == 0))
      return NULL;

    ndbassert(oldref.m_page_no != newref.m_page_no);
    ndbassert(pageP->get_entry_len(oldref.m_page_idx) == oldsz);
    memcpy(new_var_ptr, src, 4*oldsz);
    refptr->assign(&newref);
    
    pageP->free_record(oldref.m_page_idx, Var_page::CHAIN);
    update_free_page_list(fragPtr, pagePtr);    
  }
  
  return new_var_ptr;
}


/* ------------------------------------------------------------------------ */
// Get a page from one of free lists. If the desired free list is empty we
// try with the next until we have tried all possible lists.
/* ------------------------------------------------------------------------ */
Uint32
Dbtup::get_alloc_page(Fragrecord* fragPtr, Uint32 alloc_size)
{
  Uint32 i, start_index, loop= 0;
  PagePtr pagePtr;
  
  start_index= calculate_free_list_impl(alloc_size);
  if (start_index == (MAX_FREE_LIST - 1)) {
    jam();
  } else {
    jam();
    ndbrequire(start_index < (MAX_FREE_LIST - 1));
    start_index++;
  }
  for (i= start_index; i < MAX_FREE_LIST; i++) {
    jam();
    if (!fragPtr->free_var_page_array[i].isEmpty()) {
      jam();
      return fragPtr->free_var_page_array[i].firstItem;
    }
  }
  ndbrequire(start_index > 0);
  i= start_index - 1;
  LocalDLList<Page> list(c_page_pool, fragPtr->free_var_page_array[i]);
  for(list.first(pagePtr); !pagePtr.isNull() && loop < 16; )
  {
    jam();
    if (pagePtr.p->free_space >= alloc_size) {
      jam();
      return pagePtr.i;
    }
    loop++;
    list.next(pagePtr);
  }
  return RNIL;
}

Uint32
Dbtup::get_empty_var_page(Fragrecord* fragPtr)
{
  PagePtr ptr;
  LocalSLList<Page> list(c_page_pool, fragPtr->m_empty_pages);
  if (list.remove_front(ptr))
  {
    return ptr.i;
  }

  Uint32 cnt;
  allocConsPages(10, cnt, ptr.i);
  fragPtr->noOfVarPages+= cnt;
  if (unlikely(cnt == 0))
  {
    return RNIL;
  }

  PagePtr ret = ptr;
  for (Uint32 i = 0; i<cnt; i++, ptr.i++)
  {
    c_page_pool.getPtr(ptr);
    ptr.p->physical_page_id = ptr.i;
    ptr.p->page_state = ZEMPTY_MM;
    ptr.p->nextList = ptr.i + 1;
    ptr.p->prevList = RNIL;
    ptr.p->frag_page_id = RNIL;
  }
  
  if (cnt > 1)
  {
    ptr.p->nextList = RNIL;
    list.add(ret.i + 1, ptr);
  }

  c_page_pool.getPtr(ret);
  
  Var_page* page = (Var_page*)ret.p;
  page->chunk_size = cnt;
  page->next_chunk = fragPtr->m_var_page_chunks;
  fragPtr->m_var_page_chunks = ret.i;
  
  return ret.i;
}

/* ------------------------------------------------------------------------ */
// Check if the page needs to go to a new free page list.
/* ------------------------------------------------------------------------ */
void Dbtup::update_free_page_list(Fragrecord* fragPtr,
                                  Ptr<Page> pagePtr)
{
  Uint32 free_space, list_index;
  free_space= pagePtr.p->free_space;
  list_index= pagePtr.p->list_index;
  if ((free_space < c_min_list_size[list_index]) ||
      (free_space > c_max_list_size[list_index])) {
    Uint32 new_list_index= calculate_free_list_impl(free_space);
    if (list_index != MAX_FREE_LIST) {
      jam();
      /*
       * Only remove it from its list if it is in a list
       */
      LocalDLList<Page> 
	list(c_page_pool, fragPtr->free_var_page_array[list_index]);
      list.remove(pagePtr);
    }
    if (free_space < c_min_list_size[new_list_index]) {
      /*
	We have not sufficient amount of free space to put it into any
	free list. Thus the page will not be available for new inserts.
	This can only happen for the free list with least guaranteed 
	free space.
      */
      jam();
      ndbrequire(new_list_index == 0);
      pagePtr.p->list_index= MAX_FREE_LIST;
    } else {
      jam();
      LocalDLList<Page> list(c_page_pool, 
			     fragPtr->free_var_page_array[new_list_index]);
      list.add(pagePtr);
      pagePtr.p->list_index = new_list_index;
    }
  }
}

/* ------------------------------------------------------------------------ */
// Given size of free space, calculate the free list to put it into
/* ------------------------------------------------------------------------ */
Uint32 Dbtup::calculate_free_list_impl(Uint32 free_space_size) const
{
  Uint32 i;
  for (i = 0; i < MAX_FREE_LIST; i++) {
    jam();
    if (free_space_size <= c_max_list_size[i]) {
      jam();
      return i;
    }
  }
  ndbrequire(false);
  return 0;
}

Uint32* 
Dbtup::alloc_var_rowid(Fragrecord* fragPtr,
		       Tablerec* tabPtr,
		       Uint32 alloc_size,
		       Local_key* key,
		       Uint32 * out_frag_page_id)
{
  const Uint32 XXX = Tuple_header::HeaderSize + Var_part_ref::SZ32;
  tabPtr->m_offsets[MM].m_fix_header_size += XXX;
  Uint32 *ptr = alloc_fix_rowid(fragPtr, tabPtr, key, out_frag_page_id);
  tabPtr->m_offsets[MM].m_fix_header_size -= XXX;
  if (unlikely(ptr == 0))
  {
    return 0;
  }

  Local_key varref;
  if (likely(alloc_var_part(fragPtr, tabPtr, alloc_size, &varref) != 0))
  {
    Tuple_header* tuple = (Tuple_header*)ptr;
    Var_part_ref* dst = (Var_part_ref*)tuple->get_var_part_ptr(tabPtr);
    dst->assign(&varref);
    return ptr;
  }
  
  PagePtr pagePtr;
  c_page_pool.getPtr(pagePtr, key->m_page_no);
  free_fix_rec(fragPtr, tabPtr, key, (Fix_page*)pagePtr.p);
  return 0;
}
