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
        switch (static_cast<QMetaType::Type>(v.type())) {
        case QMetaType::UnknownType:
        case QMetaType::Void:
            o.pack_nil();
            break;
        case QMetaType::Bool:
            o.pack(v.toBool());
            break;
        case QMetaType::Char:
        case QMetaType::Short:
        case QMetaType::Int:
            o.pack(v.toInt());
            break;
        case QMetaType::UChar:
        case QMetaType::UShort:
        case QMetaType::UInt:
            o.pack(v.toUInt());
            break;
        case QMetaType::Long:
        case QMetaType::LongLong:
            o.pack(v.toLongLong());
            break;
        case QMetaType::ULong:
        case QMetaType::ULongLong:
            o.pack(v.toULongLong());
            break;
        case QMetaType::Float:
        case QMetaType::Double:
            o.pack(v.toDouble());
            break;
        case QMetaType::QVariantList:
        {
            QList<QVariant> list = v.toList();
            o.pack_array(list.size());
            for (auto i = list.begin(); i != list.end(); ++i)
                o.pack(*i);
        }
            break;
        case QMetaType::QVariantMap:
        {
            QMap<QString, QVariant> map = v.toMap();
            QMap<QString, QVariant>::const_iterator it = map.constBegin();
            o.pack_map(map.size());
            while (it != map.constEnd()) {
                o.pack(it.key());
                o.pack(it.value());
                ++it;
            }
        }
            break;
        case QMetaType::QString:
            o.pack(v.toString());
            break;
        case QMetaType::QStringList:
        {
            QList<QString> list = v.toStringList();
            o.pack_array(list.size());
            for (auto i = list.begin(); i != list.end(); ++i)
                o.pack(*i);
        }
            break;
        case QMetaType::QByteArray:
            o.pack(v.toByteArray());
            break;
        default:
        {
            auto tname = QMetaType::typeName(static_cast<QMetaType::Type>(v.type()));
            qWarning() << "qtmsgpackadaptor: no conversion for" << tname;
        }
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
            v.setValue(o.as<qulonglong>());
            break;
        case msgpack::type::NEGATIVE_INTEGER:
            v.setValue(o.as<qlonglong>());
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
        case msgpack::type::MAP:
        {
            QVariantMap map;
            for (unsigned int i = 0; i < o.via.map.size; ++i) {
                QString key(o.via.map.ptr[i].key.as<QString>());
                QVariant val(o.via.map.ptr[i].val.as<QVariant>());
                map.insert(key, val);
            }
            v.setValue(map);
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
