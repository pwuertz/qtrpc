#pragma once

#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtQml/QJSValue>
#include <msgpack.hpp>

namespace msgpack {
struct QByteArrayBuffer
{
    QByteArrayBuffer() = default;
    operator QByteArray&() { return buffer; }
    operator const QByteArray&() const { return buffer; }
    QByteArray* operator->() { return &buffer; }
    void write(const char* data, const size_t sz)
    {
        buffer.append(data, static_cast<int>(sz));
    }
private:
    QByteArray buffer;
};

MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
    namespace adaptor {

    template<>
    struct pack<QVariant> {
        template <typename Stream>
        packer<Stream>& operator()(msgpack::packer<Stream>& o, QVariant const& v) const {
            const auto typeId = v.userType();
            switch (typeId) {
            case QMetaType::UnknownType:
            case QMetaType::Void:
                return o.pack_nil();
            case QMetaType::Bool:
                return o.pack(v.toBool());
            case QMetaType::Char:
            case QMetaType::Short:
            case QMetaType::Int:
                return o.pack(v.toInt());
            case QMetaType::UChar:
            case QMetaType::UShort:
            case QMetaType::UInt:
                return o.pack(v.toUInt());
            case QMetaType::Long:
            case QMetaType::LongLong:
                return o.pack(v.toLongLong());
            case QMetaType::ULong:
            case QMetaType::ULongLong:
                return o.pack(v.toULongLong());
            case QMetaType::Float:
            case QMetaType::Double:
                return o.pack(v.toDouble());
            case QMetaType::QVariantList:
            {
                const QList<QVariant> list = v.toList();
                o.pack_array(list.size());
                for (const auto& value: list) {
                    o.pack(value);
                }
                return o;
            }
            break;
            case QMetaType::QVariantMap:
                return o.pack(v.toMap());
            case QMetaType::QString:
                return o.pack(v.toString());
            case QMetaType::QStringList:
            {
                const QStringList list = v.toStringList();
                o.pack_array(list.size());
                for (const auto& value: list) {
                    o.pack(value);
                }
                return o;
            }
            case QMetaType::QByteArray:
                return o.pack(v.toByteArray());
            }
            // Additional runtime dependent types
            if (typeId == qMetaTypeId<QJSValue>()) {
                return o.pack(reinterpret_cast<const QJSValue*>(v.data())->toVariant());
            }
            // Failed to serialize value, print warning and pack null
            const char* typeName = QMetaType::typeName(v.userType());
            qWarning() << "qtmsgpackadaptor: no conversion for" << typeName;
            return o.pack_nil();
        }
    };

    template<>
    struct convert<QVariant> {
        msgpack::object const& operator()(msgpack::object const& o, QVariant& v) const {
            switch (o.type) {
            case msgpack::type::NIL:
                v.setValue(nullptr);
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
                for (unsigned int i = 0; i < o.via.array.size; ++i) {
                    list << o.via.array.ptr[i].as<QVariant>();
                }
                v.setValue(list);
            }
            break;
            case msgpack::type::MAP:
            {
                v.setValue(o.as<QVariantMap>());
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
            const QByteArray encoded = v.toUtf8();
            o.pack_str(encoded.size());
            o.pack_str_body(encoded.data(), encoded.size());
            return o;
        }
    };

    template<>
    struct convert<QString> {
        msgpack::object const& operator()(msgpack::object const& o, QString& v) const {
            if (o.type != msgpack::type::STR && o.type != msgpack::type::BIN) {
                throw msgpack::type_error();
            }
            v = QString::fromUtf8(o.via.str.ptr, static_cast<int>(o.via.str.size));  // deep copy for now
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
            if (o.type != msgpack::type::BIN) {
                throw msgpack::type_error();
            }
            v = QByteArray(o.via.bin.ptr, static_cast<int>(o.via.bin.size));  // deep copy for now
            return o;
        }
    };

    template<>
    struct pack<QVariantMap> {
        template <typename Stream>
        packer<Stream>& operator()(msgpack::packer<Stream>& o, const QVariantMap& map) const {
            QMap<QString, QVariant>::const_iterator it = map.constBegin();
            o.pack_map(map.size());
            while (it != map.constEnd()) {
                o.pack(it.key());
                o.pack(it.value());
                ++it;
            }
            return o;
        }
    };

    template<>
    struct convert<QVariantMap> {
        msgpack::object const& operator()(msgpack::object const& o, QVariantMap& map) const {
            if (o.type != msgpack::type::MAP) {
                throw msgpack::type_error();
            }
            for (unsigned int i = 0; i < o.via.map.size; ++i) {
                QString key(o.via.map.ptr[i].key.as<QString>());
                QVariant val(o.via.map.ptr[i].val.as<QVariant>());
                map.insert(key, val);
            }
            return o;
        }
    };

    }  // namespace adaptor
}  // namespace msgpack version
}  // namespace msgpack
