#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,   // 正在解析请求首行
        HEADERS,        // 头
        BODY,           // 体
        FINISH,         // 完成
    };

    enum HTTP_CODE {
        NO_REQUEST = 0, //没有请求
        GET_REQUEST,    //获取请求
        BAD_REQUEST,    //错误请求
        NO_RESOURSE,    //没有资源
        FORBIDDENT_REQUEST, //禁止访问的请求
        FILE_REQUEST,       //请求一个文件
        INTERNAL_ERROR,     //内部错误
        CLOSED_CONNECTION,  //连接关闭
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;   //是否保持活状态

private:
    bool ParseRequestLine_(const std::string& line);    //解析请求行
    void ParseHeader_(const std::string& line);         //解析请求头
    void ParseBody_(const std::string& line);           //解析请求体

    void ParsePath_();              //解析路径
    void ParsePost_();              //解析端口
    void ParseFromUrlencoded_();    //从url解析编码

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);  //用户验证

    PARSE_STATE state_;     // 解析的状态
    std::string method_, path_, version_, body_;    // 请求方法，请求路径，协议版本，请求体
    std::unordered_map<std::string, std::string> header_;   // 请求头
    std::unordered_map<std::string, std::string> post_;     // post请求表单数据

    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认的网页
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; 
    static int ConverHex(char ch);  // 转换成十六进制
};


#endif //HTTP_REQUEST_H