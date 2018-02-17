#ifndef MSGPACKRPCPROTOCOL_H
#define MSGPACKRPCPROTOCOL_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <msgpack.hpp>


template <class IStream, class OStream, class Handler>
class MsgpackRpcProtocol
{
public:
    enum class MessageType {Request = 1, Response = 2, Error = 3, Event = 4};

    IStream& m_istream;
    Handler& m_handler;
    msgpack::packer<OStream> m_packer;
    msgpack::unpacker m_unpacker;

    MsgpackRpcProtocol(IStream& istream, OStream& ostream, Handler& handler) :
        m_istream(istream), m_handler(handler), m_packer(ostream) {}

    void readAvailableBytes();

    template <typename T>
    void sendRequest(const std::string& method, const T& v, std::uint64_t id);

    template <typename T>
    void sendResponse(std::uint64_t id, const T& v);

    void sendError(std::uint64_t id, const std::string& errorstr);

    template <typename T>
    void sendEvent(const std::string& name, const T& v);
};


template <class IStream, class OStream, class Handler>
inline void MsgpackRpcProtocol<IStream, OStream, Handler>::readAvailableBytes() {
    // read available bytes from stream to unpacker
    auto n_avail = m_istream.bytesAvailable();
    m_unpacker.reserve_buffer(n_avail);
    auto n_read = m_istream.read(m_unpacker.buffer(), n_avail);
    m_unpacker.buffer_consumed(n_read);

    // deserialize msgpack objects from stream
    msgpack::unpacked unpacked;
    try {
        while (m_unpacker.next(unpacked)) {
            // message is array of objects
            std::vector<msgpack::object> message;
            unpacked.get().convert(message);
            // determine message type and call the corresponding handler
            MessageType type = static_cast<MessageType>(message.at(0).as<std::uint8_t>());
            switch (type) {
            case MessageType::Request:
                // request: (type=request, method, args, id)
                m_handler.handleRequest(message.at(1).as<std::string>(), message.at(2), message.at(3).as<std::uint64_t>());
                break;
            case MessageType::Response:
                // response: (type=response, id, result)
                m_handler.handleResponse(message.at(1).as<std::uint64_t>(), message.at(2));
                break;
            case MessageType::Error:
                // error: (type=error, id, error)
                m_handler.handleError(message.at(1).as<std::uint64_t>(), message.at(2).as<std::string>());
                break;
            case MessageType::Event:
                // event: (type=event, name, args)
                m_handler.handleEvent(message.at(1).as<std::string>(), message.at(2));
                break;
            default:
                break;
            }
        }
    } catch (msgpack::unpack_error&) {
        throw std::runtime_error("error in data stream");
    } catch (msgpack::type_error&) {
        throw std::runtime_error("error in data stream");
    }
}


template <class IStream, class OStream, class Handler>
template <typename T>
inline void MsgpackRpcProtocol<IStream, OStream, Handler>::sendRequest(const std::string& method, const T& v, std::uint64_t id) {
    m_packer.pack_array(4);
    m_packer.pack(static_cast<std::uint8_t>(MessageType::Request));
    m_packer.pack(method);
    m_packer.pack(v);
    m_packer.pack(id);
}


template <class IStream, class OStream, class Handler>
template <typename T>
inline void MsgpackRpcProtocol<IStream, OStream, Handler>::sendResponse(std::uint64_t id, const T& v) {
    m_packer.pack_array(3);
    m_packer.pack(static_cast<std::uint8_t>(MessageType::Response));
    m_packer.pack(id);
    m_packer.pack(v);
}


template <class IStream, class OStream, class Handler>
inline void MsgpackRpcProtocol<IStream, OStream, Handler>::sendError(std::uint64_t id, const std::string& errorstr) {
    m_packer.pack_array(3);
    m_packer.pack(static_cast<std::uint8_t>(MessageType::Error));
    m_packer.pack(id);
    m_packer.pack(errorstr);
}


template <class IStream, class OStream, class Handler>
template <typename T>
inline void MsgpackRpcProtocol<IStream, OStream, Handler>::sendEvent(const std::string& name, const T& v) {
    m_packer.pack_array(3);
    m_packer.pack(static_cast<std::uint8_t>(MessageType::Event));
    m_packer.pack(name);
    m_packer.pack(v);
}


#endif // MSGPACKRPCPROTOCOL_H
