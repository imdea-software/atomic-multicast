#ifndef _HTABLE_H_
#define _HTABLE_H_

#include <gmodule.h>

typedef void ht_key_t;
typedef void ht_val_t;
typedef int (*ht_keyequ_fun)(ht_key_t *, ht_key_t *);

typedef GHashTable htable_t;

#define htable_init(keyhash_fun, keyequ_fun) \
    g_hash_table_new((GHashFunc) (keyhash_fun), (GEqualFunc) (keyequ_fun))

#define htable_free(ht) \
    g_hash_table_destroy((ht))

#define htable_insert(ht, key, val) \
    g_hash_table_insert((ht), (key), (val))

#define htable_remove(ht, key) \
    g_hash_table_remove((ht), (key))

#define htable_lookup(ht, key) \
    g_hash_table_lookup((ht), (key))

#define htable_lookup_extended(ht, key, ret_key, ret_val) \
    g_hash_table_lookup_extended((ht), (key), (ret_key), (ret_val))

#define htable_foreach(ht, fun, arg) \
    g_hash_table_foreach((ht), (fun), (arg))

#endif
