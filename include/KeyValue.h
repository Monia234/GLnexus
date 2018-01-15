#ifndef GLNEXUS_KEYVALUE_H
#define GLNEXUS_KEYVALUE_H
#include "data.h"

namespace GLnexus {
namespace KeyValue {

/// Abstract interface to a database underlying BCFKeyValueData. The database
/// has one or more collections of key-value records. Each collection is
/// ordered by key.

using CollectionHandle = void*;

// A read-only slice of data in-memory, passed around to avoid copying.
struct Data {
    const char* data = nullptr;
    const size_t size = 0;

    Data(const char* data_, size_t size_) : data(data_), size(size_) {}
    Data(const char* cstr) : data(cstr), size(strlen(cstr)) {}
    Data(const std::string& s) : data(s.data()), size(s.size()) {}
    virtual ~Data() = default;

    std::string str() const {
        if (data) {
            return std::string(data, size);
        } else {
            return std::string();
        }
    };
};

/// In-order iterator over records in a collection. Not thread-safe.
class Iterator {
public:
    virtual ~Iterator() = default;

    // Is the iterator positioned at a key/value pair?
    virtual bool valid() const = 0;

    // If valid(), get the current key. The resulting buffer shall remain
    // available until next() is invoked or the iterator is destroyed.
    virtual Data key() const = 0;

    // If valid(), get the current value. The resulting buffer shall remain
    // available until next() is invoked or the iterator is destroyed.
    virtual Data value() const = 0;

    // Advance the iterator to the next key/value pair. At the end of the
    // collection, next() will return OK, but valid() will be false.
    //
    // If next() returns a bad status, any further operations on the iterator
    // have undefined results.
    virtual Status next() = 0;

};

/// A DB snapshot providing consistent multiple reads if possible. Thread-safe.
class Reader {
public:
    virtual ~Reader() = default;

    /// Get the value corresponding to the key and return OK. Return NotFound
    /// if no corresponding record exists in the collection, or any error code.
    /// The output buffer will remain available at least until the shared_ptr
    /// is released.
    virtual Status get0(CollectionHandle coll, const std::string& key,
                        std::shared_ptr<Data>& value) const = 0;

    // get and copy the value into a std::string
    virtual Status get(CollectionHandle coll, const std::string& key,
                       std::string& value) const {
        std::shared_ptr<Data> v;
        Status s = get0(coll, key, v);
        if (s.ok()) {
            value = v->str();
        }
        return s;
    }

    /// Create an iterator positioned at the first key equal to or greater
    /// than the given one. If key is empty then position at the beginning of
    /// the collection.
    ///
    /// If there are no extant keys equal to or greater than the given one,
    /// the return status will be OK but it->valid() will be false.
    virtual Status iterator(CollectionHandle coll, const std::string& key,
                            std::unique_ptr<Iterator>& it) const = 0;
};

/// A batch of writes to apply atomically if possible. Thread-safe until
/// commit.
class WriteBatch {
public:
    virtual ~WriteBatch() = default;

    virtual Status put(CollectionHandle coll, const std::string& key, const Data& value) = 0;
    //virtual Status delete(Collection* coll, const std::string& key) = 0;

    /// Apply a batch of writes.
    virtual Status commit() = 0;
};

/// Main database interface for retrieving collection handles, generating
/// snapshopts to read from, and creating and applying write batches. The DB
/// object itself implements the Reader interface (with no consistency
/// guarantees between multiple calls) and the WriteBatch interface (which
/// applies one write immediately, no atomicity guarantees between multiple
/// calls). Caller must ensure that the parent DB object still exists when any
/// Reader or WriteBatch object is used. Thread-safe.
class DB : public Reader {
public:
    virtual ~DB() = default;

    /// Get the handle to a collection, or return NotFound.
    virtual Status collection(const std::string& name, CollectionHandle& coll) const = 0;

    /// Create a new collection, or return Exists.
    virtual Status create_collection(const std::string& name) = 0;

    /// Get an up-to-date snapshot.
    virtual Status current(std::unique_ptr<Reader>& snapshot) const = 0;

    /// Begin preparing a batch of writes.
    virtual Status begin_writes(std::unique_ptr<WriteBatch>& writes) = 0;

    // Base implementations of Reader and WriteBatch interfaces. They simply
    // create a snapshot just to read one record (or begin one iterator), or
    // apply a "batch" of one write. Derived classes may want to provide more
    // efficient overrides.
    Status get0(CollectionHandle coll, const std::string& key, std::shared_ptr<Data>& value) const override;
    Status iterator(CollectionHandle coll, const std::string& key, std::unique_ptr<Iterator>& it) const override;
    virtual Status put(CollectionHandle coll, const std::string& key, const Data& value);

    /// Ensure all writes are flushed to storage
    virtual Status flush() = 0;
};

}}

#endif
