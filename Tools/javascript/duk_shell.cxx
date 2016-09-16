#include <duktape.h>

#include "js_shell.h"

#include <iostream>
#include <stdio.h>

#ifdef __GNUC__
#include <dlfcn.h>
#define LOAD_SYMBOL(handle, name) dlsym(handle, name)
#else
#error "implement dll loading"
#endif

class DUKShell: public JSShell {
public:

  DUKShell() { ctx = NULL; };

  virtual ~DUKShell();

protected:

  virtual bool InitializeEngine();

  virtual bool ExecuteScript(const std::string& source, const std::string& scriptPath);

  virtual bool DisposeEngine();

private:

  duk_context *ctx;
};

DUKShell::~DUKShell() {
}

duk_ret_t console_log_impl(duk_context *ctx) {
  int nargs = duk_get_top(ctx);

  /* Mimic Node.js behavior when passed nothing. */
  if (nargs == 0) {
    puts("");
    return 0;
  }

  const char *str = duk_safe_to_string(ctx, -1);
  fprintf(stdout, "%s\n", str);
  return 0;
}

duk_ret_t load_module(duk_context *ctx) {
  std::string id = duk_get_string(ctx, 0);
  std::string lib = "lib" + id + ".so";
  std::string init_fn = "dukopen_" + id;
  void *handle = dlopen(lib.c_str(), RTLD_NOW);
  if (handle == NULL) {
      std::cout << "Error loading " << lib << std::endl;
      return 0;
  }
  /* duk_ret_t swig_duk_init(duk_context *ctx) { */
  duk_ret_t (*swig_duk_init)(duk_context*);
  swig_duk_init = (duk_ret_t(*)(duk_context*))dlsym(handle, init_fn.c_str());
  if (swig_duk_init == NULL) {
      std::cout << "Error running initializer of " << lib << std::endl;
      dlclose(handle);
      return 0;
  }
  duk_ret_t t = swig_duk_init(ctx);
  dlclose(handle);
  return t;
}

bool DUKShell::InitializeEngine() {

  if(ctx!=NULL) DisposeEngine();

  ctx = duk_create_heap_default();
  if(ctx==NULL) return false;

  /* hardcoded console module */
  duk_idx_t console_idx = duk_push_object(ctx);
  duk_push_c_function(ctx, console_log_impl, DUK_VARARGS);
  duk_put_prop_string(ctx, console_idx, "log");
  duk_put_global_string(ctx, "console");

  /* set up "generic" module handler */
  duk_push_global_object(ctx);
  duk_idx_t dukint_idx = duk_get_prop_string(ctx, -1, "Duktape");
  duk_push_c_function(ctx, load_module, 4);
  duk_put_prop_string(ctx, dukint_idx, "modSearch");
  

  return true;
}

bool DUKShell::ExecuteScript(const std::string& source, const std::string& scriptPath) {
  if (ctx == NULL) {
      std::cout << "uninitialized context" << std::endl;
      return false;
  }
    /*void duk_eval_string(duk_context *ctx, const char *src);*/
  if (duk_peval_string(ctx,source.c_str()) != 0) {
      std::cout << "" << scriptPath << ":" << duk_safe_to_string(ctx, -1) << std::endl;
      return false;
  } else {
      std::cout << duk_safe_to_string(ctx, -1) << std::endl;
      return true;
  };
}

bool DUKShell::DisposeEngine() {
    if (ctx != NULL) {
        duk_destroy_heap(ctx);
        ctx=NULL;
    }
    return true;
}

JSShell* DUKShell_Create() {
  return new DUKShell();
}
