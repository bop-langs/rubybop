#include <stdio.h>
#include "ppr.h"
#include "gc.h"
#include "internal.h"
#include "bop_api.h"
#include "bop_ports.h"


//SEARCH BRIAN in the repo to see which files were edited in MRI
//TODO get this to work
//extern bop_port_t ruby_monitor;
extern bop_port_t rubyheap_port;
extern bop_port_t rb_object_port;


extern void BOP_use(void*, size_t);
extern void BOP_promise(void*, size_t);
extern int _BOP_ppr_begin(int);
extern int _BOP_ppr_end(int);

#ifndef SEQUENTIAL
#define SEQUENTIAL (BOP_task_status() == SEQ || BOP_task_status() == UNDY)
#endif

#define OBJ_RW_SETS
/**Return true if safe*/
bool pre_bop_begin(){
  if(!rb_thread_alone()){
    //there are multiple threads happening. raise an error and no PPR!
    rb_raise(rb_eThreadError, "Multiple ruby threads at pre-ppr. Not allowing PPR to take place.");
    abort();
    return false;
  }
  bop_msg(4, "Pre-ppr check is valid! Allowing to enter PPR region");
  return true;
}

int is_sequential(){
  return SEQUENTIAL;
}

extern void set_rheap_nulll(void);

static VALUE
ppr_puts(ppr, obj)
VALUE ppr, obj;
{

    VALUE str = rb_obj_as_string(obj);
    BOP_printf("%s\n", RSTRING_PTR(str));

    return Qnil;
}

static VALUE
ppr_abort_spec(ppr, msg)
VALUE ppr, msg;
{

    VALUE str = rb_obj_as_string(msg);
    BOP_abort_spec(RSTRING_PTR(str));

    return Qnil;
}

static VALUE
ppr_abort_next_spec(ppr, msg)
VALUE ppr, msg;
{

    VALUE str = rb_obj_as_string(msg);
    BOP_abort_next_spec(RSTRING_PTR(str));

    return Qnil;
}

static VALUE
ppr_meaning() {
    bop_msg(1, "hi");
    return LONG2NUM(42);
}


//DOES NOT HAVE THE SAME RETURN VALUE AS YIELD WOULD. THIS NEEDS MORE THOUGHT
VALUE
ppr_yield(VALUE start_val)
{
    // VALUE * ret = NULL;
    bool ppr_ok = pre_bop_begin();
    if(ppr_ok)
      BOP_ppr_begin(1);
        // rb_gc_disable();
        bop_msg(3,"yielding block...");
        rb_yield(start_val);
        // rb_gc_enable();
    if(ppr_ok)
      BOP_ppr_end(1);
    return Qnil;
}
static VALUE
ordered_yield()
{
    BOP_ordered_begin(1);
        bop_msg(3,"yielding ordered block...");
        rb_yield(0);
    BOP_ordered_end(1);
    return Qnil;
}

static VALUE
ppr_start(){
  bool ppr_ok = pre_bop_begin();
  if(ppr_ok)
    BOP_ppr_begin(1);
      bop_msg(3,"starting block...");
      rb_yield(INT2NUM(BOP_spec_order( )));
  if(ppr_ok)
    BOP_ppr_end(1);
  return Qnil;
}

static VALUE
ordered_start(VALUE start_val){
  int start_int = FIX2INT(start_val);
  BOP_ordered_begin(start_int);
      bop_msg(3,"yielding ordered block...");
      rb_yield(0);
  BOP_ordered_end(start_int);
  return Qnil;
}


static VALUE
ppr_ppr_index(ppr)
VALUE ppr;
{
  return INT2NUM(BOP_ppr_index( ));
}

static VALUE
ppr_spec_order(ppr)
     VALUE ppr;
{
  return INT2NUM(BOP_spec_order( ));
}

static VALUE
verbose(ppr, level)
VALUE ppr, level;
{
  BOP_set_verbose( FIX2INT(level) );
  return level;
}

static VALUE
set_group_size(ppr, size)
VALUE ppr, size;
{
  BOP_set_group_size( FIX2INT(size) );
  return size;
}

static VALUE
get_group_size(ppr)
VALUE ppr;
{
   int ret = INT2FIX(  BOP_get_group_size() );
   return ret;
}

static VALUE rb_cPPR, rb_cOrdered;

extern void BOP_this_group_over();
VALUE ppr_over(){
  BOP_this_group_over();
  return Qnil;
}
extern char * BOP_task_str(void);
static VALUE rb_task_status(){
  return rb_str_new2( BOP_task_str() );
}

void
Init_PPR() {

    rb_cPPR = rb_define_class("PPR", rb_cProc);
    rb_define_method(rb_cPPR, "meaning", ppr_meaning, 0);
    rb_define_singleton_method(rb_cPPR, "yield", ppr_yield, 1);
    rb_define_singleton_method(rb_cPPR, "ppr_index", ppr_ppr_index, 0);
    rb_define_singleton_method(rb_cPPR, "spec_order", ppr_spec_order, 0);
    //rb_define_singleton_method(rb_cPPR, "pot", get_pot, 0);
    rb_define_singleton_method(rb_cPPR, "abort_spec", ppr_abort_spec, 1);
    rb_define_singleton_method(rb_cPPR, "abort_next_spec", ppr_abort_next_spec, 1);
    rb_define_singleton_method(rb_cPPR, "over", ppr_over, 0);

    rb_define_singleton_method(rb_cPPR, "puts", ppr_puts, 1);
    rb_define_singleton_method(rb_cPPR, "verbose", verbose, 1);
    rb_define_singleton_method(rb_cPPR, "set_group_size", set_group_size, 1);
    rb_define_singleton_method(rb_cPPR, "get_group_size", get_group_size, 0);
    rb_define_singleton_method(rb_cPPR, "start", ppr_start, 0);

    rb_define_singleton_method(rb_cPPR, "task_status", rb_task_status, 0);

    //record read and write
    // rb_define_singleton_method(rb_cPPR, "read", record_bop_rd, 1);
    // rb_define_singleton_method(rb_cPPR, "write", record_bop_wrt, 1);

    rb_define_method(rb_mKernel, "PPR", ppr_start, 0);

    //TODO get this uncommented
    //register_port(&ruby_monitor, "Ruby Object Monitoring Port");
    //register_port(&rubybop_gc_port, "RubyBOP GC Port");
    register_port(&rubyheap_port, (char*) "RubyHeap Port");
    register_port(&rb_object_port, (char*) "Ruby object port");
}

void
Init_Ordered() {
    rb_cOrdered = rb_define_class("Ordered", rb_cProc);
    rb_define_singleton_method(rb_cOrdered, "start", ordered_start, 1);
    rb_define_method(rb_mKernel, "Ordered", ordered_yield, 0);
}
