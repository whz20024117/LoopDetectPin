#ifndef LOOPDETECT_MYCONTAINERS_H
#define LOOPDETECT_MYCONTAINERS_H

#include <functional>
#include <stdio.h>
#include <string.h>
#include <iostream>


/* A simple implementation of vector, with no use of STL containers. class T can only be an integer/pointer */
template <class T>
class MyVector {
    uint64_t _size = 0;
    uint64_t capacity = 0;
    T* container = nullptr;

    T* container_resize(uint64_t n) {
        // Resize the capacity of MyVector.
        // Return nullptr if failed. Return new container pointer on succeed.

        if (n < _size) {
            fprintf(stderr, "MyVector resize error: It is illegal to resize the vector with size %lu to %lu.\n", _size, n);
            return nullptr;
        }

        if (n == capacity)
            return container;

        T* new_containter = new T[n];
        if (!new_containter) {
            fprintf(stderr, "MyVector resize error: Failure on memory allocation when resize to %lu.\n", n);
            return nullptr;
        }
        memcpy(new_containter, container, _size * sizeof(T));
        delete[] container;
        container = new_containter;
        capacity = n;

        return new_containter;
    }

public:
    MyVector() {
        _size = 0;
        capacity = 20;
        container = (T*) new T[capacity];
    }

    ~MyVector() {
        delete[] container;
    }

    void push_back(const T& val) {
        if (_size == capacity) {
            if (!container_resize(capacity + capacity / 4)) {
                return;
            }
        }

        container[_size++] = val;
    }

    void pop_back() {
        _size--;
        if ( _size < capacity / 2 ) {
            container_resize(capacity / 2);
        }
    }

    T& back() {
        if (_size == 0) {
            fprintf(stderr, "MyVector error: cannot access back when empty.\n");
            exit(-1);
        }
        return container[_size-1];
    }

    T& front() {
        if (_size == 0) {
            fprintf(stderr, "MyVector error: cannot access front when empty.\n");
            exit(-1);
        }
        return *container;
    }

    T& operator[](uint64_t i) {
        return container[i];
    }

    size_t size() {
        return _size;
    }

    bool empty() {
        if (_size == 0)
            return true;
        else
            return false;
    }
};


/* Hash table implementations */

// djb2 Hash function
size_t djb2(const void *ptr, size_t len)
{
    size_t hash = 5381ul;
    char c;
    const char *p = (const char *) ptr;

    while (len--) {
        c = *p++;
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

template <class T>
size_t myhash(T key) {
    return djb2(&key, sizeof(T));
}

const uint64_t __bucket_number_prime_list[29] = {
  5ul, 53ul, 97ul, 193ul, 389ul,
  769ul, 1543ul, 3079ul, 6151ul, 12289ul,
  24593ul, 49157ul, 98317ul, 196613ul, 393241ul,
  786433ul, 1572869ul, 3145739ul, 6291469ul, 12582917ul,
  25165843ul, 50331653ul, 100663319ul, 201326611ul, 402653189ul,
  805306457ul, 1610612741ul, 3221225473ul, 4294967291ul
};

template <class KeyT, class ValT>
class __BucketListItem {
public:
    KeyT key;
    ValT val;
    __BucketListItem *next = nullptr;
};

template <class KeyT, class ValT>
class __HashTableBase {
protected:
    uint64_t _size;
    uint64_t n_buckets;
    __BucketListItem<KeyT, ValT> **buckets;

    __HashTableBase() {
        _size = 0;
        n_buckets = __bucket_number_prime_list[0];
        buckets = new __BucketListItem<KeyT, ValT>*[n_buckets];
        if (!buckets) {
            fprintf(stderr, "MyMap bucket initial error: memory allocation failed for buckets.\n");
            exit(-1);
        }
        for (size_t i=0; i < n_buckets; i++) {
            buckets[i] = nullptr;
        }
    }

    ~__HashTableBase() {
        for (size_t i=0; i < n_buckets; i++) {
            if (buckets[i])
                free_bucket(buckets[i]);
        }
        delete[] buckets;
    }

    void free_bucket(__BucketListItem<KeyT, ValT> *bucket_node) {
        if (!bucket_node) {
            fprintf(stderr, "MyMap free error: bucket_node is nullptr.\n");
            return;
        }

        __BucketListItem<KeyT, ValT> *next_node;
        while (bucket_node->next) {
            next_node = bucket_node->next;
            delete bucket_node;
            bucket_node = next_node;
        }
        delete bucket_node;
    }

    int insert_to_bucket(KeyT key, ValT val) {
        __BucketListItem<KeyT, ValT> *item = new __BucketListItem<KeyT, ValT>;
        if (!item) {
            fprintf(stderr, "MyMap insertion error: cannot allocate memory for new item.\n");
            return -1;
        }
        item->key = key;
        item->val = val;
        item->next = nullptr;

        size_t bucket_index = get_index(key);
        if (!buckets[bucket_index]) { // no item in the list
            buckets[bucket_index] = item;
            return 0;
        }

        __BucketListItem<KeyT, ValT> *bucket_node = buckets[bucket_index];
        while (bucket_node->next) {
            if (bucket_node->key == key) {
                delete item;
                return 0;
            }
            bucket_node = bucket_node->next;
        }
        if (bucket_node->key == key) {
            delete item;
            return 0;
        }
            
        bucket_node->next = item;
        return 0;
    }

    __BucketListItem<KeyT, ValT> **buckets_enlarge_n(uint64_t new_n_buckets) {
        // Return nullptr if failed. Return new buckets pointer on succeed.

        if (new_n_buckets <= n_buckets) {
            fprintf(stderr, "MyMap bucket enlarge error: desired size is smaller than current size.\n");
            return nullptr;
        }

        __BucketListItem<KeyT, ValT> **new_buckets = \
            new __BucketListItem<KeyT, ValT>*[new_n_buckets];
        if (!new_buckets) {
            fprintf(stderr, "MyMap bucket enlarge error: memory allocation failed for new buckets with number %lu.\n", new_n_buckets);
            return nullptr;
        }

        for (size_t i=0; i < new_n_buckets; i++) {
            new_buckets[i] = nullptr;
        }

        // From now on we need to be careful. We now change the pointers.
        // Save old stuff.
        __BucketListItem<KeyT, ValT> **old_buckets = buckets;
        uint64_t old_n_buckets = n_buckets;

        // New memories
        buckets = new_buckets;
        n_buckets = new_n_buckets;

        for (size_t i=0; i < old_n_buckets; i++) {
            __BucketListItem<KeyT, ValT> *tmp_bucket = old_buckets[i];

            while (tmp_bucket) {
                if (insert_to_bucket(tmp_bucket->key, tmp_bucket->val)) {
                    goto failed;
                }
                tmp_bucket = tmp_bucket->next;
            }
        }

        // Now clean up old memories
        for (size_t i=0; i < old_n_buckets; i++) {
            __BucketListItem<KeyT, ValT> *tmp_bucket = old_buckets[i];

            if (tmp_bucket) {
                free_bucket(tmp_bucket);
            }
        }
        delete[] old_buckets;
        return buckets;

    failed:
        // if failed, we clean up newly allocated memories.
        buckets = old_buckets;
        n_buckets = old_n_buckets;

        for (size_t i=0; i < new_n_buckets; i++) {
            __BucketListItem<KeyT, ValT> *tmp_bucket = new_buckets[i];

            if (tmp_bucket) {
                free_bucket(tmp_bucket);
            }
        }
        delete[] new_buckets;
        
        return nullptr;
    }

    inline uint64_t get_index_from_hash(size_t hash) {
        return hash % n_buckets;
    }

    uint64_t get_index(KeyT key) {
        size_t hash;
        hash = myhash<KeyT>(key);
        return get_index_from_hash(hash);
    }
};


/* A simple implementation of hashset, with no use of STL containers. Key can only be an integer/pointer */
template <class KeyT>
class MySet : protected __HashTableBase<KeyT, KeyT> {
public:
    void insert(KeyT key) {
        if (__HashTableBase<KeyT, KeyT>::insert_to_bucket(key, key)) {
            return;
        }
        this->_size++;

        if (this->_size > this->n_buckets / 2) {
            unsigned idx = 0;
            uint64_t new_n_bucket = __bucket_number_prime_list[idx];

            while (new_n_bucket <= this->n_buckets) {
                new_n_bucket = __bucket_number_prime_list[++idx];
                if (idx == 28) {
                    fprintf(stderr, "MySet insertion warning: cannot enlarge anymore.\n");
                    return;
                }
            }

            __HashTableBase<KeyT, KeyT>::buckets_enlarge_n(new_n_bucket);
        }
    }

    KeyT& operator[](KeyT key) {
        uint64_t index = __HashTableBase<KeyT, KeyT>::get_index(key);
        __BucketListItem<KeyT, KeyT> *bucket_node = this->buckets[index];
        if (!bucket_node) {
            fprintf(stderr, "MySet access error: cannot find key.\n");
            exit(-1);
        }

        while (bucket_node) {
            if (bucket_node->key == key) {
                return bucket_node->val;
            }
            bucket_node = bucket_node->next;
        }
        fprintf(stderr, "MySet access error: cannot find key.\n");
        exit(-1);
    }

    MyVector<KeyT> keys() {
        // Get keys
        MyVector<KeyT> ret;
        for (size_t i=0; i < this->n_buckets; i++) {
            __BucketListItem<KeyT, KeyT> *bucket_node = this->buckets[i];

            while (bucket_node) {
                ret.push_back(bucket_node->key);
                bucket_node = bucket_node->next;
            }
        }
        return ret;
    }

    bool containsKey(KeyT key) {
        uint64_t index = __HashTableBase<KeyT, KeyT>::get_index(key);
        __BucketListItem<KeyT, KeyT> *bucket_node = this->buckets[index];
        if (!bucket_node) {
            return false;
        }
        while (bucket_node) {
            if (bucket_node->key == key) {
                return true;
            }
            bucket_node = bucket_node->next;
        }
        return false;
    }

    bool empty() {
        if (this->_size == 0)
            return true;
        else
            return false;
    }

};


/* A simple implementation of hashmap. Essentially very similar to hash set. */
template <class KeyT, class ValT>
class MyMap : protected __HashTableBase<KeyT, ValT> {
public:
    void insert(KeyT key, ValT val) {
        if (__HashTableBase<KeyT, ValT>::insert_to_bucket(key, val)) {
            return;
        }
        this->_size++;

        if (this->_size > this->n_buckets / 2) {
            unsigned idx = 0;
            uint64_t new_n_bucket = __bucket_number_prime_list[idx];

            while (new_n_bucket <= this->n_buckets) {
                new_n_bucket = __bucket_number_prime_list[++idx];
                if (idx == 28) {
                    fprintf(stderr, "MyMap insertion warning: cannot enlarge anymore.\n");
                    return;
                }
            }

            __HashTableBase<KeyT, ValT>::buckets_enlarge_n(new_n_bucket);
        }
    }

    ValT& operator[](KeyT key) {
        uint64_t index = __HashTableBase<KeyT, ValT>::get_index(key);
        __BucketListItem<KeyT, ValT> *bucket_node = this->buckets[index];
        if (!bucket_node) {
            fprintf(stderr, "MyMap access error: cannot find key.\n");
            exit(-1);
        }

        while (bucket_node) {
            if (bucket_node->key == key) {
                return bucket_node->val;
            }
            bucket_node = bucket_node->next;
        }
        fprintf(stderr, "MyMap access error: cannot find key.\n");
        exit(-1);
    }

    MyVector<KeyT> keys() {
        // Get keys
        MyVector<KeyT> ret;
        for (size_t i=0; i < this->n_buckets; i++) {
            __BucketListItem<KeyT, ValT> *bucket_node = this->buckets[i];

            while (bucket_node) {
                ret.push_back(bucket_node->key);
                bucket_node = bucket_node->next;
            }
        }
        return ret;
    }

    bool containsKey(KeyT key) {
        uint64_t index = __HashTableBase<KeyT, ValT>::get_index(key);
        __BucketListItem<KeyT, ValT> *bucket_node = this->buckets[index];
        if (!bucket_node) {
            return false;
        }
        while (bucket_node) {
            if (bucket_node->key == key) {
                return true;
            }
            bucket_node = bucket_node->next;
        }
        return false;
    }

    bool empty() {
        if (this->_size == 0)
            return true;
        else
            return false;
    }

};

#endif /* LOOPDETECT_MYCONTAINERS_H */