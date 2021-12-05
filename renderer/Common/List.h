#pragma once

#include "uncopyable.h"

namespace glacier {

struct ListElement : private Uncopyable {
    ListElement() {}
    ~ListElement() {
        RemoveFromList(); 
    }

    bool IsInList() const { return prev != nullptr; }

    bool RemoveFromList() {
        if (!IsInList())
            return false;

        prev->next = next;
        next->prev = prev;
        prev = nullptr;
        next = nullptr;
        return true;
    }

    void InsertInList(ListElement* pos) {
        if (this == pos)
            return;

        if (IsInList())
            RemoveFromList();

        prev = pos->prev;
        next = pos;
        prev->next = this;
        next->prev = this;

        return;
    }

    ListElement* prev = nullptr;
    ListElement* next = nullptr;
};

template<typename T>
struct ListNode : public ListElement {
    ListNode(T* data = nullptr) : data(data) {}
    
    T& operator*() const  { return *data; }
    T* operator->() const { return data; }

    // We know the type of prev and next element
    ListNode<T>* GetPrev() const { return static_cast<ListNode<T>*>(prev); }
    ListNode<T>* GetNext() const { return static_cast<ListNode<T>*>(next); }
    T* GetData() const { return data; }

    T* data;
};

template<typename T>
class ListIterator {
public:
    ListIterator(ListElement* node = nullptr) : node_(node) {}

    // Pre- and post-increment operator
    ListIterator& operator++()    { node_ = node_->next; return *this; }
    ListIterator  operator++(int) { ListIterator ret(*this); ++(*this); return ret; } 

    // Pre- and post-decrement operator
    ListIterator& operator--()    { node_ = node_->prev; return *this; }
    ListIterator  operator--(int) { ListIterator ret(*this); --(*this); return ret; } 

    ListNode<T>& operator*() const { return static_cast<ListNode<T>&>(*node_); }
    ListNode<T>* operator->() const { return static_cast<ListNode<T>*>(node_); }

    friend bool operator !=(const ListIterator& x, const ListIterator& y) { return x.node_ != y.node_; }
    friend bool operator ==(const ListIterator& x, const ListIterator& y) { return x.node_ == y.node_; }

private:
    template<typename S> friend class List;
    ListElement* node_;
};

template<typename T>
class ListConstIterator {
public:
    ListConstIterator(const ListElement* node = nullptr) : node_(node) {}

    // Pre- and post-increment operator
    ListConstIterator& operator++()    { node_ = node_->next; return *this; }
    ListConstIterator  operator++(int) { ListConstIterator ret(*this); ++(*this); return ret; } 

    // Pre- and post-decrement operator
    ListConstIterator& operator--()    { node_ = node_->prev; return *this; }
    ListConstIterator  operator--(int) { ListConstIterator ret(*this); --(*this); return ret; } 

    const ListNode<T>& operator*() const  { return static_cast<const ListNode<T>&>(*node_); }
    const ListNode<T>* operator->() const { return static_cast<const ListNode<T>*>(node_); }

    friend bool operator !=(const ListConstIterator& x, const ListConstIterator& y) { return x.node_ != y.node_; }
    friend bool operator ==(const ListConstIterator& x, const ListConstIterator& y) { return x.node_ == y.node_; }

private:
    template<typename S> friend class List;
    const ListElement* node_;
};

template<typename T>
class List {
public:
    using const_iterator = ListConstIterator<T>;
    using iterator = ListIterator<T>;
    using value_type = ListNode<T>;

    List() {
        root_.prev = &root_;
        root_.next = &root_;
    }

    ~List() {
        clear();
    }

    void push_back(value_type& node) {
        node.InsertInList(&root_);
    }

    void push_front(value_type& node) {
        node.InsertInList(root_.next);
    }

    void insert(iterator pos, value_type& node) {
        node.InsertInList(&(*pos));
    }

    void erase(iterator pos) {
        pos->RemoveFromList();
    }

    void pop_back() {
        if (root_.prev != &root_) {
            root_.prev->RemoveFromList();
        }
    }

    void pop_front() {
        if (root_.next != &root_) {
            root_.next->RemoveFromList();
        }
    }

    iterator begin() {
        return iterator(root_.next);
    }
    iterator end() {
        return iterator(&root_);
    }

    const_iterator begin() const {
        return const_iterator(root_.next);
    }

    const_iterator end() const {
        return const_iterator(&root_);
    }

    value_type& front() {
        assert(!empty());
        return static_cast<value_type&>(*root_.next);
    }

    value_type& back() {
        assert(!empty());
        return static_cast<value_type&>(*root_.prev);
    }

    const value_type& front() const {
        assert(!empty());
        return static_cast<const value_type&>(*root_.next);
    }

    const value_type& back() const {
        assert(!empty());
        return static_cast<const value_type&>(*root_.prev);
    }

    bool empty() const {
        return begin() == end();
    }

    size_t size() const {
        size_t size = 0;
        ListElement* node = root_.next;
        while (node != &root_) {
            node = node->next;
            size++;
        }
        return size;
    }

    void append(List& src) {
        insert(end(), src);
    }

    void clear();
    void swap(List<T>& other);

    // Insert list into list (removes elements from source)
    void insert(iterator pos, List<T>& src);
private:
    ListElement root_;
};

template<typename T>
void List<T>::clear() {
    ListElement* node = root_.next;
    while (node != &root_) {
        ListElement* next = node->next;
        node->prev = NULL;
        node->next = NULL;
        //node->SetList(NULL);
        node = next;
    }
    root_.next = &root_;
    root_.prev = &root_;
}

template<typename T>
void List<T>::swap(List<T>& other) {
    assert(this != &other);

    std::swap(other.root_.prev, root_.prev);
    std::swap(other.root_.next, root_.next);

    if (other.root_.prev == &root_)
        other.root_.prev = &other.root_;
    if (root_.prev == &other.root_)
        root_.prev = &root_;
    if (other.root_.next == &root_)
        other.root_.next = &other.root_;
    if (root_.next == &other.root_)
        root_.next = &root_;

    other.root_.prev->next = &other.root_;
    other.root_.next->prev = &other.root_;

    root_.prev->next = &root_;
    root_.next->prev = &root_;
}

template<typename T>
void List<T>::insert(iterator pos, List<T>& src) {
    assert(this != &src);
    if (src.empty())
        return;

    // Insert source before pos
    ListElement* self_head = pos.node_->prev;
    ListElement* self_tail = pos.node_;
    self_head->next = src.root_.next;
    self_tail->prev = src.root_.prev;
    self_head->next->prev = self_head;
    self_tail->prev->next = self_tail;
    // Clear source list
    src.root_.next = &src.root_;
    src.root_.prev = &src.root_;
}

}
