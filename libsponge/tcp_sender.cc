#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timeout{retx_timeout}
    , _timeoutcouter(0)
    , _outing_queue()
    , bytes_in_fly(0)
    , _remote_win_sz(1)
    , _send_sync(false)
    , _send_fin(false)
    , _consecutive_retransmissions_count(0) {}

uint64_t TCPSender::bytes_in_flight() const { return bytes_in_fly; }

void TCPSender::fill_window() {
    if (_send_fin) {
        return;
    }
    size_t win_sz = _remote_win_sz == 0 ? 1 : _remote_win_sz;
    while (win_sz > bytes_in_fly) {
        TCPSegment seq;
        if (!_send_sync) {
            seq.header().syn = true;
            _send_sync = true;
        }
        seq.header().seqno = next_seqno();
        size_t len =
            min(win_sz - bytes_in_fly - seq.header().syn, min(TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size()));
        seq.payload() = _stream.read(len);
        if (!_send_fin && _stream.eof() && win_sz > (seq.length_in_sequence_space() + bytes_in_flight())) {
            seq.header().fin = true;
            _send_fin = true;
        }

        if (_outing_queue.empty()) {
            _timeout = _initial_retransmission_timeout;
            _timeoutcouter = 0;
        }

        if (seq.length_in_sequence_space() == 0) {
            break;
        }

        _segments_out.push(seq);
        _next_seqno += seq.length_in_sequence_space();
        _outing_queue.push(std::make_pair(_next_seqno, seq));
        bytes_in_fly += seq.length_in_sequence_space();

        if (_send_fin) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_seq_no = unwrap(ackno, _isn, _next_seqno);
    _remote_win_sz = window_size;
    if (abs_seq_no > _next_seqno) {
        return;
    }

    bool resetFlag = false;

    while (!_outing_queue.empty()) {
        auto pair = _outing_queue.front();
        if (abs_seq_no >= pair.first) {
            bytes_in_fly -= pair.second.length_in_sequence_space();
            _outing_queue.pop();
            resetFlag = true;
        } else {
            break;
        }
    }

    if (resetFlag) {
        _timeoutcouter = 0;
        _timeout = _initial_retransmission_timeout;
        _consecutive_retransmissions_count = 0;
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timeoutcouter += ms_since_last_tick;
    if (_timeoutcouter >= _timeout && !_outing_queue.empty()) {
        auto pair = _outing_queue.front();
        _segments_out.push(pair.second);

        _timeout *= 2;

        _consecutive_retransmissions_count++;
        _timeoutcouter = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seq;
    seq.header().seqno = next_seqno();
    _segments_out.push(seq);
}
