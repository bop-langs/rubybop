#include "internal.h"
#include "bop_api.h"
#include "bop_ports.h"

extern st_table *ppr_pot;
extern char in_ordered_region;

#define BF_BASE 25

#define BF_NEW	(1<<(BF_BASE+0))
#define BF_USE	(1<<(BF_BASE+1))
#define BF_MOD	(1<<(BF_BASE+2))
#define BF_META	(1<<(BF_BASE+3))
#define BF_BOREC	(1<<(BF_BASE+4))  // has a borec record
#define BF_SUBOBJ	(1<<(BF_BASE+5))  // whether to monitor sub-objs

#if 0 //begin generated
def gen str
  $output += str
end

types = ["obj", "table"]
flag_vars = ["RBASIC(obj)->flags", "table->bop_flags"]
ruby_types = ["RVALUE", "st_table"]
sub_names = ["sub", "entry"]
flags = ["NEW", "USE", "MOD"]
all_flags = flags + ["BOREC", "SUBOBJ", "META"]
union = "#{all_flags.map{|i| "BF_#{i}"}*'|'}"
commands = ["ppr_new", "use", "promise"]
raise "mismatch length" unless flags.size == commands.size

$output = ''

types.size.times do |i|
  gen "\n/* monitoring routines for #{types[i]} \*\/\n\n"
  type = types[i]
  flag_var = flag_vars[i]
  gen "#define #{type}_clear_bf(#{type})  (#{flag_var})&=~(#{union})\n"

  flags.size.times do |j|
    gen "#define #{type}_#{commands[j]}_p(#{type}) (#{flag_var}&BF_#{flags[j]})\n"
  end
  gen "#define #{type}_meta_p(#{type}) (#{flag_var}&BF_META)\n"
  gen "#define #{type}_meta(#{type}) (#{flag_var})|=BF_META\n"

  gen "static void bop_scan_#{type}(#{ruby_types[i]} *);\n"

  gen "#define pot_add_#{type}(#{type}) if (!(#{flag_var}&BF_BOREC)) {st_insert(ppr_pot, (st_data_t) #{type}, (st_data_t) &bop_scan_#{type}); (#{flag_var})|=BF_BOREC;}\n"

  flags.size.times do |j|
    all = commands[j]=="ppr_new"? '' : '_all'
    gen "#define #{type}_#{commands[j]}#{all}(#{type}) "
    gen "#{type}_clear_bf(#{type}); " if flags[j].downcase == "new"
    gen "if (task_parallel_p && !(#{flag_var}&BF_META)) {pot_add_#{type}(#{type}); "
    gen "(#{flag_var})&=~BF_SUBOBJ; " if flags[j].downcase != "new"
    gen "bop_msg(5,\"#{type}_#{commands[j]}#{all} %llx -- %d\",#{type},__LINE__); "
    gen "(#{flag_var})|=BF_#{flags[j]};}\n"
  end
  gen "#define #{type}_clobber_all(#{type}) #{type}_use_all(#{type}); #{type}_promise_all(#{type})\n"

  ["use", "promise"].each_with_index do |action, j|
    gen "#define #{type}_#{action}_#{sub_names[i]}(#{type},base,len) if (task_parallel_p && !(#{flag_var}&BF_META)) if (#{flag_var}|BF_SUBOBJ) {BOP_#{action}(#{type}, sizeof(#{ruby_types[i]})); BOP_#{action}(base,len); bop_msg(5,\"#{type}_#{action}_#{sub_names[i]} %llx, %llx, %lld bytes -- %d\",#{type}, base, len, __LINE__);} else #{type}_#{action}_all(#{type})\n"
  end
end

puts $output
# manual changes: bop_scan_obj(VALUE) and BOP_promise(obj, sizeof_rvalue())
#endif //end generated

#define obj_clear_bf(obj)  (RBASIC(obj)->flags)&=~(BF_NEW|BF_USE|BF_MOD|BF_BOREC|BF_SUBOBJ|BF_META)
#define obj_ppr_new_p(obj) (RBASIC(obj)->flags)&BF_NEW
#define obj_use_p(obj) (RBASIC(obj)->flags)&BF_USE
#define obj_promise_p(obj) (RBASIC(obj)->flags)&BF_MOD
#define obj_meta_p(obj) (RBASIC(obj)->flags)&BF_META
#define obj_meta(obj) (RBASIC(obj)->flags)|=BF_META
void bop_scan_obj(VALUE);
#define pot_add_obj(obj) if (!(RBASIC(obj)->flags&BF_BOREC) && !(RBASIC(obj)->flags&BF_META)) {st_insert(ppr_pot, (st_data_t) obj, (st_data_t) &bop_scan_obj); (RBASIC(obj)->flags)|=BF_BOREC;}
#define obj_ppr_new(obj) {obj_clear_bf(obj); if (task_parallel_p) {pot_add_obj(obj); bop_msg(5,"obj_ppr_new %llx -- %s:%d",obj,__FILE__,__LINE__); RBASIC(obj)->flags|=BF_NEW; bop_msg(5,"obj %llx, %llx",obj, RBASIC(obj)->flags); if (in_ordered_region) bop_scan_obj(obj);}}
#define obj_use_all(obj) if (task_parallel_p) {pot_add_obj(obj); (RBASIC(obj)->flags)&=~BF_SUBOBJ; bop_msg(5,"obj_use_all %llx -- %s:%d",obj,__FILE__,__LINE__); (RBASIC(obj)->flags)|=BF_USE; if (in_ordered_region) bop_scan_obj(obj);}
#define obj_promise_all(obj) if (task_parallel_p) {pot_add_obj(obj); (RBASIC(obj)->flags)&=~BF_SUBOBJ; bop_msg(5,"obj_promise_all %llx -- %s:%d",obj,__FILE__,__LINE__); (RBASIC(obj)->flags)|=BF_MOD; if (in_ordered_region) bop_scan_obj(obj);}
#define obj_clobber_all(obj) obj_use_all(obj); obj_promise_all(obj)
#define obj_use_sub(obj,base,len) if (task_parallel_p) if ((RBASIC(obj)->flags)|=BF_SUBOBJ) {BOP_use(obj, sizeof_rvalue()); BOP_use(base,len); bop_msg(5,"obj_use_sub %llx, %llx, %lld bytes -- %s:%d",obj, base, len, __FILE__,__LINE__);} else obj_use_all(obj)
#define obj_promise_sub(obj,base,len) if (task_parallel_p) if ((RBASIC(obj)->flags)|=BF_SUBOBJ) {BOP_promise(obj, sizeof_rvalue()); BOP_promise(base,len); bop_msg(5,"obj_promise_sub %llx, %llx, %lld bytes -- %s:%d",obj, base, len, __FILE__,__LINE__);} else obj_promise_all(obj)
/* end generated */

monitor_t *get_monitor_func( unsigned long bop_flags );

/* Implementations of these are in gc.c */
extern bop_port_t rubybop_gc_port;
void rubybop_gc_cleanup();
