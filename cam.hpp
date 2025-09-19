#ifndef CAM_H
#define CAM_H

#include <variant>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>


// Типы в системе
struct TypeVar;
struct TypeArrow;
struct TypeConst;

using Type = std::variant<
    std::shared_ptr<TypeVar>,
    std::shared_ptr<TypeArrow>,
    std::shared_ptr<TypeConst>
>;

struct TypeVar {
    int id;
    explicit TypeVar(int i);
};

struct TypeArrow {
    Type from;
    Type to;
    TypeArrow(Type f, Type t);
};

struct TypeConst {
    std::string name;
    explicit TypeConst(const std::string& n);
};

// Генератор уникальных идентификаторов для TypeVar
class TypeVarGenerator {
    static int counter;
public:
    static int next();
    static void reset();
};

// Функции для создания типов
Type make_typevar();
Type make_typevar(int id);
Type make_arrow(Type from, Type to);
Type make_typeconst(const std::string& name);

// Вывод типа в строку
std::string type_to_string_full(Type type); 

// Выражения в КАМ
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

struct Constant {
    std::string name;
    Type type;
    explicit Constant(const std::string& n, Type t);
};

struct Variable {
    std::string name;
    Type type;
    explicit Variable(const std::string& n, Type t);
};

struct Lambda {
    std::string param;
    Type param_type;
    Expr body;
    Lambda(const std::string& p, Type pt, Expr b);
};

struct Apply {
    Expr func;
    Expr arg;
    Apply(Expr f, Expr a);
};

// Контекст типизации
using TypeContext = std::unordered_map<std::string, Type>;

// Функции для удобного создания выражений
Expr make_constant(const std::string& name, Type type);
Expr make_variable(const std::string& name, Type type);
Expr make_lambda(const std::string& param, Type param_type, Expr body);
Expr make_apply(Expr func, Expr arg);

// Унификация типов
void unify(Type t1, Type t2, std::unordered_map<int, Type>& substitutions);

// Вывод типа выражения
Type infer_type(Expr expr, TypeContext& context);

// Подстановка переменной в выражении
Expr substitute(Expr expr, const std::string& var, Expr value);

// Редукция выражения 
std::pair<Expr, bool> reduce(Expr expr);

// Полная нормальная форма
Expr normalize(Expr expr);

// Вывод выражения в читаемом виде
void print_expr(const Expr& expr);

struct UnifyVisitor {
    Type t2;
    std::unordered_map<int, Type>& substitutions;
    
    UnifyVisitor(Type t2, std::unordered_map<int, Type>& subs, int d = 0)
        : t2(t2), substitutions(subs) {}
    
    void operator()(const std::shared_ptr<TypeVar>& tv1);
    void operator()(const std::shared_ptr<TypeArrow>& ta1);
    void operator()(const std::shared_ptr<TypeConst>& tc1);
};

#endif 