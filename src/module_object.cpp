#include "natalie.hpp"

namespace Natalie {

ModuleObject::ModuleObject()
    : ModuleObject { Object::Type::Module, GlobalEnv::the()->Module() } { }

ModuleObject::ModuleObject(const char *name)
    : ModuleObject {} {
    this->set_class_name(name);
}

ModuleObject::ModuleObject(Type type, ClassObject *klass)
    : Object { type, klass }
    , m_env { new Env() } { }

Value ModuleObject::initialize(Env *env, Block *block) {
    if (block) {
        Value self = this;
        block->set_self(self);
        Value args[] = { self };
        NAT_RUN_BLOCK_AND_POSSIBLY_BREAK(env, block, Args(1, args), nullptr);
    }
    return this;
}

Value ModuleObject::include(Env *env, Args args) {
    for (int i = args.size() - 1; i >= 0; i--) {
        include_once(env, args[i]->as_module());
    }
    return this;
}

void ModuleObject::include_once(Env *env, ModuleObject *module) {
    if (m_included_modules.is_empty()) {
        m_included_modules.push(this);
        m_included_modules.push(module);
    } else {
        ssize_t this_index = -1;
        for (size_t i = 0; i < m_included_modules.size(); ++i) {
            if (m_included_modules[i] == this)
                this_index = i;
            if (m_included_modules[i] == module)
                return;
        }
        assert(this_index != -1);
        m_included_modules.insert(this_index + 1, module);
    }
}

Value ModuleObject::prepend(Env *env, Args args) {
    for (int i = args.size() - 1; i >= 0; i--) {
        prepend_once(env, args[i]->as_module());
    }
    return this;
}

void ModuleObject::prepend_once(Env *env, ModuleObject *module) {
    if (m_included_modules.is_empty()) {
        m_included_modules.push(module);
        m_included_modules.push(this);
    } else {
        for (auto m : m_included_modules) {
            if (m == module)
                return;
        }
        m_included_modules.push_front(module);
    }
}

Value ModuleObject::const_get(SymbolObject *name) const {
    auto constant = m_constants.get(name);
    if (constant)
        return constant->value();
    else
        return nullptr;
}

Value ModuleObject::const_get(Env *env, Value name) const {
    auto symbol = name->to_symbol(env, Object::Conversion::Strict);
    auto constant = const_get(symbol);
    if (!constant)
        env->raise("NameError", "uninitialized constant {}", symbol->c_str());
    return constant;
}

Value ModuleObject::const_fetch(SymbolObject *name) {
    auto constant = const_get(name);
    if (!constant) {
        printf("Constant %s is missing!\n", name->c_str());
        abort();
    }
    return constant;
}

Value ModuleObject::const_find(Env *env, SymbolObject *name, ConstLookupSearchMode search_mode, ConstLookupFailureMode failure_mode) {
    ModuleObject *search_parent = nullptr;
    Constant *constant = nullptr;

    auto check_valid = [&](Constant *constant) {
        if (search_mode == ConstLookupSearchMode::Strict && constant->is_private()) {
            if (search_parent && search_parent != GlobalEnv::the()->Object())
                env->raise_name_error(name, "private constant {}::{} referenced", search_parent->inspect_str(), name->c_str());
            else
                env->raise_name_error(name, "private constant ::{} referenced", name->c_str());
        }
        if (constant->is_deprecated()) {
            if (search_parent && search_parent != GlobalEnv::the()->Object())
                env->warn("constant {}::{} is deprecated", search_parent->inspect_str(), name->c_str());
            else
                env->warn("constant ::{} is deprecated", name->c_str());
        }
    };

    if (search_mode == ConstLookupSearchMode::NotStrict) {
        // first search in parent namespaces (not including global, i.e. Object namespace)
        search_parent = this;
        do {
            if (!search_parent || search_parent == GlobalEnv::the()->Object())
                break;
            constant = search_parent->m_constants.get(name);
            search_parent = search_parent->owner();
        } while (!constant);
        if (constant) {
            check_valid(constant);
            return constant->value();
        }
    }

    // search included modules
    Vector<ModuleObject *> modules_to_search;
    for (ModuleObject *module : m_included_modules) {
        if (module != this)
            modules_to_search.push(module);
    }
    for (size_t i = 0; i < modules_to_search.size(); ++i) {
        auto module = modules_to_search.at(i);
        constant = module->m_constants.get(name);
        if (constant)
            break;
        for (ModuleObject *m : module->m_included_modules) {
            if (m != module && m != this)
                modules_to_search.push(m);
        }
    }

    if (constant) {
        check_valid(constant);
        return constant->value();
    }

    // search in superclass hierarchy
    search_parent = this;
    do {
        constant = search_parent->m_constants.get(name);
        if (constant) break;
        search_parent = search_parent->m_superclass;
    } while (search_parent && search_parent != GlobalEnv::the()->Object());

    if (constant) {
        check_valid(constant);
        return constant->value();
    }

    if (this != GlobalEnv::the()->Object() && search_mode == ConstLookupSearchMode::NotStrict) {
        // lastly, search on the global, i.e. Object namespace
        constant = GlobalEnv::the()->Object()->m_constants.get(name);
    }

    if (constant) {
        check_valid(constant);
        return constant->value();
    }

    if (failure_mode == ConstLookupFailureMode::Null) return nullptr;

    if (search_mode == ConstLookupSearchMode::Strict) {
        env->raise_name_error(name, "uninitialized constant {}::{}", this->inspect_str(), name->c_str());
    } else {
        env->raise_name_error(name, "uninitialized constant {}", name->c_str());
    }
}

Value ModuleObject::const_set(SymbolObject *name, Value val) {
    m_constants.put(name, new Constant { name, val.object() });
    if (val->is_module()) {
        if (!val->owner()) {
            val->set_owner(this);
            if (val->singleton_class()) val->singleton_class()->set_owner(this);
        }

        if (!val->as_module()->class_name()) {
            auto class_name = name->string();
            val->as_module()->set_class_name(class_name);

            auto singleton_class = val->singleton_class();
            while (singleton_class) {
                class_name = String::format("#<Class:{}>", class_name);
                singleton_class->set_class_name(class_name);
                singleton_class = singleton_class->singleton_class();
            }
        }
    }
    return val;
}

Value ModuleObject::const_set(Env *env, Value name, Value val) {
    return const_set(name->to_symbol(env, Object::Conversion::Strict), val);
}

Value ModuleObject::constants(Env *env, Value inherit) const {
    auto ary = new ArrayObject;
    for (auto pair : m_constants)
        ary->push(pair.first);
    if (inherit == nullptr || inherit->is_truthy()) {
        for (ModuleObject *module : m_included_modules) {
            if (module != this) {
                ary->concat(*module->constants(env, inherit)->as_array());
            }
        }
    }
    return ary;
}

void ModuleObject::make_alias(Env *env, SymbolObject *new_name, SymbolObject *old_name) {
    auto method_info = find_method(env, old_name);
    assert_method_defined(env, old_name, method_info);
    define_method(env, new_name, method_info.method(), method_info.visibility());
};

void ModuleObject::alias(Env *env, SymbolObject *new_name, SymbolObject *old_name) {
    if (GlobalEnv::the()->instance_evaling()) {
        singleton_class(env)->make_alias(env, new_name, old_name);
        return;
    }
    make_alias(env, new_name, old_name);
}

Value ModuleObject::eval_body(Env *env, Value (*fn)(Env *, Value)) {
    Env body_env { m_env };
    body_env.set_caller(env);
    Value result = fn(&body_env, this);
    m_method_visibility = MethodVisibility::Public;
    m_module_function = false;
    return result;
}

Value ModuleObject::cvar_get_or_null(Env *env, SymbolObject *name) {
    if (!name->is_cvar_name())
        env->raise_name_error(name, "`{}' is not allowed as a class variable name", name->c_str());

    ModuleObject *module = this;
    Value val = nullptr;
    while (module) {
        val = module->m_class_vars.get(name, env);
        if (val)
            return val;
        module = module->m_superclass;
    }
    return nullptr;
}

Value ModuleObject::cvar_set(Env *env, SymbolObject *name, Value val) {
    if (!name->is_cvar_name())
        env->raise_name_error(name, "`{}' is not allowed as a class variable name", name->c_str());

    ModuleObject *current = this;

    Value exists = nullptr;
    while (current) {
        exists = current->m_class_vars.get(name, env);
        if (exists) {
            current->m_class_vars.put(name, val.object(), env);
            return val;
        }
        current = current->m_superclass;
    }
    m_class_vars.put(name, val.object(), env);
    return val;
}

SymbolObject *ModuleObject::define_method(Env *env, SymbolObject *name, MethodFnPtr fn, int arity, bool optimized) {
    Method *method = new Method { name->c_str(), this, fn, arity };
    if (optimized)
        method->set_optimized(true);
    define_method(env, name, method, m_method_visibility);
    if (m_module_function)
        define_singleton_method(env, name, fn, arity);
    return name;
}

SymbolObject *ModuleObject::define_method(Env *env, SymbolObject *name, Block *block) {
    Method *method = new Method { name->c_str(), this, block };
    define_method(env, name, method, m_method_visibility);
    if (m_module_function)
        define_singleton_method(env, name, block);
    return name;
}

SymbolObject *ModuleObject::undefine_method(Env *env, SymbolObject *name) {
    m_methods.put(name, MethodInfo(MethodVisibility::Public), env);
    return name;
}

// supply an empty array and it will be populated with the method names as symbols
void ModuleObject::methods(Env *env, ArrayObject *array, bool include_super) {
    for (auto pair : m_methods) {
        if (array->include(env, pair.first))
            continue;
        array->push(pair.first);
    }
    if (!include_super) {
        return;
    }
    for (ModuleObject *module : m_included_modules) {
        for (auto pair : module->m_methods) {
            if (array->include(env, pair.first))
                continue;
            array->push(pair.first);
        }
    }
    if (m_superclass) {
        m_superclass->methods(env, array);
    }
}

void ModuleObject::define_method(Env *env, SymbolObject *name, Method *method, MethodVisibility visibility) {
    m_methods.put(name, MethodInfo(visibility, method), env);
}

// returns the method and sets matching_class_or_module to where the method was found
MethodInfo ModuleObject::find_method(Env *env, SymbolObject *method_name, ModuleObject **matching_class_or_module, const Method **after_method) const {
    MethodInfo method_info;
    if (m_included_modules.is_empty()) {
        // no included modules, just search the class/module
        // note: if there are included modules, then the module chain will include this class/module
        method_info = m_methods.get(method_name, env);
        if (method_info) {
            if (!method_info.is_defined()) return method_info;
            auto method = method_info.method();
            if (after_method != nullptr && method == *after_method) {
                *after_method = nullptr;
            } else if (after_method == nullptr || *after_method == nullptr) {
                if (matching_class_or_module) *matching_class_or_module = m_klass;
                return method_info;
            }
        }
    }

    for (ModuleObject *module : m_included_modules) {
        if (module == this) {
            method_info = module->m_methods.get(method_name, env);
        } else {
            method_info = module->find_method(env, method_name, matching_class_or_module, after_method);
        }
        if (method_info) {
            if (!method_info.is_defined()) return method_info;
            auto method = method_info.method();
            if (after_method != nullptr && method == *after_method) {
                *after_method = nullptr;
            } else if (after_method == nullptr || *after_method == nullptr) {
                if (matching_class_or_module) *matching_class_or_module = module;
                return method_info;
            }
        }
    }

    if (m_superclass) {
        return m_superclass->find_method(env, method_name, matching_class_or_module, after_method);
    } else {
        return {};
    }
}

MethodInfo ModuleObject::find_method(Env *env, SymbolObject *method_name, const Method *after_method) const {
    return find_method(env, method_name, nullptr, &after_method);
}

void ModuleObject::assert_method_defined(Env *env, SymbolObject *name, MethodInfo method_info) {
    if (!method_info.is_defined()) {
        if (is_class()) {
            env->raise_name_error(name, "undefined method `{}' for class `{}'", name->c_str(), inspect_str());
        } else {
            env->raise_name_error(name, "undefined method `{}' for module `{}'", name->c_str(), inspect_str());
        }
    }
}

Value ModuleObject::instance_method(Env *env, Value name_value) {
    auto name = name_value->to_symbol(env, Object::Conversion::Strict);
    auto method_info = find_method(env, name);
    assert_method_defined(env, name, method_info);
    return new UnboundMethodObject { this, method_info.method() };
}

Value ModuleObject::public_instance_method(Env *env, Value name_value) {
    auto name = name_value->to_symbol(env, Object::Conversion::Strict);
    auto method_info = find_method(env, name);
    assert_method_defined(env, name, method_info);

    switch (method_info.visibility()) {
    case MethodVisibility::Public:
        return new UnboundMethodObject { this, method_info.method() };
    case MethodVisibility::Protected:
        if (is_class())
            env->raise_name_error(name, "method `{}' for class `{}' is protected", name->c_str(), inspect_str());
        else
            env->raise_name_error(name, "method `{}' for module `{}' is protected", name->c_str(), inspect_str());
    case MethodVisibility::Private:
        if (is_class())
            env->raise_name_error(name, "method `{}' for class `{}' is private", name->c_str(), inspect_str());
        else
            env->raise_name_error(name, "method `{}' for module `{}' is private", name->c_str(), inspect_str());
    default:
        NAT_UNREACHABLE();
    }
}

Value ModuleObject::instance_methods(Env *env, Value include_super_value, std::function<bool(MethodVisibility)> predicate) {
    bool include_super = !include_super_value || include_super_value->is_truthy();
    ArrayObject *array = new ArrayObject {};
    methods(env, array, include_super);
    array->select_in_place([this, env, predicate](Value &name_value) -> bool {
        auto name = name_value->as_symbol();
        auto method_info = find_method(env, name);
        return method_info.is_defined() && predicate(method_info.visibility());
    });
    return array;
}

Value ModuleObject::instance_methods(Env *env, Value include_super_value) {
    return instance_methods(env, include_super_value, [](MethodVisibility visibility) {
        return visibility == MethodVisibility::Public || visibility == MethodVisibility::Protected;
    });
}

Value ModuleObject::private_instance_methods(Env *env, Value include_super_value) {
    return instance_methods(env, include_super_value, [](MethodVisibility visibility) {
        return visibility == MethodVisibility::Private;
    });
}

Value ModuleObject::protected_instance_methods(Env *env, Value include_super_value) {
    return instance_methods(env, include_super_value, [](MethodVisibility visibility) {
        return visibility == MethodVisibility::Protected;
    });
}

Value ModuleObject::public_instance_methods(Env *env, Value include_super_value) {
    return instance_methods(env, include_super_value, [](MethodVisibility visibility) {
        return visibility == MethodVisibility::Public;
    });
}

ArrayObject *ModuleObject::ancestors(Env *env) {
    ModuleObject *klass = this;
    ArrayObject *ancestors = new ArrayObject {};
    do {
        if (klass->included_modules().is_empty()) {
            // note: if there are included modules, then they will include this klass
            ancestors->push(klass);
        }
        for (ModuleObject *m : klass->included_modules()) {
            ancestors->push(m);
        }
        klass = klass->m_superclass;
    } while (klass);
    return ancestors;
}

bool ModuleObject::is_subclass_of(ModuleObject *other) {
    if (other == this) {
        return false;
    }
    ModuleObject *klass = this;
    do {
        if (other == klass->m_superclass) {
            return true;
        }
        for (ModuleObject *m : klass->included_modules()) {
            if (other == m) {
                return true;
            }
        }
        klass = klass->m_superclass;
    } while (klass);
    return false;
}

bool ModuleObject::is_method_defined(Env *env, Value name_value) const {
    auto name = name_value->to_symbol(env, Conversion::Strict);
    return !!find_method(env, name);
}

String ModuleObject::inspect_str() {
    if (m_class_name) {
        if (owner() && owner() != GlobalEnv::the()->Object()) {
            return String::format("{}::{}", owner()->inspect_str(), m_class_name.value());
        } else {
            return String(m_class_name.value());
        }
    } else if (is_class()) {
        return String::format("#<Class:{}>", pointer_id());
    } else if (is_module() && m_class_name) {
        return String(m_class_name.value());
    } else {
        return String::format("#<{}:{}>", klass()->inspect_str(), pointer_id());
    }
}

Value ModuleObject::inspect(Env *env) {
    return new StringObject { inspect_str() };
}

String ModuleObject::dbg_inspect() const {
    if (m_class_name)
        return String::format("\"{}\"", m_class_name.value());
    return Object::dbg_inspect();
}

Value ModuleObject::name(Env *env) {
    if (m_class_name) {
        String name = m_class_name.value();
        auto the_owner = owner();
        if (the_owner && the_owner != GlobalEnv::the()->Object()) {
            auto owner_name = the_owner->name();
            if (owner_name) {
                name.prepend("::");
                name.prepend(owner_name.value());
            }
        }
        return new StringObject { name };
    } else {
        return NilObject::the();
    }
}

Value ModuleObject::attr_reader(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); i++) {
        Value name_obj = args[i];
        if (name_obj->type() == Object::Type::Symbol) {
            // we're good!
        } else if (name_obj->type() == Object::Type::String) {
            name_obj = SymbolObject::intern(name_obj->as_string()->string());
        } else {
            env->raise("TypeError", "{} is not a symbol nor a string", name_obj->inspect_str(env));
        }
        auto block_env = new Env {};
        block_env->var_set("name", 0, true, name_obj);
        Block *attr_block = new Block { block_env, this, ModuleObject::attr_reader_block_fn, 0 };
        define_method(env, name_obj->as_symbol(), attr_block);
    }
    return NilObject::the();
}

Value ModuleObject::attr_reader_block_fn(Env *env, Value self, Args args, Block *block) {
    Value name_obj = env->outer()->var_get("name", 0);
    assert(name_obj);
    assert(name_obj->is_symbol());
    SymbolObject *ivar_name = SymbolObject::intern(TM::String::format("@{}", name_obj->as_symbol()->c_str()));
    return self->ivar_get(env, ivar_name);
}

Value ModuleObject::attr_writer(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); i++) {
        Value name_obj = args[i];
        if (name_obj->type() == Object::Type::Symbol) {
            // we're good!
        } else if (name_obj->type() == Object::Type::String) {
            name_obj = SymbolObject::intern(name_obj->as_string()->string());
        } else {
            env->raise("TypeError", "{} is not a symbol nor a string", name_obj->inspect_str(env));
        }
        TM::String method_name = TM::String::format("{}=", name_obj->as_symbol()->c_str());
        auto block_env = new Env {};
        block_env->var_set("name", 0, true, name_obj);
        Block *attr_block = new Block { block_env, this, ModuleObject::attr_writer_block_fn, 1 };
        define_method(env, SymbolObject::intern(method_name), attr_block);
    }
    return NilObject::the();
}

Value ModuleObject::attr_writer_block_fn(Env *env, Value self, Args args, Block *block) {
    Value val = args[0];
    Value name_obj = env->outer()->var_get("name", 0);
    assert(name_obj);
    assert(name_obj->is_symbol());
    SymbolObject *ivar_name = SymbolObject::intern(TM::String::format("@{}", name_obj->as_symbol()->c_str()));
    self->ivar_set(env, ivar_name, val);
    return val;
}

Value ModuleObject::attr_accessor(Env *env, Args args) {
    attr_reader(env, args);
    attr_writer(env, args);
    return NilObject::the();
}

void ModuleObject::included_modules(Env *env, ArrayObject *modules) {
    for (ModuleObject *m : included_modules()) {
        if (m == this || modules->include(env, m))
            continue;
        modules->push(m);
        m->included_modules(env, modules);
    }
    if (m_superclass) {
        m_superclass->included_modules(env, modules);
    }
}

Value ModuleObject::included_modules(Env *env) {
    ArrayObject *modules = new ArrayObject {};
    included_modules(env, modules);
    return modules;
}

bool ModuleObject::does_include_module(Env *env, Value module) {
    module->assert_type(env, Object::Type::Module, "Module");
    for (ModuleObject *m : included_modules()) {
        if (this == m)
            continue;
        if (module == m)
            return true;
        if (m->does_include_module(env, module))
            return true;
    }
    if (m_superclass && m_superclass->does_include_module(env, module))
        return true;
    return false;
}

Value ModuleObject::define_method(Env *env, Value name_value, Value method_value, Block *block) {
    auto name = name_value->to_symbol(env, Object::Conversion::Strict);
    if (method_value) {
        if (method_value->is_proc()) {
            define_method(env, name, method_value->as_proc()->block());
        } else {
            Method *method;
            if (method_value->is_method()) {
                method = method_value->as_method()->method();
            } else if (method_value->is_unbound_method()) {
                method = method_value->as_unbound_method()->method();
            } else {
                env->raise("TypeError", "wrong argument type {} (expected Proc/Method/UnboundMethod)", method_value->klass()->inspect_str());
            }
            ModuleObject *owner = method->owner();
            if (owner != this && owner->is_class() && !owner->is_subclass_of(this)) {
                if (owner->as_class()->is_singleton()) {
                    env->raise("TypeError", "can't bind singleton method to a different class");
                } else {
                    env->raise("TypeError", "bind argument must be a subclass of {}", owner->inspect_str());
                }
            }
            define_method(env, name, method->fn(), method->arity());
        }
    } else if (block) {
        define_method(env, name, block);
    } else {
        env->raise("ArgumentError", "tried to create Proc object without a block");
    }
    return name;
}

Value ModuleObject::module_eval(Env *env, Block *block) {
    if (!block) {
        env->raise("ArgumentError", "Natalie only supports module_eval with a block");
    }
    Value self = this;
    block->set_self(self);
    auto old_method_visibility = m_method_visibility;
    auto old_module_function = m_module_function;
    Value args[] = { self };
    Value result = NAT_RUN_BLOCK_AND_POSSIBLY_BREAK(env, block, Args(1, args), nullptr);
    m_method_visibility = old_method_visibility;
    m_module_function = old_module_function;
    return result;
}

Value ModuleObject::module_exec(Env *env, Args args, Block *block) {
    if (!block)
        env->raise_local_jump_error(NilObject::the(), Natalie::LocalJumpErrorType::None);
    Value self = this;
    block->set_self(self);
    return NAT_RUN_BLOCK_AND_POSSIBLY_BREAK(env, block, args, nullptr);
}

Value ModuleObject::private_method(Env *env, Args args) {
    set_method_visibility(env, args, MethodVisibility::Private);
    return this;
}

Value ModuleObject::protected_method(Env *env, Args args) {
    set_method_visibility(env, args, MethodVisibility::Protected);
    return this;
}

Value ModuleObject::public_method(Env *env, Args args) {
    set_method_visibility(env, args, MethodVisibility::Public);
    return this;
}

Value ModuleObject::private_class_method(Env *env, Args args) {
    singleton_class(env)->set_method_visibility(env, args, MethodVisibility::Private);
    return this;
}

Value ModuleObject::public_class_method(Env *env, Args args) {
    singleton_class(env)->set_method_visibility(env, args, MethodVisibility::Public);
    return this;
}

void ModuleObject::set_method_visibility(Env *env, Args args, MethodVisibility visibility) {
    // private (no args)
    if (args.size() == 0) {
        m_method_visibility = visibility;
        m_module_function = false;
    }

    // private [:foo, :bar]
    if (args.size() == 1 && args[0]->is_array()) {
        auto array = args[0]->as_array();
        args = Args(array);
    }

    // private :foo, :bar
    for (size_t i = 0; i < args.size(); ++i) {
        auto name = args[i]->to_symbol(env, Conversion::Strict);
        ModuleObject *matching_class_or_module = nullptr;
        auto method_info = find_method(env, name, &matching_class_or_module);
        assert_method_defined(env, name, method_info);
        m_methods.put(name, MethodInfo(visibility, method_info.method()));
    }
}

Value ModuleObject::module_function(Env *env, Args args) {
    if (is_class()) {
        env->raise("TypeError", "module_function must be called for modules");
    }
    if (args.size() > 0) {
        for (size_t i = 0; i < args.size(); ++i) {
            auto name = args[i]->to_symbol(env, Conversion::Strict);
            auto method_info = find_method(env, name);
            assert_method_defined(env, name, method_info);
            auto method = method_info.method();
            define_singleton_method(env, name, method->fn(), method->arity());
            m_methods.put(name, MethodInfo(MethodVisibility::Private, method));
        }
    } else {
        m_method_visibility = MethodVisibility::Private;
        m_module_function = true;
    }
    return this;
}

Value ModuleObject::deprecate_constant(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); ++i) {
        auto name = args[i]->to_symbol(env, Conversion::Strict);
        auto constant = m_constants.get(name);
        if (!constant)
            env->raise_name_error(name, "constant {}::{} not defined", inspect_str(), name->c_str());
        constant->set_deprecated(true);
    }
    return this;
}

Value ModuleObject::private_constant(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); ++i) {
        auto name = args[i]->to_symbol(env, Conversion::Strict);
        auto constant = m_constants.get(name);
        if (!constant)
            env->raise_name_error(name, "constant {}::{} not defined", inspect_str(), name->c_str());
        constant->set_private(true);
    }
    return this;
}

Value ModuleObject::public_constant(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); ++i) {
        auto name = args[i]->to_symbol(env, Conversion::Strict);
        auto constant = m_constants.get(name);
        if (!constant)
            env->raise_name_error(name, "constant {}::{} not defined", inspect_str(), name->c_str());
        constant->set_private(false);
    }
    return this;
}

bool ModuleObject::const_defined(Env *env, Value name_value) {
    auto name = name_value->to_symbol(env, Object::Conversion::NullAllowed);
    if (!name) {
        env->raise("TypeError", "no implicit conversion of {} to String", name_value->inspect_str(env));
    }
    return !!const_find(env, name, ConstLookupSearchMode::NotStrict, ConstLookupFailureMode::Null);
}

Value ModuleObject::alias_method(Env *env, Value new_name_value, Value old_name_value) {
    auto new_name = new_name_value->to_symbol(env, Object::Conversion::NullAllowed);
    if (!new_name) {
        env->raise("TypeError", "{} is not a symbol", new_name_value->inspect_str(env));
    }
    auto old_name = old_name_value->to_symbol(env, Object::Conversion::NullAllowed);
    if (!old_name) {
        env->raise("TypeError", "{} is not a symbol", old_name_value->inspect_str(env));
    }
    make_alias(env, new_name, old_name);
    return new_name;
}

Value ModuleObject::remove_method(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); ++i) {
        auto name = args[i]->to_symbol(env, Conversion::Strict);
        auto method = m_methods.get(name, env);
        if (!method)
            env->raise_name_error(name, "method `{}' not defined in {}", name->c_str(), this->inspect_str());
        m_methods.remove(name, env);
    }
    return this;
}

Value ModuleObject::undef_method(Env *env, Args args) {
    for (size_t i = 0; i < args.size(); ++i) {
        auto name = args[i]->to_symbol(env, Conversion::Strict);
        auto method_info = find_method(env, name);
        assert_method_defined(env, name, method_info);
        undefine_method(env, name);
    }
    return this;
}

void ModuleObject::visit_children(Visitor &visitor) {
    Object::visit_children(visitor);
    visitor.visit(m_env);
    visitor.visit(m_superclass);
    for (auto pair : m_constants) {
        visitor.visit(pair.first);
        visitor.visit(pair.second);
    }
    for (auto pair : m_methods) {
        visitor.visit(pair.first);
        if (pair.second.is_defined())
            visitor.visit(pair.second.method());
    }
    for (auto pair : m_class_vars) {
        visitor.visit(pair.first);
        visitor.visit(pair.second);
    }
    for (auto module : m_included_modules) {
        visitor.visit(module);
    }
}

}
