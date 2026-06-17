#ifndef _ROCKX_CONTAINER_H_
#define _ROCKX_CONTAINER_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Typedef for a RockXArrayList.
 *
 */
typedef void* RockXArrayList;

/**
 * @brief Typedef for a RockXLinkList.
 *
 */
typedef void* RockXLinkList;

/**
 * @brief Typedef for a RockXMap.
 *
 */
typedef void* RockXMap;

/**
 * @brief Typedef for a RockXQueue.
 *
 */
typedef void* RockXQueue;

/**
 * @brief Typedef for a RockXTrie.
 *
 */
typedef void* RockXTrie;

/**
 * @brief Typedef for a RockXTrie.
 *
 */
typedef void* RockXElement;

/**
 * @brief Typedef for a RockXEntry.
 *
 */
typedef void* RockXEntry;

/**
 * @brief Typedef for a RockXIterator.
 *
 */
typedef void* RockXIterator;

/**
 * @brief Typedef for a RockXMapKey.
 *
 */
typedef void* RockXMapKey;

/**
 * @brief Typedef for a RockXMapValue.
 *
 */
typedef void* RockXMapValue;

/**
 * @brief Typedef for a RockXIntPairs.
 *
 */
typedef struct IntPairs
{
    int first;
    int second;
} IntPairs;

/**
 * @brief Typedef for a RockXIntArray.
 *
 */
typedef struct IntArray
{
    int capacity;
    int size;
    int data[0];
} IntArray;

/**
 * @brief Typedef for a RockXFloatArray.
 *
 */
typedef struct FloatArray
{
    int capacity;
    int size;
    float data[0];
} FloatArray;

/**
 * @brief Define the size of a RockXFloatArray.
 *
 * @param capacity Capacity of the float array
 * @return size_t Buffer size of the float array
 */
#define ROCKX_FLOAT_ARRAY_SIZE(capacity) (sizeof(float) * capacity + sizeof(FloatArray))

/**
 * @brief Define the size of a RockXIntArray.
 *
 * @param capacity Capacity of the int array
 * @return size_t Buffer size of the int array
 */
#define ROCKX_INT_ARRAY_SIZE(capacity) (sizeof(int) * capacity + sizeof(IntArray))

/**
 * @brief Create a RockXIntArray.
 *
 * @param capacity Capacity of the int array
 * @return IntArray* Int array
 */
IntArray* RockXIntArrayCreate(int capacity);

/**
 * @brief Destroy a RockXIntArray.
 *
 * @param array Int array
 */
void RockXIntArrayDestroy(IntArray* array);

/**
 * @brief Create a RockXFloatArray.
 *
 * @param capacity Capacity of the float array
 * @return FloatArray* Float array
 */
FloatArray* RockXFloatArrayCreate(int capacity);

/**
 * @brief Destroy a RockXFloatArray.
 *
 * @param array Float array
 */
void RockXFloatArrayDestroy(FloatArray* array);

/**
 * @brief Typedef for a function pointer that compares two RockXElement values.
 *
 * This function pointer type is used as a parameter in various container-related functions
 * to specify a custom comparison function for sorting or searching elements.
 *
 * @param value1 The first RockXElement value to compare.
 * @param value2 The second RockXElement value to compare.
 * @return An integer value indicating the comparison result:
 *         - -1 value if value1 is less than value2.
 *         - 0 if value1 is equal to value2.
 *         - 1 value if value1 is greater than value2.
 */
typedef int (*RockXElementCompareFunc)(RockXElement value1, RockXElement value2);

/**
 * @brief Function pointer type for comparing two RockXElement values.
 *
 * This function pointer type is used to define a custom comparison function for RockXElement values.
 * The comparison function should return an integer value indicating the equality of the two values.
 * If the values are equal, the function should return 0. If the values are not equal, the function
 * should return a non-zero value.
 *
 * @param value1 The first RockXElement value to compare.
 * @param value2 The second RockXElement value to compare.
 * @return An integer value indicating the equality of the two values.
 */
typedef int (*RockXElementEqualFunc)(RockXElement value1, RockXElement value2);

/**
 * @brief Function pointer type for freeing a RockXElement value.
 *
 * This function pointer type is used to define a custom free function for RockXElement values.
 * The free function should release any resources allocated for the value.
 *
 * @param value The RockXElement value to free.
 */
typedef void (*RockXElementFreeFunc)(RockXElement value);

/**
 * @brief Compares two RockXElement pointers.
 *
 * This function compares the values of two RockXElement pointers and returns an integer value indicating their
 * relationship.
 *
 * @param value1 The first RockXElement pointer to compare.
 * @param value2 The second RockXElement pointer to compare.
 * @return An integer value indicating the relationship between the two pointers:
 *         - 0 if the pointers are equal.
 *         - A negative value if value1 is less than value2.
 *         - A positive value if value1 is greater than value2.
 */
int RockXComparePointer(RockXElement value1, RockXElement value2);

/**
 * @brief Compares two strings.
 *
 * This function compares two strings and returns an integer value indicating their relative order.
 *
 * @param string1 The first string to compare.
 * @param string2 The second string to compare.
 * @return An integer value indicating the comparison result:
 *         - 0 if the strings are equal.
 *         - A negative value if `string1` is less than `string2`.
 *         - A positive value if `string1` is greater than `string2`.
 */
int RockXCompareString(RockXElement string1, RockXElement string2);

/**
 * @brief Compares two integers.
 *
 * This function compares two integers and returns the result of the comparison.
 *
 * @param int1 The first integer to compare.
 * @param int2 The second integer to compare.
 * @return A negative value if int1 is less than int2, 0 if they are equal, or a positive value if int1 is greater than
 * int2.
 */
int RockXCompareInt(RockXElement int1, RockXElement int2);

/**
 * @brief Checks if two RockXElement pointers are equal.
 *
 * This function compares the values of two RockXElement pointers and returns
 * 1 if they are equal, and 0 otherwise.
 *
 * @param value1 The first RockXElement pointer to compare.
 * @param value2 The second RockXElement pointer to compare.
 * @return 1 if the two pointers are equal, 0 otherwise.
 */
int RockXEqualPointer(RockXElement value1, RockXElement value2);

/**
 * @brief Compares two strings for equality.
 *
 * This function compares two strings, `string1` and `string2`, for equality.
 *
 * @param string1 The first string to compare.
 * @param string2 The second string to compare.
 * @return Returns 1 if the strings are equal, 0 otherwise.
 */
int RockXEqualString(RockXElement string1, RockXElement string2);

/**
 * @brief Compares two RockXElement integers for equality.
 *
 * This function compares the values of two RockXElement integers and returns
 * 1 if they are equal, and 0 otherwise.
 *
 * @param int1 The first RockXElement integer to compare.
 * @param int2 The second RockXElement integer to compare.
 * @return 1 if the integers are equal, 0 otherwise.
 */
int RockXEqualInt(RockXElement int1, RockXElement int2);

/**
 * @brief Compute the hash value of an integer.
 *
 * @param value Pointer to the integer value
 * @return unsigned long Hash value of the integer
 */
unsigned long RockXHashInt(RockXElement value);

/**
 * @brief Compute the hash value of a string.
 *
 * @param value Pointer to the string
 * @return unsigned long Hash value of the string
 */
unsigned long RockXHashString(RockXElement value);

/**
 * @brief Compute the hash value of a string without case sensitivity.
 *
 * @param value Pointer to the string
 * @return unsigned long Hash value of the string
 */
unsigned long RockXHashStringNoCase(RockXElement value);

/**
 * @brief Compute the hash value of a pointer.
 *
 * @param value Pointer
 * @return unsigned long Hash value of the pointer
 */
unsigned long RockXHashPointer(RockXElement value);

/**
 * @brief Creates a new RockXArrayList with the specified capacity.
 *
 * @param capacity The initial capacity of the RockXArrayList.
 * @return The newly created RockXArrayList.
 */
RockXArrayList RockXArrayListCreate(size_t capacity);

/**
 * @brief Destroys a RockXArrayList.
 *
 * @param list The RockXArrayList to destroy.
 */
void RockXArrayListDestroy(RockXArrayList list);

/**
 * @brief Appends an element to the end of a RockXArrayList.
 *
 * @param list The RockXArrayList to append to.
 * @param data The element to append.
 * @return 0 on success, -1 on failure.
 */
int RockXArrayListAppend(RockXArrayList list, RockXElement data);

/**
 * @brief Prepends an element to the beginning of a RockXArrayList.
 *
 * @param list The RockXArrayList to prepend to.
 * @param data The element to prepend.
 * @return 0 on success, -1 on failure.
 */
int RockXArrayListPrepend(RockXArrayList list, RockXElement data);

/**
 * @brief Inserts an element at a specified index in a RockXArrayList.
 *
 * @param list The RockXArrayList to insert into.
 * @param index The index at which to insert the element.
 * @param data The element to insert.
 * @return 0 on success, -1 on failure.
 */
int RockXArrayListInsert(RockXArrayList list, size_t index, RockXElement data);

/**
 * @brief Removes an element at a specified index from a RockXArrayList.
 *
 * @param list The RockXArrayList to remove from.
 * @param index The index of the element to remove.
 * @return 0 on success, -1 on failure.
 */
int RockXArrayListRemove(RockXArrayList list, size_t index);

/**
 * @brief Removes a range of elements from a RockXArrayList.
 *
 * @param list The RockXArrayList to remove from.
 * @param start The index of the first element to remove.
 * @param end The index of the last element to remove.
 * @return 0 on success, -1 on failure.
 */
int RockXArrayListRemoveRange(RockXArrayList list, size_t start, size_t end);

/**
 * @brief Gets the element at a specified index in a RockXArrayList.
 *
 * @param list The RockXArrayList to get from.
 * @param index The index of the element to get.
 * @return The element at the specified index.
 */
RockXElement RockXArrayListGet(RockXArrayList list, size_t index);

/**
 * @brief Sets the element at a specified index in a RockXArrayList.
 *
 * @param list The RockXArrayList to set in.
 * @param index The index of the element to set.
 * @param data The element to set.
 * @return 0 on success, -1 on failure.
 */
int RockXArrayListSet(RockXArrayList list, size_t index, RockXElement data);

/**
 * @brief Finds the index of an element in a RockXArrayList using a custom comparison function.
 *
 * @param list The RockXArrayList to search in.
 * @param compare The comparison function to use.
 * @param data The element to find.
 * @return The index of the element if found, or -1 if not found.
 */
size_t RockXArrayListIndexOf(RockXArrayList list, RockXElementEqualFunc compare, RockXElement data);

/**
 * @brief Gets the size of a RockXArrayList.
 *
 * @param list The RockXArrayList to get the size of.
 * @return The size of the RockXArrayList.
 */
size_t RockXArrayListSize(RockXArrayList list);

/**
 * @brief Clears a RockXArrayList.
 *
 * @param list The RockXArrayList to clear.
 */
void RockXArrayListClear(RockXArrayList list);

/**
 * @brief Sorts a RockXArrayList using a custom comparison function.
 *
 * @param list The RockXArrayList to sort.
 * @param compare The comparison function to use.
 */
void RockXArrayListSort(RockXArrayList list, RockXElementCompareFunc compare);

/**************************/

/**
 * @brief Appends an element to the end of a RockXLinkList.
 *
 * @param list The RockXLinkList to append to.
 * @param data The element to append.
 * @return The entry of the appended element.
 */
RockXEntry RockXLinkListAppend(RockXLinkList* list, RockXElement data);

/**
 * @brief Prepends an element to the beginning of a RockXLinkList.
 *
 * @param list The RockXLinkList to prepend to.
 * @param data The element to prepend.
 * @return The entry of the prepended element.
 */
RockXEntry RockXLinkListPrepend(RockXLinkList* list, RockXElement data);

/**
 * @brief Destroys a RockXLinkList.
 *
 * @param list The RockXLinkList to destroy.
 */
void RockXLinkListDestroy(RockXLinkList list);

/**
 * @brief Gets the previous entry in a RockXLinkList.
 *
 * @param entry The entry to get the previous entry from.
 * @return The previous entry.
 */
RockXEntry RockXLinkListPrev(RockXEntry entry);

/**
 * @brief Gets the next entry in a RockXLinkList.
 *
 * @param entry The entry to get the next entry from.
 * @return The next entry.
 */
RockXEntry RockXLinkListNext(RockXEntry entry);

/**
 * @brief Gets the first entry in a RockXLinkList.
 *
 * @param list The RockXLinkList to get the first entry from.
 * @return The first entry.
 */
RockXEntry RockXLinkListGetEntry(RockXLinkList list);

/**
 * @brief Gets the data of an entry in a RockXLinkList.
 *
 * @param entry The entry to get the data from.
 * @return The data of the entry.
 */
RockXElement RockXLinkListGetData(RockXEntry entry);

/**
 * @brief Gets the entry at a specified index in a RockXLinkList.
 *
 * @param list The RockXLinkList to get the entry from.
 * @param index The index of the entry to get.
 * @return The entry at the specified index.
 */
RockXElement RockXLinkListGetEntryAt(RockXLinkList list, size_t index);

/**
 * @brief Gets the data at a specified index in a RockXLinkList.
 *
 * @param list The RockXLinkList to get the data from.
 * @param index The index of the data to get.
 * @return The data at the specified index.
 */
RockXElement RockXLinkListGetDataAt(RockXLinkList list, size_t index);

/**
 * @brief Gets the size of a RockXLinkList.
 *
 * @param list The RockXLinkList to get the size of.
 * @return The size of the RockXLinkList.
 */
int RockXLinkListSize(RockXLinkList list);

/**
 * @brief Removes an entry from a RockXLinkList.
 *
 * @param list The RockXLinkList to remove from.
 * @param entry The entry to remove.
 * @return 0 on success, -1 on failure.
 */
int RockXLinkListRemove(RockXLinkList* list, RockXEntry entry);

/**
 * @brief Removes an element from a RockXLinkList using a custom comparison function.
 *
 * @param list The RockXLinkList to remove from.
 * @param compare The comparison function to use.
 * @param data The element to remove.
 * @return 0 on success, -1 on failure.
 */
int RockXLinkListRemoveData(RockXLinkList* list, RockXElementEqualFunc compare, RockXElement data);

/**
 * @brief Sorts a RockXLinkList using a custom comparison function.
 *
 * @param list The RockXLinkList to sort.
 * @param compare The comparison function to use.
 */
void RockXLinkListSort(RockXLinkList* list, RockXElementCompareFunc compare);

/**
 * @brief Finds an entry in a RockXLinkList using a custom comparison function.
 *
 * @param list The RockXLinkList to search in.
 * @param compare The comparison function to use.
 * @param data The element to find.
 * @return The entry of the found element.
 */
RockXEntry RockXLinkListFindData(RockXLinkList list, RockXElementEqualFunc compare, RockXElement data);

/**
 * @brief Creates an iterator for a RockXLinkList.
 *
 * @param list The RockXLinkList to create an iterator for.
 * @param iterator The iterator to create.
 */
void RockXLinkListCreateIterator(RockXLinkList* list, RockXIterator* iterator);

/**
 * @brief Destroys an iterator for a RockXLinkList.
 *
 * @param iterator The iterator to destroy.
 */
void RockXLinkListDestroyIterator(RockXIterator iterator);

/**
 * @brief Checks if an iterator has a next element.
 *
 * @param iterator The iterator to check.
 * @return 1 if the iterator has a next element, 0 otherwise.
 */
int RockXLinkListIteratorHasNext(RockXIterator iterator);

/**
 * @brief Gets the next element from an iterator.
 *
 * @param iterator The iterator to get the next element from.
 * @return The next element.
 */
RockXElement RockXLinkListIteratorNext(RockXIterator iterator);

/**************************/

/**
 * @brief Creates a new RockXQueue.
 *
 * @return The newly created RockXQueue.
 */
RockXQueue RockXQueueCreate();

/**
 * @brief Destroys a RockXQueue.
 *
 * @param queue The RockXQueue to destroy.
 */
void RockXQueueDestroy(RockXQueue queue);

/**
 * @brief Pushes an element to the head of a RockXQueue.
 *
 * @param queue The RockXQueue to push to.
 * @param data The element to push.
 * @return 0 on success, -1 on failure.
 */
int RockXQueuePushHead(RockXQueue queue, RockXElement data);

/**
 * @brief Pops an element from the head of a RockXQueue.
 *
 * @param queue The RockXQueue to pop from.
 * @return The element popped from the head of the RockXQueue.
 */
RockXElement RockXQueuePopHead(RockXQueue queue);

/**
 * @brief Peeks the element at the head of a RockXQueue.
 *
 * @param queue The RockXQueue to peek from.
 * @return The element at the head of the RockXQueue.
 */
RockXElement RockXQueuePeekHead(RockXQueue queue);

/**
 * @brief Pushes an element to the tail of a RockXQueue.
 *
 * @param queue The RockXQueue to push to.
 * @param data The element to push.
 * @return 0 on success, -1 on failure.
 */
int RockXQueuePushTail(RockXQueue queue, RockXElement data);

/**
 * @brief Pops an element from the tail of a RockXQueue.
 *
 * @param queue The RockXQueue to pop from.
 * @return The element popped from the tail of the RockXQueue.
 */
RockXElement RockXQueuePopTail(RockXQueue queue);

/**
 * @brief Peeks the element at the tail of a RockXQueue.
 *
 * @param queue The RockXQueue to peek from.
 * @return The element at the tail of the RockXQueue.
 */
RockXElement RockXQueuePeekTail(RockXQueue queue);

/**
 * @brief Checks if a RockXQueue is empty.
 *
 * @param queue The RockXQueue to check.
 * @return 1 if the RockXQueue is empty, 0 otherwise.
 */
int RockXQueueIsEmpty(RockXQueue queue);

/**
 * @brief Finds an element in a RockXQueue using a custom comparison function.
 *
 * @param queue The RockXQueue to search in.
 * @param compare The comparison function to use.
 * @param data The element to find.
 * @return The element found in the RockXQueue.
 */
RockXElement RockXQueueFindData(RockXQueue queue, RockXElementEqualFunc compare, RockXElement data);

/**************************/

/**
 * @brief The hash function type for a RockXMap.
 *
 * @param value The value to hash.
 * @return The hash value.
 */
typedef unsigned long (*RockXMapHashFunc)(RockXMapKey value);

/**
 * @brief Creates a new RockXMap.
 *
 * @param hashFunc The hash function to use.
 * @param compare The comparison function to use.
 * @return The newly created RockXMap.
 */
RockXMap RockXMapCreate(RockXMapHashFunc hashFunc, RockXElementEqualFunc compare);

/**
 * @brief Destroys a RockXMap.
 *
 * @param map The RockXMap to destroy.
 */
void RockXMapDestroy(RockXMap map);

/**
 * @brief Sets the free function for a RockXMap.
 *
 * @param map The RockXMap to set the free function for.
 * @param keyFreeFunc The free function for the keys.
 * @param valueFreeFunc The free function for the values.
 */
void RockXMapSetFreeFunction(RockXMap map, RockXElementFreeFunc keyFreeFunc, RockXElementFreeFunc valueFreeFunc);

/**
 * @brief Puts a key-value pair into a RockXMap.
 *
 * @param map The RockXMap to put the key-value pair into.
 * @param key The key to put.
 * @param value The value to put.
 * @return 0 on success, -1 on failure.
 */
int RockXMapPut(RockXMap map, RockXMapKey key, RockXMapValue value);

/**
 * @brief Gets the value associated with a key in a RockXMap.
 *
 * @param map The RockXMap to get the value from.
 * @param key The key to get the value for.
 * @return The value associated with the key.
 */
RockXMapValue RockXMapGet(RockXMap map, RockXMapKey key);

/**
 * @brief Removes a key-value pair from a RockXMap.
 *
 * @param map The RockXMap to remove the key-value pair from.
 * @param key The key to remove.
 * @return 0 on success, -1 on failure.
 */
int RockXMapRemove(RockXMap map, RockXMapKey key);

/**
 * @brief Gets the size of a RockXMap.
 *
 * @param map The RockXMap to get the size of.
 * @return The size of the RockXMap.
 */
size_t RockXMapSize(RockXMap map);

/**
 * @brief Creates an iterator for a RockXMap.
 *
 * @param map The RockXMap to create an iterator for.
 * @param iterator The iterator to create.
 */
void RockXMapCreateIterator(RockXMap map, RockXIterator* iterator);

/**
 * @brief Destroys an iterator for a RockXMap.
 *
 * @param iterator The iterator to destroy.
 */
void RockXMapDestroyIterator(RockXIterator iterator);

/**
 * @brief Checks if an iterator has a next element.
 *
 * @param iterator The iterator to check.
 * @return 1 if the iterator has a next element, 0 otherwise.
 */
int RockXMapIteratorHasNext(RockXIterator iterator);

/**
 * @brief Gets the next element from an iterator.
 *
 * @param iterator The iterator to get the next element from.
 * @param key The key of the next element.
 * @param value The value of the next element.
 * @return 0 on success, -1 on failure.
 */
int RockXMapIteratorNext(RockXIterator iterator, RockXMapKey* key, RockXMapValue* value);

/**************************/

/**
 * @brief Creates a new RockXTrie.
 *
 * @return The newly created RockXTrie.
 */
RockXTrie RockXTrieCreate();

/**
 * @brief Destroys a RockXTrie.
 *
 * @param trie The RockXTrie to destroy.
 * @param freeFunc The free function to use.
 */
void RockXTrieDestroy(RockXTrie trie, RockXElementFreeFunc freeFunc);

/**
 * @brief Inserts a key-value pair into a RockXTrie.
 *
 * @param trie The RockXTrie to insert the key-value pair into.
 * @param key The key to insert.
 * @param value The value to insert.
 * @return 0 on success, -1 on failure.
 */
int RockXTrieInsert(RockXTrie trie, const char* key, RockXElement value);

/**
 * @brief Removes a key-value pair from a RockXTrie.
 *
 * @param trie The RockXTrie to remove the key-value pair from.
 * @param key The key to remove.
 * @return 0 on success, -1 on failure.
 */
int RockXTrieRemove(RockXTrie trie, const char* key);

/**
 * @brief Looks up a key in a RockXTrie.
 *
 * @param trie The RockXTrie to look up the key in.
 * @param key The key to look up.
 * @return The value associated with the key.
 */
RockXElement RockXTrieLookup(RockXTrie trie, const char* key);

/**
 * @brief Gets the size of a RockXTrie.
 *
 * @param trie The RockXTrie to get the size of.
 * @return The size of the RockXTrie.
 */
int RockXTrieSize(RockXTrie trie);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif