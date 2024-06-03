#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include "Packet.h"

#define BUFFER_SIZE 1024

void logEvent(const std::string &event, const Packet &pkt) { //진행상황에 대해 이벤트로그 기록 및 출력
    time_t now = time(0);
    tm *ltm = localtime(&now);
    std::cout << "[" << 1 + ltm->tm_hour << ":"
              << 1 + ltm->tm_min << ":"
              << 1 + ltm->tm_sec << "] "
              << event << " - Type: " << pkt.getType()
              << ", SeqNum: " << pkt.getSeqNum()
              << ", Length: " << pkt.getLength() << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "해당 인수들을 입력해야 합니다: " << argv[0] << " <receiver_port> <drop_probability>" << std::endl;
        return 1;
    }

    int receiverPort = std::atoi(argv[1]); //receiver의 포트 및 data패킷을 드랍할 확률을 사용자로부터 받아서 할당
    float dropProbability = std::atof(argv[2]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); //udp소켓 생성 
    if (sockfd < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        return 1;
    }

    struct sockaddr_in receiverAddr, senderAddr; //수신자의 주소 정보를 설정하고 소켓바인딩 
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(receiverPort);
    receiverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (const struct sockaddr *)&receiverAddr, sizeof(receiverAddr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        close(sockfd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(senderAddr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&senderAddr, &len);
    std::string recvStr(buffer, n); //초반의 그리팅 패킷을 수신한다 
    Packet greetingPkt = Packet::deserialize(recvStr); //수신한 데이터를 역직렬화하여 패킷 객체로 변환 
    logEvent("Received Greeting", greetingPkt); 

    Packet ackPkt(Packet::ACK, 0);//그리팅 패킷에 대한 ACK 생성 및 전송 
    std::string serializedAck = ackPkt.serialize();
    sendto(sockfd, serializedAck.c_str(), serializedAck.size(), 0, (struct sockaddr *)&senderAddr, len);
    logEvent("Sent ACK", ackPkt);

    std::ofstream file("Received_target_file", std::ios::binary); //수신한 데이터를 저장할 파일을 바이너리 모드로 연다 
    if (!file.is_open()) {
        std::cerr << "Failed to open file Received_target_file" << std::endl;
        close(sockfd);
        return 1;
    }

    while (true) { //데이터 수신 루프 
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&senderAddr, &len);
        recvStr.assign(buffer, n);
        Packet dataPkt = Packet::deserialize(recvStr); //데이터를 수신하여 역직렬화 
        logEvent("Received Data", dataPkt);

        if (dataPkt.getType() == Packet::EOT) {
            break;
        }

        if ((float)rand() / RAND_MAX > dropProbability) { //랜덤 값과 드롭 확률을 비교해서 패킷을 드랍할지 결정 
            file.write(dataPkt.getData().c_str(), dataPkt.getLength()); //드랍되지 않은 경우 데이터를 기록하고 ACK 패킷 생성 및 전송
            ackPkt = Packet(Packet::ACK, dataPkt.getSeqNum());
            serializedAck = ackPkt.serialize();
            sendto(sockfd, serializedAck.c_str(), serializedAck.size(), 0, (const struct sockaddr *)&senderAddr, len);
            logEvent("Sent ACK", ackPkt);
        } else {
            logEvent("Dropped Packet", dataPkt); //드랍된 경우 
        }
    }

    file.close();

    Packet donePkt(Packet::ACK, 0); //전송 완료를 알리는 ACK패킷 생성 및 전송 
    std::string serializedDone = donePkt.serialize();
    sendto(sockfd, serializedDone.c_str(), serializedDone.size(), 0, (const struct sockaddr *)&senderAddr, len);
    logEvent("Sent WellDone", donePkt);

    close(sockfd);
    return 0;
}
