#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       //缓冲区剩余可写空间大小
    size_t ReadableBytes() const ;      //缓冲区剩余可读空间大小
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();              //指针首地址
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);    //为缓冲区开辟新空间，自动增长的缓冲区

    std::vector<char> buffer_;  // 具体装数据的vector
    std::atomic<std::size_t> readPos_;  // 目前读到的位置
    std::atomic<std::size_t> writePos_; // 目前写到的位置
};

#endif //BUFFER_H