#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <sys/time.h>
#include "Packet.h"

#define BUFFER_SIZE 1024

void logEvent(const std::string &event, const Packet &pkt) { //진행상황에 대한 이벤트 로그를 기록 및 출력하는 함수
    time_t now = time(0);
    tm *ltm = localtime(&now);
    std::cout << "[" << " "
              << 1 + ltm->tm_hour << ":"
              << 1 + ltm->tm_min << ":"
              << 1 + ltm->tm_sec << "] "
              << event << " - Type: " << pkt.getType()
              << ", SeqNum: " << pkt.getSeqNum()
              << ", Length: " << pkt.getLength() << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc != 7) { 
        std::cerr << "해당 인수들을 입력해야 합니다: " << argv[0] << " <sender_port> <receiver_ip> <receiver_port> <timeout_interval> <filename> <drop_probability>" << std::endl;
        return 1;
    }

    int senderPort = std::atoi(argv[1]); //인수들을 받아서 할당
    std::string receiverIp = argv[2];
    int receiverPort = std::atoi(argv[3]);
    int timeoutInterval = std::atoi(argv[4]);
    std::string fileName = (argv[5]);
    float dropProbability = std::atof(argv[6]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); //udp소켓 생성
    if (sockfd < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        return 1;
    }

    struct sockaddr_in receiverAddr; //수신자의 주소 정보를 설정
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(receiverPort);
    inet_aton(receiverIp.c_str(), &receiverAddr.sin_addr);

    Packet greetingPkt(Packet::DATA, 0, "Greetings."); //패킷을 생성하고 직렬화하여 수신자에게 전송
    std::string serializedGreeting = greetingPkt.serialize();
    sendto(sockfd, serializedGreeting.c_str(), serializedGreeting.size(), 0, (const struct sockaddr*)&receiverAddr, sizeof(receiverAddr));
    logEvent("Sent Greeting", greetingPkt);

    char buffer[BUFFER_SIZE]; //수신자로부터 응답 패킷을 수신하는 과정
    socklen_t len = sizeof(receiverAddr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&receiverAddr, &len);
    std::string recvStr(buffer, n);
    Packet responsePkt = Packet::deserialize(recvStr); //수신한 데이터를 역직렬화하여 패킷 객체로 변환
    logEvent("Received Response", responsePkt);

    if (responsePkt.getType() == Packet::ACK) { //응답 패킷의 유형이 ACK인 경우 파일 전송을 시작
        std::cout << "Receiver: OK. Now Sending file..." << std::endl;
        std::ifstream file(fileName, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file target_file" << std::endl;
            close(sockfd);
            return 1;
        }

        uint32_t seqNum = 1; //파일의 내용을 읽어 패킷으로 나눔
        while (!file.eof()) {
            file.read(buffer, BUFFER_SIZE);
            Packet dataPkt(Packet::DATA, seqNum++, std::string(buffer, file.gcount())); //각 패킷의 시퀀스 번호를 증가시키며 데이터를 읽는다
            std::string serializedData = dataPkt.serialize();

            while (true) { //패킷을 수신자에게 전송 
                sendto(sockfd, serializedData.c_str(), serializedData.size(), 0, (const struct sockaddr*)&receiverAddr, sizeof(receiverAddr));
                logEvent("Sent Data", dataPkt);

                fd_set readfds;
                struct timeval tv;
                FD_ZERO(&readfds);
                FD_SET(sockfd, &readfds);
                tv.tv_sec = timeoutInterval / 1000;
                tv.tv_usec = (timeoutInterval % 1000) * 1000;

                int rv = select(sockfd + 1, &readfds, NULL, NULL, &tv); //select함수를 호출하여 소켓에 대한 읽기 이벤트를 대기
                if (rv == -1) {
                    std::cerr << "Select error" << std::endl;
                    return 1;
                } else if (rv == 0) {
                    logEvent("Timeout, Resending", dataPkt); //타임아웃 발생 시 
                    continue;
                } else { 
                    n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&receiverAddr, &len);
                    recvStr.assign(buffer, n);
                    Packet ackPkt = Packet::deserialize(recvStr);
                    if (ackPkt.getType() == Packet::ACK && ackPkt.getSeqNum() == dataPkt.getSeqNum()) {//ACK 패킷을 수신했을 때 
                        logEvent("Received ACK", ackPkt);
                        break;
                    } else {
                        logEvent("Received Duplicate/Out of Order Packet", ackPkt); //중복 혹은 순서가 맞지않는 패킷 수신 시 
                    }
                }
            }
        }

        Packet finishPkt(Packet::EOT, seqNum);
        std::string serializedFinish = finishPkt.serialize();
        sendto(sockfd, serializedFinish.c_str(), serializedFinish.size(), 0, (const struct sockaddr*)&receiverAddr, sizeof(receiverAddr));
        logEvent("Sent Finish", finishPkt);

        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&receiverAddr, &len);
        recvStr.assign(buffer, n);
        Packet donePkt = Packet::deserialize(recvStr);
        logEvent("Received WellDone", donePkt); //파일 전송이 완료되면 EOT 패킷을 생성 및 전송 
        if (donePkt.getType() == Packet::ACK) { //수신자로부터 ACK 패킷을 수신하여 전송완료 확인 
            std::cout << "Receiver: WellDone." << std::endl;
        } else {
            std::cerr << "Sender: Unexpected situation occurred." << std::endl;
        }
    }

    close(sockfd);
    return 0;
}