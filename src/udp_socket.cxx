#include "udp_socket.hxx"
#include "utility.hxx"

void Udp_socket::configure_default_connections() noexcept {
         connect(this,&Udp_socket::disconnected,&Udp_socket::deleteLater);
         connect(this,&Udp_socket::readyRead,&connection_timer_,&QTimer::stop);

         connect(this,&Udp_socket::connected,[this]{
                  send_initial_request(connect_request_,State::Connect);
         });
         
         interval_timer_.callOnTimeout(this,[this]{
                  send_request(announce_request);
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
                                             send_request(scrape_request);
                                             break;
                                    }

                                    case State::Announce : {
                                             send_request(announce_request);
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

void Udp_socket::reset_time_specs() noexcept {
         timeout_factor_ = 0;
         connection_timer_.start(get_timeout());
         connection_id_valid_ = true;

         constexpr std::chrono::minutes protocol_validity_timeout {1};

         QTimer::singleShot(protocol_validity_timeout,this,[&connection_id_valid_ = connection_id_valid_]{
                  connection_id_valid_ = false;
         });
}