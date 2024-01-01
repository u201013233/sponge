#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    if (header.syn) {
        _isn = header.seqno;
        _set_syn_flag = true;
        _last_abs_seq = 0;
    } else if (!_set_syn_flag) {
        return;
    }
    _last_abs_seq = unwrap(header.seqno, _isn, _last_abs_seq);

    bool set_fin_flag = false;
    if (header.fin) {
        set_fin_flag = true;
    }
    uint64_t stream_idx = _last_abs_seq - 1 + header.syn;
    _reassembler.push_substring(seg.payload().copy(), stream_idx, set_fin_flag);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_set_syn_flag) {
        return nullopt;
    }
    uint64_t abs_ack_no = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended()) {
        ++abs_ack_no;
    }
    return WrappingInt32(_isn) + abs_ack_no;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
