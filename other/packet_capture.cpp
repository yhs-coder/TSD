#include "packet_capture.h"

// #include "sql_detector.h"

uint32_t PacketCapture::_num = 0;
std::map<AddrIPv4, std::string> PacketCapture::arp_map{};

PacketCapture::PacketCapture(const char *handle) {
    std::string data(handle);
    if (data.find(".pcap") != std::string::npos) {
        _handle = pcap_open_offline(handle, _errbuf);
    } else {
        if (!(_handle = pcap_open_live(handle, snaplen, 1, 0, _errbuf))) {
            printf("error: pcap_open_live():%s\n", _errbuf);
        }
    }
}

PacketCapture::~PacketCapture() {
    if (_handle != nullptr)
        close();
}
SqlInjectionDetector sql_detetor;
RuleLibrary rule_library;

void process_packet(u_char *arg, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    PacketCapture::_num++;
    // 打印抓取的数据包长度和实际发送的数据包长度
    // std::cout << "caplen: " << pkthdr->caplen << "\t len: " << pkthdr->len << std::endl;
    TimeStamp ts;
    // printf("%d\t", PacketCapture::_num);
    // ts.show_time(pkthdr);

    EthernetII ethernet(packet, ts.caplen(pkthdr));
    // std::cout << ethernet.type_string() << "  ";
    // if (ethernet.is_ipv6())
    // {
    //     std::cout << "This is ipv6 packet" << std::endl;
    // }
    if (ethernet.is_ipv4()) {
        IPv4Packet ip(ethernet.payload(), ethernet.payload_size());
        ip.parse();
        printf("%d\t", PacketCapture::_num);
        ts.show_time(pkthdr);
        std::cout << "source ip: " << ip.source_ip().to_string() << "  target ip: " << ip.target_ip().to_string()
                  << " " << std::endl;
        //<< to_string(ip.protocol_type()) << "\n";
        ip.debuf_info();
        switch (ip.protocol_type()) {
        // case ProtocolType::ICMP:
        //     printf("ICMP\n");
        //     break;
        case ProtocolType::TCP: {
            // printf("TCP\n");
            TcpPacket tcp(ip.payload(), ip.payload_size());
            // tcp.debug_info();
            if (tcp.is_http_message()) {
                printf("%d\t", PacketCapture::_num);
                ts.show_time(pkthdr);
                std::cout << "source ip: " << ip.source_ip().to_string() << "  target ip: " << ip.target_ip().to_string()
                          << " ,TCP " << std::endl;
                std::cout << "This is HTTP packet!" << std::endl;
                HttpParser http(tcp.payload(), tcp.payload_size());
                if (http.is_http_request()) {
                    // 进行安全检测
                    std::cout << "http请求资源路径： " << http.request_path() << std::endl;
                    std::cout << "http请求方法： " << http.request_method() << std::endl;
                    std::cout << "http请求实体： " << http.request_body() << std::endl;

                    if (http.request_method() == "GET")
                        rule_library.detect_all(http.request_path());
                    // sql_detetor.detect_sql_injection(http.request_path());
                    if (http.request_method() == "POST")
                        rule_library.detect_all(http.request_body());
                    // sql_detetor.detect_sql_injection(http.request_body());
                }
                // http.print_http_message();
            }
        } break;
        /*case ProtocolType::UDP:
        {
            UdpPacket udp(ip.payload(), ip.payload_size());
            udp.parse();

            // printf("%d\t", PacketCapture::_num);
            // ts.show_time(pkthdr);
            // std::cout << ethernet.type_string() << "  ";
            // std::cout << "source ip: " << ip.source_ip().to_string() << "  target ip: " << ip.target_ip().to_string()
            //           << " ";
            // std::cout << to_string(ip.protocol_type()) << " source_port: " << udp.source_port() << " target port: "
            //           << udp.target_port() << std::endl;

            // udp.debug_info();
            if (udp.is_dns())
            {
                DnsView dns(udp.payload(), udp.payload_size());
                dns.debug_info();
            }
        }
        break;*/
        default:
            break;
        }
    }
    // ethernet.debug_info();
    /*
    // 测试arp数据包
    if (ethernet.is_arp())
    {
        std::cout << "==============================================" << std::endl;
        ArpPacket arp(ethernet.payload(), ethernet.payload_size());
        arp.parse();
        auto source_ip = arp.source_ip().to_string();
        auto target_ip = arp.target_ip().to_string();

        switch (arp.op_type())
        {
        case ARP_REQUEST:
        {
            std::cout << "[ARP请求] " << source_ip << "(";
            arp.format_mac(arp.source_mac());
            std::cout << ") 查询 " << target_ip << " 的MAC地址" << std::endl;
        }
        break;
        case ARP_REPLY:
        {
            std::cout << "[ARP响应] " << target_ip << "(";
            arp.format_mac(arp.target_mac());
            std::cout << ") 回复 " << source_ip << "(";
            arp.format_mac(arp.source_mac());
            std::cout << ") ：" << target_ip << " 的MAC地址在我这里" << std::endl;
        }
        break;
        default:
            break;
        }
        // arp.debug_info();
        PacketCapture::arp_map.emplace(arp.source_ip(), arp.format_mac_address(arp.source_mac()));
    }*/
    // sleep(1);
}

void PacketCapture::run() {
    // 获取数据包
    if (pcap_loop(_handle, -1, process_packet, nullptr) < 0)
        throw IOException("pcap_loop");
}

void PacketCapture::close() {
    pcap_close(_handle);
}
