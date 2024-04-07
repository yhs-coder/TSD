#include "dns.h"

void Qname::debug_info(int data_type)
{
    switch (data_type)
    {
        // 数据类型是ipv地址
        case DNS_PARSE_DATA_TYPE_IPV4:
            {
                struct in_addr addr;
                addr.s_addr = this->ip;
                char *ipv4_str = inet_ntoa(addr);
                if (ipv4_str != NULL)
                {
                    printf("%s", ipv4_str);
                }
            }
            break;
        case DNS_PARSE_DATA_TYPE_DOMAIN:
            printf("%s", this->domain);
            break;
        default:
            printf("unknown");
    }
}

/*
 *  重置成员
 * */
void DnsAreaPublic::reset()
{
    _query_type = _query_class = 0;
    _size = 0;
    _data_type = DNS_PARSE_DATA_TYPE_UNDEFINED;
}

/*
 * 解析dns查询名，查询类型、查询类
 * @param dns_start -- dns报文起始
 * @param dns_size  -- dns报文总长度
 * @param offset    -- 查询/回答区域在dns报文中的偏移
 * @parse_data_type -- 解析数据类型
 * return ErrorCode
 * */
int DnsAreaPublic::parse(void *dns_start, uint32_t dns_size, uint32_t offset, int parse_data_type)
{
    // 检查dns_start起始地址和偏移量是否超出dns大小
    JUDGE_RETURN(dns_start == nullptr || offset >= dns_size, DNS_PARSE_ERROR_DATA_TYPE_ARGS);
    JUDGE_RETURN(!IS_VALID_DNS_PARSE_DATA_TYPE(parse_data_type), DNS_PARSE_ERROR_UNSPPORTED_DATA_TYPE); 
    // 判断主机字节序是否为小端
    bool is_opposite = get_endian() == LITTLE_ENDIAN;    
    // 移动到查询/回答区域
    u_char *data = (u_char*)dns_start + offset;
    reset();
    _size = Dns::parse_dns_qname(dns_start, dns_size, offset, _name, parse_data_type);
    JUDGE_RETURN(_size <= 0, DNS_PARSE_ERROR_PARSE_DOMAIN);
    _data_type = parse_data_type;

    /* 查询类型和查询类  */
    data += _size;
    memcpy(&(_query_type), data, sizeof(_query_type));
    data += sizeof(_query_type);
    memcpy(&(_query_class), data, sizeof(_query_class));
    data += sizeof(_query_class);

    //  如果当前的主机是小端字节序，就需要将网络数据包中的网络字节序转换为主机字节序（（小端）
    if (is_opposite)
    {
        _query_type = ntohs(_query_type);
        _query_class = ntohs(_query_class);
    }
   JUDGE_RETURN(!IS_VALID_DNS_QUERY_TYPE(_query_type), DNS_PARSE_ERROR_QUERY_TYPE);
   JUDGE_RETURN(!IS_VALID_DNS_QUERY_TYPE(_query_class), DNS_PARSE_ERROR_QUERY_CLASS);
   return PARSE_SUCCESS;
}

void DnsAreaPublic::debug_info()
{
    _name.debug_info(_data_type);
}

/*
 * 获取资源数据的类型
*/
int DnsResRecordArea::get_resource_data_type()
{
    if (_query_data._query_type == DNS_QUERY_TYPE_A && _query_data._query_class == DNS_QUERY_CLASS_IN)
    {
        return DNS_PARSE_DATA_TYPE_IPV4;
    }
    else if (_query_data._query_type == DNS_QUERY_TYPE_CNAME && _query_data._query_class == DNS_QUERY_CLASS_IN)
    {
        return DNS_PARSE_DATA_TYPE_DOMAIN;
    }
    else if (_query_data._query_type == DNS_QUERY_TYPE_AAAA && _query_data._query_class == DNS_QUERY_CLASS_IN)
    {
        return DNS_PARSE_DATA_TYPE_IPV6;
    }
    return DNS_PARSE_DATA_TYPE_UNDEFINED;
}


/*
 * 解析dns资源记录区域
 * @param dns_start -- dns报文起始
 * @param dns_size  -- dns报文总长度
 * @param offset    -- 在dns报文中的偏移
 * return ErrorCode
 * */
int DnsResRecordArea::parse(void *dns_start, uint32_t dns_size, uint32_t offset, int parse_data_type)
{
    JUDGE_RETURN(dns_start == nullptr || offset >= dns_size, DNS_PARSE_ERROR_DATA_TYPE_ARGS);
    JUDGE_RETURN(!IS_VALID_DNS_PARSE_DATA_TYPE(parse_data_type), DNS_PARSE_ERROR_UNSPPORTED_DATA_TYPE);
    bool is_opposite = get_endian() == LITTLE_ENDIAN;
    reset();
    int error_code = _query_data.parse(dns_start, dns_size, offset, parse_data_type);
    JUDGE_RETURN(error_code != PARSE_SUCCESS, error_code);

    u_char* data = (u_char*)dns_start + offset + _query_data._size;
    memcpy(&(_ttl), data, sizeof(_ttl));
    data += sizeof(_ttl);
    memcpy(&(_res_data_size), data, sizeof(_res_data_size));
    data += sizeof(_res_data_size);

    if (is_opposite)
    {
        _ttl = ntohl(_ttl);
        _res_data_size = ntohs(_res_data_size);
    }
    JUDGE_RETURN(_res_data_size >= DNS_DOMAIN_BUFFER_SIZE, DNS_PARSE_ERROR_RESOURCE_RECORD_AREA);
    int resource_data_type = get_resource_data_type();
    JUDGE_RETURN(!IS_VALID_DNS_PARSE_DATA_TYPE(resource_data_type), DNS_PARSE_ERROR_UNSPPORTED_DATA_TYPE);
    int parse_real_size = Dns::parse_dns_qname(dns_start, dns_size, data - (u_char*)dns_start, _res_data, resource_data_type);
    JUDGE_RETURN(parse_real_size == 0, DNS_PARSE_ERROR_RESOURCE_DATA);
    _size = _query_data._size + sizeof(_ttl) + sizeof(_res_data_size) + _res_data_size;
    return PARSE_SUCCESS;
}

void DnsResRecordArea::reset()
{
    _res_data_size = 0;
    _ttl = _size = 0;
    _query_data.reset();
}

void DnsResRecordArea::debug_info()
{
    _query_data.debug_info();
    printf("=>");
    _res_data.debug_info(get_resource_data_type());
}

Dns::Dns() : Protocol(false)
{ 
    _is_opposite_byte = get_endian() == LITTLE_ENDIAN;
}

Dns::~Dns()
{}

/*
 * 检查dns头部是够有效 
 **/
bool Dns::is_valid_dns_header()
{
    // 检查opcode字段
    JUDGE_RETURN(!IS_VALID_DNS_OPCODE(_op_code), false);
    // 检查返回码
    return IS_VALID_DNS_REPLY_CODE(_reply_code);
}

/*
 * 解析dns数据区域
 * @param - dns_start 报文起始地址
 * @param - size报文总长度
 * @return ErrorCode
 * @remark 此函数待优化
 */

int Dns::parse_dns_data_area(void *dns_start, uint32_t size)
{
    int error_code = PARSE_SUCCESS;
    uint32_t offset = DNS_HEADER_SIZE;
    // 标准查询并且返回码为0无差错，就进行解析
    if (_op_code == DNS_OPCODE_STANDARD_QUERY && _reply_code == DNS_REPLY_CODE_SUCCESS)
    {
        // 解析查询区域
        DnsAreaPublic area_public;
        for (int index = 0; index < _question_amount; ++index)
        {
            error_code = area_public.parse(dns_start, size, offset, DNS_PARSE_DATA_TYPE_DOMAIN);
            JUDGE_RETURN(error_code != PARSE_SUCCESS, error_code);
            offset += area_public._size;
            _questions.push_back(area_public);
        }

        DnsResRecordArea res_record_area;
        char str_ip[STR_IPV4_BUFFER_SIZE];
        for (int index = 0; index < _answer_amount; index++)
        {
            error_code = res_record_area.parse(dns_start, size, offset, DNS_PARSE_DATA_TYPE_DOMAIN);
            JUDGE_RETURN(error_code != PARSE_SUCCESS, error_code);
            offset += res_record_area._size;
            _answers.push_back(res_record_area);
        }
        for (int index = 0; index < _authority_amount; index++)
        {
            error_code = res_record_area.parse(dns_start, size, offset, DNS_PARSE_DATA_TYPE_DOMAIN);
            JUDGE_RETURN(error_code != PARSE_SUCCESS, error_code);
            offset += res_record_area._size;
            _authoritys.push_back(res_record_area);
        }
        for (int index = 0; index < _additional_amount; index++)
        {
            error_code = res_record_area.parse(dns_start, size, offset, DNS_PARSE_DATA_TYPE_DOMAIN);
            JUDGE_RETURN(error_code != PARSE_SUCCESS, error_code);
            offset += res_record_area._size;
            _additionals.push_back(res_record_area);
        }
    }
    return PARSE_SUCCESS;
}


const std::type_info &Dns::get_class_type_info()
{
    return typeid(*this);
}


bool Dns::check_buffer_length(void *buffer, uint32_t size)
{
    return size >= DNS_HEADER_SIZE;
}

int Dns::parse(void *buffer, uint32_t size)
{
    JUDGE_RETURN(!check_buffer_length(buffer, size), ETHERNET_PARSE_ERROR_MIN_LENGTH);
    u_char *data = (u_char*)buffer; 
    memcpy(&(_transaction_id), data, sizeof(_transaction_id));
    data += sizeof(_transaction_id);

    /* get flags */
    u_char first_byte = *data++;
    u_char second_byte = *data++;
    _is_response = DNS_QR_FLAG_GETTER(first_byte);
    _op_code = DNS_OPCODE_GETTER(first_byte);
    _is_authenticated_answer = DNS_AUTHENTICATED_ANSWER_FLAG_GETTER(first_byte);
    _is_truncated = DNS_TRUNCATED_FLAG_GETTER(first_byte);
    _is_recursion_disired = DNS_RECURSION_DISIRED_FLAG_GETTER(second_byte);
    _is_recursion_available = DNS_RECURSION_AVAILABLE_FLAG_GETTER(second_byte);
    _reply_code = DNS_REPLY_CODE_GETTER(second_byte);

    memcpy(&(_question_amount), data, sizeof(_question_amount));
    data += sizeof(_question_amount);
    memcpy(&(_answer_amount), data, sizeof(_answer_amount));
    data += sizeof(_answer_amount);
    memcpy(&(_authority_amount), data, sizeof(_authority_amount));
    data += sizeof(_authority_amount);
    memcpy(&(_additional_amount), data, sizeof(_additional_amount));
    data += sizeof(_additional_amount);

    _is_opposite_byte ? opposite_byte() : 1;
    JUDGE_RETURN(!is_valid_dns_header(), DNS_PARSE_ERROR_HEADER_INVALID);
    int error_code = parse_dns_data_area(buffer, size);
    return error_code;
}

int Dns::opposite_byte()
{
    _transaction_id = ntohs(_transaction_id);
    _question_amount = ntohs(_question_amount);
    _answer_amount = ntohs(_answer_amount);
    _authority_amount = ntohs(_authority_amount);
    _additional_amount = ntohs(_additional_amount);
    return 0;
}

int Dns::debug_info()
{
    printf("DNS: %s: ", (_is_response == DNS_QUERY_FLAG ? "query" : "response"));
    printf("questions: ");
    for (auto it : _questions)
    {
        it.debug_info();
        printf(" ");
    }
    printf(": answers: ");
    for (auto it : _answers)
    {
        it.debug_info();
        printf(" ");
    }
    printf("\n");
}

/*
 * 解析dns报文资源数据
 * @param dns_start  -- dns报文起始
 * @param dns_size   -- dms报文总长度
 * @param offset     -- 查询名在dns报文中的偏移
 * @return int       -- 实际存储用的字节长度
 * */

int Dns::parse_dns_qname(void *dns_start, uint32_t dns_size, uint32_t offset, Qname &data, int parse_data_type)
{
    bool is_opposite = get_endian() == LITTLE_ENDIAN;
    if (parse_data_type == DNS_PARSE_DATA_TYPE_DOMAIN)
    {
        memset(data.domain, 0, DNS_DOMAIN_BUFFER_SIZE);
        return Dns::parse_dns_domain(dns_start, dns_size, offset, data.domain);
    }
    else 
    {
        memcpy(&(data.ip), (u_char*)dns_start + offset, sizeof(data.ip));
        is_opposite ? data.ip = ntohl(data.ip) : 0;
        return sizeof(data.ip);
    }
}

/*
 * 解析dns报文查询名字段（域名）
 * @param dns_start     -- dns报文起始
 * @param dns_size      -- dns报文总长度
 * @param offset        -- 查询名在dns报文中的偏移
 * @param data          -- 存储查询名/域名的字符数组
 * #return int          -- 实际存储用的字节长度
 * */

int Dns::parse_dns_domain(void *dns_start, uint32_t dns_size, uint32_t offset, uint8_t *data)
{
    JUDGE_RETURN(dns_start == nullptr || offset + DNS_QUERY_NAME_MIN_SIZE > dns_size, DNS_PARSE_ERROR_QUERY_NAME_ARGS);
    // 指针移动到查询名/域名 的字段位置
    u_char *per_cname = (u_char*)dns_start + offset;
    u_char *ptr_copy = data;
    uint16_t ptr_offset = 0;        // dns报文偏移
    bool is_jump = false;           // 是否已经用过指针偏移
    uint32_t per_size = 1;          // 用来表示当前域名部分的长度，初始值设为1， 
    uint32_t real_size = 0;         // real_size 为数据的实际存储长度，指针跳转后不在计数
    bool is_opposite = get_endian() == LITTLE_ENDIAN;

    for (; per_size > 0; ptr_copy += per_size + 1, per_cname  += per_size)
    {
        /* 查看报文域名是否重复(是否有偏移指针)，指针跳转 */
        if (IS_DNS_CNAME_OFFSET_PTR(*per_cname))
        {
            memcpy(&ptr_offset, per_cname, sizeof(ptr_offset));
            is_opposite ? ptr_offset = ntohs(ptr_offset) : ptr_offset;
            ptr_offset = DNS_CNAME_OFFSET_GETTER(ptr_offset);

            JUDGE_RETURN(ptr_offset + DNS_QUERY_NAME_MIN_SIZE > dns_size, 0);
            // per_cname 移动到第一次出现域名的地方
            per_cname = (u_char*)dns_start + ptr_offset;
            is_jump ? real_size : real_size += sizeof(ptr_offset);
            is_jump = true;
        }

        /*
            冷知识 ：DNS 报文中每个域名部分的开头都是一个表示长度的字节，表示当前部分域名（标号）的长度
            per_size = *per_cname++; *per_cname拿到当前域名部分的长度，赋值给per_size;然后per_cname指针后移,指向下一个字节，即真正的域名
        */
        /* 单个域名拷贝 */
        per_size = *per_cname++;
        if (per_size > 0)
        {
            // 此时per_name指向域名，将per_size的长度拷贝给ptr_copy
            memcpy(ptr_copy, per_cname, per_size);
            // 在拷贝完部分域名后，加上一个点，表示域名部分的结束
            *(ptr_copy + per_size) = '.';
            // + 1表示是点号的长度
            is_jump ? real_size : (real_size += per_size + 1);
        }
    }

    real_size >= 2 ? ptr_copy -= 2 : ptr_copy;
    *ptr_copy = '\0';
    is_jump ? real_size : real_size += 1;       // 加上末尾的0x00
    return real_size;

}
