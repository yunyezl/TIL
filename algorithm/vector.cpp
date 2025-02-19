#include <iostream>

class Vector {
private:
    int* data;
    int size;
    int capacity;

    void resize(int newCapacity) {
        int* newData = new int[newCapacity];

        for (int i = 0; i < size; i++) {
            newData[i] = data[i];
        }

        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

public:
    Vector() : size(0), capacity(1) {
        data = new int[capacity];
    }

    ~Vector() {
        delete[] data;
    }

    int size() const { return size; }

    int capacity() {
        return capacity;
    }

    bool is_empty() {
        return size == 0;
    }

    int at(int index) {
        if (index < 0 || index >= size) {
            throw std::out_of_range("Index out of bounds");
        }
        return data[index];
    }

    int push_back(int value) {
        if (size == capacity) {
            resize(capacity * 2);
        }
        data[size] = value;
        size++;
    }

    int pop_back() {
        if (size == 0) {
            throw std::out_of_range("cannot pop from an empty vector");
        }

        int item = data[size - 1];
        size--;

        if (size > 0 && size <= capacity / 4) {
            resize(capacity / 2);
        }
        return item;
    }

    void insert(int index, int item) {
        if (size == capacity) {
            resize(capacity * 2);
        }
        
       for (int i = size; i > index; i--) {
            data[i] = data[i - 1];
       }

       data[index] = item;
       size++;
    }

    void prepend(int item) {
        if (size == capacity) {
            resize(capacity * 2);
        }

       for (int i = size; i > 0; i--) {
        data[i] = data[i - 1];
       }

       data[0] = item;
       size++;
    }

    void deleteAt(int index) {
        if (index < 0 || index >= size) {
            throw std::out_of_range("Index out of bounds");
        }

        for (int i = index; i < size - 1; i++) {
            data[i] = data[i + 1];
        }
    }

    void remove(int item) {
        int shiftCount = 0;

        for (int i = 0; i < size; i++) {
            if (data[i] == item) {
                shiftCount++;
            } else if (shiftCount > 0) {
                data[i - shiftCount] = data[i];
            }
        }

        size -= shiftCount;

        if (size > 0 && size <= capacity / 4) {
            resize(capacity / 2);
        }
    }

    int operator[](int index) const {
        if (index < 0 || index >= size) {
            throw std::out_of_range("Index out of bounds");
        }
        return data[index];
    }
};