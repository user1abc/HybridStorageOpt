#ifndef INCLUDES_MYSQL_SQL_LIST_H
#define INCLUDES_MYSQL_SQL_LIST_H
/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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
#include "my_sys.h"
#include "m_string.h" /* for TRASH */


void *sql_alloc(size_t);

#include "my_sys.h"                    /* alloc_root, TRASH, MY_WME,
                                          MY_FAE, MY_ALLOW_ZERO_PTR */
#include "m_string.h"
#include "thr_malloc.h"                         /* sql_alloc */

/* mysql standard class memory allocator */

class Sql_alloc
{
public:
  static void *operator new(size_t size) throw ()
  {
    return sql_alloc(size);
  }
  static void *operator new[](size_t size) throw ()
  {
    return sql_alloc(size);
  }
  static void *operator new[](size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr, size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root)
  { /* never called */ }
  static void operator delete[](void *ptr, MEM_ROOT *mem_root)
  { /* never called */ }
  static void operator delete[](void *ptr, size_t size) { TRASH(ptr, size); }
#ifdef HAVE_purify
  bool dummy;
  inline Sql_alloc() :dummy(0) {}
  inline ~Sql_alloc() {}
#else
  inline Sql_alloc() {}
  inline ~Sql_alloc() {}
#endif

};


/**
  Simple intrusive linked list.

  @remark Similar in nature to base_list, but intrusive. It keeps a
          a pointer to the first element in the list and a indirect
          reference to the last element.
*/
template <typename T>
class SQL_I_List :public Sql_alloc
{
public:
  uint elements;
  /** The first element in the list. */
  T *first;
  /** A reference to the next element in the list. */
  T **next;

  SQL_I_List() { empty(); }

  SQL_I_List(const SQL_I_List &tmp) : Sql_alloc()
  {
    elements= tmp.elements;
    first= tmp.first;
    next= elements ? tmp.next : &first;
  }

  inline void empty()
  {
    elements= 0;
    first= NULL;
    next= &first;
  }

  inline void link_in_list(T *element, T **next_ptr)
  {
    elements++;
    (*next)= element;
    next= next_ptr;
    *next= NULL;
  }

  inline void save_and_clear(SQL_I_List<T> *save)
  {
    *save= *this;
    empty();
  }

  inline void push_front(SQL_I_List<T> *save)
  {
    /* link current list last */
    *save->next= first;
    first= save->first;
    elements+= save->elements;
  }

  inline void push_back(SQL_I_List<T> *save)
  {
    if (save->first)
    {
      *next= save->first;
      next= save->next;
      elements+= save->elements;
    }
  }
};


/*
  Basic single linked list
  Used for item and item_buffs.
  All list ends with a pointer to the 'end_of_list' element, which
  data pointer is a null pointer and the next pointer points to itself.
  This makes it very fast to traverse lists as we don't have to
  test for a specialend condition for list that can't contain a null
  pointer.
*/


/**
  list_node - a node of a single-linked list.
  @note We never call a destructor for instances of this class.
*/

struct list_node :public Sql_alloc
{
  list_node *next;
  void *info;
  list_node(void *info_par,list_node *next_par)
    :next(next_par),info(info_par)
  {}
  list_node()					/* For end_of_list */
  {
    info= 0;
    next= this;
  }
};


extern MYSQL_PLUGIN_IMPORT list_node end_of_list;

/**
  Comparison function for list sorting.

  @param n1   Info of 1st node
  @param n2   Info of 2nd node
  @param arg  Additional info

  @return
    -1  n1 < n2
     0  n1 == n2
     1  n1 > n2
*/

typedef int (*Node_cmp_func)(void *n1, void *n2, void *arg);

class base_list :public Sql_alloc
{
protected:
  list_node *first,**last;

public:
  uint elements;

  bool operator==(const base_list &rhs) const
  {
    return
      elements == rhs.elements &&
      first == rhs.first &&
      last == rhs.last;
  }

  inline void empty() { elements=0; first= &end_of_list; last=&first;}
  inline base_list() { empty(); }
  /**
    This is a shallow copy constructor that implicitly passes the ownership
    from the source list to the new instance. The old instance is not
    updated, so both objects end up sharing the same nodes. If one of
    the instances then adds or removes a node, the other becomes out of
    sync ('last' pointer), while still operational. Some old code uses and
    relies on this behaviour. This logic is quite tricky: please do not use
    it in any new code.
  */
  inline base_list(const base_list &tmp) :Sql_alloc()
  {
    elements= tmp.elements;
    first= tmp.first;
    last= elements ? tmp.last : &first;
  }
  /**
    Construct a deep copy of the argument in memory root mem_root.
    The elements themselves are copied by pointer. If you also
    need to copy elements by value, you should employ
    list_copy_and_replace_each_value after creating a copy.
  */
  base_list(const base_list &rhs, MEM_ROOT *mem_root);
  inline base_list(bool error) { }
  inline bool push_back(void *info)
  {
    if (((*last)=new list_node(info, &end_of_list)))
    {
      last= &(*last)->next;
      elements++;
      return 0;
    }
    return 1;
  }
  inline bool push_back(void *info, MEM_ROOT *mem_root)
  {
    if (((*last)=new (mem_root) list_node(info, &end_of_list)))
    {
      last= &(*last)->next;
      elements++;
      return 0;
    }
    return 1;
  }
  inline bool push_front(void *info)
  {
    list_node *node=new list_node(info,first);
    if (node)
    {
      if (last == &first)
	last= &node->next;
      first=node;
      elements++;
      return 0;
    }
    return 1;
  }
  void remove(list_node **prev)
  {
    list_node *node=(*prev)->next;
    if (!--elements)
      last= &first;
    else if (last == &(*prev)->next)
      last= prev;
    delete *prev;
    *prev=node;
  }
  inline void concat(base_list *list)
  {
    if (!list->is_empty())
    {
      *last= list->first;
      last= list->last;
      elements+= list->elements;
    }
  }
  inline void *pop(void)
  {
    if (first == &end_of_list) return 0;
    list_node *tmp=first;
    first=first->next;
    if (!--elements)
      last= &first;
    return tmp->info;
  }
  inline void disjoin(base_list *list)
  {
    list_node **prev= &first;
    list_node *node= first;
    list_node *list_first= list->first;
    elements=0;
    while (node && node != list_first)
    {
      prev= &node->next;
      node= node->next;
      elements++;
    }
    *prev= *last;
    last= prev;
  }
  inline void prepand(base_list *list)
  {
    if (!list->is_empty())
    {
      *list->last= first;
      first= list->first;
      elements+= list->elements;
    }
  }
  /**
    @brief
    Sort the list

    @param cmp  node comparison function
    @param arg  additional info to be passed to comparison function

    @details
    The function sorts list nodes by an exchange sort algorithm.
    The order of list nodes isn't changed, values of info fields are
    swapped instead. Due to this, list iterators that are initialized before
    sort could be safely used after sort, i.e they wouldn't cause a crash.
    As this isn't an effective algorithm the list to be sorted is supposed to
    be short.
  */
  void sort(Node_cmp_func cmp, void *arg)
  {
    if (elements < 2)
      return;
    for (list_node *n1= first; n1 && n1 != &end_of_list; n1= n1->next)
    {
      for (list_node *n2= n1->next; n2 && n2 != &end_of_list; n2= n2->next)
      {
        if ((*cmp)(n1->info, n2->info, arg) > 0)
        {
          void *tmp= n1->info;
          n1->info= n2->info;
          n2->info= tmp;
        }
      }
    }
  }
  /**
    Swap two lists.
  */
  inline void swap(base_list &rhs)
  {
    swap_variables(list_node *, first, rhs.first);
    swap_variables(list_node **, last, rhs.last);
    swap_variables(uint, elements, rhs.elements);
  }
  inline list_node* last_node() { return *last; }
  inline list_node* first_node() { return first;}
  inline void *head() { return first->info; }
  inline void **head_ref() { return first != &end_of_list ? &first->info : 0; }
  inline bool is_empty() { return first == &end_of_list ; }
  inline list_node *last_ref() { return &end_of_list; }
  friend class base_list_iterator;
  friend class error_list;
  friend class error_list_iterator;

#ifdef LIST_EXTRA_DEBUG
  /*
    Check list invariants and print results into trace. Invariants are:
      - (*last) points to end_of_list
      - There are no NULLs in the list.
      - base_list::elements is the number of elements in the list.

    SYNOPSIS
      check_list()
        name  Name to print to trace file

    RETURN 
      1  The list is Ok.
      0  List invariants are not met.
  */

  bool check_list(const char *name)
  {
    base_list *list= this;
    list_node *node= first;
    uint cnt= 0;

    while (node->next != &end_of_list)
    {
      if (!node->info)
      {
        DBUG_PRINT("list_invariants",("%s: error: NULL element in the list", 
                                      name));
        return FALSE;
      }
      node= node->next;
      cnt++;
    }
    if (last != &(node->next))
    {
      DBUG_PRINT("list_invariants", ("%s: error: wrong last pointer", name));
      return FALSE;
    }
    if (cnt+1 != elements)
    {
      DBUG_PRINT("list_invariants", ("%s: error: wrong element count", name));
      return FALSE;
    }
    DBUG_PRINT("list_invariants", ("%s: list is ok", name));
    return TRUE;
  }
#endif // LIST_EXTRA_DEBUG

protected:
  void after(void *info,list_node *node)
  {
    list_node *new_node=new list_node(info,node->next);
    node->next=new_node;
    elements++;
    if (last == &(node->next))
      last= &new_node->next;
  }
};

class base_list_iterator
{
protected:
  base_list *list;
  list_node **el,**prev,*current;
  void sublist(base_list &ls, uint elm)
  {
    ls.first= *el;
    ls.last= list->last;
    ls.elements= elm;
  }
public:
  base_list_iterator() 
    :list(0), el(0), prev(0), current(0)
  {}

  base_list_iterator(base_list &list_par) 
  { init(list_par); }

  inline void init(base_list &list_par)
  {
    list= &list_par;
    el= &list_par.first;
    prev= 0;
    current= 0;
  }

  inline void *next(void)
  {
    prev=el;
    current= *el;
    el= &current->next;
    return current->info;
  }
  inline void *next_fast(void)
  {
    list_node *tmp;
    tmp= *el;
    el= &tmp->next;
    return tmp->info;
  }
  inline void rewind(void)
  {
    el= &list->first;
  }
  inline void *replace(void *element)
  {						// Return old element
    void *tmp=current->info;
    DBUG_ASSERT(current->info != 0);
    current->info=element;
    return tmp;
  }
  void *replace(base_list &new_list)
  {
    void *ret_value=current->info;
    if (!new_list.is_empty())
    {
      *new_list.last=current->next;
      current->info=new_list.first->info;
      current->next=new_list.first->next;
      if ((list->last == &current->next) && (new_list.elements > 1))
	list->last= new_list.last;
      list->elements+=new_list.elements-1;
    }
    return ret_value;				// return old element
  }
  inline void remove(void)			// Remove current
  {
    list->remove(prev);
    el=prev;
    current=0;					// Safeguard
  }
  void after(void *element)			// Insert element after current
  {
    list->after(element,current);
    current=current->next;
    el= &current->next;
  }
  inline void **ref(void)			// Get reference pointer
  {
    return &current->info;
  }
  inline bool is_last(void)
  {
    return el == &list->last_ref()->next;
  }
  friend class error_list_iterator;
};

template <class T> class List :public base_list
{
public:
  inline List() :base_list() {}
  inline List(const List<T> &tmp) :base_list(tmp) {}
  inline List(const List<T> &tmp, MEM_ROOT *mem_root) :
    base_list(tmp, mem_root) {}
  inline bool push_back(T *a) { return base_list::push_back(a); }
  inline bool push_back(T *a, MEM_ROOT *mem_root)
  { return base_list::push_back(a, mem_root); }
  inline bool push_front(T *a) { return base_list::push_front(a); }
  inline T* head() {return (T*) base_list::head(); }
  inline T** head_ref() {return (T**) base_list::head_ref(); }
  inline T* pop()  {return (T*) base_list::pop(); }
  inline void concat(List<T> *list) { base_list::concat(list); }
  inline void disjoin(List<T> *list) { base_list::disjoin(list); }
  inline void prepand(List<T> *list) { base_list::prepand(list); }
  void delete_elements(void)
  {
    list_node *element,*next;
    for (element=first; element != &end_of_list; element=next)
    {
      next=element->next;
      delete (T*) element->info;
    }
    empty();
  }

  using base_list::sort;
};


template <class T> class List_iterator :public base_list_iterator
{
public:
  List_iterator(List<T> &a) : base_list_iterator(a) {}
  List_iterator() : base_list_iterator() {}
  inline void init(List<T> &a) { base_list_iterator::init(a); }
  inline T* operator++(int) { return (T*) base_list_iterator::next(); }
  inline T *replace(T *a)   { return (T*) base_list_iterator::replace(a); }
  inline T *replace(List<T> &a) { return (T*) base_list_iterator::replace(a); }
  inline void rewind(void)  { base_list_iterator::rewind(); }
  inline void remove()      { base_list_iterator::remove(); }
  inline void after(T *a)   { base_list_iterator::after(a); }
  inline T** ref(void)	    { return (T**) base_list_iterator::ref(); }
};


template <class T> class List_iterator_fast :public base_list_iterator
{
protected:
  inline T *replace(T *a)   { return (T*) 0; }
  inline T *replace(List<T> &a) { return (T*) 0; }
  inline void remove(void)  { }
  inline void after(T *a)   { }
  inline T** ref(void)	    { return (T**) 0; }

public:
  inline List_iterator_fast(List<T> &a) : base_list_iterator(a) {}
  inline List_iterator_fast() : base_list_iterator() {}
  inline void init(List<T> &a) { base_list_iterator::init(a); }
  inline T* operator++(int) { return (T*) base_list_iterator::next_fast(); }
  inline void rewind(void)  { base_list_iterator::rewind(); }
  void sublist(List<T> &list_arg, uint el_arg)
  {
    base_list_iterator::sublist(list_arg, el_arg);
  }
};


template <typename T> class base_ilist;
template <typename T> class base_ilist_iterator;

/*
  A simple intrusive list which automaticly removes element from list
  on delete (for THD element)
  NOTE: this inherently unsafe, since we rely on <T> to have
  the same layout as ilink<T> (see base_ilist::sentinel).
  Please consider using a different strategy for linking objects.
*/

template <typename T>
class ilink
{
  T **prev, *next;
public:
  ilink() : prev(NULL), next(NULL) {}

  void unlink()
  {
    /* Extra tests because element doesn't have to be linked */
    if (prev) *prev= next;
    if (next) next->prev=prev;
    prev= NULL;
    next= NULL;
  }

  virtual ~ilink() { unlink(); }		/*lint -e1740 */

  friend class base_ilist<T>;
  friend class base_ilist_iterator<T>;
};


/* Needed to be able to have an I_List of char* strings in mysqld.cc. */

class i_string: public ilink<i_string>
{
public:
  const char* ptr;
  i_string():ptr(0) { }
  i_string(const char* s) : ptr(s) {}
};

/* needed for linked list of two strings for replicate-rewrite-db */
class i_string_pair: public ilink<i_string_pair>
{
public:
  const char* key;
  const char* val;
  i_string_pair():key(0),val(0) { }
  i_string_pair(const char* key_arg, const char* val_arg) : 
    key(key_arg),val(val_arg) {}
};


template <class T> class I_List_iterator;


template<typename T>
class base_ilist
{
  T *first;
  ilink<T> sentinel;
public:
  void empty() {
    first= static_cast<T*>(&sentinel);
    sentinel.prev= &first;
  }
  base_ilist() { empty(); }
  bool is_empty() { return first == static_cast<T*>(&sentinel); }

  /// Pushes new element in front of list.
  void push_front(T *a)
  {
    first->prev= &a->next;
    a->next= first;
    a->prev= &first;
    first= a;
  }

  /// Pushes new element to the end of the list, i.e. in front of the sentinel.
  void push_back(T *a)
  {
    *sentinel.prev= a;
    a->next= static_cast<T*>(&sentinel);
    a->prev= sentinel.prev;
    sentinel.prev= &a->next;
  }

  // Unlink first element, and return it.
  T *get()
  {
    if (is_empty())
      return NULL;
    T *first_link= first;
    first_link->unlink();
    return first_link;
  }

  T *head() { return is_empty() ? NULL : first; }

  /**
    Moves list elements to new owner, and empties current owner (i.e. this).

    @param[in,out]  new_owner  The new owner of the list elements.
                               Should be empty in input.
  */

  void move_elements_to(base_ilist *new_owner)
  {
    DBUG_ASSERT(new_owner->is_empty());
    new_owner->first= first;
    new_owner->sentinel= sentinel;
    empty();
  }

  friend class base_ilist_iterator<T>;
 private:
  /*
    We don't want to allow copying of this class, as that would give us
    two list heads containing the same elements.
    So we declare, but don't define copy CTOR and assignment operator.
  */
  base_ilist(const base_ilist&);
  void operator=(const base_ilist&);
};


template<typename T>
class base_ilist_iterator
{
  base_ilist<T> *list;
  T **el, *current;
public:
  base_ilist_iterator(base_ilist<T> &list_par) :
    list(&list_par),
    el(&list_par.first),
    current(NULL)
  {}

  T *next(void)
  {
    /* This is coded to allow push_back() while iterating */
    current= *el;
    if (current == static_cast<T*>(&list->sentinel))
      return NULL;
    el= &current->next;
    return current;
  }
};


template <class T>
class I_List :private base_ilist<T>
{
public:
  using base_ilist<T>::empty;
  using base_ilist<T>::is_empty;
  using base_ilist<T>::get;
  using base_ilist<T>::push_front;
  using base_ilist<T>::push_back;
  using base_ilist<T>::head;
  void move_elements_to(I_List<T>* new_owner) {
    base_ilist<T>::move_elements_to(new_owner);
  }
#ifndef _lint
  friend class I_List_iterator<T>;
#endif
};


template <class T> 
class I_List_iterator :public base_ilist_iterator<T>
{
public:
  I_List_iterator(I_List<T> &a) : base_ilist_iterator<T>(a) {}
  inline T* operator++(int) { return base_ilist_iterator<T>::next(); }
};

/**
  Make a deep copy of each list element.

  @note A template function and not a template method of class List
  is employed because of explicit template instantiation:
  in server code there are explicit instantiations of List<T> and
  an explicit instantiation of a template requires that any method
  of the instantiated class used in the template can be resolved.
  Evidently not all template arguments have clone() method with
  the right signature.

  @return You must query the error state in THD for out-of-memory
  situation after calling this function.
*/

template <typename T>
inline
void
list_copy_and_replace_each_value(List<T> &list, MEM_ROOT *mem_root)
{
  /* Make a deep copy of each element */
  List_iterator<T> it(list);
  T *el;
  while ((el= it++))
    it.replace(el->clone(mem_root));
}

void free_list(I_List <i_string_pair> *list);
void free_list(I_List <i_string> *list);

#endif // INCLUDES_MYSQL_SQL_LIST_H
