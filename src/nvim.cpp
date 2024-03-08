#include "nvim.hpp"
#include "rpc.hpp"

#include <fmt/format.h>
#include <range/v3/all.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <type_traits>

namespace nvim {

template <typename T, typename Func>
inline auto transform(const T& from, Func&& f) {
    auto r = ranges::views::transform(from, std::move(f));
    return std::vector<decltype(f(*from.begin()))>{r.begin(), r.end()};
}

struct LuaVisitor : boost::static_visitor<void> {
    std::ostream& s_;
    mutable bool is_value_{false};

    LuaVisitor(std::ostream& os)
        : s_{os} {}

    void operator()(uint64_t v) const {
        s_ << v;
    }

    void operator()(const msgpack::type::raw_ref& v) const {
        s_ << v.str();
    }

    void operator()(const std::string& v) const {
        if (is_value_) {
            s_ << '"' << v << '"';
        } else {
            s_ << v;
        }
    }

    void operator()(const std::vector<std::string>& v) const {
        s_ << "{";
        for (auto& e : v) {
            s_ << '"' << e << '"' << ",";
        }
        s_ << "}";
    }

    void operator()(const std::vector<msgpack::type::variant>& v) const {
        s_ << "{";
        for (auto& e : v) {
            is_value_ = true;
            boost::apply_visitor(*this, e);
            s_ << ",";
        }
        s_ << "}";
    }

    void operator()(const std::multimap<msgpack::type::variant, msgpack::type::variant>& v) const {
        s_ << "{";
        for (const auto& [k, v] : v) {
            if constexpr (std::is_same_v<std::decay_t<decltype(k)>, std::string>) {
                (*this)(k);
            } else {
                is_value_ = false;
                boost::apply_visitor(*this, k);
            }
            s_ << "=";
            is_value_ = true;
            boost::apply_visitor(*this, v);
            s_ << ",";
        }
        s_ << "}";
    }

    template <typename T>
    void operator()(T const&) const {
        assert(false && "not supported type");
    }
};
Api::Api(std::string host, std::uint16_t port)
    : rpc_(std::make_shared<rpc::Client>(std::move(host), port)) {}

auto Api::create(std::string host, std::uint16_t port) -> promise<Api> {
    auto api = Api{std::move(host), port};
    co_await api.rpc_->init();
    co_return api;
}

auto Api::rpc_channel() const -> int {
    return rpc_->channel();
}

auto Api::next_notification_id() -> int {
    return ++notification_id_cnt_;
}

auto Api::notification(std::uint32_t id) -> promise<any> {
    co_return co_await rpc_->notification(id);
}

auto Api::notifications(std::uint32_t id) -> Api::generator<any> {
    auto gen = rpc_->notifications(id);
    while (gen) {
        co_yield co_await gen;
    }
    co_return {};
}

auto Api::nvim_buf_add_highlight(integer buffer, integer ns_id, string hl_group, integer line, integer col_start,
                                 integer col_end) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_add_highlight", buffer, ns_id, hl_group, line, col_start, col_end);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_buf_attach(integer buffer, boolean send_buffer, table<string, any> opts) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_buf_attach", buffer, send_buffer, opts);
    co_return std::move(response.as_bool());
}

auto Api::nvim_buf_call(integer buffer, function) -> promise<any> {
    co_return co_await rpc_->call("nvim_buf_call", buffer);
}

auto Api::nvim_buf_clear_highlight(integer buffer, integer ns_id, integer line_start, integer line_end)
    -> promise<void> {
    co_await rpc_->call("nvim_buf_clear_highlight", buffer, ns_id, line_start, line_end);
}

auto Api::nvim_buf_clear_namespace(integer buffer, integer ns_id, integer line_start, integer line_end)
    -> promise<void> {
    co_await rpc_->call("nvim_buf_clear_namespace", buffer, ns_id, line_start, line_end);
}

auto Api::nvim_buf_create_user_command(integer buffer, string name, any command, table<string, any> opts)
    -> promise<void> {
    co_await rpc_->call("nvim_buf_create_user_command", buffer, name, command, opts);
}

auto Api::nvim_buf_del_extmark(integer buffer, integer ns_id, integer id) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_buf_del_extmark", buffer, ns_id, id);
    co_return std::move(response.as_bool());
}

auto Api::nvim_buf_del_keymap(integer buffer, string mode, string lhs) -> promise<void> {
    co_await rpc_->call("nvim_buf_del_keymap", buffer, mode, lhs);
}

auto Api::nvim_buf_del_mark(integer buffer, string name) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_buf_del_mark", buffer, name);
    co_return std::move(response.as_bool());
}

auto Api::nvim_buf_del_user_command(integer buffer, string name) -> promise<void> {
    co_await rpc_->call("nvim_buf_del_user_command", buffer, name);
}

auto Api::nvim_buf_del_var(integer buffer, string name) -> promise<void> {
    co_await rpc_->call("nvim_buf_del_var", buffer, name);
}

auto Api::nvim_buf_delete(integer buffer, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_buf_delete", buffer, opts);
}

auto Api::nvim_buf_get_changedtick(integer buffer) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_get_changedtick", buffer);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_buf_get_commands(integer buffer, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_buf_get_commands", buffer, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_buf_get_extmark_by_id(integer buffer, integer ns_id, integer id, table<string, any> opts)
    -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_buf_get_extmark_by_id", buffer, ns_id, id, opts);
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_buf_get_extmarks(integer buffer, integer ns_id, any start, any end_, table<string, any> opts)
    -> promise<std::vector<any>> {
    auto response = co_await rpc_->call("nvim_buf_get_extmarks", buffer, ns_id, start, end_, opts);
    co_return std::move(response.as_vector());
}

auto Api::nvim_buf_get_keymap(integer buffer, string mode) -> promise<std::vector<table<string, any>>> {
    auto response = co_await rpc_->call("nvim_buf_get_keymap", buffer, mode);
    std::vector<table<string, any>> result;
    for (const auto& elem : response.as_vector()) {
        table<string, any> res;
        for (const auto& [k, v] : elem.as_multimap()) {
            res.emplace(k.as_string(), v);
        }
        result.emplace_back(std::move(res));
    }
    co_return std::move(result);
}

auto Api::nvim_buf_get_lines(integer buffer, integer start, integer end_, boolean strict_indexing)
    -> promise<std::vector<string>> {
    auto response = co_await rpc_->call("nvim_buf_get_lines", buffer, start, end_, strict_indexing);
    co_return transform(response.as_vector(), [](auto t) -> string {
        return t.as_string();
    });
}

auto Api::nvim_buf_get_mark(integer buffer, string name) -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_buf_get_mark", buffer, name);
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_buf_get_name(integer buffer) -> promise<string> {
    auto response = co_await rpc_->call("nvim_buf_get_name", buffer);
    co_return std::move(response.as_string());
}

auto Api::nvim_buf_get_number(integer buffer) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_get_number", buffer);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_buf_get_offset(integer buffer, integer index) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_get_offset", buffer, index);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_buf_get_option(integer buffer, string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_buf_get_option", buffer, name);
}

auto Api::nvim_buf_get_text(integer buffer, integer start_row, integer start_col, integer end_row, integer end_col,
                            table<string, any> opts) -> promise<std::vector<string>> {
    auto response = co_await rpc_->call("nvim_buf_get_text", buffer, start_row, start_col, end_row, end_col, opts);
    co_return transform(response.as_vector(), [](auto t) -> string {
        return t.as_string();
    });
}

auto Api::nvim_buf_get_var(integer buffer, string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_buf_get_var", buffer, name);
}

auto Api::nvim_buf_is_loaded(integer buffer) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_buf_is_loaded", buffer);
    co_return std::move(response.as_bool());
}

auto Api::nvim_buf_is_valid(integer buffer) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_buf_is_valid", buffer);
    co_return std::move(response.as_bool());
}

auto Api::nvim_buf_line_count(integer buffer) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_line_count", buffer);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_buf_set_extmark(integer buffer, integer ns_id, integer line, integer col, table<string, any> opts)
    -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_set_extmark", buffer, ns_id, line, col, opts);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_buf_set_keymap(integer buffer, string mode, string lhs, string rhs, table<string, any> opts)
    -> promise<void> {
    co_await rpc_->call("nvim_buf_set_keymap", buffer, mode, lhs, rhs, opts);
}

auto Api::nvim_buf_set_lines(integer buffer, integer start, integer end_, boolean strict_indexing,
                             std::vector<string> replacement) -> promise<void> {
    co_await rpc_->call("nvim_buf_set_lines", buffer, start, end_, strict_indexing, replacement);
}

auto Api::nvim_buf_set_mark(integer buffer, string name, integer line, integer col, table<string, any> opts)
    -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_buf_set_mark", buffer, name, line, col, opts);
    co_return std::move(response.as_bool());
}

auto Api::nvim_buf_set_name(integer buffer, string name) -> promise<void> {
    co_await rpc_->call("nvim_buf_set_name", buffer, name);
}

auto Api::nvim_buf_set_option(integer buffer, string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_buf_set_option", buffer, name, value);
}

auto Api::nvim_buf_set_text(integer buffer, integer start_row, integer start_col, integer end_row, integer end_col,
                            std::vector<string> replacement) -> promise<void> {
    co_await rpc_->call("nvim_buf_set_text", buffer, start_row, start_col, end_row, end_col, replacement);
}

auto Api::nvim_buf_set_var(integer buffer, string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_buf_set_var", buffer, name, value);
}

auto Api::nvim_buf_set_virtual_text(integer buffer, integer src_id, integer line, std::vector<any> chunks,
                                    table<string, any> opts) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_buf_set_virtual_text", buffer, src_id, line, chunks, opts);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_call_dict_function(any dict, string fn, std::vector<any> args) -> promise<any> {
    co_return co_await rpc_->call("nvim_call_dict_function", dict, fn, args);
}

auto Api::nvim_call_function(string fn, std::vector<any> args) -> promise<any> {
    co_return co_await rpc_->call("nvim_call_function", fn, args);
}

auto Api::nvim_chan_send(integer chan, string data) -> promise<void> {
    co_await rpc_->call("nvim_chan_send", chan, data);
}

auto Api::nvim_clear_autocmds(table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_clear_autocmds", opts);
}

auto Api::nvim_cmd(table<string, any> cmd, table<string, any> opts) -> promise<string> {
    auto response = co_await rpc_->call("nvim_cmd", cmd, opts);
    co_return std::move(response.as_string());
}

auto Api::nvim_command(string command) -> promise<void> {
    co_await rpc_->call("nvim_command", command);
}

auto Api::nvim_command_output(string command) -> promise<string> {
    auto response = co_await rpc_->call("nvim_command_output", command);
    co_return std::move(response.as_string());
}

auto Api::nvim_complete_set(integer index, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_complete_set", index, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_create_augroup(string name, table<string, any> opts) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_create_augroup", name, opts);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_create_autocmd(std::vector<std::string> event, table<any, any> opts) -> generator<any> {
    const int id = next_notification_id();

    // there is no easy way to handle callbacks via RPC, so wrap rpcnotify() call in a lua function
    // use variant visitor to construct proper lua script
    std::ostringstream event_stream;
    boost::apply_visitor(LuaVisitor(event_stream), boost::variant<std::vector<std::string>>(std::move(event)));

    // set callback explicitly to lua function
    std::ostringstream opt_stream;
    const auto body = fmt::format(R"(function(ev) vim.fn["rpcnotify"]({0}, '{1}', ev) end)", rpc_->channel(), id);
    opts.emplace("callback", msgpack::type::raw_ref(body.data(), body.size()));
    boost::apply_visitor(LuaVisitor(opt_stream), any(std::move(opts)));

    // register via lua API
    const auto func = fmt::format(R"(lua vim.api.nvim_create_autocmd({0}, {1}))", event_stream.str(), opt_stream.str());
    co_await rpc_->call("nvim_exec2", func, std::map<std::string, std::string>{});

    // wait for notifications with this id, return call arguments, which are going to be 'ev' dict from the callback
    auto gen = rpc_->notifications(id);
    while (gen) {
        co_yield co_await gen;
    }
    co_return {};
}

auto Api::nvim_create_buf(boolean listed, boolean scratch) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_create_buf", listed, scratch);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_create_namespace(string name) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_create_namespace", name);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_create_user_command(string name, any command, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_create_user_command", name, command, opts);
}

auto Api::nvim_del_augroup_by_id(integer id) -> promise<void> {
    co_await rpc_->call("nvim_del_augroup_by_id", id);
}

auto Api::nvim_del_augroup_by_name(string name) -> promise<void> {
    co_await rpc_->call("nvim_del_augroup_by_name", name);
}

auto Api::nvim_del_autocmd(integer id) -> promise<void> {
    co_await rpc_->call("nvim_del_autocmd", id);
}

auto Api::nvim_del_current_line() -> promise<void> {
    co_await rpc_->call("nvim_del_current_line");
}

auto Api::nvim_del_keymap(string mode, string lhs) -> promise<void> {
    co_await rpc_->call("nvim_del_keymap", mode, lhs);
}

auto Api::nvim_del_mark(string name) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_del_mark", name);
    co_return std::move(response.as_bool());
}

auto Api::nvim_del_user_command(string name) -> promise<void> {
    co_await rpc_->call("nvim_del_user_command", name);
}

auto Api::nvim_del_var(string name) -> promise<void> {
    co_await rpc_->call("nvim_del_var", name);
}

auto Api::nvim_echo(std::vector<any> chunks, boolean history, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_echo", chunks, history, opts);
}

auto Api::nvim_err_write(string str) -> promise<void> {
    co_await rpc_->call("nvim_err_write", str);
}

auto Api::nvim_err_writeln(string str) -> promise<void> {
    co_await rpc_->call("nvim_err_writeln", str);
}

auto Api::nvim_eval(string expr) -> promise<any> {
    co_return co_await rpc_->call("nvim_eval", expr);
}

auto Api::nvim_eval_statusline(string str, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_eval_statusline", str, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_exec(string src, boolean output) -> promise<string> {
    auto response = co_await rpc_->call("nvim_exec", src, output);
    co_return std::move(response.as_string());
}

auto Api::nvim_exec2(string src, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_exec2", src, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_exec_autocmds(any event, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_exec_autocmds", event, opts);
}

auto Api::nvim_feedkeys(string keys, string mode, boolean escape_ks) -> promise<void> {
    co_await rpc_->call("nvim_feedkeys", keys, mode, escape_ks);
}

auto Api::nvim_get_all_options_info() -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_all_options_info");
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_autocmds(table<string, any> opts) -> promise<std::vector<any>> {
    auto response = co_await rpc_->call("nvim_get_autocmds", opts);
    co_return std::move(response.as_vector());
}

auto Api::nvim_get_chan_info(integer chan) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_chan_info", chan);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_color_by_name(string name) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_get_color_by_name", name);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_get_color_map() -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_color_map");
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_commands(table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_commands", opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_context(table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_context", opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_current_buf() -> promise<integer> {
    auto response = co_await rpc_->call("nvim_get_current_buf");
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_get_current_line() -> promise<string> {
    auto response = co_await rpc_->call("nvim_get_current_line");
    co_return std::move(response.as_string());
}

auto Api::nvim_get_current_tabpage() -> promise<integer> {
    auto response = co_await rpc_->call("nvim_get_current_tabpage");
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_get_current_win() -> promise<integer> {
    auto response = co_await rpc_->call("nvim_get_current_win");
    const auto ext = response.as_ext();
    assert(ext.size() == sizeof(std::uint16_t) + 1);
    char buf[2] = {ext.data()[2], ext.data()[1]};
    co_return reinterpret_cast<const std::uint16_t&>(*buf);
}

auto Api::nvim_get_hl(integer ns_id, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_hl", ns_id, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_hl_by_id(integer hl_id, boolean rgb) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_hl_by_id", hl_id, rgb);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_hl_by_name(string name, boolean rgb) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_hl_by_name", name, rgb);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_hl_id_by_name(string name) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_get_hl_id_by_name", name);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_get_hl_ns(table<string, any> opts) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_get_hl_ns", opts);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_get_keymap(string mode) -> promise<std::vector<table<string, any>>> {
    auto response = co_await rpc_->call("nvim_get_keymap", mode);
    std::vector<table<string, any>> result;
    for (const auto& elem : response.as_vector()) {
        table<string, any> res;
        for (const auto& [k, v] : elem.as_multimap()) {
            res.emplace(k.as_string(), v);
        }
        result.emplace_back(std::move(res));
    }
    co_return std::move(result);
}

auto Api::nvim_get_mark(string name, table<string, any> opts) -> promise<std::vector<any>> {
    auto response = co_await rpc_->call("nvim_get_mark", name, opts);
    co_return std::move(response.as_vector());
}

auto Api::nvim_get_mode() -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_mode");
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_namespaces() -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_namespaces");
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_option(string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_get_option", name);
}

auto Api::nvim_get_option_info(string name) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_option_info", name);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_option_info2(string name, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_get_option_info2", name, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_get_option_value(string name, table<string, any> opts) -> promise<any> {
    co_return co_await rpc_->call("nvim_get_option_value", name, opts);
}

auto Api::nvim_get_proc(integer pid) -> promise<any> {
    co_return co_await rpc_->call("nvim_get_proc", pid);
}

auto Api::nvim_get_proc_children(integer pid) -> promise<std::vector<any>> {
    auto response = co_await rpc_->call("nvim_get_proc_children", pid);
    co_return std::move(response.as_vector());
}

auto Api::nvim_get_runtime_file(string name, boolean all) -> promise<std::vector<string>> {
    auto response = co_await rpc_->call("nvim_get_runtime_file", name, all);
    co_return transform(response.as_vector(), [](auto t) -> string {
        return t.as_string();
    });
}

auto Api::nvim_get_var(string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_get_var", name);
}

auto Api::nvim_get_vvar(string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_get_vvar", name);
}

auto Api::nvim_input(string keys) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_input", keys);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_input_mouse(string button, string action, string modifier, integer grid, integer row, integer col)
    -> promise<void> {
    co_await rpc_->call("nvim_input_mouse", button, action, modifier, grid, row, col);
}

auto Api::nvim_list_bufs() -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_list_bufs");
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_list_chans() -> promise<std::vector<any>> {
    auto response = co_await rpc_->call("nvim_list_chans");
    co_return std::move(response.as_vector());
}

auto Api::nvim_list_runtime_paths() -> promise<std::vector<string>> {
    auto response = co_await rpc_->call("nvim_list_runtime_paths");
    co_return transform(response.as_vector(), [](auto t) -> string {
        return t.as_string();
    });
}

auto Api::nvim_list_tabpages() -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_list_tabpages");
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_list_uis() -> promise<std::vector<any>> {
    auto response = co_await rpc_->call("nvim_list_uis");
    co_return std::move(response.as_vector());
}

auto Api::nvim_list_wins() -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_list_wins");
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_load_context(table<string, any> dict) -> promise<any> {
    co_return co_await rpc_->call("nvim_load_context", dict);
}

auto Api::nvim_notify(string msg, integer log_level, table<string, any> opts) -> promise<any> {
    co_return co_await rpc_->call("nvim_notify", msg, log_level, opts);
}

auto Api::nvim_open_term(integer buffer, table<string, any> opts) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_open_term", buffer, opts);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_open_win(integer buffer, boolean enter, table<string, any> config) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_open_win", buffer, enter, config);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_out_write(string str) -> promise<void> {
    co_await rpc_->call("nvim_out_write", str);
}

auto Api::nvim_parse_cmd(string str, table<string, any> opts) -> promise<any> {
    co_return co_await rpc_->call("nvim_parse_cmd", str, opts);
}

auto Api::nvim_parse_expression(string expr, string flags, boolean highlight) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_parse_expression", expr, flags, highlight);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_paste(string data, boolean crlf, integer phase) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_paste", data, crlf, phase);
    co_return std::move(response.as_bool());
}

auto Api::nvim_put(std::vector<string> lines, string type, boolean after, boolean follow) -> promise<void> {
    co_await rpc_->call("nvim_put", lines, type, after, follow);
}

auto Api::nvim_replace_termcodes(string str, boolean from_part, boolean do_lt, boolean special) -> promise<string> {
    auto response = co_await rpc_->call("nvim_replace_termcodes", str, from_part, do_lt, special);
    co_return std::move(response.as_string());
}

auto Api::nvim_select_popupmenu_item(integer item, boolean insert, boolean finish, table<string, any> opts)
    -> promise<void> {
    co_await rpc_->call("nvim_select_popupmenu_item", item, insert, finish, opts);
}

auto Api::nvim_set_current_buf(integer buffer) -> promise<void> {
    co_await rpc_->call("nvim_set_current_buf", buffer);
}

auto Api::nvim_set_current_dir(string dir) -> promise<void> {
    co_await rpc_->call("nvim_set_current_dir", dir);
}

auto Api::nvim_set_current_line(string line) -> promise<void> {
    co_await rpc_->call("nvim_set_current_line", line);
}

auto Api::nvim_set_current_tabpage(integer tabpage) -> promise<void> {
    co_await rpc_->call("nvim_set_current_tabpage", tabpage);
}

auto Api::nvim_set_current_win(integer window) -> promise<void> {
    co_await rpc_->call("nvim_set_current_win", window);
}

auto Api::nvim_set_decoration_provider(integer ns_id, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_set_decoration_provider", ns_id, opts);
}

auto Api::nvim_set_hl(integer ns_id, string name, table<string, any> val) -> promise<void> {
    co_await rpc_->call("nvim_set_hl", ns_id, name, val);
}

auto Api::nvim_set_hl_ns(integer ns_id) -> promise<void> {
    co_await rpc_->call("nvim_set_hl_ns", ns_id);
}

auto Api::nvim_set_hl_ns_fast(integer ns_id) -> promise<void> {
    co_await rpc_->call("nvim_set_hl_ns_fast", ns_id);
}

auto Api::nvim_set_keymap(string mode, string lhs, string rhs, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_set_keymap", mode, lhs, rhs, opts);
}

auto Api::nvim_set_option(string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_set_option", name, value);
}

auto Api::nvim_set_option_value(string name, any value, table<string, any> opts) -> promise<void> {
    co_await rpc_->call("nvim_set_option_value", name, value, opts);
}

auto Api::nvim_set_var(string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_set_var", name, value);
}

auto Api::nvim_set_vvar(string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_set_vvar", name, value);
}

auto Api::nvim_strwidth(string text) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_strwidth", text);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_tabpage_del_var(integer tabpage, string name) -> promise<void> {
    co_await rpc_->call("nvim_tabpage_del_var", tabpage, name);
}

auto Api::nvim_tabpage_get_number(integer tabpage) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_tabpage_get_number", tabpage);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_tabpage_get_var(integer tabpage, string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_tabpage_get_var", tabpage, name);
}

auto Api::nvim_tabpage_get_win(integer tabpage) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_tabpage_get_win", tabpage);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_tabpage_is_valid(integer tabpage) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_tabpage_is_valid", tabpage);
    co_return std::move(response.as_bool());
}

auto Api::nvim_tabpage_list_wins(integer tabpage) -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_tabpage_list_wins", tabpage);
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_tabpage_set_var(integer tabpage, string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_tabpage_set_var", tabpage, name, value);
}

auto Api::nvim_tabpage_set_win(integer tabpage, integer win) -> promise<void> {
    co_await rpc_->call("nvim_tabpage_set_win", tabpage, win);
}

auto Api::nvim_win_call(integer window, function) -> promise<any> {
    co_return co_await rpc_->call("nvim_win_call", window);
}

auto Api::nvim_win_close(integer window, boolean force) -> promise<void> {
    co_await rpc_->call("nvim_win_close", window, force);
}

auto Api::nvim_win_del_var(integer window, string name) -> promise<void> {
    co_await rpc_->call("nvim_win_del_var", window, name);
}

auto Api::nvim_win_get_buf(integer window) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_win_get_buf", window);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_win_get_config(integer window) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_win_get_config", window);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

auto Api::nvim_win_get_cursor(integer window) -> promise<std::vector<integer>> {
    auto response = co_await rpc_->call("nvim_win_get_cursor", window);
    co_return transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
}

auto Api::nvim_win_get_height(integer window) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_win_get_height", window);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_win_get_number(integer window) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_win_get_number", window);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_win_get_option(integer window, string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_win_get_option", window, name);
}

auto Api::nvim_win_get_position(integer window) -> promise<Point> {
    auto response = co_await rpc_->call("nvim_win_get_position", window);
    const auto vec = transform(response.as_vector(), [](auto t) -> integer {
        return t.as_uint64_t();
    });
    co_return Point{.x = vec.back(), .y = vec.front()};
}

auto Api::nvim_win_get_tabpage(integer window) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_win_get_tabpage", window);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_win_get_var(integer window, string name) -> promise<any> {
    co_return co_await rpc_->call("nvim_win_get_var", window, name);
}

auto Api::nvim_win_get_width(integer window) -> promise<integer> {
    auto response = co_await rpc_->call("nvim_win_get_width", window);
    co_return std::move(response.as_uint64_t());
}

auto Api::nvim_win_hide(integer window) -> promise<void> {
    co_await rpc_->call("nvim_win_hide", window);
}

auto Api::nvim_win_is_valid(integer window) -> promise<boolean> {
    auto response = co_await rpc_->call("nvim_win_is_valid", window);
    co_return std::move(response.as_bool());
}

auto Api::nvim_win_set_buf(integer window, integer buffer) -> promise<void> {
    co_await rpc_->call("nvim_win_set_buf", window, buffer);
}

auto Api::nvim_win_set_config(integer window, table<string, any> config) -> promise<void> {
    co_await rpc_->call("nvim_win_set_config", window, config);
}

auto Api::nvim_win_set_cursor(integer window, std::vector<integer> pos) -> promise<void> {
    co_await rpc_->call("nvim_win_set_cursor", window, pos);
}

auto Api::nvim_win_set_height(integer window, integer height) -> promise<void> {
    co_await rpc_->call("nvim_win_set_height", window, height);
}

auto Api::nvim_win_set_hl_ns(integer window, integer ns_id) -> promise<void> {
    co_await rpc_->call("nvim_win_set_hl_ns", window, ns_id);
}

auto Api::nvim_win_set_option(integer window, string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_win_set_option", window, name, value);
}

auto Api::nvim_win_set_var(integer window, string name, any value) -> promise<void> {
    co_await rpc_->call("nvim_win_set_var", window, name, value);
}

auto Api::nvim_win_set_width(integer window, integer width) -> promise<void> {
    co_await rpc_->call("nvim_win_set_width", window, width);
}

auto Api::nvim_win_text_height(integer window, table<string, any> opts) -> promise<table<string, any>> {
    auto response = co_await rpc_->call("nvim_win_text_height", window, opts);
    table<string, any> res;
    for (const auto& [k, v] : response.as_multimap()) {
        res.emplace(k.as_string(), v);
    }
    co_return res;
}

} // namespace nvim
