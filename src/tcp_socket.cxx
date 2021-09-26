#include "tcp_socket.hxx"

void Tcp_socket::send_packet(const QByteArray & packet){
	const auto raw_fmt = QByteArray::fromHex(packet);
	
	write(raw_fmt.data(),raw_fmt.size());
}