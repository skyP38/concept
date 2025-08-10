#include <iostream>
#include <vector>
#include <memory>
#include <stack>
#include <unordered_map>

using namespace std;

// ==================== AST ====================
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
        if (deBruijnIdx >= 0) return "#" + to_string(deBruijnIdx);
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
    
    string toString() const override { return to_string(value); }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<Number>(*this);
    }
};

struct Lambda : Term {
    string param;
    TermPtr body;
    
    Lambda(string p, TermPtr b) : param(p), body(b) {}
    
    string toString() const override { 
        return "(λ" + param + "." + body->toString() + ")"; 
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        unordered_map<string, int> new_env = env;
        new_env[param] = level;
        return make_shared<Lambda>(param, body->toDeBruijn(level + 1, new_env));
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

// ==================== CAM Machine ====================
enum class CAMCommand {
    ACCESS,
    GRAB,
    PUSH,
    APPLY,
    RETURN
};

class CAMMachine {
    vector<pair<CAMCommand, int>> C;  // Code
    vector<int> A;                    // Environment
    stack<pair<vector<pair<CAMCommand, int>>, vector<int>>> M;  // Dump
    stack<int> S;                     // Stack for arguments

    void step() {
        if (C.empty()) return;
        
        auto [cmd, arg] = C.front();
        C.erase(C.begin());
        
        switch (cmd) {
            case CAMCommand::PUSH:
                S.push(arg);
                break;
                
            case CAMCommand::GRAB:
                M.push({C, A});  // Сохраняем текущее состояние
                C.clear();       // Очищаем код для выполнения тела функции
                break;
                
            case CAMCommand::ACCESS:
                if (A.empty()) {
                    throw runtime_error("Accessing variable in empty environment");
                }
                // Всегда берем первый элемент окружения
                S.push(A[0]);
                break;
                
            case CAMCommand::APPLY: {
                if (S.size() < 2) throw runtime_error("Stack underflow in APPLY");
                int arg = S.top(); S.pop();
                int func = S.top(); S.pop();
                
                // Помещаем аргумент в окружение
                A = {arg};
                // Помещаем функцию на стек
                S.push(func);
                break;
            }
                
            case CAMCommand::RETURN:
                if (S.empty()) throw runtime_error("Stack underflow");
                int result = S.top();
                S.pop();
                
                if (M.empty()) {
                    S.push(result);
                    C.clear();
                    return;
                }
                
                auto [savedCode, savedEnv] = M.top();
                M.pop();
                C = savedCode;
                A = savedEnv;
                S.push(result);
                break;
        }
    }

public:
    int run(const vector<pair<CAMCommand, int>>& program) {
        C = program;
        A.clear();
        while (!M.empty()) M.pop();
        while (!S.empty()) S.pop();
        
        while (!C.empty()) {
            step();
        }
        
        if (S.size() != 1) {
            throw runtime_error("Invalid final stack state. Stack size: " + to_string(S.size()));
        }
        return S.top();
    }
};

// ==================== CAM Compiler ====================
class CAMCompiler {
    vector<pair<CAMCommand, int>> compileTerm(TermPtr term, int env_size = 0) {
        vector<pair<CAMCommand, int>> code;
        
        if (auto var = dynamic_pointer_cast<Variable>(term)) {
            code.emplace_back(CAMCommand::ACCESS, 0);  // Всегда обращаемся к первому элементу
        }
        else if (auto num = dynamic_pointer_cast<Number>(term)) {
            code.emplace_back(CAMCommand::PUSH, num->value);
        }
        else if (auto lam = dynamic_pointer_cast<Lambda>(term)) {
            // Компилируем тело функции с увеличенным размером окружения
            auto body_code = compileTerm(lam->body, env_size + 1);
            
            // Генерируем код для лямбда-выражения
            code.emplace_back(CAMCommand::GRAB, 0);
            code.insert(code.end(), body_code.begin(), body_code.end());
            code.emplace_back(CAMCommand::RETURN, 0);
        }
        else if (auto app = dynamic_pointer_cast<Application>(term)) {
            // Сначала компилируем аргумент
            auto arg_code = compileTerm(app->arg, env_size);
            // Затем компилируем функцию
            auto func_code = compileTerm(app->func, env_size);
            
            // Объединяем код
            code.insert(code.end(), arg_code.begin(), arg_code.end());
            code.insert(code.end(), func_code.begin(), func_code.end());
            code.emplace_back(CAMCommand::APPLY, 0);
        }
        
        return code;
    }

public:
    vector<pair<CAMCommand, int>> compile(TermPtr term) {
        auto dbTerm = term->toDeBruijn();
        return compileTerm(dbTerm);
    }
};


// ==================== Tests ====================
void run_test(const string& name, TermPtr term) {
    cout << "=== Test: " << name << " ===" << endl;
    cout << "Term: " << term->toString() << endl;
    
    try {
        CAMCompiler compiler;
        CAMMachine machine;
        
        auto program = compiler.compile(term);
        
        cout << "Compiled program: ";
        for (auto [cmd, arg] : program) {
            switch (cmd) {
                case CAMCommand::ACCESS: cout << "ACCESS(" << arg << ") "; break;
                case CAMCommand::GRAB: cout << "GRAB "; break;
                case CAMCommand::PUSH: cout << "PUSH(" << arg << ") "; break;
                case CAMCommand::APPLY: cout << "APPLY "; break;
                case CAMCommand::RETURN: cout << "RETURN "; break;
            }
        }
        cout << endl;
        
        int result = machine.run(program);
        cout << "Result: " << result << endl;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    
    cout << "-------------------" << endl;
}

// ==================== Main ====================
int main() {
    // Basic tests
    run_test("Identity function applied to 42",
        make_shared<Application>(
            make_shared<Lambda>("x", make_shared<Variable>("x")),
            make_shared<Number>(42)
        ));
    
    run_test("Constant function",
        make_shared<Application>(
            make_shared<Lambda>("x", make_shared<Number>(10)),
            make_shared<Number>(20)
        ));
    
    run_test("Function application",
        make_shared<Application>(
            make_shared<Lambda>("f", make_shared<Application>(
                make_shared<Variable>("f"),
                make_shared<Number>(5)
            )),
            make_shared<Lambda>("x", make_shared<Variable>("x"))
        ));
    
    // More complex tests
    run_test("Nested lambda",
        make_shared<Application>(
            make_shared<Lambda>("x", make_shared<Application>(
                make_shared<Lambda>("y", make_shared<Variable>("x")),
                make_shared<Number>(10)
            )),
            make_shared<Number>(20)
        ));
    
    run_test("Multiple applications",
        make_shared<Application>(
            make_shared<Application>(
                make_shared<Lambda>("f", make_shared<Lambda>("x", 
                    make_shared<Application>(
                        make_shared<Variable>("f"),
                        make_shared<Variable>("x")
                    )
                )),
                make_shared<Lambda>("y", make_shared<Variable>("y"))
            ),
            make_shared<Number>(30)
        ));
    
    // Error cases
    run_test("Unbound variable",
        make_shared<Variable>("x"));
    
    run_test("Malformed application",
        make_shared<Application>(
            make_shared<Number>(10),
            make_shared<Number>(20)
        ));
    
    return 0;
}