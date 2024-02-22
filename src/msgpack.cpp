module;

#include <msgpack.hpp>

export module msgpack;

export namespace pack {
enum { REQUEST = 0, RESPONSE = 1, NOTIFY = 2 };

template <typename... U>
auto pack_request(const std::string& method, const U&... u) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    std::uint32_t msgid_ = 0;
    pk.pack_array(4);

    pk.pack(static_cast<std::uint32_t>(REQUEST));
    pk.pack(msgid_++);
    pk.pack(method);

    pk.pack_array(sizeof...(u));
    int _[] = {(pk.pack(u), 0)...};

    return buffer;
    //
    // pk.pack_array(sizeof...(u));
    // detail::pack(pk, u...);
    //
    // msgpack::object_handle oh = msgpack::unpack(sbuf.data(), sbuf.size());
    //
    // msgpack::object deserialized = oh.get();
    //
    // std::cout << "sbuf = " << deserialized << std::endl;
    //
    // socket_.write(sbuf.data(), sbuf.size(), 5);
    //
    // msgpack::unpacker unpacker;
    // unpacker.reserve_buffer(32*1024ul);
    //
    // size_t rlen = socket_.read(unpacker.buffer(), unpacker.buffer_capacity(),
    // 5); msgpack::unpacked result; unpacker.buffer_consumed(rlen);
    //
    // /*
    // while(unpacker.next(result)) {
    //     const msgpack::object &obj = result.get();
    //     std::cout << "res = " << obj << std::endl;
    //     result.zone().reset();
    // }
    // */
    //
    // //TODO: full-state response handler should be implemented
    // unpacker.next(result);
    // const msgpack::object &obj = result.get();
    // std::cout << "res = " << obj << std::endl;
    // msgpack::type::tuple<int64_t, int64_t, Object, Object> dst;
    // obj.convert(dst);
    // return dst.get<3>();
}

auto unpack() {
    // deserializes these objects using msgpack::unpacker.
    msgpack::unpacker pac;

    // feeds the buffer.
    pac.reserve_buffer(1024);
    // memcpy(pac.buffer(), buffer.data(), buffer.size());
    // pac.buffer_consumed(buffer.size());
    // now starts streaming deserialization.
    msgpack::object_handle oh;
    while (pac.next(oh)) {
        // std::cout << oh.get() << std::endl;
    }
}

} // namespace pack

// this is to workaround a bug with modules and instantiate template here
static_assert(sizeof(pack::pack_request("test", 1, 2, 3)));
