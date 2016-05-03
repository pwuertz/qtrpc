#ifndef QTMSGPACKADAPTOR_H
#define QTMSGPACKADAPTOR_H

#include <QVariant>
#include <QString>
#include <QByteArray>
#include <msgpack.hpp>

namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
namespace adaptor {

template<>
struct pack<QVariant> {
    template <typename Stream>
    packer<Stream>& operator()(msgpack::packer<Stream>& o, QVariant const& v) const {
        // packing member variables as an array.
        switch (v.type()) {
        case QVariant::Type::Invalid:
            o.pack_nil();
            break;
        case QVariant::Type::Bool:
            o.pack(v.toBool());
            break;
        case QVariant::Type::Int:
            o.pack(v.toInt());
            break;
        case QVariant::Type::UInt:
            o.pack(v.toUInt());
            break;
        case QVariant::Type::LongLong:
            o.pack(v.toLongLong());
            break;
        case QVariant::Type::ULongLong:
            o.pack(v.toULongLong());
            break;
        case QVariant::Type::Double:
            o.pack(v.toDouble());
            break;
        case QVariant::Type::List:
        {
            QList<QVariant> list = v.toList();
            o.pack_array(list.size());
            for (auto i = list.begin(); i != list.end(); ++i)
                o.pack(*i);
        }
            break;
        case QVariant::Type::String:
            o.pack(v.toString());
            break;
        case QVariant::Type::StringList:
        {
            QList<QString> list = v.toStringList();
            o.pack_array(list.size());
            for (auto i = list.begin(); i != list.end(); ++i)
                o.pack(*i);
        }
            break;
        case QVariant::Type::ByteArray:
            o.pack(v.toByteArray());
            break;
        default:
            o.pack_nil();
            break;
        }
        return o;
    }
};

template<>
struct convert<QVariant> {
    msgpack::object const& operator()(msgpack::object const& o, QVariant& v) const {

        switch (o.type) {
        case msgpack::type::NIL:
            v = QVariant();
            break;
        case msgpack::type::BOOLEAN:
            v.setValue(o.as<bool>());
            break;
        case msgpack::type::FLOAT:
            v.setValue(o.as<double>());
            break;
        case msgpack::type::POSITIVE_INTEGER:
            v.setValue(o.as<std::uint64_t>());
            break;
        case msgpack::type::NEGATIVE_INTEGER:
            v.setValue(o.as<std::int64_t>());
            break;
        case msgpack::type::STR:
            v.setValue(o.as<QString>());
            break;
        case msgpack::type::BIN:
            v.setValue(o.as<QByteArray>());
            break;
        case msgpack::type::ARRAY:
        {
            QVariantList list;
            for (unsigned int i = 0; i < o.via.array.size; ++i)
                list << o.via.array.ptr[i].as<QVariant>();
            v.setValue(list);
        }
            break;
        default:
            break;
        }
        return o;
    }
};

template<>
struct pack<QString> {
    template <typename Stream>
    packer<Stream>& operator()(msgpack::packer<Stream>& o, QString const& v) const {
        QByteArray encoded = v.toUtf8();
        o.pack_str(encoded.size());
        o.pack_str_body(encoded.data(), encoded.size());
        return o;
    }
};

template<>
struct convert<QString> {
    msgpack::object const& operator()(msgpack::object const& o, QString& v) const {
        if (o.type != msgpack::type::STR)
            throw msgpack::type_error();
        v = QString::fromUtf8(o.via.str.ptr, o.via.str.size);  // deep copy for now
        return o;
    }
};

template<>
struct pack<QByteArray> {
    template <typename Stream>
    packer<Stream>& operator()(msgpack::packer<Stream>& o, QByteArray const& v) const {
        o.pack_bin(v.size());
        o.pack_bin_body(v.data(), v.size());
        return o;
    }
};

template<>
struct convert<QByteArray> {
    msgpack::object const& operator()(msgpack::object const& o, QByteArray& v) const {
        if (o.type != msgpack::type::BIN)
            throw msgpack::type_error();
        v = QByteArray(o.via.bin.ptr, o.via.bin.size);  // deep copy for now
        return o;
    }
};

}
}
}

#endif // QTMSGPACKADAPTOR_H
