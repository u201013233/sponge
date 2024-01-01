#include "stream_reassembler.hh"

#include <cassert>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassemble_map()
    , _next_assembled_idx(0)
    , _unassembled_bytes_num(0)
    , _eof_idx(-1)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto pos_iter = _unassemble_map.upper_bound(index);
    if (pos_iter != _unassemble_map.begin()) {
        pos_iter--;
    }

    // pos_iter 表示第一个小于index的子串
    size_t new_idx = index;
    if (pos_iter != _unassemble_map.end() && pos_iter->first <= index) {
        const size_t up_idx = pos_iter->first;
        auto sec = pos_iter->second;
        if (up_idx + sec.length() > index) {
            new_idx = up_idx + sec.length();
        }
    } else if (index < _next_assembled_idx) {
        new_idx = _next_assembled_idx;
    }
    const size_t start_pos = new_idx - index;
    ssize_t data_size = data.size() - start_pos;

    pos_iter = _unassemble_map.lower_bound(new_idx);
    while (pos_iter != _unassemble_map.end() && new_idx <= pos_iter->first) {
        const size_t data_end_pos = new_idx + data_size;
        if (pos_iter->first < data_end_pos) {
            // 部分重叠
            if (pos_iter->first + pos_iter->second.size() > data_end_pos) {
                data_size = pos_iter->first - new_idx;
                break;
            } else {
                _unassembled_bytes_num -= pos_iter->second.size();
                pos_iter = _unassemble_map.erase(pos_iter);
                continue;
            }
        } else {
            break;
        }
    }

    size_t first_unaccpet_idx = _next_assembled_idx + _capacity - _output.buffer_size();
    if (new_idx + data_size > first_unaccpet_idx) {
        data_size = first_unaccpet_idx - new_idx;
    }


    if (data_size > 0) {
        const string new_data = data.substr(start_pos, data_size);
        if (new_idx == _next_assembled_idx) {
            size_t write_byte = _output.write(new_data);
            _next_assembled_idx += write_byte;
            // 如果没写全，则将其保存起来
            if (write_byte < new_data.size()) {
                const size_t can_store_byte = min(_capacity - _unassembled_bytes_num, new_data.size() - write_byte);
                if (can_store_byte) {
                    const string data_to_store = new_data.substr(write_byte, can_store_byte);
                    _unassembled_bytes_num += data_to_store.size();
                    _unassemble_map.insert(make_pair(_next_assembled_idx, std::move(data_to_store)));
                }
            }
        } else {
            const size_t can_store_byte = min(_capacity - _unassembled_bytes_num, new_data.size());
            if (can_store_byte) {
                const string data_to_store = new_data.substr(0, can_store_byte);
                _unassembled_bytes_num += data_to_store.size();
                _unassemble_map.insert(make_pair(new_idx, std::move(data_to_store)));
            }
        }
    }

    for (auto iter = _unassemble_map.begin(); iter != _unassemble_map.end();) {
        assert(_next_assembled_idx <= iter->first);
        if (iter->first == _next_assembled_idx) {
            const size_t write_num = _output.write(iter->second);
            _next_assembled_idx += write_num;
            if (write_num < iter->second.size()) {
                _unassembled_bytes_num += iter->second.size() - write_num;
                _unassemble_map.insert(make_pair(_next_assembled_idx, std::move(iter->second.substr(write_num))));
                _unassembled_bytes_num -= iter->second.size();
                _unassemble_map.erase(iter);
                break;
            }
            _unassembled_bytes_num -= iter->second.size();
            iter = _unassemble_map.erase(iter);
        } else {
            break;
        }
    }

    if (eof) {
        _eof_idx = index + data.size();
    }
    if (_eof_idx <= _next_assembled_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes_num; }

bool StreamReassembler::empty() const { return _unassembled_bytes_num == 0; }
