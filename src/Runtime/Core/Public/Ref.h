#pragma once

template <typename T>
struct Ref
{
    T* ptr = nullptr;

    // Auto-dereference for easy usage: transform->position
    T* operator->() { return ptr; }
    const T* operator->() const { return ptr; }
    
    T& operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }

    Ref<T>& operator=(T& other) { ptr = &other; return *this; }

    Ref<T>& operator=(T* other) { ptr = other; return *this; }
    
    // Check if valid (though in our system, it should always be valid inside Update)
    operator bool() const { return ptr != nullptr; }
};