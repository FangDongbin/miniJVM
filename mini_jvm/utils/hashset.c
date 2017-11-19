//
// Created by gust on 2017/8/30.
//
#include <stdlib.h>
#include <stdio.h>
#include "math.h"
#include "d_type.h"
#include "hashset.h"
#include "hashtable.h"

static s32 HASH_SET_DEFAULT_SIZE = 1024 * 1024;

HashsetEntry *hashset_find_entry(Hashset *hash_table,
                                 HashsetKey key);


static inline HashsetEntry *_hashset_get_entry(Hashset *set) {
    return jvm_calloc(sizeof(HashsetEntry));
}

static inline void _hashset_free_entry(Hashset *set, HashsetEntry *entry) {
    jvm_free(entry);
}

unsigned int hashset_allocate_table(Hashset *set, unsigned int size) {
    if (size) {
        set->table = jvm_calloc(size *
                                sizeof(HashsetEntry *));
        if (set->table)set->table_size = size;
    }

    return set->table != NULL;
}


unsigned long _DEFAULT_HashsetHash(HashsetKey kmer) {
    return (unsigned long) kmer;
}

Hashset *hashset_create() {
    Hashset *set;

    set = (Hashset *) jvm_calloc(sizeof(Hashset));

    if (set == NULL) {
        return NULL;
    }

    set->entries = 0;

    if (!hashset_allocate_table(set, HASH_SET_DEFAULT_SIZE)) {
        jvm_free(set);

        return NULL;
    }

    pthread_spin_init(&set->lock, PTHREAD_PROCESS_PRIVATE);
    return set;
}

void hashset_destory(Hashset *set) {
    if (!set)return;
    HashsetEntry *rover;
    HashsetEntry *next;
    unsigned long long int i;

    pthread_spin_lock(&set->lock);
    {
        for (i = 0; i < set->table_size; ++i) {
            rover = set->table[i];
            while (rover != NULL) {
                next = rover->next;
                _hashset_free_entry(set, rover);
                rover = next;
            }
        }
    }
    pthread_spin_unlock(&set->lock);

    pthread_spin_destroy(&set->lock);
    jvm_free(set->table);
    jvm_free(set);
}


void hashset_clear(Hashset *set) {
    HashsetEntry *rover;
    HashsetEntry *next;
    unsigned int i;

    pthread_spin_lock(&set->lock);
    {
        for (i = 0; i < set->table_size; ++i) {
            rover = set->table[i];
            while (rover != NULL) {
                next = rover->next;
                _hashset_free_entry(set, rover);
                rover = next;
            }
            set->table[i] = NULL;
        }
        set->entries = 0;
    }
    pthread_spin_unlock(&set->lock);

    if (set->table_size > HASH_SET_DEFAULT_SIZE) {
        jvm_free(set->table);
        set->table = NULL;
        set->table_size = 0;
        if (!hashset_allocate_table(set, HASH_SET_DEFAULT_SIZE)) {
            jvm_free(set);
        }
    }
}


int hashset_put(Hashset *set, HashsetKey key) {
    HashsetEntry *rover;
    HashsetEntry *newentry;
    unsigned long long int index;
    int success = 0;

    pthread_spin_lock(&set->lock);
    {
        if ((set->entries << 1) >= set->table_size) {
            hashset_resize(set, set->table_size << 1);
        }

        index = DEFAULT_HASH_FUNC(key) % set->table_size;

        rover = set->table[index];

        while (rover != NULL) {
            if (DEFAULT_HASH_EQUALS_FUNC(rover->key, key) != 0) {
                success = 2;
                break;
            }
            rover = rover->next;
        }

        if (!success) {
            newentry = _hashset_get_entry(set);
            if (newentry != NULL) {
                newentry->key = key;
                newentry->next = set->table[index];
                set->table[index] = newentry;
                ++set->entries;
                success = 1;
            } else{
                int debug = 1;
            }
        }
    }
    pthread_spin_unlock(&set->lock);
    if (success != 1) {
        int debug = 1;
    }
    return success;
}


HashsetKey hashset_get(Hashset *set, HashsetKey key) {
    HashsetKey *ret = HASH_NULL;
    pthread_spin_lock(&set->lock);
    {
        HashsetEntry *rover = hashset_find_entry(set, key);
        if (rover)ret = rover->key;
    }
    pthread_spin_unlock(&set->lock);
    return ret;
}

HashsetEntry *hashset_find_entry(Hashset *set, HashsetKey key) {
    HashsetEntry *rover;
    unsigned long long int index;
    index = DEFAULT_HASH_FUNC(key) % set->table_size;
    rover = set->table[index];

    while (rover != NULL) {
        if (DEFAULT_HASH_EQUALS_FUNC(key, rover->key) != 0) {
            return rover;
        }
        rover = rover->next;
    }
    return HASH_NULL;
}

int hashset_remove(Hashset *set, HashsetKey key, int resize) {
    HashsetEntry *rover;
    HashsetEntry *pre;
    HashsetEntry *next;
    unsigned long long int index;
    unsigned int result;

    pthread_spin_lock(&set->lock);
    {
        if (resize && (set->entries << 3) < set->table_size) {
            hashset_resize(set, set->table_size >> 1);
        }

        index = DEFAULT_HASH_FUNC(key) % set->table_size;

        result = 0;
        rover = set->table[index];
        pre = rover;

        while (rover != NULL) {
            next = rover->next;
            if (DEFAULT_HASH_EQUALS_FUNC(key, rover->key) != 0) {
                if (pre == rover)set->table[index] = next;
                else pre->next = next;

                _hashset_free_entry(set, rover);
                --set->entries;
                result = 1;
                break;
            }
            pre = rover;
            rover = next;
        }
    }
    pthread_spin_unlock(&set->lock);
    if (key&&result != 1) {
        int debug = 1;
    }
    return result;
}

unsigned long long int hashset_num_entries(Hashset *set) {
    return set->entries;
}

void hashset_iterate(Hashset *set, HashsetIterator *iterator) {
    unsigned long long int chain;

    iterator->set = set;
    iterator->next_entry = NULL;
    for (chain = 0; chain < set->table_size; ++chain) {

        if (set->table[chain] != NULL) {
            iterator->next_entry = set->table[chain];
            iterator->curr_chain = iterator->next_chain = chain;
            break;
        }
    }
}

int hashset_iter_has_more(HashsetIterator *iterator) {
    return iterator->next_entry != NULL;
}


HashsetEntry *hashset_iter_next_entry(HashsetIterator *iterator) {
    Hashset *set;
    unsigned long long int chain;

    iterator->curr_entry = iterator->next_entry;
    iterator->curr_chain = iterator->next_chain;

    set = iterator->set;
    if (iterator->next_entry == NULL) {
        return HASH_NULL;
    }

    if (iterator->curr_entry->next != NULL) {
        iterator->next_entry = iterator->curr_entry->next;
    } else {
        chain = iterator->next_chain + 1;
        iterator->next_entry = NULL;
        while (chain < set->table_size) {
            if (set->table[chain] != NULL) {
                iterator->next_entry = set->table[chain];
                break;
            }
            ++chain;
        }
        iterator->next_chain = chain;
    }
    return iterator->curr_entry;
}

HashsetKey hashset_iter_next_key(HashsetIterator *iterator) {
    HashsetEntry *current_entry = hashset_iter_next_entry(iterator);
    if (current_entry)return current_entry->key;
    return HASH_NULL;
}

HashsetKey hashset_iter_remove(HashsetIterator *iterator) {
    HashsetKey key = HASH_NULL;
    if (iterator->curr_entry) {
        Hashset *set = iterator->set;
        pthread_spin_lock(&set->lock);
        {
            HashsetEntry *prev = NULL;
            HashsetEntry *rover = set->table[iterator->curr_chain];
            if (rover == iterator->curr_entry) {
                set->table[iterator->curr_chain] = iterator->curr_entry->next;
            } else {
                while (rover != iterator->curr_entry) {
                    prev = rover;
                    rover = rover->next;
                }
                prev->next = rover->next;
            }
            key = iterator->curr_entry->key;
            _hashset_free_entry(set, iterator->curr_entry);
            iterator->curr_entry = NULL;
            --set->entries;
        }
        pthread_spin_unlock(&set->lock);

    }
    return key;
}

int hashset_resize(Hashset *set, unsigned long long int size) {
    HashsetEntry **old_table;
    unsigned long long int old_table_size;
    HashsetEntry *rover;
    HashsetEntry *next;
    unsigned long long int index;
    unsigned int i;

    if (size != 0) {
        old_table = set->table;
        old_table_size = set->table_size;

        if (!hashset_allocate_table(set, (unsigned int) size)) {
            printf("CRITICAL: FAILED TO ALLOCATE HASH TABLE!\n");

            set->table = old_table;
            set->table_size = old_table_size;

            return 0;
        }
        for (i = 0; i < old_table_size; ++i) {
            rover = old_table[i];

            while (rover != NULL) {
                next = rover->next;
                index = DEFAULT_HASH_FUNC(rover->key) % set->table_size;
//                if (rover->next && rover->next == set->table[index]) {
//                    int debug = 1;
//                }
                rover->next = set->table[index];
                set->table[index] = rover;
                rover = next;
            }
        }
        jvm_free(old_table);
        printf("hashset resize:%lld\n",size);
    }

    return 1;
}


unsigned long long int hashset_count(Hashset *set) {
    HashsetEntry *rover;
    unsigned long long int i, count;
    count = 0;

    for (i = 0; i < set->table_size; i++) {
        rover = set->table[i];
        while (rover != NULL) {
            count++;
            rover = rover->next;
        }
    }

    return count;
}
