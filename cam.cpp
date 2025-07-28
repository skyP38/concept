#include <iostream>
#include <variant>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <stdexcept>

#include "cam.hpp"

// Реализация TypeVar
TypeVar::TypeVar(int i) : id(i) {}

// Реализация TypeArrow
TypeArrow::TypeArrow(Type f, Type t) : from(f), to(t) {}

// Реализация TypeConst
TypeConst::TypeConst(const std::string& n) : name(n) {}

// Реализация TypeVarGenerator
int TypeVarGenerator::counter = 0;

int TypeVarGenerator::next() { return counter++; }
void TypeVarGenerator::reset() { counter = 0; }

// Функции для создания типов
Type make_typevar() {
    return std::make_shared<TypeVar>(TypeVarGenerator::next());
}

Type make_typevar(int id) {
    return std::make_shared<TypeVar>(id);
}

Type make_arrow(Type from, Type to) {
    return std::make_shared<TypeArrow>(from, to);
}

Type make_typeconst(const std::string& name) {
    return std::make_shared<TypeConst>(name);
}

// Реализация выражений
Constant::Constant(const std::string& n, Type t) : name(n), type(t) {}
Variable::Variable(const std::string& n, Type t) : name(n), type(t) {}
Lambda::Lambda(const std::string& p, Type pt, Expr b) : param(p), param_type(pt), body(b) {}
Apply::Apply(Expr f, Expr a) : func(f), arg(a) {}

// Функции для создания выражений
Expr make_constant(const std::string& name, Type type) {
    return std::make_shared<Constant>(name, type);
}

Expr make_variable(const std::string& name, Type type) {
    return std::make_shared<Variable>(name, type);
}

Expr make_lambda(const std::string& param, Type param_type, Expr body) {
    return std::make_shared<Lambda>(param, param_type, body);
}

Expr make_apply(Expr func, Expr arg) {
    return std::make_shared<Apply>(func, arg);
}

// Унификация типов
void UnifyVisitor::operator()(const std::shared_ptr<TypeVar>& tv1) {
    if (auto tv2 = std::get_if<std::shared_ptr<TypeVar>>(&t2)) {
        if (tv1->id == (*tv2)->id) return;
    }
    
    if (auto ta2 = std::get_if<std::shared_ptr<TypeArrow>>(&t2)) {
        throw std::runtime_error("Cannot unify type variable with arrow type");
    }
    
    if (auto tc2 = std::get_if<std::shared_ptr<TypeConst>>(&t2)) {
        throw std::runtime_error("Cannot unify type variable with constant type");
    }
    
    substitutions[tv1->id] = t2;
}

void UnifyVisitor::operator()(const std::shared_ptr<TypeArrow>& ta1) {    
    if (auto ta2 = std::get_if<std::shared_ptr<TypeArrow>>(&t2)) {
        UnifyVisitor from_visitor((*ta2)->from, substitutions);
        std::visit(from_visitor, ta1->from);
        
        UnifyVisitor to_visitor((*ta2)->to, substitutions);
        std::visit(to_visitor, ta1->to);
    } else if (auto tv2 = std::get_if<std::shared_ptr<TypeVar>>(&t2)) {
        substitutions[(*tv2)->id] = Type(ta1);
    } else {
        throw std::runtime_error("Type mismatch in arrow unification");
    }
}

void UnifyVisitor::operator()(const std::shared_ptr<TypeConst>& tc1) {
    if (auto tc2 = std::get_if<std::shared_ptr<TypeConst>>(&t2)) {
        if (tc1->name != (*tc2)->name) {
            throw std::runtime_error("Type constant mismatch: " + tc1->name + " vs " + (*tc2)->name);
        }
    } else if (auto tv2 = std::get_if<std::shared_ptr<TypeVar>>(&t2)) {
        substitutions[(*tv2)->id] = Type(tc1);
    } else {
        throw std::runtime_error("Type mismatch in constant unification");
    }
}

void unify(Type t1, Type t2, std::unordered_map<int, Type>& subs) {
    auto apply_subs = [&](Type& t) {
        if (auto tv = std::get_if<std::shared_ptr<TypeVar>>(&t)) {
            auto it = subs.find((*tv)->id);
            if (it != subs.end()) t = it->second;
        }
    };
    
    apply_subs(t1);
    apply_subs(t2);
    
    if (t1 == t2) return;
    
    UnifyVisitor visitor(t2, subs);
    std::visit(visitor, t1);
}

// Вывод типа выражения
struct InferVisitor {
    TypeContext& context;
    std::unordered_map<int, Type> substitutions;

    Type apply_all_subs(Type t) {
        if (auto tv = std::get_if<std::shared_ptr<TypeVar>>(&t)) {
            auto it = substitutions.find((*tv)->id);
            if (it != substitutions.end()) return apply_all_subs(it->second);
        } else if (auto ta = std::get_if<std::shared_ptr<TypeArrow>>(&t)) {
            return make_arrow(apply_all_subs((*ta)->from), apply_all_subs((*ta)->to));
        }
        return t;
    }
    
    Type operator()(const std::shared_ptr<Constant>& c) {
        return c->type;
    }
    
    Type operator()(const std::shared_ptr<Variable>& v) {
        auto it = context.find(v->name);
        if (it == context.end()) {
            throw std::runtime_error("Unbound variable: " + v->name);
        }
        return it->second;
    }
    
    Type operator()(const std::shared_ptr<Lambda>& l) {
        TypeContext new_context = context;
        new_context[l->param] = l->param_type;
        Type body_type = infer_type(l->body, new_context);
        return make_arrow(l->param_type, body_type);
    }
    
    Type operator()(const std::shared_ptr<Apply>& a) {
        Type func_type = infer_type(a->func, context);
        Type arg_type = infer_type(a->arg, context);
        Type result_type = make_typevar();
        
        try {
            unify(func_type, make_arrow(arg_type, result_type), substitutions);
            auto apply_subs = [&](Type t) -> Type {
                if (auto tv = std::get_if<std::shared_ptr<TypeVar>>(&t)) {
                    auto it = substitutions.find((*tv)->id);
                    if (it != substitutions.end()) return apply_all_subs(it->second);
                } else if (auto ta = std::get_if<std::shared_ptr<TypeArrow>>(&t)) {
                    return make_arrow(apply_all_subs((*ta)->from), apply_all_subs((*ta)->to));
                }
                return t;
            };
            
            return apply_subs(result_type); 
            
        } catch (const std::runtime_error& e) {
            throw std::runtime_error("Type error in application: " + std::string(e.what()) + 
                   "\nFunction type: " + type_to_string_full(func_type) +
                   "\nArgument type: " + type_to_string_full(arg_type));
        }
    }
};

Type infer_type(Expr expr, TypeContext& context) {
    InferVisitor visitor{context};
    Type result = std::visit(visitor, expr);
    
    std::function<Type(Type)> apply_all_subs = [&](Type t) -> Type {
        if (auto tv = std::get_if<std::shared_ptr<TypeVar>>(&t)) {
            auto it = visitor.substitutions.find((*tv)->id);
            if (it != visitor.substitutions.end()) return apply_all_subs(it->second);
        } else if (auto ta = std::get_if<std::shared_ptr<TypeArrow>>(&t)) {
            return make_arrow(apply_all_subs((*ta)->from), apply_all_subs((*ta)->to));
        }
        return t;
    };
    
    return apply_all_subs(result);
}

// Подстановка переменной в выражении
Expr substitute(Expr expr, const std::string& var, Expr value) {
    if (auto v = std::get_if<std::shared_ptr<Variable>>(&expr)) {
        if ((*v)->name == var) {
            return value;
        }
        return expr;
    }
    
    if (auto l = std::get_if<std::shared_ptr<Lambda>>(&expr)) {
        if ((*l)->param == var) {
            return expr; // Связанная переменная
        }
        return make_lambda((*l)->param, (*l)->param_type, substitute((*l)->body, var, value));
    }
    
    if (auto a = std::get_if<std::shared_ptr<Apply>>(&expr)) {
        return make_apply(
            substitute((*a)->func, var, value),
            substitute((*a)->arg, var, value)
        );
    }
    
    return expr; // Для констант
}

// Редукция выражения 
std::pair<Expr, bool> reduce(Expr expr) {
    if (auto a = std::get_if<std::shared_ptr<Apply>>(&expr)) {
        auto [new_func, reduced] = reduce((*a)->func);
        if (reduced) {
            return {make_apply(new_func, (*a)->arg), true};
        }
        
        // Если функция - лямбда, применяем бета-редукцию
        if (auto l = std::get_if<std::shared_ptr<Lambda>>(&new_func)) {
            return {substitute((*l)->body, (*l)->param, (*a)->arg), true};
        }
        
        auto [new_arg, arg_reduced] = reduce((*a)->arg);
        if (arg_reduced) {
            return {make_apply((*a)->func, new_arg), true};
        }
    }
    
    return {expr, false};
}

// Полная нормальная форма
Expr normalize(Expr expr) {
    Expr current = expr;
    bool reduced;
    do {
        std::tie(current, reduced) = reduce(current);
    } while (reduced);
    return current;
}

// Вывод выражения
void print_expr(const Expr& expr) {
    if (auto c = std::get_if<std::shared_ptr<Constant>>(&expr)) {
        std::cout << (*c)->name;
    } else if (auto v = std::get_if<std::shared_ptr<Variable>>(&expr)) {
        std::cout << (*v)->name;
    } else if (auto l = std::get_if<std::shared_ptr<Lambda>>(&expr)) {
        std::cout << "λ" << (*l)->param << ":" << type_to_string_full((*l)->param_type) << ".";
        print_expr((*l)->body);
    } else if (auto a = std::get_if<std::shared_ptr<Apply>>(&expr)) {
        std::cout << "(";
        print_expr((*a)->func);
        std::cout << " ";
        print_expr((*a)->arg);
        std::cout << ")";
    }
}

struct FullTypePrinter {
    std::unordered_map<int, Type> var_types;
    
    std::string operator()(const std::shared_ptr<TypeVar>& tv) {
        auto it = var_types.find(tv->id);
        if (it != var_types.end()) return type_to_string_full(it->second);
        return "?" + std::to_string(tv->id);
    }
    
    std::string operator()(const std::shared_ptr<TypeArrow>& ta) {
        return "(" + type_to_string_full(ta->from) + " -> " + type_to_string_full(ta->to) + ")";
    }
    
    std::string operator()(const std::shared_ptr<TypeConst>& tc) {
        return tc->name;
    }
};

std::string type_to_string_full(Type type) {
    FullTypePrinter printer;
    return std::visit(printer, type);
}