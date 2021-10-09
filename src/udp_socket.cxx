#include "udp_socket.hxx"
#include "utility.hxx"

void Udp_socket::configure_default_connections() noexcept {
         connect(this,&Udp_socket::disconnected,&Udp_socket::deleteLater);
         connect(this,&Udp_socket::readyRead,&connection_timer_,&QTimer::stop);

         connect(this,&Udp_socket::connected,[this]{
                  send_initial_request(connect_request_,State::Connect);
         });
         
         interval_timer_.callOnTimeout(this,[this]{
                  send_request(announce_request_);
         });

         connection_timer_.callOnTimeout(this,[this]{
                  constexpr auto protocol_max_factor_limit = 8;

                  if(++timeout_factor_ <= protocol_max_factor_limit){
                           switch(state_){
                                    case State::Connect : {
                                             send_request(connect_request_);
                                             break;
                                    }

                                    case State::Scrape : { 
                                             send_request(scrape_request_);
                                             break;
                                    }

                                    case State::Announce : {
                                             send_request(announce_request_);
                                             break;
                                    }

                                    default : {
                                             __builtin_unreachable();
                                    }
                           }

                           connection_timer_.start(get_timeout());
                  }else{
                           //todo alert the tracker about connection timeout
                           connection_timer_.stop();
                           disconnectFromHost();
                  }
         });
}

void Udp_socket::send_packet(const QByteArray & hex_packet) noexcept {
         const auto raw_packet = QByteArray::fromHex(hex_packet);
         write(raw_packet);

         constexpr auto txn_id_offset = 12;
         [[maybe_unused]] bool conversion_success = true;

         const auto sent_txn_id = util::extract_integer<std::uint32_t>(raw_packet,txn_id_offset);

         assert(conversion_success);
         txn_id_ = sent_txn_id;
}