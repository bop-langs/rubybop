#include <internal.h>
#include <stdio.h>
#include <ppr.h>
#include <../bop/bop_api.h>

#include <../bop/bop_ports.h>
extern bop_port_t ruby_monitor;

VALUE proc_invoke _((VALUE, VALUE, VALUE, VALUE)); // eval.c, line 235

static VALUE
ppr_puts(ppr, obj)
VALUE ppr, obj;
{

    VALUE str = rb_obj_as_string(obj);
    BOP_printf("%s\n", RSTRING(str)->ptr);

    return Qnil;
}

static VALUE
ppr_abort_spec(ppr, msg)
VALUE ppr, msg;
{

    VALUE str = rb_obj_as_string(msg);
    BOP_abort_spec(RSTRING(str)->ptr);

    return Qnil;
}

static VALUE
ppr_abort_next_spec(ppr, msg)
VALUE ppr, msg;
{

    VALUE str = rb_obj_as_string(msg);
    BOP_abort_next_spec(RSTRING(str)->ptr);

    return Qnil;
}

static VALUE
ppr_meaning() {
    bop_msg(1, "hi");
    return LONG2NUM(42);
}

static VALUE
ppr_call(ppr, args)
VALUE ppr, args; /* OK */
{
  BOP_ppr_begin(1);

    VALUE ret = proc_invoke(ppr, args, Qundef, 0);

    // if (!NIL_P(ret))
    //  BOP_abort_spec("PPR returns a non-nil value");

    if (task_parallel_p) ppr_pot_upload( );

  BOP_ppr_end(1);

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
  
  RARRAY(ary)->ptr[ RARRAY(ary)->len ++ ] = key;

  return ST_CONTINUE;
}

static VALUE
get_pot(void)
{
  VALUE ary = rb_ary_new2( ppr_pot->num_entries );
  st_foreach( ppr_pot, add_i, ary );
  return ary;
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
ordered_call(ordered, args)
VALUE ordered, args;
{
    BOP_ordered_begin( 1 );

    VALUE ret = proc_invoke(ordered, args, Qundef, 0);

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
	return rb_funcall(ppr, rb_intern("call"), 0);
}

static VALUE
kernel_ordered(void)
{
	VALUE ordered = rb_funcall(rb_cOrdered, rb_intern("new"), 0);
	return rb_funcall(ordered, rb_intern("call"), 0);
}

void
Init_PPR() {
    rb_cPPR = rb_define_class("PPR", rb_cProc);
    rb_define_method(rb_cPPR, "meaning", ppr_meaning, 0);
    rb_define_method(rb_cPPR, "call", ppr_call, -2);
    rb_define_singleton_method(rb_cPPR, "ppr_index", ppr_ppr_index, 0);
    rb_define_singleton_method(rb_cPPR, "spec_order", ppr_spec_order, 0);
    rb_define_singleton_method(rb_cPPR, "pot", get_pot, 0);
    rb_define_singleton_method(rb_cPPR, "abort_spec", ppr_abort_spec, 1);
    rb_define_singleton_method(rb_cPPR, "abort_next_spec", ppr_abort_next_spec, 1);

    rb_define_singleton_method(rb_cPPR, "puts", ppr_puts, 1);
    rb_define_singleton_method(rb_cPPR, "verbose", verbose, 1);
    rb_define_singleton_method(rb_cPPR, "set_group_size", set_group_size, 1);
    rb_define_singleton_method(rb_cPPR, "get_group_size", get_group_size, 0);

    rb_define_method(rb_mKernel, "PPR", kernel_ppr, 0);

    register_port(&ruby_monitor, "Ruby Object Monitoring Port");
    register_port(&rubybop_gc_port, "RubyBOP GC Port");
}

void
Init_Ordered() {
    rb_cOrdered = rb_define_class("Ordered", rb_cProc);
    rb_define_method(rb_cOrdered, "call", ordered_call, -2);

    rb_define_method(rb_mKernel, "Ordered", kernel_ordered, 0);
}
