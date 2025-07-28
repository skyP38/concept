#include <iostream>
#include <memory>
#include <stdexcept>

template <typename T>
class InfiniteList {
private:
    struct Node {
        T data;
        std::shared_ptr<Node> next;
        
        Node(const T& data) : data(data), next(nullptr) {}
    };
    
    std::shared_ptr<Node> head;
    std::shared_ptr<Node> tail;
    size_t size;
    
public:
    // Конструктор
    InfiniteList() : head(nullptr), tail(nullptr), size(0) {}
    
    // Добавление элемента в конец списка
    void push_back(const T& value) {
        auto newNode = std::make_shared<Node>(value);
        
        if (!head) {
            head = newNode;
            tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        
        tail->next = head;
        
        size++;
    }
    
    // Получение элемента по индексу
    T& at(size_t index) {
        if (size == 0) {
            throw std::out_of_range("List is empty");
        }
        
        index = index % size;
        
        auto current = head;
        for (size_t i = 0; i < index; ++i) {
            current = current->next;
        }
        
        return current->data;
    }
    
    // Получение размера списка
    size_t getSize() const {
        return size;
    }
    
    bool isEmpty() const {
        return size == 0;
    }
    
    // для обхода списка
    class Iterator {
    private:
        std::shared_ptr<Node> current;
        size_t steps;
        size_t maxSteps;
        
    public:
        Iterator(std::shared_ptr<Node> start, size_t max = 0) 
            : current(start), steps(0), maxSteps(max) {}
        
        Iterator& operator++() {
            if (current) {
                current = current->next;
                steps++;
                if (maxSteps > 0 && steps >= maxSteps) {
                    current = nullptr;
                }
            }
            return *this;
        }
        
        T& operator*() {
            return current->data;
        }
        
        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }
    };
    
    Iterator begin(size_t maxIterations = 0) {
        return Iterator(head, maxIterations);
    }
    
    Iterator end() {
        return Iterator(nullptr);
    }
};

int main() {
    InfiniteList<int> list;
    
    for (int i = 1; i <= 5; ++i) {
        list.push_back(i);
    }
    
    std::cout << "Element at index 7: " << list.at(7) << std::endl; // 3
    std::cout << "Element at index 12: " << list.at(12) << std::endl; // 2
    
    std::cout << "First 10 elements in infinite loop: ";
    size_t count = 0;
    for (auto it = list.begin(10); it != list.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;
    
    return 0;
}