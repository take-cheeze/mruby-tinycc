#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/error.h>
#include <mruby/string.h>
#include <mruby/variable.h>

// #pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#include <libtcc.h>
#include <tcc.h>
// #pragma GCC diagnostic pop

static void
raise_tcc_error(mrb_state *M, mrb_value state, const char *msg, mrb_value file, mrb_value program)
{
  mrb_value const exc = mrb_exc_new_str(M, mrb_class_get(M, "TCCError"), mrb_str_new_cstr(M, msg));
  mrb_iv_set(M, exc, mrb_intern_lit(M, "@state"), state);
  mrb_iv_set(M, exc, mrb_intern_lit(M, "@file"), file);
  mrb_iv_set(M, exc, mrb_intern_lit(M, "@program"), program);
  mrb_exc_raise(M, exc);
}

typedef struct mrb_tcc_state {
  mrb_state *M;
  TCCState *T;
  mrb_value errors, error_callback;
} mrb_tcc_state;

static void
mrb_tcc_state_free(mrb_state *M, void *p)
{
  mrb_tcc_state *s = (mrb_tcc_state*)p;
  tcc_delete(s->T);
  mrb_free(M, s);
}

static mrb_data_type const mrb_tcc_state_type = {
  "TCCState", mrb_tcc_state_free
};

static TCCState*
get_tcc_state(mrb_state *M, mrb_value v)
{
  return ((mrb_tcc_state*)DATA_PTR(v))->T;
}

static void
mrb_tcc_error_func(void *ptr, char const *str) {
  mrb_tcc_state *s = (mrb_tcc_state*)ptr;
  mrb_value str_val = mrb_str_new_cstr(s->M, str);
  mrb_ary_push(s->M, s->errors, str_val);

  if (!mrb_nil_p(s->error_callback)) {
    mrb_yield(s->M, s->error_callback, str_val);
  }
}

static mrb_value
mrb_tcc_init(mrb_state *M, mrb_value self)
{
  mrb_tcc_state *p = mrb_malloc(M, sizeof(mrb_tcc_state));
  p->T = tcc_new();
  p->M = M;
  p->errors = mrb_ary_new(M);
  p->error_callback = mrb_nil_value();
  mrb_iv_set(M, self, mrb_intern_lit(M, "errors"), p->errors);
  mrb_data_init(self, p, &mrb_tcc_state_type);
  tcc_set_error_func(p->T, p, mrb_tcc_error_func);
  return self;
}

static mrb_value
mrb_tcc_set_lib_path(mrb_state *M, mrb_value self)
{
  char *str;
  mrb_get_args(M, "z", &str);
  tcc_set_lib_path(get_tcc_state(M, self), str);
  return self;
}

static mrb_value
mrb_tcc_set_options(mrb_state *M, mrb_value self)
{
  char *str;
  mrb_get_args(M, "z", &str);
  return tcc_set_options(get_tcc_state(M, self), str), self;
}

static mrb_value
mrb_tcc_add_include_path(mrb_state *M, mrb_value self)
{
  char *str;
  mrb_get_args(M, "z", &str);
  tcc_add_include_path(get_tcc_state(M, self), str);
  return self;
}

static mrb_value
mrb_tcc_add_sysinclude_path(mrb_state *M, mrb_value self)
{
  char *str;
  mrb_get_args(M, "z", &str);
  tcc_add_sysinclude_path(get_tcc_state(M, self), str);
  return self;
}

static mrb_value
mrb_tcc_define_symbol(mrb_state *M, mrb_value self)
{
  mrb_sym sym;
  char *val;
  mrb_get_args(M, "nz", &sym, &val);
  tcc_define_symbol(get_tcc_state(M, self), mrb_sym2name(M, sym), val);
  return self;
}

static mrb_value
mrb_tcc_undefine_symbol(mrb_state *M, mrb_value self)
{
  mrb_sym sym;
  mrb_get_args(M, "z", &sym);
  tcc_undefine_symbol(get_tcc_state(M, self), mrb_sym2name(M, sym));
  return self;
}

static mrb_value
mrb_tcc_add_file(mrb_state *M, mrb_value self)
{
  char *f;
  mrb_get_args(M, "z", &f);
  if (tcc_add_file(get_tcc_state(M, self), f) == -1) {
    mrb_value file_val;
    mrb_get_args(M, "S", &file_val);
    raise_tcc_error(M, self, "add file error", file_val, mrb_nil_value());
  }
  return self;
}

static mrb_value
mrb_tcc_compile_string(mrb_state *M, mrb_value self)
{
  char *prog;
  mrb_get_args(M, "z", &prog);
  if (tcc_compile_string(get_tcc_state(M, self), prog) == -1) {
    mrb_value prog_val;
    mrb_get_args(M, "S", &prog_val);
    raise_tcc_error(M, self, "compile error", mrb_nil_value(), prog_val);
  }
  return self;
}

static mrb_value
mrb_tcc_set_output_type(mrb_state *M, mrb_value self)
{
  mrb_value out;
  int t = 0;
  mrb_get_args(M, "o", &out);
  if (mrb_fixnum_p(out)) {
    t = mrb_fixnum(out);
  } else if (mrb_symbol_p(out)) {
    mrb_sym const sym = mrb_symbol(out);
    t =
        sym == mrb_intern_lit(M, "memory")? TCC_OUTPUT_MEMORY:
        sym == mrb_intern_lit(M, "exe")? TCC_OUTPUT_EXE:
        sym == mrb_intern_lit(M, "dll")? TCC_OUTPUT_DLL:
        sym == mrb_intern_lit(M, "obj")? TCC_OUTPUT_OBJ:
        0;
  }
  if (t == 0) {
    mrb_raisef(M, mrb_class_get(M, "ArgumentError"), "invalid output type: %S", out);
  }
  tcc_set_output_type(get_tcc_state(M, self), t);
  return out;
}

static mrb_value
mrb_tcc_add_library_path(mrb_state *M, mrb_value self)
{
  char *str;
  mrb_get_args(M, "z", &str);
  tcc_add_library_path(get_tcc_state(M, self), str);
  return self;
}

static mrb_value
mrb_tcc_add_library(mrb_state *M, mrb_value self)
{
  char *lib;
  mrb_get_args(M, "z", &lib);
  if (tcc_add_library(get_tcc_state(M, self), lib) == -1) {
    raise_tcc_error(M, self, "add library error", mrb_nil_value(), mrb_nil_value());
  }
  return self;
}

static mrb_value
mrb_tcc_add_symbol(mrb_state *M, mrb_value self)
{
  mrb_sym sym;
  mrb_value ptr;
  mrb_get_args(M, "no", &sym, &ptr);
  if (mrb_cptr_p(ptr)) {
    mrb_raisef(M, mrb_class_get(M, "ArgumentError"), "expected pointer: %S", ptr);
  }
  tcc_add_symbol(get_tcc_state(M, self), mrb_sym2name(M, sym), mrb_cptr(ptr));
  return self;
}

static mrb_value
mrb_tcc_output_file(mrb_state *M, mrb_value self)
{
  char *out;
  mrb_get_args(M, "z", &out);
  if (tcc_output_file(get_tcc_state(M, self), out) == -1) {
    raise_tcc_error(M, self, "output file error", mrb_nil_value(), mrb_nil_value());
  }
  return self;
}

static mrb_value
mrb_tcc_run(mrb_state *M, mrb_value self)
{
  mrb_value *args;
  mrb_int args_len;
  char *argv[256];
  int res;
  mrb_get_args(M, "*", &args, &args_len);
  if (args_len > 256) {
    mrb_raisef(M, mrb_class_get(M, "ArgumentError"), "too many arguments: %S", mrb_fixnum_value(args_len));
  }
  for (mrb_int i = 0; i < args_len; ++i) {
    argv[i] = mrb_str_to_cstr(M, args[i]);
  }
  res = tcc_run(get_tcc_state(M, self), args_len, argv);
  if (res == -1) {
    raise_tcc_error(M, self, "main function running error", mrb_nil_value(), mrb_nil_value());
  }
  return mrb_fixnum_value(res);
}

static mrb_value
mrb_tcc_relocate(mrb_state *M, mrb_value self)
{
  mrb_value ptr;
  int res;
  mrb_get_args(M, "o", &ptr);
  if (mrb_nil_p(ptr)) {
    res = tcc_relocate(get_tcc_state(M, self), NULL);
  } else if (mrb_cptr_p(ptr)) {
    res = tcc_relocate(get_tcc_state(M, self), mrb_cptr(ptr));
  } else {
    mrb_raisef(M, mrb_class_get(M, "ArgumentError"), "invalid relocate pointer: %S", ptr);
  }

  if (res != -1) { return mrb_fixnum_value(res); }
  else {
    raise_tcc_error(M, self, "relocate error", mrb_nil_value(), mrb_nil_value());
  }
  return self;
}

static mrb_value
mrb_tcc_symbol(mrb_state *M, mrb_value self)
{
  mrb_sym sym;
  void *ret;
  mrb_get_args(M, "n", &sym);
  ret = tcc_get_symbol(get_tcc_state(M, self), mrb_sym2name(M, sym));
  return ret? mrb_cptr_value(M, ret) : mrb_nil_value();
}

static mrb_value
mrb_tcc_errors(mrb_state *M, mrb_value self)
{
  return mrb_iv_get(M, self, mrb_intern_lit(M, "errors"));
}

static mrb_value
mrb_tcc_error_callback(mrb_state *M, mrb_value self)
{
  mrb_value b;
  mrb_get_args(M, "&", &b);
  mrb_iv_set(M, self, mrb_intern_lit(M, "error_callback"), b);
  ((mrb_tcc_state*)DATA_PTR(self))->error_callback = b;
  return self;
}

#define def_bool_opt(name)                              \
  static mrb_value                                      \
  mrb_tcc_set_ ## name (mrb_state *M, mrb_value self) { \
    mrb_bool b;                                         \
    mrb_get_args(M, "b", &b);                           \
    get_tcc_state(M, self)->name = b;                   \
    return mrb_bool_value(b);                           \
  }                                                     \

def_bool_opt(verbose)
def_bool_opt(nostdinc)
def_bool_opt(nostdlib)
def_bool_opt(nocommon)
def_bool_opt(static_link)
def_bool_opt(rdynamic)
def_bool_opt(symbolic)
def_bool_opt(alacarte_link)

def_bool_opt(char_is_unsigned)
def_bool_opt(leading_underscore)
def_bool_opt(ms_extensions)
def_bool_opt(dollars_in_identifiers)
def_bool_opt(ms_bitfields)

def_bool_opt(warn_write_strings)
def_bool_opt(warn_unsupported)
def_bool_opt(warn_error)
def_bool_opt(warn_none)
def_bool_opt(warn_implicit_function_declaration)
def_bool_opt(warn_gcc_compat)

#undef def_bool_opt

static mrb_value
mrb_tcc_catch_error(mrb_state *M, mrb_value self)
{
  mrb_value b, ret;
  TCCState *st = get_tcc_state(M, self);
  mrb_get_args(M, "&", &b);

  if (setjmp(st->error_jmp_buf) == 0) {
    st->error_set_jmp_enabled = 1;

    ret = mrb_yield_argv(M, b, 0, NULL);
  } else {
    mrb_value errors = mrb_iv_get(M, self, mrb_intern_lit(M, "errors"));
    st->error_set_jmp_enabled = 0;
    mrb_assert(st->nb_errors > 0);
    ret = RARRAY_PTR(errors)[RARRAY_LEN(errors) - 1];
    raise_tcc_error(M, self, mrb_str_to_cstr(M, ret), mrb_nil_value(), mrb_nil_value());
  }
  st->error_set_jmp_enabled = 0;

  return ret;
}

void
mrb_mruby_tinycc_gem_init(mrb_state *M)
{
  struct RClass *cls = mrb_define_class(M, "TCCState", M->object_class);
  MRB_SET_INSTANCE_TT(cls, MRB_TT_DATA);

  mrb_define_class(M, "TCCError", M->eStandardError_class);

#define def_const(n) mrb_define_const(M, cls, #n, mrb_fixnum_value(TCC_ ## n))
  def_const(OUTPUT_MEMORY);
  def_const(OUTPUT_EXE);
  def_const(OUTPUT_DLL);
  def_const(OUTPUT_OBJ);
  // internal:
  def_const(OUTPUT_PREPROCESS);
#undef def_const

  mrb_define_const(M, cls, "RELOCATE_AUTO", mrb_cptr_value(M, TCC_RELOCATE_AUTO));

#define def_meth(name, func, args) mrb_define_method(M, cls, name, mrb_tcc_ ## func, args)
  def_meth("initialize", init, MRB_ARGS_NONE());

  def_meth("lib_path=", set_lib_path, MRB_ARGS_REQ(1));
  def_meth("options=", set_options, MRB_ARGS_REQ(1));

#define def_bool_opt(name) def_meth(#name "=", set_ ## name, MRB_ARGS_REQ(1))

  def_bool_opt(verbose);
  def_bool_opt(nostdinc);
  def_bool_opt(nostdlib);
  def_bool_opt(nocommon);
  def_bool_opt(static_link);
  def_bool_opt(rdynamic);
  def_bool_opt(symbolic);
  def_bool_opt(alacarte_link);

  def_bool_opt(char_is_unsigned);
  def_bool_opt(leading_underscore);
  def_bool_opt(ms_extensions);
  def_bool_opt(dollars_in_identifiers);
  def_bool_opt(ms_bitfields);

  def_bool_opt(warn_write_strings);
  def_bool_opt(warn_unsupported);
  def_bool_opt(warn_error);
  def_bool_opt(warn_none);
  def_bool_opt(warn_implicit_function_declaration);
  def_bool_opt(warn_gcc_compat);

#undef def_bool_opt

  def_meth("add_include_path", add_include_path, MRB_ARGS_REQ(1));
  def_meth("add_sysinclude_path", add_sysinclude_path, MRB_ARGS_REQ(1));

  def_meth("define_symbol", define_symbol, MRB_ARGS_REQ(2));
  def_meth("undefine_symbol", undefine_symbol, MRB_ARGS_REQ(1));

  def_meth("add_file", add_file, MRB_ARGS_REQ(1));
  def_meth("compile_string", compile_string, MRB_ARGS_REQ(1));

  def_meth("output_type=", set_output_type, MRB_ARGS_REQ(1));
  def_meth("add_library", add_library, MRB_ARGS_REQ(1));
  def_meth("add_library_path", add_library_path, MRB_ARGS_REQ(1));
  def_meth("add_symbol", add_symbol, MRB_ARGS_REQ(1));
  def_meth("output_file", output_file, MRB_ARGS_REQ(1));
  def_meth("run", run, MRB_ARGS_ANY());
  def_meth("relocate", relocate, MRB_ARGS_REQ(1));
  def_meth("symbol", symbol, MRB_ARGS_REQ(1));

  def_meth("errors", errors, MRB_ARGS_NONE());
  def_meth("error_callback", error_callback, MRB_ARGS_BLOCK());

  def_meth("catch_error", catch_error, MRB_ARGS_BLOCK());
#undef def_meth
}

void mrb_mruby_tinycc_gem_final(mrb_state *M) {}
