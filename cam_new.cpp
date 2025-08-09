#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <unordered_map>
#include <stack>

using namespace std;

// ==================== AST с де Брауновскими индексами ====================
struct Term;
using TermPtr = shared_ptr<Term>;

struct Term {
    virtual ~Term() = default;
    virtual string toString() const = 0;
    virtual TermPtr toDeBruijn(int level = 0, const unordered_map<string, int>& env = {}) const = 0;
};

struct Variable : Term {
    string name;
    int deBruijnIdx = -1;
    
    Variable(string n) : name(n) {}
    
    string toString() const override {
        if (deBruijnIdx >= 0) return "v" + to_string(deBruijnIdx);
        return name;
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        auto newVar = make_shared<Variable>(*this);
        if (env.count(name)) {
            newVar->deBruijnIdx = level - env.at(name) - 1;
        } else {
            throw runtime_error("Unbound variable: " + name);
        }
        return newVar;
    }
};

struct Number : Term {
    int value;
    Number(int v) : value(v) {}
    
    string toString() const override { 
        return to_string(value); 
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<Number>(*this);
    }
};

struct Lambda : Term {
    string param;
    TermPtr body;
    
    Lambda(string p, TermPtr b) : param(p), body(b) {}
    
    string toString() const override { 
        return "(lambda " + param + ". " + body->toString() + ")"; 
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        unordered_map<string, int> new_env = env;
        new_env[param] = level;
        auto newBody = body->toDeBruijn(level + 1, new_env);
        return make_shared<Lambda>(param, newBody);
    }
};

struct Application : Term {
    TermPtr func;
    TermPtr arg;
    
    Application(TermPtr f, TermPtr a) : func(f), arg(a) {}
    
    string toString() const override { 
        return "(" + func->toString() + " " + arg->toString() + ")"; 
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<Application>(
            func->toDeBruijn(level, env), 
            arg->toDeBruijn(level, env));
    }
};

struct BinaryOp : Term {
    char op;
    TermPtr left;
    TermPtr right;
    
    BinaryOp(char o, TermPtr l, TermPtr r) : op(o), left(l), right(r) {}
    
    string toString() const override {
        return "(" + left->toString() + " " + op + " " + right->toString() + ")";
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<BinaryOp>(op, 
            left->toDeBruijn(level, env), 
            right->toDeBruijn(level, env));
    }
};

// ==================== Парсер ====================
class Parser {
    string input;
    size_t pos = 0;
    
    void skipWhitespace() {
        while (pos < input.size() && isspace(input[pos])) pos++;
    }
    
    char peek() {
        skipWhitespace();
        if (pos >= input.size()) return '\0';
        return input[pos];
    }
    
    char next() {
        skipWhitespace();
        if (pos >= input.size()) throw runtime_error("Unexpected end of input");
        return input[pos++];
    }
    
    bool match(const string& s) {
        size_t old_pos = pos;
        try {
            for (char c : s) {
                if (next() != c) {
                    pos = old_pos;
                    return false;
                }
            }
            return true;
        } catch (...) {
            pos = old_pos;
            return false;
        }
    }
    
    TermPtr parseTerm() {
        if (peek() != '(') {
            if (isdigit(peek())) {
                return parseNumber();
            } else if (isalpha(peek())) {
                return parseVariable();
            }
            throw runtime_error("Unexpected character");
        }
        
        next(); // consume '('
        if (match("lambda")) {
            return parseLambda();
        }
        
        // Application or BinaryOp
        auto left = parseTerm();
        char c = peek();
        
        if (c == '+' || c == '*') {
            next();
            auto right = parseTerm();
            if (next() != ')') throw runtime_error("Expected ')'");
            return make_shared<BinaryOp>(c, left, right);
        } else {
            auto arg = parseTerm();
            if (next() != ')') throw runtime_error("Expected ')'");
            return make_shared<Application>(left, arg);
        }
    }
    
    TermPtr parseLambda() {
        string param;
        while (isalpha(peek())) param += next();
        
        if (param.empty()) {
            throw runtime_error("Expected parameter name after lambda");
        }
        
        if (next() != '.') throw runtime_error("Expected '.' after lambda parameter");
        
        auto body = parseTerm();
        if (next() != ')') throw runtime_error("Expected ')' after lambda body");
        return make_shared<Lambda>(param, body);
    }
    
    TermPtr parseNumber() {
        string num;
        while (isdigit(peek())) num += next();
        return make_shared<Number>(stoi(num));
    }
    
    TermPtr parseVariable() {
        string var;
        while (isalpha(peek())) var += next();
        return make_shared<Variable>(var);
    }

public:
    TermPtr parse(const string& str) {
        input = str;
        pos = 0;
        auto term = parseTerm();
        if (pos != input.size()) throw runtime_error("Extra input at end");
        return term;
    }
};

// ==================== Компиляция в КАМ ====================
enum class CAMCommand {
    ACCESS,
    GRAB,
    PUSH,
    RETURN,
    ADD,
    MUL,
    CONST
};

// ==================== Исполнение КАМ ====================
class CAMMachine {
    stack<int> eval_stack;
    vector<vector<int>> env_stack;
    
    void executeCommand(CAMCommand cmd, int arg = 0) {
        switch (cmd) {
            case CAMCommand::ACCESS: {
                if (env_stack.empty()) throw runtime_error("No environment for access");
                
                // Ищем переменную в окружениях от внутреннего к внешнему
                int current_env = env_stack.size() - 1;
                while (current_env >= 0) {
                    if (arg < env_stack[current_env].size()) {
                        eval_stack.push(env_stack[current_env][arg]);
                        return;
                    }
                    arg -= env_stack[current_env].size();
                    current_env--;
                }
                throw runtime_error("Invalid variable access: " + to_string(arg));
            }
            case CAMCommand::GRAB: {
                // Создаем новое окружение для параметра
                env_stack.push_back({});
                break;
            }
            case CAMCommand::PUSH: {
                if (eval_stack.empty()) throw runtime_error("Stack underflow");
                int val = eval_stack.top();
                eval_stack.pop();
                
                if (env_stack.empty()) throw runtime_error("No environment to push to");
                // Добавляем значение в текущее окружение
                env_stack.back().push_back(val);
                break;
            }
            case CAMCommand::RETURN: {
                if (eval_stack.empty()) throw runtime_error("Stack underflow");
                int result = eval_stack.top();
                eval_stack.pop();
                
                if (env_stack.empty()) throw runtime_error("Env stack underflow");
                env_stack.pop_back(); // Удаляем окружение параметра
                
                eval_stack.push(result);
                break;
            }
            case CAMCommand::ADD: {
                if (eval_stack.size() < 2) throw runtime_error("Stack underflow");
                int b = eval_stack.top(); eval_stack.pop();
                int a = eval_stack.top(); eval_stack.pop();
                eval_stack.push(a + b);
                break;
            }
            case CAMCommand::MUL: {
                if (eval_stack.size() < 2) throw runtime_error("Stack underflow");
                int b = eval_stack.top(); eval_stack.pop();
                int a = eval_stack.top(); eval_stack.pop();
                eval_stack.push(a * b);
                break;
            }
            case CAMCommand::CONST: {
                eval_stack.push(arg);
                break;
            }
        }
    }

public:
    int execute(const vector<CAMCommand>& code) {
        eval_stack = stack<int>();
        env_stack.clear();
        env_stack.push_back({}); // Глобальное окружение

        for (size_t i = 0; i < code.size(); ) {
            CAMCommand cmd = code[i];
            if (cmd == CAMCommand::ACCESS || cmd == CAMCommand::CONST) {
                executeCommand(cmd, static_cast<int>(code[i+1]));
                i += 2;
            } else {
                executeCommand(cmd);
                i++;
            }
        }
        
        if (eval_stack.size() != 1) {
            throw runtime_error("Invalid result state, stack size: " + to_string(eval_stack.size()));
        }
        return eval_stack.top();
    }
};

class CAMCompiler {
    vector<CAMCommand> compileTerm(TermPtr term, int env_size = 0) {
        vector<CAMCommand> code;
        
        if (auto var = dynamic_pointer_cast<Variable>(term)) {
            if (var->deBruijnIdx < 0) throw runtime_error("Unbound variable: " + var->name);
            code.push_back(CAMCommand::ACCESS);
            // Индекс переменной в текущем окружении
            code.push_back(static_cast<CAMCommand>(env_size - var->deBruijnIdx  - 1));
        }
        else if (auto num = dynamic_pointer_cast<Number>(term)) {
            code.push_back(CAMCommand::CONST);
            code.push_back(static_cast<CAMCommand>(num->value));
        }
        else if (auto lam = dynamic_pointer_cast<Lambda>(term)) {
            // Компилируем тело с увеличенным размером окружения
            auto bodyCode = compileTerm(lam->body, env_size + 1);
            
            // Оборачиваем тело в GRAB и RETURN
             // Оборачиваем тело в GRAB и RETURN
            code.push_back(CAMCommand::GRAB);
            code.push_back(CAMCommand::PUSH);
            code.insert(code.end(), bodyCode.begin(), bodyCode.end());
            code.push_back(CAMCommand::RETURN);
        }
        else if (auto app = dynamic_pointer_cast<Application>(term)) {
             // Сначала аргумент, затем функцию
            auto argCode = compileTerm(app->arg, env_size);
            auto funcCode = compileTerm(app->func, env_size);
            
            code.insert(code.end(), argCode.begin(), argCode.end());
            code.insert(code.end(), funcCode.begin(), funcCode.end());
            // code.push_back(CAMCommand::PUSH);
        }
        else if (auto binop = dynamic_pointer_cast<BinaryOp>(term)) {
            auto leftCode = compileTerm(binop->left, env_size);
            auto rightCode = compileTerm(binop->right, env_size);
            code.insert(code.end(), leftCode.begin(), leftCode.end());
            code.insert(code.end(), rightCode.begin(), rightCode.end());
            code.push_back(binop->op == '+' ? CAMCommand::ADD : CAMCommand::MUL);
        }
        
        return code;
    }

public:
    vector<CAMCommand> compile(TermPtr term) {
        auto dbTerm = term->toDeBruijn(0, {});
        return compileTerm(dbTerm);
    }
};

// ==================== Тестирование ====================

int main() {
    Parser parser;
    CAMCompiler compiler;
    CAMMachine machine;
    
    vector<string> tests = {
        "42", // Просто число
        "(1 + 2)", // Бинарная операция
        "((lambda x. x) 42)", // Идентичная функция
        "((lambda x. (x + 1)) 42)", // Прибавление 1
        "((lambda x. (x * 2)) 11)", // Умножение на 2
        "((lambda x. ((lambda y. (x + y)) 10)) 32)" // Вложенные лямбды
    };
    
    for (const auto& test : tests) {
        cout << "Testing: " << test << endl;
        try {
            TermPtr term = parser.parse(test);
            cout << "Parsed: " << term->toString() << endl;
            auto dbTerm = term->toDeBruijn(0, {});
            cout << "DeBruijn: " << dbTerm->toString() << endl;
            auto code = compiler.compile(term);
            
            cout << "CAM code: ";
            for (size_t i = 0; i < code.size(); ) {
                CAMCommand cmd = code[i];
                switch (cmd) {
                    case CAMCommand::ACCESS: 
                        cout << "ACCESS(" << static_cast<int>(code[i+1]) << ") "; 
                        i += 2;
                        break;
                    case CAMCommand::CONST: 
                        cout << "CONST(" << static_cast<int>(code[i+1]) << ") "; 
                        i += 2;
                        break;
                    case CAMCommand::GRAB: cout << "GRAB "; i++; break;
                    case CAMCommand::PUSH: cout << "PUSH "; i++; break;
                    case CAMCommand::RETURN: cout << "RETURN "; i++; break;
                    case CAMCommand::ADD: cout << "ADD "; i++; break;
                    case CAMCommand::MUL: cout << "MUL "; i++; break;
                }
            }
            cout << endl;
            
            int result = machine.execute(code);
            cout << "Result: " << result << endl;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
        }
        cout << "-------------------" << endl;
    }
    
    return 0;
}



