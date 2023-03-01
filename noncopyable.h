#pragma once

/*
 * noncopyable.h
 * 被继承以后，派生类对象可以正常的构造和析构，但是不能拷贝和赋值操作
 *  Created on: 2023年2月28日
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};