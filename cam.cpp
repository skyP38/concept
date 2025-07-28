#include <iostream>
#include <variant>
#include <vector>
#include <memory>
#include <functional>
#include <string>

// Типы выражений в КАМ
struct Apply;
struct Lambda;
struct Variable;
struct Constant;

using Expr = std::variant<
    std::shared_ptr<Apply>,
    std::shared_ptr<Lambda>,
    std::shared_ptr<Variable>,
    std::shared_ptr<Constant>
>;

// Атомарные выражения
struct Constant {
    std::string name;
    explicit Constant(const std::string& n) : name(n) {}
};

struct Variable {
    std::string name;
    explicit Variable(const std::string& n) : name(n) {}
};

// Композитные выражения
struct Lambda {
    std::string param;
    Expr body;
    Lambda(const std::string& p, Expr b) : param(p), body(b) {}
};

struct Apply {
    Expr func;
    Expr arg;
    Apply(Expr f, Expr a) : func(f), arg(a) {}
};

// Функции для удобного создания выражений
Expr make_constant(const std::string& name) {
    return std::make_shared<Constant>(name);
}

Expr make_variable(const std::string& name) {
    return std::make_shared<Variable>(name);
}

Expr make_lambda(const std::string& param, Expr body) {
    return std::make_shared<Lambda>(param, body);
}

Expr make_apply(Expr func, Expr arg) {
    return std::make_shared<Apply>(func, arg);
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
            return expr; // Связанная переменная, не подставляем
        }
        return make_lambda((*l)->param, substitute((*l)->body, var, value));
    }
    
    if (auto a = std::get_if<std::shared_ptr<Apply>>(&expr)) {
        return make_apply(
            substitute((*a)->func, var, value),
            substitute((*a)->arg, var, value)
        );
    }
    
    return expr; // Для констант
}

// Редукция выражения (один шаг)
std::pair<Expr, bool> reduce(Expr expr) {
    if (auto a = std::get_if<std::shared_ptr<Apply>>(&expr)) {
        // Пробуем редуцировать функцию
        auto [new_func, reduced] = reduce((*a)->func);
        if (reduced) {
            return {make_apply(new_func, (*a)->arg), true};
        }
        
        // Если функция - лямбда, применяем бета-редукцию
        if (auto l = std::get_if<std::shared_ptr<Lambda>>(&new_func)) {
            return {substitute((*l)->body, (*l)->param, (*a)->arg), true};
        }
        
        // Пробуем редуцировать аргумент
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

// Вывод выражения в читаемом виде
void print_expr(const Expr& expr) {
    if (auto c = std::get_if<std::shared_ptr<Constant>>(&expr)) {
        std::cout << (*c)->name;
    } else if (auto v = std::get_if<std::shared_ptr<Variable>>(&expr)) {
        std::cout << (*v)->name;
    } else if (auto l = std::get_if<std::shared_ptr<Lambda>>(&expr)) {
        std::cout << "λ" << (*l)->param << ".";
        print_expr((*l)->body);
    } else if (auto a = std::get_if<std::shared_ptr<Apply>>(&expr)) {
        std::cout << "(";
        print_expr((*a)->func);
        std::cout << " ";
        print_expr((*a)->arg);
        std::cout << ")";
    }
}

int main() {
    // Пример: (λx.x) y → y
    Expr identity = make_lambda("x", make_variable("x"));
    Expr example = make_apply(identity, make_constant("y"));
    
    std::cout << "Исходное выражение: ";
    print_expr(example);
    std::cout << std::endl;
    
    Expr result = normalize(example);
    
    std::cout << "Результат: ";
    print_expr(result);
    std::cout << std::endl;
    
    return 0;
}