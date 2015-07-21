#include <stdio.h>
#include "ppr.h"
#include "gc.h"
#include "internal.h"
#include "../bop/bop_api.h"
#include "../bop/bop_ports.h"

//TODO get get ppr_mon to work

//SEARCH BRIAN in the repo to see which files were edited in MRI
//TODO get this to work
//extern bop_port_t ruby_monitor;
extern int _BOP_ppr_begin();
extern int _BOP_ppr_end();
//VALUE proc_invoke _((VALUE, VALUE, VALUE, VALUE)); // eval.c, line 235

extern void BOP_use(void*, size_t);
extern void BOP_promise(void*, size_t);

#ifndef SEQUENTIAL
#define SEQUENTIAL (BOP_task_status() == SEQ || BOP_task_status() == UNDY)
#endif

static int recurse = 1;

void BOP_obj_use(VALUE obj){
  if (recurse){
    int size = 0;
    recurse = 0;
    size = rb_obj_memsize_of(obj);
    if(size!=0){
      bop_msg(5, "Using object %p at address %p with size %d", (obj), &obj, size);
      BOP_use(obj, size);
    }
    recurse = 1;
  }
}
void BOP_obj_promise(VALUE obj){
  if (recurse){
    int size = 0;
    recurse = 0;
    size = rb_obj_memsize_of(obj);
    if(size!=0){
      bop_msg(5, "Promising object %p at address %p with size %d", (obj), &obj, size);
      BOP_promise(obj, size);
    }
    recurse = 1;
  }
}

void _BOP_obj_use_promise(VALUE obj, const char* file, int line, const char* function){
  if(!SEQUENTIAL && (obj != NULL|| !FIXNUM_P(obj))){
    int size = rb_obj_memsize_of(obj);
    if (size > 0){
      bop_msg(1, "USE PROMISE \tfile: %s line: %d function: %s\t object: %016llx size: %d\t", file, line, function, obj, size);
      BOP_obj_use(obj);
      BOP_obj_promise(obj);
    }
  }
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

static VALUE
ppr_use(VALUE ppr, VALUE obj)
{
    BOP_obj_use(obj);
    return obj;
}

static VALUE
ppr_promise(VALUE ppr, VALUE obj)
{
    BOP_obj_promise(obj);
    return obj;
}

static VALUE
ppr_yield()
{
    //set_rheap_null();
    BOP_ppr_begin(1);
        rb_gc_disable();
        //set_rheap_null();
        bop_msg(3,"yielding block...");
        rb_yield(0);
        rb_gc_enable();
    BOP_ppr_end(1);
    return Qnil;
}
static VALUE
ordered_yield()
{
    //set_rheap_null();
    BOP_ordered_begin(1);
        bop_msg(3,"yielding ordered block...");
        rb_yield(0);
    BOP_ordered_end(1);
    return Qnil;
}





static VALUE
ppr_info(ppr, obj)
VALUE ppr, obj;
{
    char buf1[50], buf2[50], buf3[50], buf4[50];
    sprintf(buf1, "Value (hex): %lx", (unsigned long int) obj);
    sprintf(buf2, "Masked Value (hex): %lx", (unsigned long int) obj & 0xfffffffffffffffe);
    sprintf(buf3, "RShift Value (dec): %ld", ((unsigned long int) obj) >> 1);
    rb_funcall(ppr, rb_intern("puts"), 1, rb_str_new2(buf1));
    rb_funcall(ppr, rb_intern("puts"), 1, rb_str_new2(buf2));
    rb_funcall(ppr, rb_intern("puts"), 1, rb_str_new2(buf3));

    return Qnil;
}
//TODO get these to work
/*
extern st_table *ppr_pot;
void bop_scan_table( st_table* );

static int add_i( VALUE key, VALUE obj, VALUE ary ) {
  if ( key == ary ) return ST_CONTINUE;

  if ( CLASS_OF(key) == NULL ) {
    bop_msg( 5, "pot obj %llx has no class (terminated)", key );
    return ST_CONTINUE;
  }

  if ( (void*) obj == bop_scan_table ) {
    bop_msg( 5, "pot st_table %llx has %d entries",
	     key, ((st_table*)key)->num_entries );
    return ST_CONTINUE;
  }

  RARRAY_PTR(ary)[ RARRAY(ary)->as.heap.len ++ ] = key;

  return ST_CONTINUE;
}


//TODO get these to work
static VALUE
get_pot(void)
{
  VALUE ary = rb_ary_new2( ppr_pot->num_entries );
  st_foreach( ppr_pot, add_i, ary );
  return ary;
}
*/

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
ordered_call(ordered, args)
VALUE ordered, args;
{
    BOP_ordered_begin( 1 );

    VALUE ret = rb_proc_call(ordered, args);

    BOP_ordered_end( 1 );

    return Qnil;
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

static VALUE
kernel_ppr(void)
{
	VALUE ppr = rb_funcall(rb_cPPR, rb_intern("new"), 0);
	return rb_funcall(ppr, rb_intern("yield"), 0);
}

static VALUE
kernel_ordered(void)
{
	VALUE ordered = rb_funcall(rb_cOrdered, rb_intern("new"), 0);
	return rb_funcall(ordered, rb_intern("yield"), 0);
}

void
Init_PPR() {

    rb_cPPR = rb_define_class("PPR", rb_cProc);
    rb_define_method(rb_cPPR, "meaning", ppr_meaning, 0);
    rb_define_singleton_method(rb_cPPR, "use", ppr_use, 1);
    rb_define_singleton_method(rb_cPPR, "promise", ppr_promise, 1);
    rb_define_singleton_method(rb_cPPR, "yield", ppr_yield, 0);
    rb_define_singleton_method(rb_cPPR, "ppr_index", ppr_ppr_index, 0);
    rb_define_singleton_method(rb_cPPR, "spec_order", ppr_spec_order, 0);
    //rb_define_singleton_method(rb_cPPR, "pot", get_pot, 0);
    rb_define_singleton_method(rb_cPPR, "abort_spec", ppr_abort_spec, 1);
    rb_define_singleton_method(rb_cPPR, "abort_next_spec", ppr_abort_next_spec, 1);

    rb_define_singleton_method(rb_cPPR, "puts", ppr_puts, 1);
    rb_define_singleton_method(rb_cPPR, "verbose", verbose, 1);
    rb_define_singleton_method(rb_cPPR, "set_group_size", set_group_size, 1);
    rb_define_singleton_method(rb_cPPR, "get_group_size", get_group_size, 0);

    rb_define_method(rb_mKernel, "PPR", ppr_yield, 0);

    //TODO get this uncommented
    //register_port(&ruby_monitor, "Ruby Object Monitoring Port");
    //register_port(&rubybop_gc_port, "RubyBOP GC Port");
}

void
Init_Ordered() {
    rb_cOrdered = rb_define_class("Ordered", rb_cProc);
    rb_define_method(rb_cOrdered, "call", ordered_call, -2);

    rb_define_method(rb_mKernel, "Ordered", ordered_yield, 0);
}
