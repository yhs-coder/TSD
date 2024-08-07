#pragma once
#include <arpa/inet.h>
#include <netdb.h>  // 相同的头文件也用于 C++
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
// 加密头文件
#include <openssl/err.h>       // OpenSSL库错误处理相关的函数和类型定义
#include <openssl/ossl_typ.h>  //  OpenSSL库中基本数据类型定义
#include <openssl/ssl.h>       // OpenSSL 中用于 SSL 和 TLS 通信的核心头文件
#define INVALID_SOCKET -1

class SmtpBase {
protected:
    struct EmailInfo {
        std::string smtp_server;                       // smtp服务器ip地址
        std::string server_port;                       // smtp服务端口
        std::string charset;                           // 邮件编码
        std::string sender;                            // 发件人
        std::string sender_email;                      // 发送者邮箱
        std::string password;                          // smtp服务器授权码
        std::string recipient;                         // 收件人
        std::string recipient_email;                   // 收件人邮箱
        std::map<std::string, std::string> recv_list;  // 收件人列表<email, name>
        std::string subject;                           // 邮件主题
        std::string message;                           // 邮件内容
    };

public:
    virtual ~SmtpBase() {}

    // /**
    //  * @brief 简单发送文本邮件
    //  * @param   from 发送者的帐号
    //  * @param   password 发送者密码
    //  * @param   to 收件人
    //  * @param   subject 主题
    //  * @param   message  邮件内容
    //  */
    virtual int send_email(const std::string& from, const std::string& password, const std::string& to, const std::string& subject, const std::string& message) = 0;

    // 获取错误消息
    std::string get_last_error() {
        return _last_error_msg;
    }

    // 接收内容
    virtual int recv_msg(void* buf, int num) = 0;
    // 发送内容
    virtual int send_msg(const void* buf, int num) = 0;
    // 开始/结束连接
    virtual int start_connect() = 0;
    virtual int end_connect() = 0;

protected:
    std::string _last_error_msg;  // 最新错误消息
};

// 未加密的邮件发送，端口25
class SmtpEmail : public SmtpBase {
public:
    SmtpEmail(const std::string& email_host, const std::string& port);
    ~SmtpEmail();
    // int send_mail(const std::string& from, const std::string& password, const std::string& to, const std::string& subject, const std::string& message);
    int send_email(const std::string& from, const std::string& password, const std::string& to, const std::string& subject, const std::string& message);

protected:
    int recv_msg(void* buf, int num);
    // 发送内容
    int send_msg(const void* buf, int num);
    // 开始连接，成功返回0
    int start_connect();
    // 结束连接
    int end_connect();
    virtual std::string get_email_body(const EmailInfo& info);

private:
    // smtp通信
    int smtp_comunicate(const EmailInfo& info);

protected:
    addrinfo* _addrinfo;
    int _socket_fd;      // socket连接描述符
    std::string _host;   // smtp服务器地址
    std::string _port;   // smtp服务端口
    bool _is_connected;  // 是否连接
};

// 使用25端口，未加密的简单邮件发送
class EasySmtpEmail : public SmtpEmail {
public:
    using SmtpEmail::SmtpEmail;
    virtual std::string get_email_body(const EmailInfo& info);
};

// ssl加密处理，端口465
class SslSmtpEmail : public SmtpEmail {
public:
    using SmtpEmail::SmtpEmail;
    // 开始连接，成功返回0
    int start_connect();
    // 结束连接
    int end_connect();
    ~SslSmtpEmail();

protected:
    // 接收内容
    int recv_msg(void* buf, int num);
    // 发送内容
    int send_msg(const void* buf, int num);

private:
    // SSL_CTX 类型是一个非常重要的数据结构，它代表了 SSL/TLS 协议的上下文环境。
    // 上下文环境包含了配置信息、证书、私钥、密码算法、会话缓存、以及 SSL/TLS 连接所需的其他设置。
    SSL_CTX* _ctx;
    SSL* _ssl;
    // std::string get_email_body(const EmailInfo& info);
};

// // 将一个容器（如向量、列表等）中的元素连接成一个字符串，元素之间用指定的分隔符 delim 分隔
// template <typename T>
// std::string join(T& data, const std::string& delim) {
//     if (data.size() <= 0) {
//         return std::string();
//     }
//     std::stringstream ss;
//     for (size_t i = 0; i < vecData.size(); ++i) {
//         if (i > 0)  // 添加分隔符，除了第一个元素
//             ss << delim;
//         ss << vecData[i];
//     }
//     return ss.str();
// }

// // base64编码
// static char* base64_encode(char const* orig_singed, unsigned orig_length) {
//     static const char base64_char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//     unsigned char const* orig = (unsigned char const*)orig_singed;  // in case any input bytes have the MSB set
//     if (orig == NULL)
//         return NULL;

//     unsigned const num_orig_24bit_values = orig_length / 3;
//     bool have_padding = orig_length > num_orig_24bit_values * 3;
//     bool have_padding2 = orig_length == num_orig_24bit_values * 3 + 2;
//     unsigned const num_result_bytes = 4 * (num_orig_24bit_values + have_padding);
//     char* result = new char[num_result_bytes + 3];  // allow for trailing '/0'

//     // Map each full group of 3 input bytes into 4 output base-64 characters:
//     unsigned i;
//     for (i = 0; i < num_orig_24bit_values; ++i) {
//         result[4 * i + 0] = base64_char[(orig[3 * i] >> 2) & 0x3F];
//         result[4 * i + 1] = base64_char[(((orig[3 * i] & 0x3) << 4) | (orig[3 * i + 1] >> 4)) & 0x3F];
//         result[4 * i + 2] = base64_char[((orig[3 * i + 1] << 2) | (orig[3 * i + 2] >> 6)) & 0x3F];
//         result[4 * i + 3] = base64_char[orig[3 * i + 2] & 0x3F];
//     }

//     // Now, take padding into account.  (Note: i == num_orig_24bit_values)
//     if (have_padding) {
//         result[4 * i + 0] = base64_char[(orig[3 * i] >> 2) & 0x3F];
//         if (have_padding2) {
//             result[4 * i + 1] = base64_char[(((orig[3 * i] & 0x3) << 4) | (orig[3 * i + 1] >> 4)) & 0x3F];
//             result[4 * i + 2] = base64_char[(orig[3 * i + 1] << 2) & 0x3F];
//         } else {
//             result[4 * i + 1] = base64_char[((orig[3 * i] & 0x3) << 4) & 0x3F];
//             result[4 * i + 2] = '=';
//         }
//         result[4 * i + 3] = '=';
//     }

//     result[num_result_bytes] = '\0';
//     return result;
// }

// int SmtpEmail::start_connect() {
//     _socket_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (_socket_fd == INVALID_SOCKET) {
//         _last_error_msg = "error on creating socket fd.";
//         return -1;
//     }
//     addrinfo in_addrinfo = {0};
//     in_addrinfo.ai_family = AF_INET;
//     in_addrinfo.ai_socktype = SOCK_STREAM;
//     printf("host:%s port:%s \n", _host.c_str(), _port.c_str());
//     // 获取网络地址信息，跨平台
//     if (getaddrinfo(_host.c_str(), _port.c_str(), &in_addrinfo, &_addrinfo) != 0) {
//         _last_error_msg = "error on calling getadrrinfo().";
//         return -2;
//     }
//     struct addrinfo* p = _addrinfo;
//     for (; p != nullptr; p = p->ai_next) {
//         struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
//         void* addr = &(ipv4->sin_addr);
//         char ipstr[INET_ADDRSTRLEN];
//         inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
//         std::cout << "IP Address: " << ipstr << std::endl;
//     }
//     if (connect(_socket_fd, _addrinfo->ai_addr, _addrinfo->ai_addrlen) != 0) {
//         _last_error_msg = "error on calling connect().";
//         return -3;
//     }
//     return 0;
// }

// int SmtpEmail::end_connect() {
//     freeaddrinfo(_addrinfo);
//     close(_socket_fd);
//     return 0;
// }

// int SmtpEmail::recv_msg(void* buf, int num) {
//     return recv(_socket_fd, (char*)buf, num, 0);
// }

// int SmtpEmail::send_msg(const void* buf, int num) {
//     return send(_socket_fd, (char*)buf, num, 0);
// }

// int SmtpEmail::send_email(const std::string& from, const std::string& password, const std::string& to, const std::string& subject, const std::string& message) {
//     EmailInfo info;
//     info.charset = "UTF-8";
//     info.sender = from;
//     info.password = password;
//     info.sender_email = from;
//     info.recipient_email = to;
//     info.recv_list[to] = "";
//     info.subject = subject;
//     info.message = message;
//     // std::cout << info.charset << " " << info.sender << " " << info.password << " " << info.sender_email
//     //           << " " << info.recipient_email << " " << info.subject << " " << info.message << std::endl;
//     return smtp_comunicate(info);
// }

// int SmtpEmail::smtp_comunicate(const EmailInfo& info) {
//     if (start_connect() != 0)
//         return -1;
//     char buf[1024] = {0};
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     // 220 表示邮件服务器已准备好接受客户端的命令
//     if (strncmp(buf, "220", 3) != 0) {
//         _last_error_msg = buf;
//         return -220;
//     }
//     // 向服务器发送helo
//     std::string command = "HELO MSG\r\n";
//     send_msg(command.c_str(), command.size());

//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     // 250 OK - 命令执行成功。
//     if (strncmp(buf, "250", 3) != 0) {
//         _last_error_msg = buf;
//         return -250;
//     }
//     // 进行登录验证
//     // command = "AUTH Login ";
//     // std::string auth = '\0' + info.sender_email + '\0' + info.password;
//     // command += base64_encode(auth.data(), auth.size());
//     // command += "\r\n";
//     // send_msg(command.c_str(), command.size());
//     command = "AUTH LOGIN\r\n";
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;

//     command = base64_encode(info.sender_email.data(), info.sender_email.size());
//     command += "\r\n";
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;

//     command = base64_encode(info.password.data(), info.password.size());
//     command += "\r\n";
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     // 235 授权登陆成功Authentication successful
//     if (strncmp(buf, "235", 3) != 0) {
//         _last_error_msg = buf;
//         return -235;
//     }

//     // 设置邮件发送者的邮件地址
//     command = "MAIL FROM:<" + info.sender_email + ">\r\n";
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     // 250 OK - 命令执行成功
//     if (strncmp(buf, "250", 3) != 0) {
//         _last_error_msg = buf;
//         return -250;
//     }

//     // 设置邮件接收者的邮件地址
//     command = "RCPT TO:<" + info.recipient_email + ">\r\n";
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     // 250 OK - 命令执行成功
//     if (strncmp(buf, "250", 3) != 0) {
//         _last_error_msg = buf;
//         return -250;
//     }

//     // 准备发送邮件数据
//     command = "DATA\r\n";
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     // 354 End data with <CR><LF>.<CR><LF>
//     if (strncmp(buf, "354", 3) != 0) {
//         _last_error_msg = buf;
//         return -354;
//     }
//     command = std::move(get_email_body(info));
//     send_msg(command.c_str(), command.size());
//     memset(buf, 0, 1000);
//     recv_msg(buf, 999);
//     std::cout << buf << std::endl;
//     if (strncmp(buf, "250", 3) != 0) {
//         _last_error_msg = buf;
//         return -250;
//     }
//     // 结束邮件发送
//     command = "QUIT\r\n";
//     send_msg(command.c_str(), command.size());
//     end_connect();
//     return 0;
// }

// std::string SmtpEmail::get_email_body(const EmailInfo& info) {
//     // 设定邮件的发送者名称、接收者名称、邮件主题，邮件内容
//     std::ostringstream omessage;
//     // =?UTF-8? 表示字符集是 UTF-8, ?= 表示编码文字的结束
//     // ?b?后面跟随的是一个Base64编码的字符串
//     omessage << "From: =?" << info.charset << "?b?" << base64_encode(info.sender.c_str(), info.sender.size())
//              << "?= <" << info.sender_email << ">\r\n";
//     omessage << "To: =?" << info.charset << "?b?" << base64_encode(info.recipient.c_str(), info.recipient.size())
//              << "?= <" << info.recipient_email << ">\r\n";

//     omessage << "Subject: =?" << info.charset << "?b?" << base64_encode(info.subject.c_str(), info.subject.size())
//              << "?=\r\n";
//     omessage << "\r\n";
//     // omessage << base64_encode(info.message.c_str(), info.message.size());
//     omessage << info.message.c_str();
//     omessage << "\r\n\r\n";
//     omessage << "\r\n.\r\n";
//     return omessage.str();
// }

// SmtpEmail::SmtpEmail(const std::string& email_host, const std::string& port)
//     : _host(email_host), _port(port) {}

// SmtpEmail::~SmtpEmail() {}

// std::string EasySmtpEmail::get_email_body(const EmailInfo& info) {
//     // 设定邮件的发送者名称、接收者名称、邮件主题，邮件内容
//     std::ostringstream omessage;
//     // =?UTF-8? 表示字符集是 UTF-8, ?= 表示编码文字的结束
//     // ?b?后面跟随的是一个Base64编码的字符串
//     omessage << "From: =?" << info.charset << "?b?" << base64_encode(info.sender.c_str(), info.sender.size())
//              << "?= <" << info.sender_email << ">\r\n";
//     omessage << "To: =?" << info.charset << "?b?" << base64_encode(info.recipient.c_str(), info.recipient.size())
//              << "?= <" << info.recipient_email << ">\r\n";

//     omessage << "Subject: =?" << info.charset << "?b?" << base64_encode(info.subject.c_str(), info.subject.size())
//              << "?=\r\n";
//     omessage << "\r\n";
//     omessage << base64_encode(info.message.c_str(), info.message.size());
//     omessage << "\r\n\r\n";
//     omessage << "\r\n.\r\n";
//     return omessage.str();
// }

// /*************************************SslSmtpEmail类成员函数*********************************************************************/

// SslSmtpEmail::~SslSmtpEmail() {}

// int SslSmtpEmail::start_connect() {
//     if (SmtpEmail::start_connect() == 0) {
//         // 初始化 SSL 库
//         SSL_library_init();
//         // 添加所有可用的加密算法到 OpenSSL 的算法管理器
//         OpenSSL_add_all_algorithms();

//         // 加载错误字符串，使得在后续的 OpenSSL 调用中，如果发生错误，
//         // 可以通过 ERR_error_string 函数将错误码转换为人类可读的错误信息
//         SSL_load_error_strings();
//         // 创建了一个新的 SSL 上下文
//         // SSLv23_client_method() 指定了上下文的方法为 SSL/TLS 客户端方法，
//         // 这意味着它将用于创建一个客户端 SSL 对象。
//         // _ctx 是一个指向 SSL_CTX 结构的指针，它在后续步骤中会被用来配置 SSL 连接。
//         _ctx = SSL_CTX_new(SSLv23_client_method());

//         // 基于刚刚创建的 SSL 上下文 _ctx 创建了一个新的 SSL 对象_ssl。
//         // SSL 对象包含了 SSL 连接的状态，如握手状态、加密上下文等
//         _ssl = SSL_new(_ctx);
//         // 将一个已经建立的 socket 文件描述符 _socket_fd 关联到SSL对象_ssl上。
//         // _socket_fd 必须是一个已经连接到服务器的 socket。通过这个调用，SSL 对象知道了数据应该通过哪个 socket 来传输。
//         SSL_set_fd(_ssl, _socket_fd);
//         // 函数尝试完成 SSL 握手过程，以便在客户端和服务器之间建立一个加密的 SSL 连接
//         SSL_connect(_ssl);
//     }
//     return 0;
// }

// int SslSmtpEmail::end_connect() {
//     // 释放资源
//     SSL_shutdown(_ssl);
//     SSL_free(_ssl);
//     SSL_CTX_free(_ctx);

//     SmtpEmail::end_connect();
//     return 0;
// }

// int SslSmtpEmail::recv_msg(void* buf, int num) {
//     // OpenSSL 库中用于从SSL连接中读取数据的函数
//     // SSL_read 是非阻塞的，如果当前没有数据可读，它会立即返回 0。
//     return SSL_read(_ssl, buf, num);
// }
// int SslSmtpEmail::send_msg(const void* buf, int num) {
//     // OpenSSL库中用于从SSL连接中写入数据的函数
//     // 用于执行加密数据的发送操作
//     return SSL_write(_ssl, buf, num);
// }
