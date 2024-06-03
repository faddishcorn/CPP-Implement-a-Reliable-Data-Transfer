#ifndef PACKET_H
#define PACKET_H

#include <string>
#include <cstdint>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp>

class Packet {
public:
    enum Type { DATA, ACK, EOT }; //데이터 패킷, ACK 패킷, EOT 패킷을 열거형으로 정의

    Packet() : type(DATA), seqNum(0), length(0) {} //기본 생성자
    Packet(Type type, uint32_t seqNum, const std::string& data = "")
        : type(type), seqNum(seqNum), data(data), length(data.size()) {}

    static Packet deserialize(const std::string& serializedStr) { //직렬화된 문자열을 받아서 Packet객체로 변환
        std::istringstream iss(serializedStr);
        boost::archive::text_iarchive ia(iss);
        Packet pkt;
        ia >> pkt;
        return pkt;
    }

    std::string serialize() const { //Packet 객체를 직렬화하여 문자열로 반환
        std::ostringstream oss;
        boost::archive::text_oarchive oa(oss);
        oa << *this;
        return oss.str();
    }

    Type getType() const { return type; } //패킷의 멤버 변수들을 반환하는 접근자 함수들
    uint32_t getSeqNum() const { return seqNum; }
    uint32_t getLength() const { return length; }
    const std::string& getData() const { return data; }

private:
    Type type;
    uint32_t seqNum;
    uint32_t length;
    std::string data;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & type;
        ar & seqNum;
        ar & length;
        ar & data;
    }
};

#endif 
