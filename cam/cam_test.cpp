#include "cam.hpp"
#include <iostream>

void run_tests() {
    TypeVarGenerator::reset();
    
    // Базовые типы
    Type bool_type = make_typeconst("Bool");
    Type int_type = make_typeconst("Int");
    
    // Константы
    Expr true_const = make_constant("true", bool_type);
    Expr false_const = make_constant("false", bool_type);
    Expr zero = make_constant("0", int_type);
    
    // Тест 1: Идентификационная функция для Bool
    TypeContext context;
    context["true"] = bool_type;
    context["false"] = bool_type;
    context["0"] = int_type;
    
    Expr id_bool = make_lambda("x", bool_type, make_variable("x", bool_type));
    Expr apply_id = make_apply(id_bool, true_const);
    
    std::cout << "Тест 1: Идентификационная функция для Bool\n";
    std::cout << "Выражение: ";
    print_expr(apply_id);
    std::cout << "\n";
    
    try {
        Type type = infer_type(apply_id, context);
        std::cout << "Тип: " << type_to_string_full(type) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Ошибка типизации: " << e.what() << "\n";
    }
    
    Expr normalized = normalize(apply_id);
    std::cout << "Результат: ";
    print_expr(normalized);
    std::cout << "\n\n";
    
    // Тест 2: Константная функция
    Expr const_func = make_lambda("x", bool_type, false_const);
    Expr apply_const = make_apply(const_func, true_const);
    
    std::cout << "Тест 2: Константная функция\n";
    std::cout << "Выражение: ";
    print_expr(apply_const);
    std::cout << "\n";
    
    try {
        Type type = infer_type(apply_const, context);
        std::cout << "Тип: " << type_to_string_full(type) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Ошибка типизации: " << e.what() << "\n";
    }
    
    normalized = normalize(apply_const);
    std::cout << "Результат: ";
    print_expr(normalized);
    std::cout << "\n\n";
    
    // Тест 3: Неправильное применение типов
    Expr bad_apply = make_apply(id_bool, zero);
    
    std::cout << "Тест 3: Неправильное применение типов\n";
    std::cout << "Выражение: ";
    print_expr(bad_apply);
    std::cout << "\n";
    
    try {
        Type type = infer_type(bad_apply, context);
        std::cout << "Тип: " << type_to_string_full(type) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Ошибка типизации (ожидаемо): " << e.what() << "\n";
    }
    
    // Тест 4: Функция высшего порядка
    Type arrow_bool_bool = make_arrow(bool_type, bool_type);
    Expr apply_to_true = make_lambda("f", arrow_bool_bool, 
                                   make_apply(make_variable("f", arrow_bool_bool), true_const));
    
    Expr test_higher_order = make_apply(apply_to_true, id_bool);
    
    std::cout << "Тест 4: Функция высшего порядка\n";
    std::cout << "Выражение: ";
    print_expr(test_higher_order);
    std::cout << "\n";
    
    try {
        Type type = infer_type(test_higher_order, context);
        std::cout << "Тип: " << type_to_string_full(type) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Ошибка типизации: " << e.what() << "\n";
    }
    
    normalized = normalize(test_higher_order);
    std::cout << "Результат: ";
    print_expr(normalized);
    std::cout << "\n\n";
    
    // Тест 5: Самоприменение (ошибка типизации)
    Expr self_apply = make_lambda("x", make_typevar(), 
                                 make_apply(make_variable("x", make_typevar()), 
                                            make_variable("x", make_typevar())));
    
    std::cout << "Тест 5: Самоприменение\n";
    std::cout << "Выражение: ";
    print_expr(self_apply);
    std::cout << "\n";
    
    try {
        Type type = infer_type(self_apply, context);
        std::cout << "Тип: " << type_to_string_full(type) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Ошибка типизации (ожидаемо): " << e.what() << "\n";
    }
}

int main() {
    try {
        run_tests();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}