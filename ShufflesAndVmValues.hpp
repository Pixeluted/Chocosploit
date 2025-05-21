//
// Created by Pixeluted on 04/02/2025.
//
#pragma once
#include <cstdint>

template <typename T> struct vmvalue1
{
public:
    operator const T() const
    {
        return (T)((uintptr_t)storage - (uintptr_t)this);
    }

    void operator=(const T& value)
    {
        storage = (T)((uintptr_t)value + (uintptr_t)this);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T get() {
        return (T)((uintptr_t)storage - (uintptr_t)this);
    }
private:
    T storage;
};


template <typename T> struct vmvalue2
{
public:
    operator const T() const
    {
        return (T)((uintptr_t)this - (uintptr_t)storage);
    }

    void operator=(const T& value)
    {
        storage = (T)((uintptr_t)this - (uintptr_t)value);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T get() {
        return (T)((uintptr_t)this - (uintptr_t)storage);
    }
private:
    T storage;
};


template <typename T> struct vmvalue3
{
public:
    operator const T() const
    {
        return (T)((uintptr_t)this ^ (uintptr_t)storage);
    }

    void operator=(const T& value)
    {
        storage = (T)((uintptr_t)value ^ (uintptr_t)this);
    }

    const T operator->() const
    {
        return operator const T();
    }

    T get() {
        return (T)((uintptr_t)this ^ (uintptr_t)storage);
    }
private:
    T storage;
};


template <typename T> struct vmvalue4
{
public:
    operator const T() const
    {
        return (T)((uintptr_t)this + (uintptr_t)storage);
    }

    void operator=(const T& value)
    {
        storage = (T)((uintptr_t)value - (uintptr_t)this);
    }


    const T operator->() const
    {
        return operator const T();
    }

    T get() {
        return (T)((uintptr_t)this + (uintptr_t)storage);
    }
private:
    T storage;
};

template <typename T> struct vmvalue0
{
public:
    // Conversion operator - just returns the stored value
    operator const T() const
    {
        return storage;
    }

    // Assignment operator - just stores the value
    void operator=(const T& value)
    {
        storage = value;
    }

    // Arrow operator - returns the stored value
    const T operator->() const
    {
        return storage;
    }

    // Getter method - returns the stored value
    T get() {
        return storage;
    }

private:
    T storage;
};


#define comma ,

#define VM_SHUFFLE3(s, a1, a2, a3) a1 s a2 s a3
#define VM_SHUFFLE4(s, a1, a2, a3, a4) a3 s a2 s a1 s a4
#define VM_SHUFFLE5(s, a1, a2, a3, a4, a5) a1 s a5 s a2 s a3 s a4
#define VM_SHUFFLE6(s, a1, a2, a3, a4, a5, a6) a2 s a1 s a4 s a6 s a5 s a3
#define VM_SHUFFLE7(s, a1, a2, a3, a4, a5, a6, a7) a4 s a1 s a3 s a5 s a7 s a2 s a6
#define VM_SHUFFLE8(s, a1, a2, a3, a4, a5, a6, a7, a8) a6 s a2 s a1 s a7 s a3 s a8 s a4 s a5
#define VM_SHUFFLE9(s, a1, a2, a3, a4, a5, a6, a7, a8, a9) a1 s a5 s a3 s a2 s a4 s a9 s a8 s a7 s a6

#define UserdataMeta vmvalue1
#define ProtoTypeInfo vmvalue1

#define LuaStateMember vmvalue4
#define ProtoDebugInsn vmvalue4
#define ClosureCont vmvalue4

#define ProtoDebugName vmvalue3

#define TStringHash vmvalue2
#define ProtoMember vmvalue2
#define ClosureDebugname vmvalue2