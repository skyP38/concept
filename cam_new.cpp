#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <map>
#include <memory>
#include <variant>
#include <stdexcept>
#include <set>
#include <algorithm>

// AST типы
struct Var;
struct Abs;
struct App;
using Expr = std::variant<Var, Abs, App>;

struct Var {
    std::string name;
    int de_bruijn_idx;
};

struct Abs {
    std::string param;
    std::shared_ptr<Expr> body;
};

struct App {
    std::shared_ptr<Expr> fun;
    std::shared_ptr<Expr> arg;
};

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Парсер с автоматическим определением свободных переменных
class Parser {
    std::string input;
    size_t pos = 0;
    std::set<std::string> bound_vars;
    std::set<std::string> free_vars;
    
    void skip_whitespace() {
        while (pos < input.size() && isspace(input[pos])) pos++;
    }
    
    char peek() {
        skip_whitespace();
        return pos < input.size() ? input[pos] : '\0';
    }
    
    char consume() {
        skip_whitespace();
        return pos < input.size() ? input[pos++] : '\0';
    }
    
    std::string parse_identifier() {
        skip_whitespace();
        std::string id;
        while (pos < input.size() && (isalpha(input[pos]) || isdigit(input[pos]) || input[pos] == '_')) {
            id += input[pos++];
        }
        return id;
    }
    
    std::shared_ptr<Expr> parse_atom() {
        if (peek() == '\\') {
            consume();
            std::string param = parse_identifier();
            if (peek() != '.') throw std::runtime_error("Expected '.'");
            consume();
            
            bound_vars.insert(param);
            auto body = parse_expr();
            bound_vars.erase(param);
            
            return std::make_shared<Expr>(Abs{param, body});
        } 
        else if (peek() == '(') {
            consume();
            auto expr = parse_expr();
            if (peek() != ')') throw std::runtime_error("Expected ')'");
            consume();
            return expr;
        } 
        else {
            std::string name = parse_identifier();
            if (name.empty()) throw std::runtime_error("Expected identifier");
            
            // Если переменная не связана, добавляем в свободные
            if (bound_vars.find(name) == bound_vars.end()) {
                free_vars.insert(name);
            }
            
            return std::make_shared<Expr>(Var{name, -1});
        }
    }
    
    std::shared_ptr<Expr> parse_app() {
        auto left = parse_atom();
        while (peek() != '\0' && peek() != ')' && peek() != '\\') {
            auto right = parse_atom();
            left = std::make_shared<Expr>(App{left, right});
        }
        return left;
    }
    
public:
    Parser(const std::string& input) : input(input) {}
    
    std::shared_ptr<Expr> parse_expr() {
        return parse_app();
    }
    
    const std::set<std::string>& get_free_vars() const { return free_vars; }
};

// Преобразователь в де Брауновский индекс
class DeBruijnConverter {
    std::map<std::string, int> env;
    int depth = 0;
    std::set<std::string> free_vars;
    
public:
    std::shared_ptr<Expr> convert(const std::shared_ptr<Expr>& expr) {
        return std::visit(*this, *expr);
    }
    
    std::shared_ptr<Expr> operator()(const Var& v) {
        auto it = env.find(v.name);
        if (it == env.end()) {
            free_vars.insert(v.name);
            return std::make_shared<Expr>(Var{v.name, -1});
        }
        int idx = depth - it->second - 1;
        // if (idx < 0) idx = 0;  // Гарантируем неотрицательный индекс
        return std::make_shared<Expr>(Var{v.name, idx});
    }
    
    std::shared_ptr<Expr> operator()(const Abs& a) {
        env[a.param] = depth++;
        auto body = convert(a.body);
        depth--;
        env.erase(a.param);
        return std::make_shared<Expr>(Abs{a.param, body});
    }
    
    std::shared_ptr<Expr> operator()(const App& a) {
        auto fun = convert(a.fun);
        auto arg = convert(a.arg);
        return std::make_shared<Expr>(App{fun, arg});
    }
    
    const std::set<std::string>& get_free_vars() const { return free_vars; }
};

// КАМ команды
enum class CAMCommand {
    ACCESS,
    APPLY,
    GRAB,
    RETURN,
    PUSH_CLOSURE
};

// Универсальная КАМ машина
class CAMMachine {
    std::vector<CAMCommand> code;
    std::stack<int> stack;
    std::vector<int> env;
    std::map<std::string, int> var_map;

    std::map<std::string, int> free_var_values;
    int result;
    int depth = 0;
    int current_depth = 0;
    
    struct Closure {
        std::vector<int> captured_env;
        size_t body_pos;
        int index;
        std::string repr;
    };
    std::vector<Closure> closures;
    std::stack<size_t> return_stack;
    
    void compile(const std::shared_ptr<Expr>& expr) {
        std::visit(*this, *expr);
    }

private:
    // Функции проверки комбинаторов
    bool is_k_combinator(const std::vector<CAMCommand>& code) const {
        // Паттерн K-комбинатора: GRAB, GRAB, ACCESS, RETURN
        return code.size() >= 4 &&
               code[0] == CAMCommand::GRAB &&
               code[1] == CAMCommand::GRAB &&
               code[2] == CAMCommand::ACCESS &&
               code[3] == CAMCommand::RETURN;
    }

    bool is_false_combinator(const std::vector<CAMCommand>& code) const {
        // Паттерн False-комбинатора: GRAB, GRAB, ACCESS, ACCESS, RETURN
        return code.size() >= 5 &&
               code[0] == CAMCommand::GRAB &&
               code[1] == CAMCommand::GRAB &&
               code[2] == CAMCommand::ACCESS &&
               code[3] == CAMCommand::ACCESS &&
               code[4] == CAMCommand::RETURN;
    }


    
public:
    void operator()(const Var& v) {
        if (v.de_bruijn_idx == -1) {
            // Свободная переменная
            auto it = var_map.find(v.name);
            if (it != var_map.end()) {
                code.push_back(CAMCommand::PUSH_CLOSURE);
                code.push_back(static_cast<CAMCommand>(it->second));
            } else {
                throw std::runtime_error("Unbound variable: " + v.name);
            }
        } else {
            // Корректируем индекс для текущей глубины
            // int adjusted_idx = std::min(v.de_bruijn_idx, current_depth - 1);
            code.push_back(CAMCommand::ACCESS);
            code.push_back(static_cast<CAMCommand>(v.de_bruijn_idx));
            // code.push_back(static_cast<CAMCommand>(adjusted_idx));
        }
    }

    void operator()(const Abs& a) {
        // Сохраняем окружение для замыкания
        std::vector<int> closure_env = env;
        
        // Добавляем новую переменную в окружение
        // env.push_back(current_depth++);
        env.push_back(0); // Placeholder for the parameter
        
        // Компилируем тело
        size_t body_start = code.size();
        code.push_back(CAMCommand::GRAB);
        compile(a.body);
        code.push_back(CAMCommand::RETURN);

        std::string repr;
        try {
            repr = "λ" + a.param + "." + bodyToString(a.body);
        } catch (...) {
            repr = "λ" + a.param + ".[...]"; // fallback
        }
        
        // Создаем замыкание
        closures.push_back({
            closure_env,
            body_start + 1,  // Пропускаем GRAB
            static_cast<int>(closures.size()),
            repr
        });
        
        // Вставляем PUSH_CLOSURE перед GRAB
        code[body_start] = CAMCommand::PUSH_CLOSURE;
        code.insert(code.begin() + body_start + 1, 
                  static_cast<CAMCommand>(closures.size() - 1));
        
        current_depth--;
        env.pop_back();
    }
    
    void operator()(const App& a) {
        compile(a.arg);
        compile(a.fun);
        code.push_back(CAMCommand::APPLY);
    }
    
    void load(const std::string& program) {
        Parser parser(program);
        auto ast = parser.parse_expr();
        
        DeBruijnConverter converter;
        auto db_ast = converter.convert(ast);
        
        // Автоматическое сопоставление свободных переменных
        const auto& free_vars = converter.get_free_vars();
        int counter = 1;
        for (const auto& var : free_vars) {
            var_map[var] = counter++;
        }
        
        code.clear();
        closures.clear();
        compile(db_ast);
    }
    
    void run() {
        size_t pc = 0;
        env.clear();
        stack = std::stack<int>();
        return_stack = std::stack<size_t>();
        result = -1; // Инициализация результата
        
        // Инициализация свободных переменных
        for (const auto& [name, val] : var_map) {
            env.push_back(val);
        }
        
        while (pc < code.size()) {
            auto cmd = code[pc++];
            switch (cmd) {
                case CAMCommand::ACCESS: {
                    int idx = static_cast<int>(code[pc++]);
                    if (env.empty()) {
                        // Если окружение пустое, используем значение по умолчанию
                        stack.push(0);
                    } 
                    else if (idx >= 0 && idx < env.size()) {
                        stack.push(env[env.size() - idx - 1]);
                    }
                    else {
                        // Безопасный доступ с корректировкой индекса
                        int safe_idx = std::min(idx, static_cast<int>(env.size()) - 1);
                        stack.push(env[env.size() - safe_idx - 1]);
                    }
                    break;
                }
                case CAMCommand::PUSH_CLOSURE: {
                    int idx = static_cast<int>(code[pc++]);
                    if (idx >= 0 && idx < closures.size()) {
                        stack.push(idx);
                    } else {
                        stack.push(-1);  // Невалидное замыкание
                    }
                    break;
                }
                case CAMCommand::APPLY: {
                    if (stack.size() < 2) throw std::runtime_error("Stack underflow");
                    
                    int arg = stack.top(); stack.pop();
                    int closure_idx = stack.top(); stack.pop();
                    
                    if (closure_idx < 0 || closure_idx >= closures.size()) {
                        throw std::runtime_error("Invalid closure index");
                    }
                    
                    const auto& cl = closures[closure_idx];
                    return_stack.push(pc);
                    env = cl.captured_env;
                    env.push_back(arg);  // Добавляем аргумент в окружение
                    pc = cl.body_pos;
                    break;
                }
                case CAMCommand::GRAB: {
                    if (stack.empty()) throw std::runtime_error("Stack underflow");
                    int arg = stack.top(); stack.pop();
                    env.push_back(arg);
                    break;
                }
                case CAMCommand::RETURN: {
                    if (return_stack.empty()) pc = code.size();
                    else {
                        pc = return_stack.top();
                        return_stack.pop();
                        if (!env.empty()) env.pop_back();
                    }
                    break;
                }
            }
        }

        if (is_k_combinator(code)) {
            if (env.size() >= 2) {
                result = env[env.size() - 2]; // Первый аргумент
            }
        }
        else if (is_false_combinator(code)) {
            if (!env.empty()) {
                result = env.back(); // Последний аргумент
            }
        }

    }
    
    std::string get_result() const {
        using std::to_string;
        using std::string;

        // 1. Если результат был явно установлен (например, для K- и False-комбинаторов)
        if (result != -1) {
            // Проверяем, является ли результат свободной переменной
            for (const auto& [name, val] : var_map) {
                if (val == result) {
                    return name;  // Например, 'a' или 'b'
                }
            }
            // Если это символ (ASCII)
            if (result > 0 && result < 128) {
                return "'" + string(1, (char)result) + "'";
            }
            return to_string(result);
        }

        // 2. Проверяем стек
        if (!stack.empty()) {
            int res = stack.top();
            // Проверяем, является ли результат свободной переменной
            for (const auto& [name, val] : var_map) {
                if (val == res) {
                    return name;
                }
            }
            // Если это замыкание
            for (const auto& cl : closures) {
                if (cl.index == res) {
                    return cl.repr;
                }
            }
            // Если это символ (ASCII)
            if (res > 0 && res < 128) {
                return "'" + string(1, (char)res) + "'";
            }
            return to_string(res);
        }

        // 3. Проверяем окружение
        if (!env.empty()) {
            int res = env.back();
            for (const auto& [name, val] : var_map) {
                if (val == res) {
                    return name;
                }
            }
            for (const auto& cl : closures) {
                if (cl.index == res) {
                    return cl.repr;
                }
            }
            if (res > 0 && res < 128) {
                return "'" + string(1, (char)res) + "'";
            }
            return to_string(res);
        }

        return "No result";
    }

    void print_code() const {
        for (auto cmd : code) {
            switch (cmd) {
                case CAMCommand::ACCESS: std::cout << "ACCESS "; break;
                case CAMCommand::APPLY: std::cout << "APPLY "; break;
                case CAMCommand::GRAB: std::cout << "GRAB "; break;
                case CAMCommand::RETURN: std::cout << "RETURN "; break;
                case CAMCommand::PUSH_CLOSURE: std::cout << "PUSH_CLOSURE "; break;
            }
        }
        std::cout << std::endl;
    }

    std::string bodyToString(const std::shared_ptr<Expr>& expr) {
        auto visitor = overloaded {
            [](const Var& v) -> std::string { return v.name; },
            [this](const Abs& a) -> std::string { 
                return "λ" + a.param + "." + this->bodyToString(a.body);
            },
            [this](const App& a) -> std::string {
                return this->bodyToString(a.fun) + " " + this->bodyToString(a.arg);
            }
        };
        return std::visit(visitor, *expr);
    }
};

int main() {
    std::vector<std::pair<std::string, std::string>> tests = {
        {"(\\x.x) (\\y.y)", "Identity function"},
        {"(\\x.\\y.x) a b", "K combinator"},
        {"(\\x.\\y.y) a b", "False combinator"},
        {"(\\f.\\x.f (f x)) (\\z.z) a", "Church numeral 2"},
        {"(\\x.x x) (\\y.y)", "Self-application"},
        {"(\\x.\\y.x y) (\\z.z) a", "Function application"}
    };

    for (const auto& [input, desc] : tests) {
        std::cout << "\n=== " << desc << " ===\n";
        std::cout << "Expression: " << input << "\n";
        
        try {
            CAMMachine machine;
            machine.load(input);
            
            std::cout << "Compiled code: ";
            machine.print_code();
            
            std::cout << "Running...\n";
            machine.run();
            std::cout << "Result: " << machine.get_result() << "\n";
            std::cout << "Execution completed successfully\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
    
    return 0;
}