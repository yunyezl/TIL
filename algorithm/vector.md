# 벡터

**1. 벡터의 기본 개념**

- 동적 배열(Dynamic Array)은 크기를 미리 지정할 필요 없이 필요에 따라 크기가 자동으로 조절되는 배열
- 기본 배열과 달리, 새로운 요소를 추가할 공간이 부족할 경우 새로운 배열을 할당하고 기존 데이터를 복사하는 방식으로 동작

**2. 기본적인 벡터 구조 설계**

벡터는 보통 다음과 같은 핵심 속성을 가진다.
- `capacity`: 현재 할당된 배열의 크기
- `size`: 실제로 사용 중인 요소 개수
- `data`: 요소를 저장하는 동적 배열

**3. 자동 리사이징 벡터 구현하기**

```cpp
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
```